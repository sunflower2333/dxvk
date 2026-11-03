#!/usr/bin/env python3
"""Actual-source Windows DLL lifecycle control; no GPU or driver calls."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import time

ROOT = Path('/home/sunf/droidvm-repos')
TOOLCHAIN = ROOT / 'artifacts/toolchains/llvm-mingw-20261006-ucrt-ubuntu-22.04-x86_64/bin'

def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def block(raw, signature):
    start = raw.index(signature)
    end = raw.index('{', start) + 1
    depth = 1
    while depth:
        depth += (raw[end] == '{') - (raw[end] == '}')
        end += 1
    return raw[start:end]

def globals_for(raw, names):
    found = []
    for name in names:
        holder = re.search(r'^dxvk::umd::ProcessStorage<[^\n]+> ' + name + r'Lifetime;\nauto& ' + name + r' = ' + name + r'Lifetime.get\(\);', raw, re.M)
        plain = re.search(r'^(?:std::|dxvk::umd::)[^\n]+ ' + name + r';', raw, re.M)
        item = holder or plain
        assert item, name
        found.append(item)
    for expression in (r'^bool deviceStorageClosing = false;',):
        item = re.search(expression, raw, re.M)
        if item:
            found.append(item)
    if 'struct DeviceStorageShutdown {' in raw:
        guard = block(raw, 'struct DeviceStorageShutdown {') + ' deviceStorageShutdown;'
        found.append(re.search(re.escape(guard), raw))
    assert all(found)
    return '\n'.join(x.group(0) for x in sorted(found, key=lambda x: x.start())) + '\n'

def extract(source, out, variant):
    snap = out / 'source/src/umd'
    snap.mkdir(parents=True)
    for name in ('umd_ddi.cpp', 'umd_d3d9_device.cpp', 'umd_adapter.cpp', 'umd_d3d9_adapter.cpp', 'umd_runtime_service.h', 'umd_private_children.h', 'umd_process_storage.h'):
        shutil.copyfile(source / 'src/umd' / name, snap / name)
    native = (snap / 'umd_ddi.cpp').read_text()
    legacy = (snap / 'umd_d3d9_device.cpp').read_text()
    if variant == 'late-lock-check':
        native = native.replace('    if (dxvk::umd::processTerminating) return;\n', '', 1)
        needle = '      std::lock_guard<std::mutex> lock(deviceStorageMutex);\n      deviceStorageClosing = true;'
        assert needle in native
        native = native.replace(needle, '      std::lock_guard<std::mutex> lock(deviceStorageMutex);\n      if (dxvk::umd::processTerminating) return;\n      deviceStorageClosing = true;', 1)
    if variant == 'no-child-holder':
        for name, kind in (('queryStorage', 'Query'), ('layoutStorage', 'InputLayout')):
            old = 'dxvk::umd::ProcessStorage<dxvk::umd::PrivateChildren<' + kind + ', dxvk::umd::RuntimeService>> ' + name + 'Lifetime;\nauto& ' + name + ' = ' + name + 'Lifetime.get();'
            assert old in native
            native = native.replace(old, 'dxvk::umd::PrivateChildren<' + kind + ', dxvk::umd::RuntimeService> ' + name + ';', 1)
    if variant == 'no-legacy-holder':
        for name in ('devices', 'runtimeDevices'):
            old = 'dxvk::umd::ProcessStorage<std::unordered_map<HANDLE, std::shared_ptr<Device>>> ' + name + 'Lifetime;\nauto& ' + name + ' = ' + name + 'Lifetime.get();'
            assert old in legacy
            legacy = legacy.replace(old, 'std::unordered_map<HANDLE, std::shared_ptr<Device>> ' + name + ';', 1)
    if variant == 'always-skip':
        native = native.replace('if (dxvk::umd::processTerminating) return;', 'if (true) return;', 1)
        header = (snap / 'umd_process_storage.h').read_text()
        assert 'if (!processTerminating)' in header
        (snap / 'umd_process_storage.h').write_text(header.replace('if (!processTerminating)', 'if (false)'))
    (snap / 'umd_ddi.cpp').write_text(native)
    (snap / 'umd_d3d9_device.cpp').write_text(legacy)
    if variant == 'no-adapter-holder':
        for name, key in (('umd_adapter.cpp', 'void*'), ('umd_d3d9_adapter.cpp', 'HANDLE')):
            file = snap / name
            text = file.read_text()
            old = 'dxvk::umd::ProcessStorage<std::unordered_map<' + key + ', std::shared_ptr<Adapter>>> adaptersLifetime;\nauto& adapters = adaptersLifetime.get();'
            assert old in text
            file.write_text(text.replace(old, 'std::unordered_map<' + key + ', std::shared_ptr<Adapter>> adapters;', 1))
    for name in ('umd_runtime_service.h', 'umd_private_children.h', 'umd_process_storage.h'):
        shutil.copyfile(snap / name, out / name)
    if 'BOOL WINAPI DllMain(' in native:
        dllmain = block(native, 'extern "C" BOOL WINAPI DllMain(').replace('DllMain(', 'ProductionDllMain(', 1)
    else:
        dllmain = 'extern "C" BOOL WINAPI ProductionDllMain(HINSTANCE, DWORD, LPVOID) { return TRUE; }'
    if variant == 'no-flag':
        assert 'dxvk::umd::processTerminating = true;' in dllmain
        dllmain = dllmain.replace('dxvk::umd::processTerminating = true;', '(void)0;')
    (out / 'production_dllmain.inc').write_text(dllmain + '\n')
    for kind, raw in (('native', native), ('legacy', legacy)):
        inc = out / kind
        inc.mkdir()
        adapter = (snap / ('umd_d3d9_adapter.cpp' if kind == 'legacy' else 'umd_adapter.cpp')).read_text()
        (inc / 'adapter_globals.inc').write_text(globals_for(adapter, ('adaptersMutex', 'adapters')))
        (inc / 'adapter_close.inc').write_text(block(adapter, 'HRESULT APIENTRY closeAdapter(') + '\n')
        (inc / 'device_close.inc').write_text(block(raw, 'HRESULT close() noexcept') + '\n' + block(raw, '~Device()') + '\n')
        if kind == 'legacy':
            (inc / 'registry_globals.inc').write_text(globals_for(raw, ('devicesMutex', 'devices', 'runtimeDevices')))
            (inc / 'result_mapping.inc').write_text(block(raw, 'HRESULT result(HRESULT hr)') + '\n')
            (inc / 'device_destroy.inc').write_text(block(raw, 'void releaseRuntime(') + '\n' + block(raw, 'HRESULT APIENTRY destroyDevice(') + '\n')
        else:
            (inc / 'registry_globals.inc').write_text(globals_for(raw, ('deviceStorageMutex', 'deviceStorage', 'queryStorage', 'layoutStorage')))
            (inc / 'child_types.inc').write_text(block(raw, 'struct ComRetirement final') + ';\n' + block(raw, 'struct Child {') + ';\n')
            (inc / 'device_record.inc').write_text(re.search(r'enum class DevicePhase[^;]+;', raw).group(0) + '\n' + block(raw, 'struct DeviceRecord {') + ';\n')
            (inc / 'child_retirement.inc').write_text('template<typename Object, typename... Interfaces>\n' + block(raw, 'void retireRegisteredChild(') + '\n' + block(raw, 'void retireRegisteredQuery(') + '\n' + block(raw, 'void clearRegisteredChildren(Device* device) noexcept') + '\n')
            (inc / 'device_release.inc').write_text(block(raw, 'void releaseDeviceStorage(') + '\n')
            (inc / 'device_destroy.inc').write_text(block(raw, 'void APIENTRY destroyDevice(') + '\n')

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--source', type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--arches', nargs='+', choices=['arm64', 'x64', 'x86'], default=['x64'])
    parser.add_argument('--variant', choices=['candidate', 'before', 'no-flag', 'late-lock-check', 'no-child-holder', 'no-legacy-holder', 'no-adapter-holder', 'always-skip'], default='candidate')
    parser.add_argument('--run-wine', action='store_true')
    parser.add_argument('--case', action='append', dest='selected')
    args = parser.parse_args()
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=False)
    extract(args.source.resolve(), out, args.variant)
    for name in ('umd-dll-process-teardown.cpp', 'umd-dll-process-host.cpp'):
        shutil.copyfile(Path(__file__).with_name(name), out / name)
    builds = []
    for arch in args.arches:
        dest = out / arch
        dest.mkdir()
        compiler = TOOLCHAIN / ({'x64': 'x86_64', 'arm64': 'aarch64', 'x86': 'i686'}[arch] + '-w64-mingw32-clang++')
        for kind in ('native', 'legacy', 'host'):
            output = dest / ('registry-host.exe' if kind == 'host' else kind + '-registry.dll')
            command = [str(compiler), '-std=c++17', '-O1', '-g', '-static', '-pthread', '-I', str(out)]
            if kind == 'host':
                command += [str(out / 'umd-dll-process-host.cpp'), '-municode']
            else:
                command += ['-I', str(out / kind), str(out / 'umd-dll-process-teardown.cpp'), '-shared']
                if kind == 'legacy':
                    command += ['-DFIXTURE_LEGACY=1']
            command += ['-o', str(output)]
            with (dest / (kind + '-build.log')).open('w') as log:
                built = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT)
            row = dict(arch=arch, kind=kind, command=command, exit=built.returncode)
            if built.returncode == 0:
                row['sha256'] = digest(output)
                data = subprocess.run(['llvm-readobj', '--file-headers', '--coff-imports', '--coff-exports', str(output)], capture_output=True, text=True, check=True)
                (dest / (kind + '-pe.txt')).write_text(data.stdout)
            builds.append(row)
            print(json.dumps(row), flush=True)
    cases = []
    if args.run_wine and all(x['exit'] == 0 for x in builds):
        assert 'x64' in args.arches
        root = ROOT / 'artifacts/vkd3d-umd-port-20261018'
        env = dict(os.environ, WINEDEBUG='-all', WINEPREFIX=str(root / 'wine-prefix'), WINEDLLPATH=str(root / 'wine-root/usr/lib/x86_64-linux-gnu/wine'), WINEDLLOVERRIDES='winemenubuilder.exe=d;winedbg.exe=d')
        for kind in ('native', 'legacy'):
            for policy in (('inline', 'worker') if kind == 'native' else ('worker',)):
                for mode, warm in (('exit', 'cold'), ('exit', 'warm'), ('exit-locked', 'warm'), ('unload', 'cold'), ('unload', 'warm'), ('close-exit', 'cold'), ('close-exit', 'warm'), ('close-unload', 'cold'), ('close-unload', 'warm')):
                    name = '-'.join((kind, policy, mode, warm))
                    if args.selected and name not in args.selected:
                        continue
                    command = [str(root / 'wine-root/usr/lib/wine/wine64'), str(out / 'x64/registry-host.exe'), str(out / 'x64' / (kind + '-registry.dll')), policy, mode, warm]
                    start = time.monotonic()
                    with (out / (name + '.stdout')).open('w') as stdout, (out / (name + '.stderr')).open('w') as stderr:
                        try:
                            result = subprocess.run(command, env=env, stdout=stdout, stderr=stderr, timeout=15)
                            row = dict(name=name, exit=result.returncode, timeout=False)
                        except subprocess.TimeoutExpired:
                            row = dict(name=name, exit=None, timeout=True)
                    text = (out / (name + '.stdout')).read_text()
                    marker = 'ORACLE_UNLOAD_CLEAN_PASS' if mode in ('unload', 'close-unload') else 'ORACLE_EXIT_NO_CLEANUP_PASS'
                    row.update(seconds=round(time.monotonic() - start, 3), command=command,
                               passed=row['exit'] == 0 and marker in text,
                               setup_complete='HOST_SETUP_RETURN' in text, held_lock='REGISTRY_LOCK_HELD' in text,
                               callback_count=text.count('RUNTIME_CALLBACK'))
                    cases.append(row)
                    print(json.dumps(row), flush=True)
    receipt = dict(variant=args.variant, builds=builds, cases=cases,
                   inputs=[dict(path=str(p.relative_to(out)), sha256=digest(p)) for p in sorted((out / 'source').rglob('*')) if p.is_file()],
                   fixture_sha256=digest(out / 'umd-dll-process-teardown.cpp'), host_sha256=digest(out / 'umd-dll-process-host.cpp'),
                   passed=all(x['exit'] == 0 for x in builds) and all(x['passed'] for x in cases),
                   boundary='Actual production close/destructor/ordered registries/DllMain and real RuntimeService in a Windows DLL; controlled COM/GPU peers, local Wine CPU only. TerminateProcess(90) is an explicit oracle failure, not a natural access violation.')
    (out / 'result.json').write_text(json.dumps(receipt, indent=2) + '\n')
    return 0 if receipt['passed'] else 1

if __name__ == '__main__':
    raise SystemExit(main())
