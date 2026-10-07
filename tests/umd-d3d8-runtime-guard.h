#pragma once
// Keep genuine D3D8's legacy API headers separate from the modern DDI header.
int d3d8RuntimeFrontGuard(const wchar_t* frontend) noexcept;
