# Findings

- Existing front modes load genuine Microsoft D3D8 but substitute its KMT IAT
  filename result. Neither those controls nor app-local DXVK8 prove ordinary routing.
- Ordinary DX8 is x86 on this target. Legacy WoW slot0 serves both DX8 and DX9;
  genuine D3D8 directly requests Interface8. Public core entry already supports it.
- No new mandatory DDI8 surface gap is established by this audit. Remaining
  first-submit issue is shared with Api9 and is being diagnosed by ROOT.
- Native ARM64 name queries cannot prove WoW selection. Read-only x86 helper
  uses supplied LUID/session0-safe OpenAdapterFromLuid, no HDC/factory/core load.
- Existing strict x86 payload and projected caps stay fixed. Future CI core/run
  and actual forward LUID/source fields are not fabricated.
- New ordinary held and names records use fresh temp, flush/close and rename
  without replacement. Missing system runtime fails through ordinary hold.
