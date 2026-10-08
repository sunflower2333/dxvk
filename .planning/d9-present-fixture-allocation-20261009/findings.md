# Findings

ColorFill02 ARM64 compile/link/import inspection passed. The complete fixture
failed at check 143242, line 3698, after the genuine new ColorFill marker and
existing ValidateDevice, D8 bridge and Vista markers. The mocked allocation
uses fixed 71; production owner tokens use a process-wide nextHandle sequence.
The setup did not keep those independent numeric identities distinct.

The correction chooses mock allocation 72 when the owner is 71, otherwise
71; both setup and post-Present checks require distinct identities and the
post-Present callback must contain that exact selected allocation. This
preserves rejection of accidental forwarding of a UMD resource token.

Independent failed-originals proof:
`/home/sunf/droidvm-repos/artifacts/d3d9-colorfill-native-20261009/independent-native-controlled-0c97035-02-failure-01/native-ColorFill02-failed-prefix-and-closure-reviewed.json`
15953 bytes, SHA256
`537a671ef897b9ee37d8aef03625cb6e2e2e816eaf841102ff3052c7c2767461`.
680 finite joins; attempt failed honestly; ownership finalized with no pending
scoped children, transports or parents. No x64 execution or hardware proof.
