// SPDX-License-Identifier: MIT
// Included by the SYSTEM probe after its common window and evidence helpers.
// The Android observer, rather than GDI, validates actual fullscreen scanout.
bool displayCycleRequested = false, displayCycleCompleted = false;
bool displayCyclePreserve = false;
HRESULT displayCycleRestore = E_FAIL;
UINT displayCycleFrames = 0;
std::string displayCyclePhases = "[";

void traceCycleDevice(ID3D11Device* device, UINT phase, const char* step) {
  std::fprintf(stderr, "DISPLAY_CYCLE_DEVICE phase=%u step=%s reason=%08lx tick=%llu\n",
    phase, step, static_cast<unsigned long>(device->GetDeviceRemovedReason()),
    static_cast<unsigned long long>(GetTickCount64()));
  std::fflush(stderr);
}

template<typename Exit, typename Query, typename Clock, typename Pump>
HRESULT restoreCycleWindowed(Exit exit, Query query, Clock clock, Pump pump) {
  const ULONGLONG start = clock();
  for (;;) {
    BOOL fullscreen = TRUE;
    HRESULT observed = query(&fullscreen);
    if (observed == S_OK && !fullscreen) return S_OK;
    const HRESULT hr = exit();
    fullscreen = TRUE; observed = query(&fullscreen);
    if (observed == S_OK && !fullscreen) return S_OK;
    if (FAILED(hr)) return hr;
    if (observed != S_OK) return observed;
    if (clock() - start >= 2000) return DXGI_ERROR_WAIT_TIMEOUT;
    pump();
  }
}

struct CycleHold { ULONGLONG published; bool acknowledged; };
template<typename Publish, typename Clock, typename Pump, typename Acknowledged>
CycleHold holdCycleAfterPublication(Publish publish, Clock clock, Pump pump, Acknowledged acknowledged) {
  publish(); // Includes the actual close of the ready receipt.
  const ULONGLONG published = clock();
  for (;;) {
    if (clock() - published >= 8000) return {published, false};
    const bool received = acknowledged();
    if (clock() - published >= 8000) return {published, false};
    if (received) return {published, true};
    pump();
  }
}

bool cycleAcknowledged(const std::wstring& path, UINT phase, UINT64 creationTime) {
  HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
    FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE) {
    const DWORD error = GetLastError();
    require(error == ERROR_FILE_NOT_FOUND || error == ERROR_SHARING_VIOLATION, "cycle-ack-open", HRESULT_FROM_WIN32(error));
    return false;
  }
  std::array<DWORD, 4> row{}; DWORD bytes = 0;
  const DWORD size = GetFileSize(file, nullptr);
  const BOOL read = ReadFile(file, row.data(), DWORD(sizeof(row)), &bytes, nullptr);
  const BOOL closed = CloseHandle(file);
  require(read && closed && size == sizeof(row) && bytes == sizeof(row)
    && row[0] == GetCurrentProcessId() && row[1] == phase
    && ((UINT64(row[3]) << 32) | row[2]) == creationTime, "cycle-exact-closed-observer-ack");
  return true;
}

template<typename Check> void displayCycleControls(Check check) {
  ULONGLONG tick = 0;
  UINT attempts = 0; BOOL full = TRUE;
  HRESULT hr = restoreCycleWindowed([&] {
    if (++attempts == 1) return DXGI_STATUS_MODE_CHANGE_IN_PROGRESS;
    full = FALSE; return S_OK;
  }, [&](BOOL* value) { *value = full; return S_OK; }, [&] { return tick; }, [&] { tick += 50; });
  check(hr == S_OK && attempts == 2 && !full, "cycle-recovery-retries-transition-before-release");
  tick = 0; attempts = 0;
  hr = restoreCycleWindowed([&] { ++attempts; return DXGI_STATUS_MODE_CHANGE_IN_PROGRESS; },
    [](BOOL* value) { *value = TRUE; return S_OK; }, [&] { return tick; }, [&] { tick += 50; });
  check(hr == DXGI_ERROR_WAIT_TIMEOUT && tick == 2000 && attempts == 41, "cycle-recovery-rejects-still-fullscreen");
  hr = restoreCycleWindowed([] { return DXGI_ERROR_DEVICE_REMOVED; },
    [](BOOL* value) { *value = TRUE; return S_OK; }, [&] { return tick; }, [&] { tick += 50; });
  check(hr == DXGI_ERROR_DEVICE_REMOVED, "cycle-recovery-preserves-removal-error");
  attempts = 0;
  hr = restoreCycleWindowed([&] { ++attempts; return E_FAIL; },
    [](BOOL* value) { *value = FALSE; return S_OK; }, [&] { return tick; }, [&] { tick += 50; });
  check(hr == S_OK && attempts == 0, "cycle-recovery-already-windowed");
  tick = 0;
  auto held = holdCycleAfterPublication([&] { tick += 7000; },
    [&] { return tick; }, [&] { tick += 50; }, [&] { return tick >= 7500; });
  check(held.published == 7000 && held.acknowledged && tick == 7500, "cycle-delayed-publication-allows-observer-ack");
  tick = 0;
  held = holdCycleAfterPublication([&] { tick += 7000; },
    [&] { return tick; }, [&] { tick += 50; }, [] { return false; });
  check(held.published == 7000 && !held.acknowledged && tick == 15000, "cycle-missing-observer-ack-times-out-after-publication");
  tick = 0;
  held = holdCycleAfterPublication([] {}, [&] { return tick; }, [&] { tick = 9000; }, [&] { return tick >= 8500; });
  check(!held.acknowledged && tick == 9000, "cycle-rejects-ack-after-scheduling-stall");
  tick = 0;
  held = holdCycleAfterPublication([] {}, [&] { return tick; }, [] {}, [&] { tick = 8000; return true; });
  check(!held.acknowledged && tick == 8000, "cycle-rejects-ack-read-crossing-deadline");
}

void displayCycle(ID3D11Device* device, ID3D11DeviceContext* context,
    IDXGIFactory* factory, HWND window, const std::wstring& directory) {
  FILETIME created{}, exited{}, kernel{}, user{};
  require(GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user) != FALSE,
    "cycle-producer-creation-time");
  const UINT64 creationTime = (UINT64(created.dwHighDateTime) << 32) | created.dwLowDateTime;
  ComPtr<IDXGISwapChain> chain;
  struct Restore {
    ComPtr<IDXGISwapChain>& chain;
    ID3D11DeviceContext* context;
    HWND window;
    const std::wstring& directory;
    ~Restore() {
      context->ClearState(); context->Flush();
      displayCycleRestore = chain ? restoreCycleWindowed(
        [&] { return chain->SetFullscreenState(FALSE, nullptr); },
        [&](BOOL* full) { return chain->GetFullscreenState(full, nullptr); },
        [] { return GetTickCount64(); }, [] {
          MSG message{};
          for (UINT i = 0; i < 256 && PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE); ++i) {
            TranslateMessage(&message); DispatchMessageW(&message);
          }
          MsgWaitForMultipleObjectsEx(0, nullptr, 25, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
        }) : S_OK;
      if (displayCycleRestore != S_OK) {
        // Releasing a still-fullscreen chain is forbidden by DXGI. Preserve
        // a failure receipt, then terminate this owned test process without
        // releasing that COM object. The controller must report exit 3.
        try { saveText(directory, L"cycle-restore-failed.json", "{\"restored\":false,\"result\":"
          + std::to_string(displayCycleRestore) + ",\"exitCode\":3}\n"); } catch (...) { }
        std::fprintf(stderr, "DISPLAY_CYCLE_FATAL restore=%08lx exit=3\n",
          static_cast<unsigned long>(displayCycleRestore)); std::fflush(stderr);
        chain.Detach();
        TerminateProcess(GetCurrentProcess(), 3);
        std::abort();
      }
      if (!SetWindowPos(window, HWND_TOPMOST, 64, 64, Width, Height, SWP_SHOWWINDOW)
          && displayCycleRestore == S_OK)
        displayCycleRestore = HRESULT_FROM_WIN32(GetLastError());
    }
  } restore{chain, context, window, directory};
  DXGI_SWAP_CHAIN_DESC desc{};
  desc.BufferDesc.Width = Width; desc.BufferDesc.Height = Height;
  desc.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
  desc.SampleDesc.Count = 1; desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  desc.BufferCount = 1; desc.OutputWindow = window;
  desc.Windowed = TRUE;
  desc.SwapEffect = displayCyclePreserve ? DXGI_SWAP_EFFECT_SEQUENTIAL : DXGI_SWAP_EFFECT_DISCARD;
  exact(factory->CreateSwapChain(device, &desc, &chain), "cycle-create-bgra-swapchain");
  ComPtr<IDXGIOutput> output;
  exact(chain->GetContainingOutput(&output), "cycle-containing-output");
  DXGI_OUTPUT_DESC monitor{}; exact(output->GetDesc(&monitor), "cycle-output-description");
  const LONG fullWidth = monitor.DesktopCoordinates.right - monitor.DesktopCoordinates.left;
  const LONG fullHeight = monitor.DesktopCoordinates.bottom - monitor.DesktopCoordinates.top;
  require(monitor.AttachedToDesktop && fullWidth == 3040 && fullHeight == 1904
    && monitor.DesktopCoordinates.left == 0 && monitor.DesktopCoordinates.top == 0,
    "cycle-existing-unrotated-3040x1904-output");
  DEVMODEW mode{}; mode.dmSize = sizeof(mode);
  require(EnumDisplaySettingsExW(monitor.DeviceName, ENUM_CURRENT_SETTINGS, &mode, 0)
    && mode.dmPelsWidth == UINT(fullWidth) && mode.dmPelsHeight == UINT(fullHeight)
    && mode.dmDisplayFrequency == 165, "cycle-preserve-existing-165hz-mode");
  const std::array<std::array<float, 4>, 4> colors{{
    {{1, 1, 0, 1}}, {{0, 1, 0, 1}}, {{0, 0, 1, 1}}, {{1, 0, 1, 1}}
  }};
  // Phase 0 must differ from the preceding ordinary probe's red frame, so
  // a stale redirection surface cannot satisfy the physical display oracle.
  const char* colorNames[]{"ffff00", "00ff00", "0000ff", "ff00ff"};
  for (UINT phase = 0; phase < colors.size(); ++phase) {
    std::fprintf(stderr, "DISPLAY_CYCLE phase=%u entering\n", phase); std::fflush(stderr);
    traceCycleDevice(device, phase, "enter");
    // No application references to back buffers or views cross these calls.
    if (phase == 1) {
      readyWindow(window);
      SetForegroundWindow(window); SetFocus(window);
      require(GetForegroundWindow() == window && GetFocus() == window, "cycle-owned-window-focus");
      exact(chain->SetFullscreenState(TRUE, output.Get()), "cycle-enter-fullscreen");
      exact(chain->ResizeBuffers(1, UINT(fullWidth), UINT(fullHeight),
        DXGI_FORMAT_B8G8R8A8_UNORM, 0), "cycle-fullscreen-resize");
    } else if (phase == 2) {
      exact(chain->SetFullscreenState(FALSE, nullptr), "cycle-return-windowed");
      traceCycleDevice(device, phase, "returned-windowed");
      require(SetWindowPos(window, HWND_TOPMOST, 64, 64, Width, Height, SWP_SHOWWINDOW),
        "cycle-restore-small-window");
      traceCycleDevice(device, phase, "small-window");
      // A blt-model swapchain may retain its full-size buffer on return to a
      // small window. This deliberately exercises that real copy path before
      // ResizeBuffers replaces the former fullscreen allocation.
    } else if (phase == 3) {
      exact(chain->ResizeBuffers(1, Width, Height, DXGI_FORMAT_B8G8R8A8_UNORM, 0),
        "cycle-windowed-resize");
    }
    readyWindow(window);
    traceCycleDevice(device, phase, "window-ready");
    BOOL fullscreen = FALSE;
    exact(chain->GetFullscreenState(&fullscreen, nullptr), "cycle-read-display-state");
    require((fullscreen != FALSE) == (phase == 1), "cycle-exact-display-state");
    DXGI_SWAP_CHAIN_DESC actual{};
    exact(chain->GetDesc(&actual), "cycle-read-swapchain-description");
    require(actual.BufferCount == 1 && actual.SwapEffect == desc.SwapEffect,
      "cycle-exact-single-buffer-preservation-policy");
    DEVMODEW observedMode{}; observedMode.dmSize = sizeof(observedMode);
    require(EnumDisplaySettingsExW(monitor.DeviceName, ENUM_CURRENT_SETTINGS, &observedMode, 0)
      && observedMode.dmPelsWidth == mode.dmPelsWidth && observedMode.dmPelsHeight == mode.dmPelsHeight
      && observedMode.dmDisplayFrequency == mode.dmDisplayFrequency
      && observedMode.dmDisplayOrientation == mode.dmDisplayOrientation, "cycle-desktop-mode-preserved");
    ComPtr<ID3D11Texture2D> target, staging;
    exact(chain->GetBuffer(0, IID_PPV_ARGS(&target)), "cycle-get-backbuffer");
    traceCycleDevice(device, phase, "got-backbuffer");
    D3D11_TEXTURE2D_DESC texture{}; target->GetDesc(&texture);
    const UINT expectedWidth = phase == 1 || phase == 2 ? UINT(fullWidth) : Width;
    const UINT expectedHeight = phase == 1 || phase == 2 ? UINT(fullHeight) : Height;
    require(texture.Width == expectedWidth && texture.Height == expectedHeight
      && texture.Format == DXGI_FORMAT_B8G8R8A8_UNORM && texture.SampleDesc.Count == 1,
      "cycle-exact-backbuffer");
    ComPtr<ID3D11RenderTargetView> view;
    const HRESULT viewResult = device->CreateRenderTargetView(target.Get(), nullptr, &view);
    traceCycleDevice(device, phase, "created-view");
    exact(viewResult, "cycle-create-view");
    context->ClearRenderTargetView(view.Get(), colors[phase].data());
    D3D11_TEXTURE2D_DESC read{};
    read.Width = Width; read.Height = Height; read.MipLevels = 1; read.ArraySize = 1;
    read.Format = texture.Format; read.SampleDesc.Count = 1;
    read.Usage = D3D11_USAGE_STAGING;
    read.CPUAccessFlags = D3D11_CPU_ACCESS_READ | (displayCyclePreserve ? D3D11_CPU_ACCESS_WRITE : 0);
    exact(device->CreateTexture2D(&read, nullptr, &staging), "cycle-create-bounded-readback");
    const D3D11_BOX box{0, 0, 0, Width, Height, 1};
    context->CopySubresourceRegion(staging.Get(), 0, 0, 0, 0, target.Get(), 0, &box);
    D3D11_MAPPED_SUBRESOURCE mapped{};
    exact(context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped), "cycle-map-readback");
    const UINT64 span = UINT64(Height - 1) * mapped.RowPitch + Width * 4;
    bool pixels = mapped.pData && mapped.RowPitch >= Width * 4 && span <= UINTPTR_MAX
      && reinterpret_cast<UINT_PTR>(mapped.pData) <= UINTPTR_MAX - (span - 1);
    std::array<unsigned char, Width * Height * 4> original{};
    if (pixels) {
      for (UINT y = 0; y < Height; ++y) for (UINT x = 0; x < Width; ++x) {
        const auto* p = static_cast<const unsigned char*>(mapped.pData) + size_t(y) * mapped.RowPitch + x * 4;
        std::memcpy(original.data() + (y * Width + x) * 4, p, 4);
        pixels = pixels && p[0] == UINT(colors[phase][2] * 255)
          && p[1] == UINT(colors[phase][1] * 255) && p[2] == UINT(colors[phase][0] * 255) && p[3] == 255;
      }
    }
    context->Unmap(staging.Get(), 0);
    save(directory, (L"cycle-phase-" + std::to_wstring(phase) + L".raw").c_str(),
      original.data(), DWORD(original.size()));
    require(pixels, "cycle-literal-bgra-readback");
    exact(device->GetDeviceRemovedReason(), "cycle-device-live-before-present");
    waitVisibleSwapchain(chain.Get(), window);
    exact(chain->Present(1, 0), "cycle-present-exact-s-ok");
    exact(device->GetDeviceRemovedReason(), "cycle-device-live-after-present");
    if (displayCyclePreserve) {
      // SEQUENTIAL preserves this back buffer across Present. A copy oracle
      // after Present catches a false-success flip that discards its storage.
      // Poison the old readback first: a missing second copy must fail rather
      // than compare the previous successful readback with itself.
      mapped = {};
      exact(context->Map(staging.Get(), 0, D3D11_MAP_WRITE, 0, &mapped), "cycle-map-preservation-poison");
      const UINT64 poisonSpan = UINT64(Height - 1) * mapped.RowPitch + Width * 4;
      const bool poisonMapped = mapped.pData && mapped.RowPitch >= Width * 4 && poisonSpan <= UINTPTR_MAX
        && reinterpret_cast<UINT_PTR>(mapped.pData) <= UINTPTR_MAX - (poisonSpan - 1);
      if (poisonMapped) {
        for (UINT y = 0; y < Height; ++y)
          std::memset(static_cast<unsigned char*>(mapped.pData) + size_t(y) * mapped.RowPitch, 0x5a, Width * 4);
      }
      context->Unmap(staging.Get(), 0);
      require(poisonMapped, "cycle-poison-readback-layout");
#ifndef DXVK_TEST_SKIP_PRESERVED_COPY
      context->CopySubresourceRegion(staging.Get(), 0, 0, 0, 0, target.Get(), 0, &box);
#endif
      mapped = {};
      exact(context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped), "cycle-map-preserved-backbuffer");
      const UINT64 preservedSpan = UINT64(Height - 1) * mapped.RowPitch + Width * 4;
      bool preserved = mapped.pData && mapped.RowPitch >= Width * 4 && preservedSpan <= UINTPTR_MAX
        && reinterpret_cast<UINT_PTR>(mapped.pData) <= UINTPTR_MAX - (preservedSpan - 1);
      if (preserved) {
        for (UINT y = 0; y < Height; ++y)
          preserved = preserved && !std::memcmp(static_cast<const unsigned char*>(mapped.pData)
            + size_t(y) * mapped.RowPitch, original.data() + y * Width * 4, Width * 4);
      }
      context->Unmap(staging.Get(), 0);
      require(preserved, "cycle-sequential-backbuffer-content-preserved");
    }
    RECT client{}; POINT origin{};
    require(GetClientRect(window, &client) && ClientToScreen(window, &origin)
      && client.right == (phase == 1 ? fullWidth : LONG(Width))
      && client.bottom == (phase == 1 ? fullHeight : LONG(Height)), "cycle-client-size");
    const POINT sample{origin.x + (phase == 1 ? 64 : 0), origin.y + (phase == 1 ? 64 : 0)};
    const ULONGLONG start = GetTickCount64();
    std::string row = "{\"pid\":" + std::to_string(GetCurrentProcessId())
      + ",\"processCreationFileTime\":" + std::to_string(creationTime)
      + ",\"phase\":" + std::to_string(phase) + ",\"fullscreen\":" + (fullscreen ? "true" : "false")
      + ",\"sourceWidth\":" + std::to_string(texture.Width) + ",\"sourceHeight\":" + std::to_string(texture.Height)
      + ",\"swapEffect\":" + std::to_string(actual.SwapEffect)
      + ",\"preservedPixels\":" + std::to_string(displayCyclePreserve ? Width * Height : 0)
      + ",\"rectangle\":[" + std::to_string(sample.x) + ',' + std::to_string(sample.y) + ",16,16]"
      + ",\"rgb\":" + jsonAscii(colorNames[phase]) + ",\"rawPixels\":256,\"presentResult\":0"
      + ",\"desktopWidth\":" + std::to_string(observedMode.dmPelsWidth)
      + ",\"desktopHeight\":" + std::to_string(observedMode.dmPelsHeight)
      + ",\"desktopHz\":" + std::to_string(observedMode.dmDisplayFrequency)
      + ",\"readyWriteStartTick\":" + std::to_string(start)
      + ",\"holdProtocol\":\"closed-receipt-ack-v1\",\"holdMs\":8000";
    const auto held = holdCycleAfterPublication([&] {
      saveText(directory, (L"cycle-phase-" + std::to_wstring(phase) + L"-ready.json").c_str(), row + "}\n");
    }, [] { return GetTickCount64(); }, [&] {
      readyWindow(window); MsgWaitForMultipleObjectsEx(0, nullptr, 25, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
    }, [&] {
      return cycleAcknowledged(directory + L"\\cycle-phase-" + std::to_wstring(phase) + L".ack", phase, creationTime);
    });
    require(held.acknowledged, "cycle-physical-observer-ack-timeout");
    BOOL after = FALSE;
    exact(chain->GetFullscreenState(&after, nullptr), "cycle-stable-held-display-state");
    RECT finalClient{}; POINT finalOrigin{};
    require(after == fullscreen && GetClientRect(window, &finalClient) && ClientToScreen(window, &finalOrigin)
      && EqualRect(&client, &finalClient) && origin.x == finalOrigin.x && origin.y == finalOrigin.y,
      "cycle-stable-held-window");
    row += ",\"observerAcknowledged\":true,\"publishedTick\":" + std::to_string(held.published)
      + ",\"holdEndTick\":" + std::to_string(GetTickCount64()) + '}';
    saveText(directory, (L"cycle-phase-" + std::to_wstring(phase) + L"-done.json").c_str(), row + '\n');
    if (displayCycleFrames++) displayCyclePhases += ',';
    displayCyclePhases += row;
    context->ClearState(); context->Flush();
    traceCycleDevice(device, phase, "flushed");
  }
  displayCycleCompleted = true;
}
