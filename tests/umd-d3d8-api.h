#pragma once
#include <windows.h>

// Modern Microsoft SDKs do not ship the D3D8 API headers. Preserve the pinned
// Wine/MinGW legacy headers verbatim; supply only their spelling compatibility
// for the native MSVC compiler, not a replacement COM or DDI ABI.
#ifndef __MINGW32__
typedef BOOL WINBOOL;
#ifndef __MSABI_LONG
#define __MSABI_LONG(value) value##L
#endif
#endif
#include <d3d8.h>
