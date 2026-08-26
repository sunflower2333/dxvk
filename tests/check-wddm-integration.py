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
