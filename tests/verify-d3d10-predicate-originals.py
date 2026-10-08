#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Independently check retained D10 predicate pixels, queries and KMT ownership."""
import argparse
import hashlib
import json
import re
import struct
import tempfile
from pathlib import Path

SOURCE_SHA256 = "2cb4e70b6b12c94bde9667253db41c88ba1c96f5cf63b1fe98821d9d6abf9812"
COLORS = "BRBBBBRRRRRRRRBBBBBBBBRRRRBBBBRRRGGR"
RGBA = {"B": bytes((0, 0, 0, 255)), "R": bytes((255, 0, 0, 255)), "G": bytes((0, 255, 0, 255))}
COUNT_FIELDS = ("queries", "contexts", "context_closes", "allocations", "deallocations", "locks", "unlocks",
                "renders", "escapes", "residents", "evictions", "wrong_threads", "bad_cookies", "core_errors",
                "malformed_outputs", "remaining_allocations", "remaining_residents", "pending_paging")


def require(value, message):
    if not value:
        raise AssertionError(message)


def shader_chunk(binary, stage):
    require(len(binary) >= 36 and binary[:4] == b"DXBC", "original DXBC header")
    one, extent, count = struct.unpack_from("<3I", binary, 20)
    require(one == 1 and extent == len(binary) and 1 <= count <= 32
            and 32 + 4 * count <= len(binary), "original DXBC extent/chunk table")
    ranges, tags, found = [], set(), []
    for offset in struct.unpack_from("<" + "I" * count, binary, 32):
        require(offset % 4 == 0 and 32 + 4 * count <= offset <= len(binary) - 8, "original chunk offset")
        tag = binary[offset:offset + 4]
        length = struct.unpack_from("<I", binary, offset + 4)[0]
        end = offset + 8 + length
        require(tag not in tags and end <= len(binary)
                and all(end <= left or offset >= right for left, right in ranges), "original chunk range/duplicate/overlap")
        tags.add(tag); ranges.append((offset, end))
        if tag in (b"SHDR", b"SHEX"):
            code = binary[offset + 8:end]
            require(length >= 12 and length % 4 == 0, "original shader token extent")
            version, words = struct.unpack_from("<2I", code)
            require(version == (stage << 16) | 0x40 and words * 4 == length, "original SM4 stage/word count")
            found.append(code)
    require(len(found) == 1, "exactly one original shader code chunk")
    return found[0]


def unpack(raw, count, name):
    require(len(raw) == count * 8, "original word extent: " + name)
    return struct.unpack("<" + "Q" * count, raw)


def verify(directory, stdout):
    directory, stdout = Path(directory), Path(stdout)
    require(directory.is_dir() and not directory.is_symlink(), "real original directory")
    require(stdout.is_file() and not stdout.is_symlink(), "real process stdout original")
    wanted, originals = set(), []

    def load(name):
        path = directory / name
        require(path.is_file() and not path.is_symlink(), "missing/nonregular original: " + name)
        raw = path.read_bytes(); wanted.add(name)
        originals.append(dict(name=name, bytes=len(raw), sha256=hashlib.sha256(raw).hexdigest()))
        return raw

    require(hashlib.sha256(load("predicate-original.hlsl")).hexdigest() == SOURCE_SHA256, "fixed predicate HLSL")
    for stem, stage in (("vs-vs_main", 1), ("ps-ps_main", 0)):
        require(shader_chunk(load(stem + ".dxbc"), stage) == load(stem + ".tokens"), "original tokens join: " + stem)
    require(len(COLORS) == 36, "independent frame count")
    for frame, color in enumerate(COLORS):
        require(load(f"frame-{frame:02d}.raw") == RGBA[color] * 256, f"literal frame {frame} pixels")
    queries = unpack(load("queries.bin"), 12, "queries")
    require(queries == (0, 0, 0, 1, 1, 1, 0, 1, 2, 0, 0, 1), "actual false/true/false issued results and query-read counts")
    bindings = unpack(load("bindings.bin"), 257, "bindings")
    require(bindings[0] > 0, "actual performance-counter frequency")
    intervals = []
    for i in range(32):
        row = bindings[1 + i * 8:1 + (i + 1) * 8]
        generation = i // 8 if i < 24 else 2
        value = i % 8 // 4 if i < 24 else (i - 24) // 4
        command = i % 4 if i < 24 else 4 + (i - 24) % 4
        require(row[:6] == (i + 2, generation, int(generation == 1), value, command, 0), "binding scene/inversion/reuse/no-preread")
        require(row[7] >= row[6], "monotonic original binding clocks")
        intervals.append(row[7] - row[6])
    owners = unpack(load("ownership.bin"), 16, "ownership")
    require(owners[0] > 0 and owners[1:3] == (1, 1) and owners[3] == owners[4] > 0
            and owners[5] == owners[6] > 0 and owners[7] > 0 and owners[8] >= 2
            and owners[9] == owners[10] > 0 and owners[11:15] == (0, 0, 0, 0)
            and owners[15] == 1, "real callback/owner balance before raw close")
    require(unpack(load("raw-close.bin"), 1, "raw close") == (0,), "actual successful raw close")
    require({p.name for p in directory.iterdir()} == wanted and len(wanted) == 45, "exact 45-file output closure")
    text = stdout.read_text(encoding="utf-8", errors="strict")
    require("D3D10_PREDICATE_KMT_FAILURE" not in text and "D3D10_PREDICATE_KMT_FAIL " not in text
            and "DX11_CORE_ERROR" not in text and "D3D10_PREDICATE_KMT_CLEANUP" not in text, "no unclassified failures/callbacks")
    pattern = (r"^DX11_KMT_COUNTS queries=(\d+) contexts=(\d+)/(\d+) allocations=(\d+)/(\d+) locks=(\d+)/(\d+) "
               r"renders=(\d+) escapes=(\d+) residency=(\d+)/(\d+) wrong_threads=(\d+) bad_cookies=(\d+) "
               r"core_errors=(\d+) malformed_outputs=(\d+) remaining_allocations=(\d+) remaining_residents=(\d+) pending_paging=(\d+)$")
    counts = re.findall(pattern, text, re.M)
    require(len(counts) == 1, "one actual transport count record")
    counts = tuple(map(int, counts[0]))
    require(counts[:15] == owners[:15] and counts[15:] == (0, 0, 0), "retained count join and no owners before raw cleanup")
    lines = re.findall(r"^D3D10_PREDICATE_KMT_BIND frame=(\d+) generation=(\d+) expected_result=(\d+) value=(\d+) command=(\d+) pre_reads=(\d+) ticks=(\d+)$", text, re.M)
    require(len(lines) == 32, "all actual binding markers")
    for i, line in enumerate(lines):
        require(tuple(map(int, line)) == bindings[1 + i * 8:7 + i * 8] + (intervals[i],), "binding clock/count original join")
    images = re.findall(r"^D3D10_PREDICATE_KMT_IMAGE frame=(\d+) pixels=256 bytes=1024 mismatches=0$", text, re.M)
    require(tuple(map(int, images)) == tuple(range(36)), "ordered complete literal readback markers")
    require(re.findall(r"^D3D10_PREDICATE_KMT_RAW_CLOSE hr=([0-9a-f]{8})$", text, re.M) == ["00000000"], "one successful raw close marker")
    passed = "D3D10_PREDICATE_KMT_PASS profile=10_0 draws=27 pixels=9216 hr=00000000 pending_completion_guaranteed=0 ordinary_runtime_admission=0"
    require(text.splitlines().count(passed) == 1 and text.splitlines()[-1] == passed, "actual bounded PASS and closed admission")
    return dict(passed=True, frames=36, checked_pixels=9216, draw_attempts=27, resource_commands=8, bindings=32,
                query_generations=3, original_file_count=45, binding_frequency=bindings[0], binding_elapsed_ticks=intervals,
                counts=dict(zip(COUNT_FIELDS, counts)), original_files=originals,
                hardware_admission=False, pending_completion_guaranteed=False,
                provenance_scope="Caller must join actual process exit, named core SHA/path/LUID and compiler originals separately.")


def selftest(root):
    controls = []
    with tempfile.TemporaryDirectory(prefix="predicate10-reader-", dir=root) as temporary:
        base = Path(temporary); directory = base / "originals"; directory.mkdir(); log = base / "stdout.raw"
        fixture = Path(__file__).with_name("umd-d3d10-predicate-hardware-probe.cpp").read_text()
        source = re.findall(r'constexpr char Hlsl\[\] = R"\((.*?)\)";', fixture, re.S)
        require(len(source) == 1, "one fixture HLSL definition")
        (directory / "predicate-original.hlsl").write_bytes(source[0].encode())
        for stem, stage in (("vs-vs_main", 1), ("ps-ps_main", 0)):
            code = struct.pack("<3I", (stage << 16) | 0x40, 3, 62 | (1 << 24))
            binary = b"DXBC" + bytes(16) + struct.pack("<4I", 1, 44 + len(code), 1, 36) + b"SHDR" + struct.pack("<I", len(code)) + code
            (directory / (stem + ".dxbc")).write_bytes(binary); (directory / (stem + ".tokens")).write_bytes(code)
        for frame, color in enumerate(COLORS): (directory / f"frame-{frame:02d}.raw").write_bytes(RGBA[color] * 256)
        q = (0, 0, 0, 1, 1, 1, 0, 1, 2, 0, 0, 1)
        (directory / "queries.bin").write_bytes(struct.pack("<12Q", *q))
        b, lines = [10000000], []
        for i in range(32):
            g = i // 8 if i < 24 else 2
            v = i % 8 // 4 if i < 24 else (i - 24) // 4
            c = i % 4 if i < 24 else 4 + (i - 24) % 4
            b.extend((i + 2, g, int(g == 1), v, c, 0, 100, 101))
            lines.append(f"D3D10_PREDICATE_KMT_BIND frame={i + 2} generation={g} expected_result={int(g == 1)} value={v} command={c} pre_reads=0 ticks=1")
        (directory / "bindings.bin").write_bytes(struct.pack("<257Q", *b))
        o = (1, 1, 1, 2, 2, 3, 3, 4, 2, 5, 5, 0, 0, 0, 0, 1)
        (directory / "ownership.bin").write_bytes(struct.pack("<16Q", *o)); (directory / "raw-close.bin").write_bytes(bytes(8))
        lines += [f"D3D10_PREDICATE_KMT_IMAGE frame={i} pixels=256 bytes=1024 mismatches=0" for i in range(36)]
        lines += ["DX11_KMT_COUNTS queries=1 contexts=1/1 allocations=2/2 locks=3/3 renders=4 escapes=2 residency=5/5 wrong_threads=0 bad_cookies=0 core_errors=0 malformed_outputs=0 remaining_allocations=0 remaining_residents=0 pending_paging=0",
                  "D3D10_PREDICATE_KMT_RAW_CLOSE hr=00000000",
                  "D3D10_PREDICATE_KMT_PASS profile=10_0 draws=27 pixels=9216 hr=00000000 pending_completion_guaranteed=0 ordinary_runtime_admission=0"]
        log.write_text("\n".join(lines) + "\n"); verify(directory, log); controls.append("synthetic complete original closure")

        def rejects(path, changed, label):
            saved = path.read_bytes(); path.write_bytes(changed)
            try:
                try: verify(directory, log)
                except AssertionError: controls.append(label)
                else: raise AssertionError("reader accepted mutation: " + label)
            finally: path.write_bytes(saved)

        for i in range(36):
            path = directory / f"frame-{i:02d}.raw"; changed = bytearray(path.read_bytes()); changed[-1] ^= 1
            rejects(path, changed, f"frame{i} last-pixel alpha")
        for name, count in (("queries.bin", 12), ("ownership.bin", 16)):
            path = directory / name
            for i in range(count):
                changed = bytearray(path.read_bytes()); changed[i * 8] ^= 1
                rejects(path, changed, f"{name} word{i}")
        for i in range(6):
            changed = bytearray((directory / "bindings.bin").read_bytes()); changed[(1 + i) * 8] ^= 1
            rejects(directory / "bindings.bin", changed, f"binding identity field{i}")
        changed = bytearray((directory / "bindings.bin").read_bytes()); struct.pack_into("<Q", changed, 8 * 8, 99)
        rejects(directory / "bindings.bin", changed, "binding clock reversal")
        rejects(directory / "bindings.bin", bytes(8) + (directory / "bindings.bin").read_bytes()[8:], "zero clock frequency")
        rejects(directory / "predicate-original.hlsl", b"modified source", "HLSL source drift")
        rejects(directory / "raw-close.bin", struct.pack("<Q", 0x80004005), "raw close failure")
        rejects(directory / "ps-ps_main.tokens", b"different tokens", "original SHDR token substitution")
        path = directory / "vs-vs_main.dxbc"
        for offset, value, label in ((20, 2, "DXBC version"), (24, 1, "DXBC extent"), (28, 0, "DXBC no chunks"),
                                     (32, 32, "DXBC chunk inside table"), (40, 1000, "DXBC chunk overrun"),
                                     (44, 0x50, "wrong SM/stage"), (48, 99, "wrong token word count")):
            changed = bytearray(path.read_bytes()); struct.pack_into("<I", changed, offset, value); rejects(path, changed, label)
        for before, after, label in (("ticks=1", "ticks=2", "stdout clock join"), ("renders=4", "renders=5", "stdout count join"),
                                     ("remaining_allocations=0", "remaining_allocations=1", "remaining owner"),
                                     ("ordinary_runtime_admission=0", "ordinary_runtime_admission=1", "admission marker"),
                                     ("frame=35 pixels", "frame=34 pixels", "image sequence"),
                                     ("hr=00000000\nD3D10_PREDICATE_KMT_PASS", "hr=80004005\nD3D10_PREDICATE_KMT_PASS", "stdout raw-close failure")):
            rejects(log, log.read_bytes().replace(before.encode(), after.encode(), 1), label)
        rejects(log, b"DX11_CORE_ERROR hr=80004005\n" + log.read_bytes(), "unclassified core callback")
        extra = directory / "unjoined.bin"; extra.write_bytes(b"extra")
        try:
            try: verify(directory, log)
            except AssertionError: controls.append("extra original file")
            else: raise AssertionError("reader accepted extra file")
        finally: extra.unlink()
        missing = directory / "queries.bin"; saved = missing.read_bytes(); missing.unlink()
        try:
            try: verify(directory, log)
            except AssertionError: controls.append("missing query original")
            else: raise AssertionError("reader accepted missing original")
        finally: missing.write_bytes(saved)
        verify(directory, log)
    return dict(passed=True, controls=len(controls), control_names=controls, synthetic=True, shader_execution=False,
                hardware_execution=False, hardware_admission=False, pending_completion_guaranteed=False)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", nargs="?", type=Path)
    parser.add_argument("--stdout", type=Path)
    parser.add_argument("--selftest", action="store_true")
    parser.add_argument("--temp-root", type=Path)
    args = parser.parse_args()
    if args.selftest:
        result = selftest(args.temp_root)
    else:
        require(args.directory is not None and args.stdout is not None, "directory and actual --stdout required")
        result = verify(args.directory, args.stdout)
    print(json.dumps(result, indent=2))
