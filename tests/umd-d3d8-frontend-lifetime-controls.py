#!/usr/bin/env python3
"""Exercise the production frontend owner's reference transitions on a CPU model.

The Windows loader primitives are modeled; this does not prove native DLL
unload/reload or graphics execution. The production owner body stays exact.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import time

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--scratch', type=Path, required=True)
args = parser.parse_args()
repo = Path(__file__).resolve().parents[1]
source = repo / 'tests/umd-d3d8-runtime-probe.cpp'
text = source.read_text()
begin = text.index('struct FrontendReference {')
end = text.index('\n};', begin) + 3
owner = text[begin:end]
assert args.scratch.is_dir()
control = args.scratch / 'exact-frontend-reference-controls.cpp'
assert not control.exists()
control.write_text(r'''
#include <cassert>
#include <cstdint>
#include <cstdarg>
#include <cwchar>
#include <stdexcept>
#include <string>
#include <type_traits>
#include "umd-d3d8-runtime-policy.h"
namespace policy = dxvk::test::runtime8;
using HMODULE = void*;
constexpr unsigned IMAGE_FILE_MACHINE_I386 = 0x14c;
constexpr unsigned LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR = 0x100;
constexpr unsigned LOAD_LIBRARY_SEARCH_DEFAULT_DIRS = 0x1000;
constexpr int FALSE = 0;
const wchar_t* selected = L"C:\\Users\\Public\\DxvkD3D8Runtime-37b8a2d-icd02\\front\\viogpu-d3d8-runtime-front.dll";
struct Loader {
 unsigned refs=0, physicalLoads=0, loads=0, frees=0;
 bool goodPath=true, goodMachine=true, goodExport=true;
} loader;
HMODULE handle() { return reinterpret_cast<HMODULE>(uintptr_t(1)); }
HMODULE GetModuleHandleW(const wchar_t*) { return loader.refs ? handle() : nullptr; }
HMODULE LoadLibraryExW(const wchar_t* p, void*, unsigned flags) {
 assert(std::wstring(p)==selected && flags==(0x100|0x1000));
 ++loader.loads;
 if(!loader.refs) ++loader.physicalLoads;
 ++loader.refs;
 return handle();
}
int FreeLibrary(HMODULE p) { assert(p==handle() && loader.refs); ++loader.frees; --loader.refs; return 1; }
unsigned moduleMachine(HMODULE p) { assert(p==handle() && loader.refs); return loader.goodMachine ? 0x14c : 0xaa64; }
std::wstring path(HMODULE p) { assert(p==handle() && loader.refs); return loader.goodPath ? selected : L"C:\\Other\\frontend.dll"; }
void* GetProcAddress(HMODULE p, const char*) { assert(p==handle() && loader.refs); return loader.goodExport ? handle() : nullptr; }
int _wcsicmp(const wchar_t* a,const wchar_t* b) { return std::wcscmp(a,b); }
void trace(const char*,...) { }
void require(bool value,const char* why) { if(!value) throw std::runtime_error(why); }
''' + owner + r'''
static_assert(!std::is_copy_constructible_v<FrontendReference>);
static_assert(!std::is_copy_assignable_v<FrontendReference>);
struct RuntimeReference {
 HMODULE module;
 RuntimeReference():module(LoadLibraryExW(selected,nullptr,0x1100)){}
 ~RuntimeReference(){FreeLibrary(module);}
};
int main() {
 // A runtime reference can disappear between caps and public creation;
 // the independently owned reference keeps the same physical DLL instance.
 {
  FrontendReference owner; owner.open(selected);
  assert(loader.refs==1 && loader.physicalLoads==1);
  {RuntimeReference caps; assert(loader.refs==2);}
  assert(loader.refs==1 && GetModuleHandleW(selected));
  {RuntimeReference publicDevice; assert(loader.refs==2 && loader.physicalLoads==1);}
  assert(loader.refs==1 && owner.release());
  assert(loader.refs==0 && loader.loads==3 && loader.frees==3);
  assert(owner.release() && loader.frees==3);
 }
 assert(loader.refs==0 && loader.frees==3);
 // Exception unwinding releases runtime references before the owner.
 loader={};
 try { FrontendReference owner; owner.open(selected); RuntimeReference runtime; throw std::runtime_error("runtime-failure"); }
 catch(const std::runtime_error&){}
 assert(loader.refs==0 && loader.physicalLoads==1 && loader.loads==2 && loader.frees==2);
 // A foreign frontend reference is rejected and never adopted or released.
 loader={};loader.refs=1;
 try { FrontendReference owner; owner.open(selected); assert(false); }
 catch(const std::runtime_error&){}
 assert(loader.refs==1 && loader.loads==0 && loader.frees==0);
 FreeLibrary(handle());
 // The exact production owner cleans up its own load after every identity
 // failure. None of these cases constructs a core or opens an adapter.
 for(unsigned fault=0;fault<3;++fault) {
  loader={};loader.goodPath=fault!=0;loader.goodMachine=fault!=1;loader.goodExport=fault!=2;
  try { FrontendReference owner; owner.open(selected); assert(false); }
  catch(const std::runtime_error&){}
  assert(loader.refs==0 && loader.loads==1 && loader.frees==1);
 }
 // Invalid owned paths fail before acquiring a loader reference.
 loader={};
 try { FrontendReference owner; owner.open(L"C:\\Other\\frontend.dll");assert(false); }
 catch(const std::runtime_error&){}
 assert(loader.refs==0 && loader.loads==0 && loader.frees==0);
 return 0;
}
''')

def pin(path):
    return dict(path=str(path), bytes=path.stat().st_size,
                sha256=hashlib.sha256(path.read_bytes()).hexdigest())

receipts = []
binary = args.scratch / 'exact-frontend-reference-controls'
commands = [('lifetime-model-build', ['clang++', '-std=c++17', '-O1', '-Wall', '-Wextra', '-Werror', '-fsanitize=address,undefined', '-I'+str(repo/'tests'), str(control), '-o', str(binary)]), ('lifetime-model-controls', [str(binary)])]
for name, argv in commands:
    out = args.scratch / (name+'.stdout.raw')
    err = args.scratch / (name+'.stderr.raw')
    receipt = args.scratch / (name+'.process-original.json')
    assert not out.exists() and not err.exists() and not receipt.exists()
    start = time.monotonic()
    timeout = False
    with out.open('xb') as stdout, err.open('xb') as stderr:
        process = subprocess.Popen(argv, stdout=stdout, stderr=stderr)
        row = dict(argv=argv, pid=process.pid, completed=False, exit=None,
                   timeout=False, reaped=False, pipes_drained=False)
        receipt.write_text(json.dumps(row, indent=2)+'\n')
        try:
            process.wait(timeout=30)
        except subprocess.TimeoutExpired:
            timeout=True; process.kill(); process.wait(timeout=10)
        row.update(completed=True, exit=process.returncode, timeout=timeout,
                   reaped=True, seconds=time.monotonic()-start)
    row.update(pipes_drained=True, stdout=pin(out), stderr=pin(err))
    receipt.write_text(json.dumps(row, indent=2)+'\n')
    receipts.append(pin(receipt))
    assert not timeout and process.returncode==0, name
assert source.read_text()==text
proof = args.scratch/'frontend-reference-model-originals-01.json'
assert not proof.exists()
proof.write_text(json.dumps(dict(status='PASS', source=pin(source), exact_owner_body_sha256=hashlib.sha256(owner.encode()).hexdigest(), control=pin(control), cases=7, owned_processes=receipts, modeled_Windows_loader_primitives=True, native_DLL_execution=False, target_calls=0, core_factories=0, GPU_runs=0), indent=2)+'\n')
print(json.dumps(pin(proof)))
