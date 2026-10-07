# Findings

Microsoft CreateGeometryShaderWithStreamOutput accepts prior-stage VS/DS DXBC;
embedded DXVK creates a passthrough GS for Xfb shaders. Null DDI code carries an
output signature, not instructions. Resolve SO from that signature and cache the
actual active producer's bytecode. D3D11 signature-only mapping shares the full
shader mapper's declaration validation without inventing a shader token stream.
Generic varyings retain raw 32-bit bits. Required output masks must exist in the
producer; no output write occurs after a missing-output rejection.
D3D11 slice: stream 0/no rasterization/plain varyings or Position/prior no classes.
https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11device-creategeometryshaderwithstreamoutput
