#!/usr/bin/env python3
"""Independent exact 384-pixel oracle; requires real probe stdout, never runs GPU."""
import argparse
import json
import re
from pathlib import Path

COLORS = (0xFF123456, 0xFF739A4C, 0xFFC0568E, 0xFF288CB0, 0xFF623A81, 0xFF91B742)


def verify(text):
    pixels = {}
    for line in text.splitlines():
        match = re.fullmatch(r"D3D8_PIXEL stage=(\d+) x=(\d+) y=(\d+) value=([0-9a-f]{8})", line)
        if not match:
            continue
        stage, x, y = map(int, match.group(1, 2, 3))
        key = (stage, x, y)
        assert key not in pixels, f"duplicate {key}"
        assert 1 <= stage <= 6 and 0 <= x < 8 and 0 <= y < 8, f"out of range {key}"
        pixels[key] = int(match[4], 16)
    assert len(pixels) == 384, f"pixel count {len(pixels)}"
    for stage, color in enumerate(COLORS, 1):
        for y in range(8):
            for x in range(8):
                assert pixels[stage, x, y] == color, f"pixel mismatch {(stage, x, y)}"
    assert text.count("D3D8_OFFSCREEN PASS stages=6 pixels=384 shader=VS1.1/PS1.1 dynamic_texture=1 resets=1 presents=0") == 1
    assert text.count("D3D8_COMPLETE mode=offscreen") == 1
    assert not re.search(r"D3D8_(?:ERROR|FAILED|UNAVAILABLE|SELECTOR)", text)
    calls = re.findall(r"^D3D8_API operation=(\S+) hr=([0-9a-f]{8})$", text, re.M)
    assert calls and all(hr == "00000000" for _, hr in calls)
    for operation, count in {"CreateDevice-HAL-hardwareVP": 1, "CreateVertexShader-1.1": 1,
                             "CreatePixelShader-1.1": 1, "CreateTexture-dynamic": 1,
                             "CopyRects-RT-to-systemmem": 6, "Reset": 1}.items():
        assert sum(name == operation for name, _ in calls) == count, f"API count {operation}"
    runtime = re.findall(r"^D3D8_RUNTIME path=(.+) machine=(\w+) pointer_bytes=(\d+) sdk_version=220 caps_bytes=212$", text, re.M)
    assert len(runtime) == 1 and runtime[0][1:] == ("014c", "4"), "target requires x86 system8"
    assert runtime[0][0].lower() == r"c:\windows\syswow64\d3d8.dll", "system runtime path"
    adapters = re.findall(r"D3D8_ADAPTER index=(\d+) identifier_hr=00000000 caps_hr=00000000 vendor=1af4 device=1050 devcaps=([0-9a-f]+)", text)
    assert len(adapters) == 1 and int(adapters[0][1], 16) & 0x90000 == 0x90000, "VirtIO hardware HAL"
    checksum = 2166136261
    for stage in range(1, 7):
        for y in range(8):
            for x in range(8):
                for byte in pixels[stage, x, y].to_bytes(4, "little"):
                    checksum = ((checksum ^ byte) * 16777619) & 0xFFFFFFFF
    return {"pixels": len(pixels), "stages": 6, "fnv1a": f"{checksum:08x}", "scope": "genuine-system8-offscreen"}


def selfcheck():
    # This checks rejection behavior only; synthetic output is never acceptance evidence.
    rows = [r"D3D8_RUNTIME path=C:\Windows\SysWOW64\d3d8.dll machine=014c pointer_bytes=4 sdk_version=220 caps_bytes=212",
            "D3D8_ADAPTER index=0 identifier_hr=00000000 caps_hr=00000000 vendor=1af4 device=1050 devcaps=00090000"]
    for operation, count in {"CreateDevice-HAL-hardwareVP": 1, "CreateVertexShader-1.1": 1,
                             "CreatePixelShader-1.1": 1, "CreateTexture-dynamic": 1,
                             "CopyRects-RT-to-systemmem": 6, "Reset": 1}.items():
        rows += [f"D3D8_API operation={operation} hr=00000000"] * count
    rows += [f"D3D8_PIXEL stage={stage} x={x} y={y} value={color:08x}"
             for stage, color in enumerate(COLORS, 1) for y in range(8) for x in range(8)]
    rows += ["D3D8_OFFSCREEN PASS stages=6 pixels=384 shader=VS1.1/PS1.1 dynamic_texture=1 resets=1 presents=0",
             "D3D8_COMPLETE mode=offscreen adapters=1 create_device=1 presents=0 registry_writes=0"]
    text = "\n".join(rows)
    result = verify(text)
    controls = [text.replace("value=ff123456", "value=ff123457", 1),
                text.replace("machine=014c pointer_bytes=4", "machine=aa64 pointer_bytes=8"),
                text + "\nD3D8_PIXEL stage=1 x=0 y=0 value=ff123456",
                text.replace("operation=Reset hr=00000000", "operation=Reset hr=8876086c")]
    for control in controls:
        try:
            verify(control)
        except AssertionError:
            continue
        raise AssertionError("negative oracle accepted")
    return {"selfcheck": "PASS", "synthetic_only": True, "rejects": len(controls), **result}


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("stdout", nargs="?", type=Path)
    parser.add_argument("--selfcheck", action="store_true")
    args = parser.parse_args()
    assert args.selfcheck or args.stdout, "provide actual stdout or --selfcheck"
    print(json.dumps(selfcheck() if args.selfcheck else verify(args.stdout.read_text(encoding="utf-8-sig")), indent=2))
