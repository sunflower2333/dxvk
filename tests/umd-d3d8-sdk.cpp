#include "../src/umd/umd_d3d8_compat.h"
#include <d3d9.h>

bool d3d8SdkProjectionContract() {
  D3DCAPS9 source{};
  source.DeviceType = D3DDEVTYPE_HAL;
  source.VertexShaderVersion = D3DVS_VERSION(2, 0);
  source.MaxVertexShaderConst = 256;
  source.PixelShaderVersion = D3DPS_VERSION(2, 0);
  source.PixelShader1xMaxValue = 8.f;
  source.Caps2 = D3DCAPS2_DYNAMICTEXTURES | D3DCAPS2_CANAUTOGENMIPMAP;
  source.Caps3 = D3DCAPS3_COPY_TO_VIDMEM | D3DCAPS3_COPY_TO_SYSTEMMEM;
  std::array<uint32_t, 55> destination{};
  destination.front() = destination.back() = 0x13572468;
  return dxvk::umd::projectD3D8Caps9(source, destination.data() + 1, dxvk::umd::d3d8CapsBytes)
      == dxvk::umd::D3D8CapsResult::Success
    && destination.front() == 0x13572468 && destination.back() == 0x13572468
    && destination[1] == D3DDEVTYPE_HAL && destination[4] == D3DCAPS2_DYNAMICTEXTURES
    && destination[5] == 0 && destination[50] == D3DVS_VERSION(1, 1)
    && destination[51] == 96 && destination[52] == D3DPS_VERSION(1, 4)
    && destination[53] == 0x41000000;
}
