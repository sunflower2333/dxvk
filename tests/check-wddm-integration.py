#!/usr/bin/env python3

from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent


def require_once(source: str, fragment: str, message: str) -> None:
    if source.count(fragment) != 1:
        raise RuntimeError(message)


def main() -> int:
    device_filter = (ROOT / "src/dxvk/dxvk_device_filter.cpp").read_text(encoding="utf-8")
    require_once(
        device_filter,
        "Invalid DXVK_FILTER_DEVICE_LUID/dxvk.deviceLuid value ",
        "an invalid WDDM LUID filter diagnostic prefix must appear exactly once",
    )
    require_once(
        device_filter,
        "rejects all adapters: ",
        "an invalid WDDM LUID filter diagnostic must describe fail-closed behavior",
    )
    if "m_matchDeviceLuid.clear()" in device_filter:
        raise RuntimeError("an invalid explicit WDDM LUID filter must not fall back to an arbitrary adapter")
    require_once(
        device_filter,
        "m_flags.set(DxvkDeviceFilterFlag::MatchDeviceLuid);",
        "the WDDM LUID filter must remain active for every non-empty explicit value",
    )
    require_once(
        device_filter,
        "convertLUID(adapterInfo.deviceLuid) != m_matchDeviceLuid",
        "the WDDM LUID filter must compare the byte-exact Vulkan identity",
    )

    d3d10_smoke = (ROOT / "tests/wddm-d3d10-smoke.cpp").read_text(encoding="utf-8")
    require_once(
        d3d10_smoke,
        "D3D10CreateDevice1(adapter, D3D10_DRIVER_TYPE_HARDWARE",
        "the D3D10 smoke must use the supported HARDWARE driver type",
    )

    d3d9_smoke = (ROOT / "tests/wddm-d3d9-smoke.cpp").read_text(encoding="utf-8")
    for fragment, message in (
        ("D3DCOLOR_ARGB(255, 0, 0, 255)", "D3D9 must clear to a contrasting blue"),
        ("DrawPrimitiveUP(D3DPT_TRIANGLELIST, 2", "D3D9 must submit a real draw"),
        ("passed = VerifyRedPixels(locked);", "D3D9 must validate the full readback"),
    ):
        require_once(d3d9_smoke, fragment, message)

    d3d11_smoke = (ROOT / "tests/wddm-d3d11-smoke.cpp").read_text(encoding="utf-8")
    for fragment, message in (
        ("D3DCompile(", "D3D11 must compile the target-side shaders"),
        ("context->Draw(3, 0);", "D3D11 must submit a real draw"),
        ("const float clearColor[4] = {0.0f, 0.0f, 1.0f, 1.0f};", "D3D11 must clear to a contrasting blue"),
        ("passed = verify_red_pixels(mapped);", "D3D11 must validate the full readback"),
    ):
        require_once(d3d11_smoke, fragment, message)

    workflow = (ROOT / ".github/workflows/build-wddm-arm64.yml").read_text(encoding="utf-8")
    for fragment, message in (
        ("/link d3d11.lib d3dcompiler.lib dxgi.lib", "D3D11 smoke must link the shader compiler"),
        ("$imports -notmatch 'D3DCompile'", "CI must verify the D3D compiler import"),
    ):
        require_once(workflow, fragment, message)

    d3d11_swapchain = (ROOT / "src/d3d11/d3d11_swapchain.cpp").read_text(
        encoding="utf-8"
    )
    require_once(
        d3d11_swapchain,
        "if (!pRegion)\n      return E_INVALIDARG;",
        "the D3D11 source-region path must reject a null rectangle",
    )
    require_once(
        d3d11_swapchain,
        "uint64_t(pRegion->right) > m_desc.Width",
        "the D3D11 source-region path must bound-check the right edge",
    )
    require_once(
        d3d11_swapchain,
        "cSourceRect     = sourceRect,",
        "D3D11 presentation must snapshot the source rectangle for the CS task",
    )
    require_once(
        d3d11_swapchain,
        "cSwapImage, cSourceRect);",
        "D3D11 presentation must pass the source rectangle to the blitter",
    )
    if "// TODO implement\n    return E_NOTIMPL;" in d3d11_swapchain:
        raise RuntimeError("D3D11 source-region handling must not remain a stub")

    clear_stale_filter = "Remove-Item Env:DXVK_FILTER_DEVICE_LUID -ErrorAction SilentlyContinue"
    for relative in (
        "scripts/launch-wddm-arm64.ps1",
        "scripts/run-wddm-smoke-arm64.ps1",
    ):
        source = (ROOT / relative).read_text(encoding="utf-8")
        require_once(
            source,
            clear_stale_filter,
            f"{relative} must clear an inherited LUID filter when none was requested",
        )

    print("DXVK WDDM integration policy passed")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except RuntimeError as error:
        print(f"error: {error}")
        raise SystemExit(1)
