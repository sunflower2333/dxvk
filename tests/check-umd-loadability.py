#!/usr/bin/env python3
"""Loadability policy for the native WDDM UMD.

A D3D UMD that the runtime can actually activate has to satisfy four coupled
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
  4. gap-mask-accuracy the admission mask in umd_contract.cpp must match the
                       capabilities the source actually implements, in BOTH
                       directions.  See the witness table below.

On (4): until 2026-09-18 this check tested one thing -- that the mask no longer
claimed the retired LegacyPipelineCallbacks bit -- while printing "mask matches
the implemented table".  It did not.  Rewriting the mask as `return 0;` (gate
fully open, nothing implemented) passed every row of this script, and the
driver-side packaging gate only asserted that the printed state began "closed;
remaining: ", never which gaps.  A mask shrunk from eight gaps to one was green
end to end.  Nothing verified the gate; its author kept it honest by hand.

The witness table below removes that.  Each source gap is tied to a STRUCTURAL
fact about the source -- a stub body, a rejected flag, an absent token, a fixed
allowlist -- that holds exactly while the capability is unimplemented.  A gap
the source still witnesses but the mask does not claim fails; a gap the mask
claims but no witness supports also fails.  Comments are never witnesses: a
comment can be edited without changing behaviour.

Not every admission requirement is derivable this way, and pretending otherwise
would rebuild the same false confidence.  A gap whose code is written but
unproven on the target hardware has no source witness at all, because there is
nothing in the source to point at.  Those are declared in ACCEPTANCE_GAPS and
reported separately, as asserted rather than verified.  Dropping one from the
mask requires editing this file too, so it is a deliberate act in two places
instead of a one-line change that stays green.

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

# The original Vista-era DXGI base table.  Frozen ABI: later revisions are
# separate structs (DXGI1_1_DDI_BASE_FUNCTIONS and up), so this list cannot
# drift.  It is still cross-checked against dxgiddi.h when that header is
# reachable, because a hardcoded list nobody verifies is the bug above again.
DXGI_BASE_SLOTS = [
    "pfnPresent", "pfnGetGammaCaps", "pfnSetDisplayMode",
    "pfnSetResourcePriority", "pfnQueryResourceResidency",
    "pfnRotateResourceIdentities", "pfnBlt",
]

# Every system value a D3D10 signature can carry, by the semantic name a
# reconstructed DXBC signature chunk has to emit for it.
SYSTEM_VALUE_SEMANTICS = {
    "SV_Position", "SV_ClipDistance", "SV_CullDistance",
    "SV_RenderTargetArrayIndex", "SV_ViewportArrayIndex", "SV_VertexID",
    "SV_PrimitiveID", "SV_InstanceID", "SV_IsFrontFace", "SV_SampleIndex",
    "SV_Target", "SV_Depth", "SV_Coverage",
}

# Admission requirements with no source witness: the code is written, and what
# is missing is proof on the target, which no static check can observe.  Stated
# here so the report can separate "verified against the source" from "asserted".
ACCEPTANCE_GAPS = {
    "RuntimeThreading":
        "worker dispatch and Flush submission are implemented "
        "(umd_runtime_service.h); WARP fixtures are not native runtime proof",
}


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


def declared_dxgi_slots(header: Path):
    """DXGI_DDI_BASE_FUNCTIONS members, or None when dxgiddi.h is unreachable.

    dxgiddi.h ships beside d3d10umddi.h in an installed SDK but not in every
    extracted header tree, so its absence is tolerated; DXGI_BASE_SLOTS still
    drives the check.  When it is present it must agree.
    """
    for candidate in [header.parent / "dxgiddi.h",
                      *(Path(directory) / "dxgiddi.h"
                        for directory in os.environ.get("INCLUDE", "").split(os.pathsep)
                        if directory)]:
        if not candidate.is_file():
            continue
        lines = candidate.read_text(encoding="utf-8", errors="replace").splitlines()
        try:
            start = next(i for i, l in enumerate(lines)
                         if "typedef struct DXGI_DDI_BASE_FUNCTIONS" in l)
            end = next(i for i, l in enumerate(lines)
                       if i > start and "DXGI_DDI_BASE_FUNCTIONS;" in l)
        except StopIteration:
            continue
        return re.findall(r"\*\s*(pfn[A-Za-z0-9_]+)\s*\)", "\n".join(lines[start:end]))
    return None


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


def function_body(source: str, marker: str, where: str) -> str:
    """The brace-matched body of the definition introduced by `marker`."""
    start = source.find(marker)
    if start < 0:
        raise Failure(f"{where}: no definition matching {marker!r}; "
                      "a witness is stale and this check cannot be trusted")
    opening = source.find("{", start)
    if opening < 0:
        raise Failure(f"{where}: {marker!r} has no body")
    depth = 0
    for index in range(opening, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if not depth:
                return source[opening + 1:index]
    raise Failure(f"{where}: unbalanced braces after {marker!r}")


def derive_gaps(sources, dxgi_filled):
    """Derive the gap set the source still witnesses.

    Returns {gap: [witness descriptions]}.  A gap is derived when ANY of its
    witnesses still holds, because one unimplemented half is enough to make the
    capability unusable.
    """
    ddi = sources["umd_ddi.cpp"]
    shader = sources["umd_shader.cpp"]
    policy = sources["umd_output_policy.h"]
    every = "\n".join(sources.values())

    open_resource = function_body(ddi, "void APIENTRY openResource(", "umd_ddi.cpp")
    create_shader = function_body(ddi, "void createShader(", "umd_ddi.cpp")
    set_targets = function_body(ddi, "void APIENTRY setRenderTargets(", "umd_ddi.cpp")
    create_data = function_body(ddi, "HRESULT createResourceData(", "umd_ddi.cpp")
    emitted = set(re.findall(r'"(SV_[A-Za-z]+)"', shader))

    witnesses = {
        "OpenedResources": [
            ("the OpenResource body never reaches the renderer",
             "backend" not in open_resource),
            ("nothing can be created shared: D3D10_DDI_RESOURCE_MISC_SHARED is "
             "absent from the UMD",
             "D3D10_DDI_RESOURCE_MISC_SHARED" not in every),
        ],
        "CompleteResources": [
            ("no Texture3D: D3D10DDIRESOURCE_TEXTURE3D is absent from the UMD",
             "D3D10DDIRESOURCE_TEXTURE3D" not in every),
            ("shared creation is rejected: D3D10_DDI_RESOURCE_MISC_SHARED is "
             "absent from the UMD",
             "D3D10_DDI_RESOURCE_MISC_SHARED" not in every),
        ],
        "PrimaryAndDxgi": [
            ("CreateResource rejects a primary description",
             "pPrimaryDesc" in create_data),
            (f"DXGI base table fills {len(dxgi_filled)}/{len(DXGI_BASE_SLOTS)} slots",
             len(dxgi_filled) < len(DXGI_BASE_SLOTS)),
        ],
        "CompleteShaderSemantics": [
            ("the shader bridge emits only "
             + ", ".join(sorted(emitted)) + " of "
             + str(len(SYSTEM_VALUE_SEMANTICS)) + " system values",
             bool(SYSTEM_VALUE_SEMANTICS - emitted)),
        ],
        "MultipleRenderTargets": [
            ("SetRenderTargets applies a fixed render-target format allowlist",
             "object->format != DXGI_FORMAT_" in set_targets),
            ("pixel-shader outputs are restricted to Float32",
             "ShaderScalar::Float32" in policy),
        ],
        "StreamOutput": [
            ("CreateShader rejects null bytecode, so the runtime's "
             "signature-only geometry shader cannot be served",
             "!code ||" in create_shader),
        ],
        "Predication": [
            ("predication is resolved on the CPU: the UMD never calls the "
             "backend's SetPredication",
             "->SetPredication(" not in every),
        ],
    }

    return {gap: [text for text, holds in tests if holds]
            for gap, tests in witnesses.items()
            if any(holds for _, holds in tests)}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--wdk")
    options = parser.parse_args()

    header = find_wdk_header(options.wdk)
    slots, psgp = mandatory_slots(header)
    sources = {name: (ROOT / "src/umd" / name).read_text(encoding="utf-8")
               for name in ("umd_ddi.cpp", "umd_adapter.cpp", "umd_contract.cpp",
                            "umd_shader.cpp", "umd_output_policy.h", "umd_view.h")}
    ddi = sources["umd_ddi.cpp"]
    adapter = sources["umd_adapter.cpp"]
    contract = sources["umd_contract.cpp"]
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

    # The runtime calls the DXGI base table as unconditionally as the device
    # table, so the same coherence rule applies: a null slot there is an access
    # violation on the first multi-buffer swapchain, not a capability report.
    declared_dxgi = declared_dxgi_slots(header)
    if declared_dxgi is not None and declared_dxgi != DXGI_BASE_SLOTS:
        raise Failure("DXGI_BASE_SLOTS is stale against dxgiddi.h: "
                      f"header declares {', '.join(declared_dxgi)}")
    dxgi_filled = set(re.findall(r"\bdxgiTable->(pfn[A-Za-z0-9_]+)\s*=", ddi))
    dxgi_unknown = sorted(dxgi_filled - set(DXGI_BASE_SLOTS))
    if dxgi_unknown:
        raise Failure("assigned non-members of DXGI_DDI_BASE_FUNCTIONS: "
                      f"{', '.join(dxgi_unknown)}")
    dxgi_missing = [name for name in DXGI_BASE_SLOTS if name not in dxgi_filled]
    record("dxgi-coherence", not (gate_open and dxgi_missing),
           f"gate closed; DXGI base table {len(dxgi_filled)}/{len(DXGI_BASE_SLOTS)}"
           + (f", missing {', '.join(dxgi_missing)}" if dxgi_missing else "")
           if not gate_open else
           "DXGI base table complete" if not dxgi_missing else
           "GATE OPEN WITH NULL DXGI SLOTS - the runtime will fault: "
           + ", ".join(dxgi_missing))

    # The mask must match the source in both directions.  See the module
    # docstring: this replaced a check that only rejected one retired bit.
    derived = derive_gaps(sources, dxgi_filled)
    unclaimed = sorted(set(derived) - mask)
    unwitnessed = sorted(mask - set(derived) - set(ACCEPTANCE_GAPS)
                         - {"LegacyPipelineCallbacks"})
    dropped = sorted(set(ACCEPTANCE_GAPS) - mask)
    stale = sorted(mask & {"LegacyPipelineCallbacks"}) if table_complete else []
    faults = []
    if unclaimed:
        faults.append("source still restricts " + ", ".join(unclaimed)
                      + " but the mask does not claim it")
    if unwitnessed:
        faults.append("mask claims " + ", ".join(unwitnessed)
                      + " with no source witness; implement-then-clear, or the "
                        "witness in this script is stale")
    if dropped:
        faults.append("mask dropped declared acceptance gap "
                      + ", ".join(dropped)
                      + "; remove it from ACCEPTANCE_GAPS here when the target "
                        "proof lands")
    if stale:
        faults.append("mask still claims retired " + ", ".join(stale))
    claimed_acceptance = sorted(set(ACCEPTANCE_GAPS) & mask)
    record("gap-mask-accuracy", not faults,
           f"{len(derived)} source gaps derived and claimed; "
           f"{len(claimed_acceptance)} asserted without a source witness "
           f"({', '.join(claimed_acceptance) or 'none'})"
           if not faults else "; ".join(faults))

    evidence = [f"  witness   {gap.ljust(24)} {witness}"
                for gap in sorted(derived) for witness in derived[gap]]
    evidence += [f"  asserted  {gap.ljust(24)} {ACCEPTANCE_GAPS[gap]}"
                 for gap in claimed_acceptance]

    width = max(len(area) for area, _, _ in results)
    failed = 0
    for area, ok, detail in results:
        if not ok:
            failed += 1
        print(f"{'PASS' if ok else 'FAIL'}  {area.ljust(width)}  {detail}")
    print()
    for line in evidence:
        print(line)
    # Canonical, sorted, machine-readable form of the mask.  Consumers pin this
    # exact line rather than re-parsing the prose above: the driver package's
    # build-umd.ps1 compares it verbatim, so a narrowed gap set cannot reach a
    # package without the narrowing appearing in that script's diff too.
    print(f"\nUMD gap set: {', '.join(sorted(mask)) if mask else '(open)'}")
    print(f"WDK header: {header}")
    print(f"UMD loadability policy: {'PASSED' if not failed else f'{failed} FAILED'}")
    return 1 if failed else 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Failure as error:
        print(f"error: {error}")
        raise SystemExit(1)
