# GPU source checkpoint, 2026-11-08

Preserve the current native DXVK UMD allocation, native shared surface, primary access, legacy device and process teardown changes. All 975 retained stage73 source inputs match this working tree byte for byte. The installed ARM64 core remains f00afdb615a482de26f51ab20ac39a5c18632392b9eae02ffa2504faab45ec7d. The independent process teardown fixture is a controlled host test; Edge WebGL still requires post-host-fix validation. This source checkpoint makes no full conformance or performance claim.
