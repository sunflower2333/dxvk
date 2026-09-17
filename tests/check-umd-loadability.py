#!/usr/bin/env python3
"""Loadability policy for the native WDDM UMD.

A D3D UMD that the runtime can actually activate has to satisfy three coupled
properties.  They are checked together here because satisfying one alone is
what produces DXGI_ERROR_UNSUPPORTED with no diagnostic:

  1. mandatory-table   every D3D10DDI_DEVICEFUNCS slot the WDK declares for a
                       hardware UMD is non-null.  The runtime calls them
                       unconditionally; a null slot is an access violation.
  2. version-coherence a DDI version may be advertised only while the
                       admission gate is open, and the gate may only open on a
                       complete table.
  3. caps-coherence    GetCaps must encode a non-zero 3DPIPELINESUPPORT level
                       exactly when the gate is open.  A zeroed caps structure
                       makes the runtime fail device creation outright.

Run with no arguments.  --wdk points at an alternative d3d10umddi.h.
"""

import argparse
import os
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

# Slots declared only under D3D10PSGP belong to a software rasterizer plug-in.
# A hardware UMD has no such fields and must not be asked to fill them.
PSGP = "D3D10PSGP"

WDK_SEARCH = [
    "reference/codes/mesa-wddm2-residency-20260913/build-local/wdk/c/Include/10.0.26100.0/um/d3d10umddi.h",
    "reference/codes/mesa-wddm2-residency-20260913/build-local/ci-headers/wdkum/d3d10umddi.h",
]


class Failure(RuntimeError):
    pass


def find_wdk_header(explicit):
    if explicit:
        path = Path(explicit)
        if not path.is_file():
            raise Failure(f"no d3d10umddi.h at {path}")
        return path
    for parent in [ROOT, *ROOT.parents]:
        for relative in WDK_SEARCH:
            candidate = parent / relative
            if candidate.is_file():
                return candidate
    # A Visual Studio developer environment (CI) names the selected kit's
    # include directories; use the header the UMD itself compiles against.
    for directory in os.environ.get("INCLUDE", "").split(os.pathsep):
        candidate = Path(directory) / "d3d10umddi.h"
        if directory and candidate.is_file():
            return candidate
    raise Failure("could not locate a WDK d3d10umddi.h; pass --wdk")


def mandatory_slots(header: Path):
    """D3D10DDI_DEVICEFUNCS members, excluding the D3D10PSGP-only ones."""
    lines = header.read_text(encoding="utf-8", errors="replace").splitlines()
    try:
        start = next(i for i, l in enumerate(lines)
                     if "typedef struct D3D10DDI_DEVICEFUNCS" in l)
        end = next(i for i, l in enumerate(lines)
                   if i > start and "} D3D10DDI_DEVICEFUNCS" in l)
    except StopIteration:
        raise Failure("D3D10DDI_DEVICEFUNCS not found in the WDK header")
    slots, excluded, depth, psgp_depth = [], [], 0, None
    for line in lines[start:end]:
        text = line.strip()
        if text.startswith("#if"):
            depth += 1
            if psgp_depth is None and PSGP in text:
                psgp_depth = depth
            continue
        if text.startswith("#endif"):
            if psgp_depth == depth:
                psgp_depth = None
            depth -= 1
            continue
        if text.startswith("#"):
            # An unexpected guard would silently change the slot set.
            raise Failure(f"unhandled preprocessor guard in DEVICEFUNCS: {text}")
        found = re.search(r"\b(pfn[A-Za-z0-9_]+)\s*;", text)
        if not found:
            continue
        (excluded if psgp_depth is not None else slots).append(found.group(1))
    if not slots:
        raise Failure("parsed zero DEVICEFUNCS slots; the header shape changed")
    return slots, excluded


def filled_slots(source: str):
    return set(re.findall(r"\btable->(pfn[A-Za-z0-9_]+)\s*=", source))


def gap_mask(contract: str):
    body = contract.split("runtimeMissingD3D10Requirements", 1)[-1]
    body = body.split("return", 1)[-1].split(";", 1)[0]
    body = re.sub(r"//[^\n]*", "", body)
    return {name for name in re.findall(r"\b([A-Z][A-Za-z]+)\b", body)}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--wdk")
    options = parser.parse_args()

    header = find_wdk_header(options.wdk)
    slots, psgp = mandatory_slots(header)
    ddi = (ROOT / "src/umd/umd_ddi.cpp").read_text(encoding="utf-8")
    adapter = (ROOT / "src/umd/umd_adapter.cpp").read_text(encoding="utf-8")
    contract = (ROOT / "src/umd/umd_contract.cpp").read_text(encoding="utf-8")
    exports = (ROOT / "src/umd/viogpudxvk.def").read_text(encoding="utf-8")

    filled = filled_slots(ddi)
    missing = [name for name in slots if name not in filled]
    unknown = sorted(filled - set(slots) - set(psgp))
    table_complete = not missing

    results = []

    def record(area, ok, detail):
        results.append((area, ok, detail))

    record("mandatory-table", table_complete,
           f"{len(slots) - len(missing)}/{len(slots)} WDK slots filled"
           + (f"; missing {', '.join(missing)}" if missing else "")
           + f" ({len(psgp)} D3D10PSGP-only slots excluded)")

    record("table-no-strays", not unknown,
           "no assignment outside DEVICEFUNCS" if not unknown
           else f"assigned non-members: {', '.join(unknown)}")

    names = {line.strip() for line in exports.splitlines()}
    have_open = {"OpenAdapter10", "OpenAdapter10_2"} <= names
    forbidden = names & {"OpenAdapter", "D3D11CreateDevice", "D3D10CreateDevice"}
    record("exports", have_open and not forbidden,
           "OpenAdapter10 + OpenAdapter10_2 exported"
           + (f"; forbidden export {', '.join(sorted(forbidden))}" if forbidden else "")
           if have_open else "missing an OpenAdapter entry point")

    # The gate must be the single authority: every negotiation site consults it.
    gated = [
        ("GetSupportedVersions", "runtimeSupportsD3D10() ? 1 : 0" in adapter),
        ("GetCaps 3DPIPELINESUPPORT",
         bool(re.search(r"D3D11DDICAPS_3DPIPELINESUPPORT\s*&&\s*dxvk::umd::runtimeSupportsD3D10\(\)", adapter))),
        ("OpenAdapter10", "!development && !dxvk::umd::runtimeSupportsD3D10()" in adapter),
        ("CreateDevice", "!admitted(*adapter)" in adapter),
    ]
    ungated = [name for name, ok in gated if not ok]
    record("gate-coverage", not ungated,
           "every negotiation site consults the admission gate" if not ungated
           else f"ungated negotiation: {', '.join(ungated)}")

    mask = gap_mask(contract)
    gate_open = not mask
    record("gate-state", True,
           "closed; remaining: " + ", ".join(sorted(mask)) if mask else "OPEN")

    # The property that actually prevents a crash on the shared target: the
    # gate may never open while a mandatory slot is still null.
    record("version-coherence", table_complete or not gate_open,
           "gate closed or table complete" if table_complete or not gate_open
           else "GATE OPEN WITH NULL MANDATORY SLOTS - the runtime will fault")

    # A retired flag must not still be claimed by the mask.
    stale = mask & {"LegacyPipelineCallbacks"} if table_complete else set()
    record("gap-mask-accuracy", not stale,
           "mask matches the implemented table" if not stale
           else f"mask still claims retired gaps: {', '.join(sorted(stale))}")

    width = max(len(area) for area, _, _ in results)
    failed = 0
    for area, ok, detail in results:
        if not ok:
            failed += 1
        print(f"{'PASS' if ok else 'FAIL'}  {area.ljust(width)}  {detail}")
    print(f"\nWDK header: {header}")
    print(f"UMD loadability policy: {'PASSED' if not failed else f'{failed} FAILED'}")
    return 1 if failed else 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Failure as error:
        print(f"error: {error}")
        raise SystemExit(1)
