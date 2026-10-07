#!/usr/bin/env python3
"""Run portable selector/pin contracts and exact semantic controls, without Windows APIs."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

CONTROLS = {
    'name-version': ('version != dx9DriverNameVersion', '(version & 0u) != 0u',
                     '!select(output, 0, 1, queryBytes, version)'),
    'original-status': ('originalStatus != 0', '(originalStatus & 0) != 0', '!select(output, status)'),
    'core-prefix': ('if (asciiFold(path[prefixBytes + i]) != asciiFold(commit[i])) return false;',
                    'if (commit.empty()) return false;',
                    '!policy::ownedCorePath(View(corePath), View(u"abcdef6789abcdef0123456789abcdef01234567"))'),
    'hex-alphabet': ("character <= Char('f')", "character <= Char('g')", '!policy::hexIdentity(invalid, 40)'),
}


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    header = root / 'tests/umd-d3d8-runtime-policy.h'
    fixture = root / 'tests/umd-d3d8-runtime-policy.cpp'
    sources = {str(path): sha(path) for path in [header, fixture]}
    proof = {'scope': 'GCC/Clang ASan+UBSan CPU contracts only', 'commands': [],
             'sources_before': sources, 'target_operations': 0, 'gpu_runs': 0,
             'native_windows_execution': False}

    def save():
        (output / 'verified.json').write_text(json.dumps(proof, indent=2) + '\n')

    def run(name, command, expected=0, seconds=60):
        row = {'name': name, 'command': command, 'expected': expected, 'deadline_seconds': seconds}
        proof['commands'].append(row)
        save()
        result = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=seconds)
        (output / (name + '.stdout.txt')).write_bytes(result.stdout)
        (output / (name + '.stderr.txt')).write_bytes(result.stderr)
        row['exit'] = result.returncode
        save()
        if result.returncode != expected:
            raise RuntimeError(f'{name}: exit {result.returncode}; original stdout/stderr retained')
        return result

    try:
        original_header = header.read_text()
        original_tests = fixture.read_text()
        assert original_tests.count('std::exit(1)') == 1
        negative_tests = original_tests.replace('std::exit(1)', 'std::_Exit(1)')
        for compiler in ['g++', 'clang++']:
            label = compiler.replace('+', 'p')
            common = [compiler, '-std=c++17', '-O1', '-g', '-Wall', '-Wextra', '-Wshadow', '-Werror',
                      '-fsanitize=address,undefined', '-fno-omit-frame-pointer']
            run(label + '-version', [compiler, '--version'])
            binary = output / (label + '-positive')
            run(label + '-positive-compile', common + [str(fixture), '-o', str(binary)])
            result = run(label + '-positive-run', [str(binary)], seconds=30)
            assert result.stderr == b'' and b'PASS checks=306;' in result.stdout
            for name, (before, after, assertion) in CONTROLS.items():
                assert original_header.count(before) == 1
                control = output / (label + '-' + name)
                control.mkdir()
                (control / header.name).write_text(original_header.replace(before, after))
                (control / fixture.name).write_text(negative_tests)
                mutation = {'production_before': before, 'production_after': after,
                            'semantic_expression_count': 1, 'test_change': 'std::exit(1) -> std::_Exit(1)',
                            'intended_assertion': assertion, 'original_header_sha256': sources[str(header)],
                            'mutated_header_sha256': sha(control / header.name)}
                (control / 'mutation.json').write_text(json.dumps(mutation, indent=2) + '\n')
                binary = control / 'negative'
                run(label + '-' + name + '-compile', common + [str(control / fixture.name), '-o', str(binary)])
                result = run(label + '-' + name + '-run', [str(binary)], 1, 30)
                error = result.stderr.decode()
                assert error.count('D3D8 runtime policy CHECK failed') == 1 and assertion in error
                assert b'Sanitizer' not in result.stderr
        proof['sources_after'] = {str(path): sha(path) for path in [header, fixture]}
        assert sources == proof['sources_after']
        proof.update(status='PASS', positive_checks_per_compiler=306, exact_negative_controls=8)
        save()
        print(json.dumps({'status': proof['status'], 'positive_checks_per_compiler': 306,
                          'exact_negative_controls': 8, 'target_operations': 0}, indent=2))
    except Exception as error:
        proof.update(status='FAIL', error=str(error))
        save()
        raise


if __name__ == '__main__':
    main()
