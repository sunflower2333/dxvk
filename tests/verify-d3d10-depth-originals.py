#!/usr/bin/env python3
"""Check retained D3D10/10.1 depth planes, original tokens and query results."""
import argparse
import hashlib
import json
import re
import struct
import tempfile
from pathlib import Path

SOURCE_SHA256 = "1b7af97f09e884ca524751e45d26c7a82c98010720b15f8b33df567a7f36bccd"
QUERY_FIELDS = ("IAVertices", "IAPrimitives", "VSInvocations", "GSInvocations",
                "GSPrimitives", "CInvocations", "CPrimitives", "PSInvocations",
                "occlusion", "deviceRemoved")
NULL_PS_CASES = (0, 4, 5)
OCCLUSION = (256, 256, 256, 0, 0, 256)


def shader_cases():
    index = 0
    for model in (40, 41):
        for entry, stage in (("vs", 1), ("ps_depth", 0), ("ps_empty", 0), ("ps_discard", 0)):
            index += 1
            profile = f"{'vs' if stage else 'ps'}_4_{model % 10}"
            yield f"depth-fxc-{index:02d}-{entry}-{profile}", stage, model


def shader_chunk(binary, stage, model):
    """Independently bound every DXBC chunk before extracting original SM4 code."""
    if len(binary) < 36 or binary[:4] != b"DXBC":
        raise AssertionError("Original DXBC header")
    one, extent, count = struct.unpack_from("<3I", binary, 20)
    if one != 1 or extent != len(binary) or not 1 <= count <= 32 or 32 + 4 * count > len(binary):
        raise AssertionError("Original DXBC extent or chunk table")
    ranges, tags, found = [], set(), []
    for offset in struct.unpack_from("<" + "I" * count, binary, 32):
        if offset % 4 or offset < 32 + 4 * count or offset + 8 > len(binary):
            raise AssertionError("Original DXBC chunk offset")
        tag = binary[offset:offset + 4]
        length = struct.unpack_from("<I", binary, offset + 4)[0]
        end = offset + 8 + length
        if tag in tags or end > len(binary) or any(offset < right and left < end for left, right in ranges):
            raise AssertionError("Original DXBC chunk duplicate, extent or overlap")
        ranges.append((offset, end))
        tags.add(tag)
        if tag in (b"SHDR", b"SHEX"):
            code = binary[offset + 8:end]
            if length < 12 or length % 4:
                raise AssertionError("Original token extent")
            version, words = struct.unpack_from("<2I", code)
            expected = (stage << 16) | (0x40 if model == 40 else 0x41)
            if version != expected or 4 * words != len(code):
                raise AssertionError("Original SM4 stage/version/word count")
            found.append(code)
    if len(found) != 1:
        raise AssertionError("Exactly one original code chunk is required")
    return found[0]


def expected_depth(case):
    words = []
    for _ in range(16):
        for x in range(16):
            value = 0.25 + (2 * x + 1) / 128 if case in (0, 2) else 0.625 if case == 1 else 1.0
            words.append(struct.unpack("<I", struct.pack("<f", value))[0])
    return words


def verify(directory):
    directory = Path(directory)
    if not directory.is_dir() or directory.is_symlink():
        raise AssertionError("Original directory must be a real directory")
    wanted, originals, queries = set(), [], []
    checks = depth_words = query_words = 0

    def load(name):
        nonlocal checks
        path = directory / name
        if path.is_symlink() or not path.is_file():
            raise AssertionError("Missing or nonregular original: " + name)
        raw = path.read_bytes()
        wanted.add(name)
        checks += 1
        originals.append(dict(path=str(path.resolve()), bytes=len(raw), sha256=hashlib.sha256(raw).hexdigest()))
        return raw

    if hashlib.sha256(load("depth-original.hlsl")).hexdigest() != SOURCE_SHA256:
        raise AssertionError("Original source differs from the fixed depth oracle")
    checks += 1
    for prefix, stage, model in shader_cases():
        if shader_chunk(load(prefix + ".dxbc"), stage, model) != load(prefix + ".tokens"):
            raise AssertionError("Retained tokens differ from original FXC container: " + prefix)
        checks += 1
    for model in (40, 41):
        for case in range(6):
            expected = expected_depth(case)
            for role in ("native", "public"):
                prefix = f"depth-model{model}-case{case}-{role}"
                raw = load(prefix + ".bin")
                if len(raw) != 256 * 4:
                    raise AssertionError("Original depth readback extent: " + prefix)
                image = struct.unpack("<256I", raw)
                for index, (actual, oracle) in enumerate(zip(image, expected)):
                    checks += 1
                    depth_words += 1
                    if actual != oracle:
                        raise AssertionError(f"{prefix} pixel{index}: {actual:08x} != {oracle:08x}")
                raw = load(prefix + "-query.bin")
                if len(raw) != 10 * 8:
                    raise AssertionError("Original query readback extent: " + prefix)
                values = struct.unpack("<10Q", raw)
                query_words += len(values)
                counters = dict(zip(QUERY_FIELDS, values))
                invariants = (values[0] == 3, values[1] == 1, values[2] > 0,
                              values[3] == 0, values[4] == 0,
                              case == 3 or (values[7] == 0 if case in NULL_PS_CASES else values[7] > 0),
                              values[8] == OCCLUSION[case], values[9] == 0)
                checks += len(invariants)
                if not all(invariants):
                    raise AssertionError(f"Original query invariants: {prefix}: {counters}")
                # Clipper/helper/early-depth counts may legitimately differ between
                # the public D3D10 runtime and the native fixture's D3D11 backend.
                # The all-discard shader has no surviving samples establishing a
                # positive PS lower bound; retain its counter as an observation.
                queries.append(dict(model=model, case=case, role=role, counters=counters))
    if {path.name for path in directory.iterdir()} != wanted:
        raise AssertionError("Exact original output closure")
    if len(originals) != 65 or depth_words != 6144 or query_words != 240:
        raise AssertionError("Independent frame/program counts")
    return dict(passed=True, scope="original SM4/4.1 tokens, literal depth planes and query invariants",
                checks=checks, profiles=2, cases_per_profile=6, scenes=12,
                depth_frames=24, query_frames=24, fxc_programs=8, original_file_count=65,
                depth_words=depth_words, query_words=query_words, queries=queries, original_files=originals,
                hardware_admission=False, registration=False,
                compiler_provenance_scope="caller must separately join actual source/compiler/process originals")


def selftest(root):
    """Synthetic reader controls only; this never executes or attests a shader."""
    controls = []
    with tempfile.TemporaryDirectory(prefix="depth10-reader-", dir=root) as temporary:
        directory = Path(temporary)
        fixture = Path(__file__).with_name("umd-d3d10-depth.cpp").read_text()
        match = re.search(r'static constexpr char source\[\]\s*=\s*R"\((.*?)\)";', fixture, re.S)
        if not match:
            raise AssertionError("Missing source literal for synthetic reader controls")
        (directory / "depth-original.hlsl").write_bytes(match.group(1).encode())
        for prefix, stage, model in shader_cases():
            code = struct.pack("<3I", (stage << 16) | (0x40 if model == 40 else 0x41), 3, 62 | (1 << 24))
            binary = b"DXBC" + bytes(16) + struct.pack("<4I", 1, 44 + len(code), 1, 36) + b"SHDR" + struct.pack("<I", len(code)) + code
            (directory / (prefix + ".dxbc")).write_bytes(binary)
            (directory / (prefix + ".tokens")).write_bytes(code)
        for model in (40, 41):
            for case in range(6):
                raw = struct.pack("<256I", *expected_depth(case))
                for role in ("native", "public"):
                    prefix = f"depth-model{model}-case{case}-{role}"
                    (directory / (prefix + ".bin")).write_bytes(raw)
                    # Deliberately different legal counts test that role equality
                    # is not accidentally required by the independent reader.
                    pixel_invocations = 0 if case in NULL_PS_CASES else 256 if role == "native" else 260
                    query = (3, 1, 3 if role == "native" else 6, 0, 0,
                             0 if case == 4 else 1, 0 if case == 4 else 2 if role == "public" else 1,
                             pixel_invocations, OCCLUSION[case], 0)
                    (directory / (prefix + "-query.bin")).write_bytes(struct.pack("<10Q", *query))
        positive = verify(directory)
        controls.append("synthetic valid originals with unequal legal role counters")

        def reject(name, replacement, description):
            path = directory / name
            original = path.read_bytes()
            path.write_bytes(replacement(original))
            try:
                verify(directory)
            except AssertionError:
                controls.append(description)
            else:
                raise AssertionError("Mutation accepted: " + description)
            finally:
                path.write_bytes(original)

        def word_mutation(name, offset, value, description, width="I"):
            def change(raw):
                changed = bytearray(raw)
                struct.pack_into("<" + width, changed, offset, value)
                return changed
            reject(name, change, description)

        for case, value, label in ((0, 0x3E800000, "interpolated null-PS depth"),
                                  (1, 0x3F800000, "dedicated depth output"),
                                  (2, 0x3F800000, "empty-PS interpolated depth"),
                                  (3, 0x3F200000, "discard suppresses depth writes"),
                                  (4, 0x3F200000, "disabled viewport suppresses depth writes"),
                                  (5, 0x3F200000, "unbound depth resource remains clear")):
            word_mutation(f"depth-model40-case{case}-native.bin", 0, value, label)
        word_mutation("depth-model41-case0-public.bin", 4, 0x3F000000, "public pixel differs from literal plane")
        query_mutations = ((0, 0, 0, "IA vertex count"), (0, 1, 2, "IA primitive count"),
                           (0, 2, 0, "VS invocation missing"), (0, 3, 1, "unexpected GS invocation"),
                           (0, 4, 1, "unexpected GS primitive"), (0, 7, 1, "NULL PS invocation"),
                           (1, 7, 0, "dedicated-depth PS invocation missing"),
                           (2, 7, 0, "empty PS invocation missing"),
                           (0, 8, 255, "covered occlusion count"), (3, 8, 256, "discard occlusion count"),
                           (4, 8, 256, "disabled viewport occlusion count"),
                           (5, 8, 0, "targetless occlusion count"), (0, 9, 0x887A0005, "device removed"))
        for case, index, value, label in query_mutations:
            word_mutation(f"depth-model40-case{case}-native-query.bin", index * 8, value, label, "Q")
        path = directory / "depth-model40-case3-native-query.bin"
        original = path.read_bytes()
        changed = bytearray(original)
        struct.pack_into("<Q", changed, 7 * 8, 0)
        path.write_bytes(changed)
        try:
            verify(directory)
            controls.append("all-discard PS zero invocations remains observational")
        finally:
            path.write_bytes(original)
        prefix = "depth-fxc-01-vs-vs_4_0"
        word_mutation(prefix + ".tokens", 8, 58 | (1 << 24), "retained token mismatch")
        for offset, value, label in ((20, 0, "DXBC header version"), (24, 60, "DXBC total extent"),
                                     (32, 35, "DXBC chunk alignment"), (40, 16, "DXBC chunk extent"),
                                     (44, 0x10050, "SM5 profile rejection"), (44, 0x40, "wrong shader stage"),
                                     (48, 4, "token word count")):
            word_mutation(prefix + ".dxbc", offset, value, label)
        reject("depth-model40-case0-native.bin", lambda raw: raw[:-4], "truncated depth image")
        reject("depth-model40-case0-native-query.bin", lambda raw: raw[:-8], "truncated query")
        reject("depth-original.hlsl", lambda raw: raw + b"\n", "changed original source")
        for name, action in (("foreign-output.bin", "extra"),
                             ("depth-model40-case0-native.bin", "missing"),
                             ("depth-model40-case0-native.bin", "symlink")):
            path = directory / name
            original = path.read_bytes() if path.exists() else None
            if action == "extra":
                path.write_bytes(b"extra")
            else:
                path.unlink()
                if action == "symlink":
                    path.symlink_to(directory / "depth-model40-case0-public.bin")
            try:
                verify(directory)
            except AssertionError:
                controls.append("original closure " + action)
            else:
                raise AssertionError("Closure mutation accepted: " + action)
            finally:
                if path.is_symlink() or path.exists():
                    path.unlink()
                if original is not None:
                    path.write_bytes(original)
        verify(directory)
        controls.append("restored synthetic originals")
    return dict(passed=True, scope="synthetic reader logic selftest", controls=len(controls), control_names=controls,
                original_file_count=positive["original_file_count"], depth_words=positive["depth_words"],
                query_words=positive["query_words"], native_execution=False, compiler_execution=False,
                hardware_admission=False, registration=False)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--directory", type=Path)
    mode.add_argument("--selftest-root", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    result = verify(args.directory) if args.directory else selftest(args.selftest_root)
    if args.output:
        with args.output.open("x") as output:
            json.dump(result, output, indent=2)
    print(json.dumps({key: value for key, value in result.items() if key not in ("original_files", "queries", "control_names")}))


if __name__ == "__main__":
    main()
