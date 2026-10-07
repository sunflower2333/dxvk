#!/usr/bin/env python3
"""Directly review preserved DLL exports, native ownership and target release."""
import hashlib
import importlib.util
import json
from pathlib import Path
import struct
import sys
import tarfile

sys.dont_write_bytecode = True
WORKSPACE = Path('/home/sunf/droidvm-repos')
BASE = WORKSPACE / 'artifacts/dxvk-native-d3d8-system-device-20261008'
RUN = BASE / 'guest-wow64-export-originals-02'


def sha(data):
    return hashlib.sha256(data).hexdigest()


def read(path):
    return json.loads(path.read_text(encoding='utf-8-sig'))


def pin(path):
    data = path.read_bytes()
    return dict(path=str(path), bytes=len(data), sha256=sha(data))


def checked(row):
    path = Path(row['path'])
    assert all(pin(path)[k] == row[k] for k in ('bytes', 'sha256')), path
    return path


def closed(row, out, err):
    assert row['Pid'] > 0 and row['ProcessHandle'] > 0
    assert row['Exited'] and row['ExitCodeAvailable'] and row['ExitCode'] == 0
    assert row['PipesDrained'] and not row['TimedOut'] and not row['ChildStillRunning'] and not row['Failure']
    assert row['StdoutBytes'] == len(out) and row['StderrBytes'] == len(err)


class PE:
    def __init__(self, data):
        self.data = data
        assert len(data) >= 64 and data[:2] == b'MZ'
        pe = self.u32(60)
        assert pe <= len(data) - 24 and data[pe:pe + 4] == b'PE\0\0'
        self.machine, count = self.unpack('<HH', pe + 4)
        optional_size = self.u16(pe + 20)
        optional = pe + 24
        magic = self.u16(optional)
        assert magic in (0x10b, 0x20b)
        directories = optional + (96 if magic == 0x10b else 112)
        assert directories + 8 <= optional + optional_size <= len(data)
        self.export_rva, self.export_size = self.unpack('<II', directories)
        table = optional + optional_size
        self.sections = []
        for i in range(count):
            offset = table + 40 * i
            assert offset + 40 <= len(data)
            name = data[offset:offset + 8].rstrip(b'\0').decode('ascii')
            virtual_size, rva, size, raw = self.unpack('<IIII', offset + 8)
            flags = self.u32(offset + 36)
            assert raw + size <= len(data)
            self.sections.append((name, rva, raw, size, virtual_size, flags))

    def unpack(self, fmt, offset):
        assert 0 <= offset <= len(self.data) - struct.calcsize(fmt)
        return struct.unpack_from(fmt, self.data, offset)

    def u16(self, offset):
        return self.unpack('<H', offset)[0]

    def u32(self, offset):
        return self.unpack('<I', offset)[0]

    def locate(self, rva, size=1):
        matches = [section for section in self.sections if section[1] <= rva and
                   rva - section[1] <= section[3] - size]
        assert len(matches) == 1, (hex(rva), size)
        section = matches[0]
        return section[2] + rva - section[1], section

    def string(self, rva):
        offset, section = self.locate(rva)
        end = self.data.find(b'\0', offset, section[2] + section[3])
        assert end >= offset
        return self.data[offset:end].decode('ascii')

    def exports(self):
        assert self.export_rva and self.export_size >= 40
        offset, _ = self.locate(self.export_rva, 40)
        base, functions, names, function_rva, names_rva, ordinals_rva = self.unpack('<IIIIII', offset + 16)
        assert 0 < names <= functions < 100000
        functions_offset, _ = self.locate(function_rva, 4 * functions)
        names_offset, _ = self.locate(names_rva, 4 * names)
        ordinals_offset, _ = self.locate(ordinals_rva, 2 * names)
        exports = {}
        for index in range(names):
            name = self.string(self.u32(names_offset + index * 4))
            ordinal = self.u16(ordinals_offset + index * 2)
            assert ordinal < functions and name not in exports
            rva = self.u32(functions_offset + ordinal * 4)
            assert rva > 0
            forwarder = None
            if self.export_rva <= rva < self.export_rva + self.export_size:
                forwarder = self.string(rva)
            else:
                self.locate(rva)
            exports[name] = dict(ordinal=base + ordinal, rva=hex(rva), forwarder=forwarder)
        return exports

    def wow64_contracts(self):
        sections = [item for item in self.sections if item[0] == '.apiset']
        assert len(sections) == 1
        section = sections[0]
        data = self.data[section[2]:section[2] + section[3]]
        def fields(offset, count):
            assert 0 <= offset <= len(data) - count * 4
            return struct.unpack_from('<' + 'I' * count, data, offset)
        version, size, _, count, entries, _, _ = fields(0, 7)
        assert version == 6 and 28 <= size <= len(data) and count < 10000
        data = data[:size]
        def string(offset, length):
            assert length % 2 == 0 and 0 <= offset <= len(data) - length
            return data[offset:offset + length].decode('utf-16-le')
        contracts = []
        for index in range(count):
            _, name_offset, name_length, _, values, value_count = fields(entries + index * 24, 6)
            name = string(name_offset, name_length)
            if not name.startswith('api-ms-win-core-wow64-'):
                continue
            hosts = []
            for value_index in range(value_count):
                _, alias_offset, alias_length, host_offset, host_length = fields(values + value_index * 20, 5)
                hosts.append(dict(alias=string(alias_offset, alias_length), host=string(host_offset, host_length)))
            contracts.append(dict(contract=name, values=hosts))
        return dict(version=version, count=count, wow64_contracts=contracts)


def main():
    release_path = RUN / 'target-readonly-export-release-01.json'
    assert pin(release_path)['sha256'] == '772fe285e354056a947ef34b6cd0959ee4ffa7e2d1b29494581a968ad58f8249'
    release = read(release_path)
    assert release['released'] and release['released_from'] == '/root/verify_ewdk_build' and release['released_to'] == '/root'
    assert release['pending_owned_processes'] == release['pending_owned_transports'] == release['tasks_created'] == 0
    for field in ('probe_execution', 'registry_writes', 'installation', 'VM_changes', 'continuation', 'source_files_changed'):
        assert release[field] is False
    assert release['core_loads'] == release['KMT_calls'] == release['runtime_factories'] == release['GPU_runs'] == 0
    checked(release['original_review'])
    archive = checked(release['original_archive'])
    assert pin(archive)['sha256'] == '504184770a846a14bcfcdd304594ee3e4fdfb696993d9a316e7d29595205ef14'
    collection = read(Path(str(archive) + '.collection-original.json'))
    assert collection['completed'] and collection['unchanged']
    assert collection['archive_bytes'] == archive.stat().st_size == 1462431
    assert collection['archive_sha256'] == pin(archive)['sha256']
    expected = {row['name']: row for row in collection['members']}
    assert len(expected) == len(collection['members']) == 16
    members = {}
    with tarfile.open(archive) as tar:
        for item in tar:
            name = item.name.removeprefix('./')
            assert not Path(name).is_absolute() and '..' not in Path(name).parts
            assert item.isdir() or item.isfile()
            if item.isdir():
                continue
            assert name in expected and name not in members
            data = tar.extractfile(item).read()
            assert len(data) == expected[name]['bytes'] and sha(data) == expected[name]['sha256']
            assert data == (RUN / 'originals' / name).read_bytes()
            members[name] = data
    assert set(members) == set(expected)
    decoded = lambda name: json.loads(members[name].decode('utf-8-sig'))
    pins = decoded('input-pins-original.json')
    assert sha(members['input-pins-original.json']) == 'a5e1987c18ca58f8a4299cb537d5527a35673f3e474d1439b0efbbfa6672e365'
    for row in pins['inputs']:
        data = members['executed-inputs-original/' + row['name']]
        assert len(data) == row['bytes'] and sha(data) == row['sha256']
        assert data == (BASE / 'wow64-export-inputs-handoff-01/inputs' / row['name']).read_bytes()
    inspection = decoded('inspect-process-original.json')
    closed(inspection['process'], members['inspect.stdout.raw'], members['inspect.stderr.raw'])
    assert members['inspect.stdout.raw'] == b'SYSTEM_D3D8_WOW64_EXPORT_ORIGINALS_PASS files=3 source_writes=0 KMT_calls=0 runtime_factories=0 GPU_runs=0\r\n'
    closed(collection['process'], Path(str(archive) + '.stdout.raw').read_bytes(), Path(str(archive) + '.stderr.raw').read_bytes())
    assert inspection['deadline_ms'] == 30000 and collection['command']['deadline_ms'] == 60000
    assert release['native_processes'] == [inspection['process'], collection['process']]
    raw_hash = 'd8cf5089bfe02483e8fc3014645ebe08a2683ad53b2a9052637eb46586e0ddad'
    assert inspection['raw_source_sha256'] == collection['raw_source_sha256'] == raw_hash
    assert decoded('raw-owner-typecompile-original.json') == dict(source_sha256=raw_hash, type='DxvkRawProcessF4_02', compiled=True)
    result = decoded('source-copy-source-after-original.json')
    assert result['completed'] and result['status'] == 'PASS' and result['failure'] is None
    assert result['process']['pid'] == inspection['process']['Pid'] and result['process']['machine'] == 0xaa64 and result['process']['pointer_bytes'] == 8
    assert result['sources_before'] == result['sources_after'] and len(result['copies']) == 3
    for field in ('source_writes', 'KMT_calls', 'runtime_factories', 'core_loads', 'GPU_runs'):
        assert result[field] == 0
    providers = {}
    for source, copy in zip(result['sources_before'], result['copies']):
        name = source['path'].split('\\')[-1]
        data = members['system-DLL-originals/' + name]
        assert copy['source_path'] == source['path']
        assert all(copy[k] == source[k] for k in ('bytes', 'sha256', 'version', 'machine'))
        assert len(data) == source['bytes'] and sha(data) == source['sha256']
        providers[name] = PE(data)
        assert providers[name].machine == source['machine'] == (0xaa64 if name == 'apisetschema.dll' else 0x14c)
    kernel32, kernelbase = providers['kernel32.dll'].exports(), providers['kernelbase.dll'].exports()
    assert len(kernel32) == 1656 and len(kernelbase) == 2024
    assert 'GetSystemWow64Directory2W' not in kernel32 and 'GetSystemWow64Directory2A' not in kernel32
    assert kernel32['IsWow64Process2']['forwarder'] == 'api-ms-win-core-wow64-l1-1-1.IsWow64Process2'
    assert kernelbase['GetSystemWow64Directory2W'] == dict(ordinal=834, rva='0x20ca80', forwarder=None)
    assert kernelbase['IsWow64Process2'] == dict(ordinal=1029, rva='0x14a5f0', forwarder=None)
    for name in ('GetSystemWow64Directory2W', 'IsWow64Process2'):
        _, section = providers['kernelbase.dll'].locate(int(kernelbase[name]['rva'], 16))
        assert section[5] & 0x20000000, name
    contracts = providers['apisetschema.dll'].wow64_contracts()
    assert contracts == dict(version=6, count=973, wow64_contracts=[dict(contract='api-ms-win-core-wow64-l1-1-3', values=[dict(alias='', host='kernelbase.dll')])])
    before, after = decoded('readiness-before-original.json'), decoded('readiness-after-original.json')
    for field in ('device_id', 'status', 'binary', 'installed_package', 'driver_binding', 'service', 'service_state', 'pnp_error', 'pnp_status', 'key'):
        assert before['active_device'][field] == after['active_device'][field]
    assert before['desktop'] == after['desktop'] and sorted(row['Id'] for row in before['desktop']) == [1864, 4464]
    assert before['active_device']['binary']['sha256'] == 'd48e118a89b83df49e1da2f4b26e57b13d7f40ee6a42ad19ff6ea0328989650a'
    assert before['active_device']['binary']['bytes'] == 411040 and before['active_device']['pnp_error'] == 0
    static_path = RUN / 'originals/executed-inputs-original/verify-registration-originals-09.py'
    assert pin(static_path)['sha256'] == '28f29b0e69e06adb78fc2d6da10d3cd3d1709cfbb7da4619c47b0f90cbbbe051'
    spec = importlib.util.spec_from_file_location('original_static_policy', static_path)
    policy = importlib.util.module_from_spec(spec); spec.loader.exec_module(policy)
    static = policy.verify_registration(before, after)
    assert static['verified'] and len(static['protected_static_value_names']) == 34
    hosts = []
    for directory in (BASE / 'guest-wow64-export-originals-01', RUN):
        for row in read(directory / 'host-process-originals.json'):
            assert row['pid'] > 0 and row['exited'] and not row['timed_out']
            checked(row['stdout']); checked(row['stderr'])
            hosts.append(row)
    assert len(hosts) == release['host_transports'] == 8
    assert [row['exit'] for row in hosts] == [0, 0, 0, 1, 1, 0, 0, 0]
    assert all(row['completed'] for row in release['orchestration_tools'])
    proof = dict(schema='root-wow64-export-originals-direct-review-v1', verified=True, release_accepted=True,
                 archive=pin(archive), release=pin(release_path), source=pin(Path(__file__)), original_files=16,
                 original_helpers=5, source_copy_source_after_equal=True, native_processes=release['native_processes'],
                 host_transports=8, protected_static_values=34, desktop=before['desktop'],
                 kernel32_named_exports=len(kernel32), kernelbase_named_exports=len(kernelbase),
                 kernel32_directory2W_absent=True, kernel32_directory2A_absent=True,
                 kernel32_IsWow64Process2=kernel32['IsWow64Process2'], kernelbase_directory2W=kernelbase['GetSystemWow64Directory2W'],
                 kernelbase_IsWow64Process2=kernelbase['IsWow64Process2'], native_APIset=contracts,
                 runtime_API_execution=False, KMT_calls=0, factories=0, GPU_runs=0, target_calls=False,
                 whole_registry_equality=False)
    output = RUN / 'root-wow64-export-originals-direct-review-01.json'
    with output.open('x') as stream:
        json.dump(proof, stream, indent=2); stream.write('\n')
    print(json.dumps(dict(verified=True, release_accepted=True, original_files=16,
                         kernel32_directory2W_absent=True, kernelbase_directory2W_present=True,
                         native_APIset=contracts, target_calls=False, proof=pin(output))))


if __name__ == '__main__':
    main()
