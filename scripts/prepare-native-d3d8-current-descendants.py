#!/usr/bin/env python3
"""Prepare fresh parser and USER handoffs from admitted current-core inputs.

This recipe is local only. It never runs the parser, staging, SSH, a USER task,
or a GPU workload. The generated hosts retain their existing explicit execution
arguments and authorization checks. Default mode only checks frozen templates.
"""
import argparse
import ast
import copy
import difflib
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys
import time

WORKSPACE = Path('/home/sunf/droidvm-repos')
ARTIFACTS = WORKSPACE / 'artifacts/dxvk-native-d3d8-system-device-20261008'
OLD_RUN_ID = 'frontend-lifetime-fbd7afd-01'
OLD_CORE = 'de72dc2e97bd8e4ea70c5bf89c26918d06065723'
OLD_CI = 37711793677
OLD_RUNNER = '7d9cfd0d61b8622da87f545ea1358a4f25cab404'
RUNNER = 'ad60a25b06f435f32af63ffe7f64e6f0b5de6a25'
PROBE = 'fbd7afdbdd277d9b2327a13efda5a0aca3905b25'
SETUP = '37b8a2dde5bfe7022ccc676594c1f57621cc8ff9'
CPU = 'e3ac12646a1b55742d575109063ae78507af5cec'
CURRENT_VERIFIER_SHA = 'bfe78c8ce6d1b12e1e07d4686a3c90a02d4457a17e48cf23ebb3a0e98f6ce658'
PHASES = ('names', 'enumerate', 'offscreen', 'present')
PENDING = ['Separate ROOT-authorized names/enumerate/offscreen/present execution and original review']
LOCK = Path(__file__).with_name('native-d3d8-descendant-template-lock.json')


def require(value, message):
    if not value:
        raise ValueError(message)


def pin(path):
    raw = path.read_bytes()
    return dict(path=str(path), bytes=len(raw), sha256=hashlib.sha256(raw).hexdigest())


def read(path):
    return json.loads(path.read_text(encoding='utf-8-sig'))


def original(row):
    path = Path(row['path'])
    require(path.is_absolute() and pin(path) == row, 'Pinned original differs: ' + str(path))
    return path


def write(path, value):
    require(not path.exists(), 'Fresh output required: ' + str(path))
    path.write_text(json.dumps(value, indent=2) + '\n')


def once(text, before, after):
    require(text.count(before) == 1, 'Expected one frozen binding: ' + repr(before))
    return text.replace(before, after, 1)


def frozen_templates():
    lock = read(LOCK)
    require(lock['schema'] == 'system-d3d8-descendant-template-lock-v1'
            and lock['historical_run'] == OLD_RUN_ID, 'Wrong template lock')
    require(len(lock['original_templates']) == 32, 'Exact parser and four phase templates required')
    return {row['path']: original(row).read_bytes() for row in lock['original_templates']}


def old_packet(phase=None):
    name = 'phase-native-parser-handoff-' if phase is None else f'limited-user-i386-{phase}-handoff-'
    return ARTIFACTS / (name + OLD_RUN_ID)


def old_host_name(phase=None):
    return ('deferred-phase-native-parser-' if phase is None
            else f'deferred-limited-user-i386-{phase}-') + OLD_RUN_ID + '.py'


def new_host_name(run_id, phase=None):
    return old_host_name(phase).replace(OLD_RUN_ID, run_id)


def checked_host_hash(text):
    candidates = []
    for node in ast.walk(ast.parse(text)):
        if (isinstance(node, ast.Compare) and isinstance(node.left, ast.Subscript)
                and isinstance(node.left.slice, ast.Constant) and node.left.slice.value == 'sha256'
                and len(node.comparators) == 1 and isinstance(node.comparators[0], ast.Constant)
                and isinstance(node.comparators[0].value, str)
                and re.fullmatch('[0-9a-f]{64}', node.comparators[0].value)):
            candidates.append(node.comparators[0].value)
    require(len(candidates) == 1, 'One original outer host hash required')
    return candidates[0]


class Bindings:
    def __init__(self, out, source, ci_run, manifest_sha, parser_sha, stage_packet,
                 stage_output, extra_source_commits):
        self.out, self.source, self.ci_run = out, source, ci_run
        self.run_id = f'current-core-{source[:7]}-{ci_run}-01'
        self.manifest_sha, self.parser_sha = manifest_sha, parser_sha
        self.extra_source_commits = extra_source_commits
        self.parser = out / ('phase-native-parser-handoff-' + self.run_id)
        self.packets = {p: out / f'limited-user-i386-{p}-handoff-{self.run_id}' for p in PHASES}
        self.parser_output = out / ('guest-phase-native-parser-' + self.run_id)
        self.outputs = {p: out / f'guest-limited-user-i386-{p}-{self.run_id}' for p in PHASES}
        self.admitted_manifest = self.parser / 'phase-inputs-admitted-original.json'
        self.stage_packet, self.stage_output = stage_packet, stage_output
        self.stage_proof = stage_output / 'owned-ICD-staging-originals-verified-03.json'
        self.stage_root = stage_output / 'root-staging03-originals-and-explicit-release-admitted-01.json'
        self.hal_proof = self.outputs['enumerate'] / 'native-I386-enumerate-originals-verified-01.json'
        self.hal_root = self.outputs['enumerate'] / 'root-enumerate01-current-core-originals-and-explicit-release-admitted-01.json'
        self.offscreen_proof = self.outputs['offscreen'] / 'native-I386-offscreen-originals-verified-01.json'
        self.offscreen_root = self.outputs['offscreen'] / 'root-offscreen01-current-core-originals-and-explicit-release-admitted-01.json'
        old_parser = old_packet()
        old_stage = ARTIFACTS / 'original-plus-derived-I386-candidate-staging-de72dc2-icd02'
        old_stage_output = ARTIFACTS / ('guest-' + old_stage.name)
        self.replacements = [
            (str(old_parser / 'phase-inputs-admitted-pending-corrected-02.json'), str(self.admitted_manifest)),
            (str(old_parser / 'phase-inputs-admitted-original.json'), str(self.admitted_manifest)),
            (str(ARTIFACTS / f'guest-limited-user-i386-enumerate-{OLD_RUN_ID}' /
                 'root-enumerate01-frontend-lifetime-originals-and-explicit-release-admitted-01.json'), str(self.hal_root)),
            (str(ARTIFACTS / f'guest-limited-user-i386-offscreen-{OLD_RUN_ID}' /
                 'root-offscreen01-frontend-lifetime-originals-and-explicit-release-admitted-01.json'), str(self.offscreen_root)),
            (str(old_stage_output), str(stage_output)), (str(old_stage), str(stage_packet)),
            (str(ARTIFACTS / ('guest-phase-native-parser-' + OLD_RUN_ID)), str(self.parser_output)),
            (str(old_parser), str(self.parser)),
        ]
        for phase in PHASES:
            self.replacements += [(str(old_packet(phase)), str(self.packets[phase])),
                (str(ARTIFACTS / f'guest-limited-user-i386-{phase}-{OLD_RUN_ID}'), str(self.outputs[phase]))]
        self.replacements += [(OLD_RUN_ID, self.run_id), (OLD_CORE, source),
                              (str(OLD_CI), str(ci_run)), ('de72dc2', source[:7]),
                              (OLD_RUNNER, RUNNER)]

    def change(self, text):
        for before, after in self.replacements:
            text = text.replace(before, after)
        return text

    def data(self, value):
        if isinstance(value, dict):
            return {k: self.data(v) for k, v in value.items()}
        if isinstance(value, list):
            return [self.data(v) for v in value]
        if isinstance(value, str):
            return self.change(value)
        return self.ci_run if type(value) is int and value == OLD_CI else value

    def parser_reader(self, text, old_manifest_sha):
        text = self.change(text).replace(old_manifest_sha, self.manifest_sha)
        needle = " setup_proof,setup_result,setup_members=verifier.verify_owned_icd_native_setup(manifest['native_setup'])"
        text = once(text, needle, " require_current=verifier.verify_current_core(manifest)\n"
            + " assert require_current=={'source':manifest['core_source'],'run':manifest['core_ci_run'],'sha256':next(r['sha256'] for r in manifest['files'] if r['role']=='core')}\n"
            + needle)
        needle = " for name in ['verify-native-d3d8-system-phase.py','verify-native-d3d8-system-device.py']:"
        origins = {name: self.extra_source_commits[name] for name in
                   ('verify-native-d3d8-system-phase.py', 'verify-native-d3d8-system-device.py')}
        text = once(text, needle, ' extra_source_commits=' + repr(origins) + '\n' + needle)
        text = once(text, "manifest['runner_source']+':scripts/'+name",
                    "extra_source_commits[name]+':scripts/'+name")
        return text

    def phase_reader(self, text, old_parser_sha):
        # verify_archive already reopens the full current-core originals. Retain
        # that existing acceptance/failed-attempt/explicit-release workflow.
        return self.change(text).replace(old_parser_sha, self.parser_sha)


def save_script(path, before, after):
    require(not path.exists(), 'Fresh script required')
    if path.suffix == '.py':
        ast.parse(after, filename=str(path))
    path.write_text(after)
    diff = ''.join(difflib.unified_diff(before.splitlines(True), after.splitlines(True),
                fromfile='frozen-predecessor/' + path.name, tofile=str(path)))
    path.with_name(path.name + '.vs-original.diff').write_text(diff)


def run_default(host):
    """Own one local plan child; the execution switch is never provided."""
    stdout = host.parent / 'default-plan.stdout.raw'
    stderr = host.parent / 'default-plan.stderr.raw'
    argv = [sys.executable, str(host)]
    require(not stdout.exists() and not stderr.exists(), 'Fresh local capture required')
    start = time.monotonic()
    timed_out = False
    with stdout.open('xb') as out, stderr.open('xb') as err:
        child = subprocess.Popen(argv, stdout=out, stderr=err)
        write(host.parent / 'default-plan.started-original.json',
              dict(argv=argv, pid=child.pid, cwd=str(Path.cwd()), deadline_seconds=30))
        try:
            child.wait(timeout=30)
        except subprocess.TimeoutExpired:
            timed_out = True
            child.kill()
            child.wait(timeout=10)
    result = dict(argv=argv, pid=child.pid, completed=True, reaped=True,
        exit=child.returncode, timeout=timed_out, seconds=time.monotonic() - start,
        stdout_file_closed=out.closed, stderr_file_closed=err.closed,
        stdout=pin(stdout), stderr=pin(stderr), target_calls=0)
    write(host.parent / 'default-plan.process-original.json', result)
    require(not timed_out and child.returncode == 0 and stderr.stat().st_size == 0,
            'Local default plan failed; preserve raw process originals')
    plan = read(stdout)
    require(plan['target_calls_executed'] is False and plan['ready'] is False,
            'Default plan must not execute target calls')
    return result, plan


def preview_bindings(templates):
    """Check all source transformations without inventing an admitted core."""
    bindings = Bindings(Path('/nonexistent/local-binding-preview'), 'TEST_BINDINGS_ONLY', 0,
        'TEST_MANIFEST_BINDING_ONLY', 'TEST_PARSER_BINDING_ONLY',
        Path('/nonexistent/local-stage-preview'), Path('/nonexistent/local-stage-output-preview'),
        {'verify-native-d3d8-system-phase.py': RUNNER,
         'verify-native-d3d8-system-device.py': OLD_RUNNER})
    descriptor = json.loads(templates[str(old_packet() / 'phase-parser-inputs-01.json')])
    transformed = []
    for key, raw in templates.items():
        path = Path(key)
        if path.suffix not in ('.py', '.ps1'):
            continue
        before = raw.decode()
        if path.name == 'review-native-parser-frontend-lifetime-originals-01.py':
            after = bindings.parser_reader(before, descriptor['manifest_sha256'])
        elif path.name == 'review-current-phase-originals-01.py':
            after = bindings.phase_reader(before, pin(old_packet() / 'phase-parser-inputs-01.json')['sha256'])
        else:
            after = bindings.change(before)
            if path.name == 'run-phase-native-parser-prefix-01.ps1':
                after = after.replace(descriptor['manifest_sha256'], bindings.manifest_sha)
        if path.suffix == '.py':
            ast.parse(after, filename='local-binding-preview/' + path.name)
        require(OLD_CORE not in after and str(OLD_CI) not in after and OLD_RUN_ID not in after,
                'Historical execution binding retained: ' + key)
        # No preview is written as a packet and no core hash is constructed.
        transformed.append(dict(original=pin(path), preview_syntax_checked=path.suffix == '.py',
            binding_only_preview=True, target_calls=0))
    require(len(transformed) == 17, 'All parser and USER source templates required')
    return transformed


def validate_inputs(phase, stage):
    prepared_path = phase / 'prepared-current-core-phase-source-01.json'
    prepared = read(prepared_path)
    manifest_path = original(prepared['manifest'])
    require(manifest_path == phase / 'helpers/phase-inputs-original.json', 'Exact phase input directory required')
    manifest = read(manifest_path)
    require(prepared['prepared'] is True and prepared['ready'] is False
            and manifest['ready'] is False and manifest['native_phase_parse'] == 'pending'
            and manifest['payload_staged'] is False and manifest['hardware_admission'] is False,
            'Only the actual unready current-core phase packet is accepted')
    require(manifest['probe_source'] == PROBE and manifest['runner_source'] == RUNNER
            and manifest['native_cpu']['accepted'] is True and manifest['native_cpu']['source'] == CPU
            and manifest['native_setup']['accepted'] is True and manifest['native_setup']['source'] == SETUP
            and manifest['native_lifetime']['accepted'] is True and manifest['native_lifetime']['source'] == PROBE,
            'Separate unchanged native producer scopes must remain accepted')
    reference = original(prepared['current_core_reference'])
    require(manifest['current_core']['ROOT_reference'] == pin(reference), 'Current-core reference differs')
    verifier = phase / 'helpers/verify-native-d3d8-system-phase.py'
    require(pin(verifier)['sha256'] == CURRENT_VERIFIER_SHA, 'Frozen actual current-core verifier required')
    scope = {'__name__': 'descendant_current_core_join', '__file__': str(verifier)}
    exec(compile(verifier.read_bytes(), str(verifier), 'exec'), scope)
    identity = scope['verify_current_core'](manifest)
    require(identity == prepared['current_core_identity'], 'Actual phase current-core identity differs')
    helpers = phase / 'helpers'
    require(len(list(helpers.iterdir())) == 11, 'Exact ten helpers and unready manifest required')
    for row in manifest['helpers']:
        p = helpers / row['name']
        require(p.stat().st_size == row['bytes'] and pin(p)['sha256'] == row['sha256'], 'Phase helper differs')
    extra = {}
    for row in prepared['extra_helpers']:
        original({k: row[k] for k in ('path', 'bytes', 'sha256')})
        original(row['source_original'])
        extra[Path(row['path']).name] = row['source_commit']
    require(extra['verify-native-d3d8-system-phase.py'] == RUNNER
            and extra['verify-native-d3d8-system-device.py'] == OLD_RUNNER, 'Separate extra-helper Git origins required')
    stage_final_path = stage / 'current-core-staging-final-review-descriptor-01.json'
    stage_final = read(stage_final_path)
    require(stage_final['prepared'] is True and stage_final['authorized'] is False
            and stage_final['source_commit'] == identity['source'] and stage_final['ci_run'] == identity['run']
            and stage_final['current_core_reference'] == pin(reference)
            and stage_final['prepared_phase_manifest'] == pin(manifest_path)
            and stage_final['historical_core_reused'] is False and stage_final['target_calls'] == 0,
            'Fresh stage packet must bind the exact actual phase/core reference')
    for key in ('inputs_descriptor', 'prepared_host_inputs', 'host', 'outer', 'reader', 'native_prefix', 'native_collector'):
        original(stage_final[key])
    stage_descriptor = read(original(stage_final['inputs_descriptor']))
    require(stage_descriptor['candidate_root'] == prepared['candidate_root']
            and stage_descriptor['current_core_reference'] == pin(reference)
            and stage_descriptor['prepared_phase_manifest'] == pin(manifest_path)
            and stage_descriptor['core_source'] == identity['source']
            and stage_descriptor['ci_run'] == identity['run'], 'Phase/stage exact input tuple differs')
    stage_prepared = read(original(stage_final['prepared_host_inputs']))
    stage_cores = [row for row in stage_prepared['distinct_payload_originals'] if row['path'] == 'viogpudxvk.dll']
    phase_core = next(row for row in manifest['files'] if row['role'] == 'core')
    require(len(stage_cores) == 1 and stage_cores[0]['bytes'] == phase_core['bytes']
            and stage_cores[0]['sha256'] == phase_core['sha256']
            and stage_cores[0]['source_commit'] == identity['source'], 'Prepared stage selected another core')
    # The actual frozen stage outer determines its future output directory.
    stage_outer = original(stage_final['outer']).read_text()
    tree = ast.parse(stage_outer)
    assignment = next(n.value for n in tree.body if isinstance(n, ast.Assign)
        and any(isinstance(t, ast.Name) and t.id == 'OUTPUT' for t in n.targets))
    require(isinstance(assignment, ast.BinOp) and isinstance(assignment.op, ast.Div)
            and isinstance(assignment.left, ast.Attribute) and assignment.left.attr == 'parent'
            and isinstance(assignment.left.value, ast.Name) and assignment.left.value.id == 'PACKET'
            and isinstance(assignment.right, ast.Constant) and isinstance(assignment.right.value, str)
            and assignment.right.value == 'guest-original-plus-derived-I386-candidate-staging-'
                + identity['source'][:7] + '-icd02', 'Unexpected stage outer output binding')
    stage_output = stage.parent / assignment.right.value
    anchors = [prepared_path, reference, stage_final_path, original(stage_final['inputs_descriptor'])]
    for key in ('archive', 'proof', 'reader', 'ROOT_admission', 'outer'):
        anchors.append(original(manifest['native_lifetime'][key]))
    anchors += [original(manifest['native_setup']['ROOT_admission'])]
    # This immutable base CPU release is already part of the admitted phase.
    anchors.append(ARTIFACTS / 'guest-native-enumeration-completion-e3ac126-10/root-native-CPU10-originals-and-release-admitted-01.json')
    return prepared, manifest, identity, extra, stage_output, anchors


def materialize(phase, stage, out, templates):
    prepared, manifest, identity, extra, stage_output, anchors = validate_inputs(phase, stage)
    require(out.is_absolute() and out.is_relative_to(WORKSPACE / 'artifacts') and not out.exists(),
            'Fresh categorized artifact output required')
    old_parser = old_packet()
    old_descriptor = json.loads(templates[str(old_parser / 'phase-parser-inputs-01.json')])
    bindings = Bindings(out, identity['source'], identity['run'],
        pin(phase / 'helpers/phase-inputs-original.json')['sha256'], 'pending-local-parser-descriptor',
        stage, stage_output, extra)
    require(not bindings.parser_output.exists() and all(not p.exists() for p in bindings.outputs.values()),
            'Every target result output must be fresh')
    out.mkdir()
    bindings.parser.mkdir()
    shutil.copytree(phase / 'helpers', bindings.parser / 'inputs')
    parser = bindings.parser
    host = parser / new_host_name(bindings.run_id)
    for name in ('run-phase-native-parser-prefix-01.ps1', 'collect-phase-native-parser-prefix-01.ps1', old_host_name()):
        before = templates[str(old_parser / name)].decode()
        after = bindings.change(before)
        if name == 'run-phase-native-parser-prefix-01.ps1':
            after = after.replace(old_descriptor['manifest_sha256'], bindings.manifest_sha)
        save_script(parser / name.replace(OLD_RUN_ID, bindings.run_id), before, after)
    descriptor = bindings.data(old_descriptor)
    descriptor.update(ready=False, runner_source=RUNNER, manifest_sha256=bindings.manifest_sha,
        prefix_sha256=pin(parser / 'run-phase-native-parser-prefix-01.ps1')['sha256'],
        collector_sha256=pin(parser / 'collect-phase-native-parser-prefix-01.ps1')['sha256'],
        inputs=[dict(name=p.name, bytes=p.stat().st_size, sha256=pin(p)['sha256']) for p in sorted((parser / 'inputs').iterdir())],
        original_proofs=[pin(p) for p in anchors], current_core_reference=manifest['current_core']['ROOT_reference'],
        current_core_identity=identity, old_staging_or_HAL_acceptance_inherited=False)
    descriptor_path = parser / 'phase-parser-inputs-01.json'
    write(descriptor_path, descriptor)
    bindings.parser_sha = pin(descriptor_path)['sha256']
    parser_prepared = bindings.data(json.loads(templates[str(old_parser / 'prepared-phase-parser-01.json')]))
    parser_prepared.update(local_inputs=[dict(pin(p), transfer=True) for p in sorted((parser / 'inputs').iterdir())]
        + [dict(pin(parser / n), transfer=True) for n in ('run-phase-native-parser-prefix-01.ps1', 'collect-phase-native-parser-prefix-01.ps1', 'phase-parser-inputs-01.json')]
        + [dict(pin(p), transfer=False) for p in anchors], orchestrator_sha256=pin(host)['sha256'],
        recommended_output=str(bindings.parser_output), current_source_commit=RUNNER,
        new_core_source=identity['source'], hardware_admission=False)
    write(parser / 'prepared-phase-parser-01.json', parser_prepared)
    before = templates[str(old_parser / 'execute-owned-current-parser-01.py')].decode()
    after = bindings.change(before)
    after = once(after, checked_host_hash(after), pin(host)['sha256'])
    save_script(parser / 'execute-owned-current-parser-01.py', before, after)
    before = templates[str(old_parser / 'review-native-parser-frontend-lifetime-originals-01.py')].decode()
    save_script(parser / 'review-native-parser-current-core-originals-01.py', before,
                bindings.parser_reader(before, old_descriptor['manifest_sha256']))
    auth = bindings.data(json.loads(templates[str(old_parser / 'ownership-authorization.template.json')]))
    auth.update(authorized=False, descriptor_sha256=bindings.parser_sha, orchestrator_sha256=pin(host)['sha256'],
                latest_accepted_release='pending actual latest ROOT-reviewed explicit release')
    write(parser / 'ownership-authorization.template.json', auth)
    local_results = [run_default(host)]
    require(local_results[0][1]['expected_owned_native_children'] == 2, 'Unchanged AST-only scope required')

    for phase_name in PHASES:
        old = old_packet(phase_name)
        packet = bindings.packets[phase_name]
        packet.mkdir()
        shutil.copytree(parser / 'inputs', packet / 'inputs')
        descriptor_name = f'i386-{phase_name}-inputs-01.json'
        d = bindings.data(json.loads(templates[str(old / descriptor_name)]))
        d.update(ready=False, runner_source=RUNNER, core_source=identity['source'], core_CI=identity['run'],
            original_manifest_sha256=bindings.manifest_sha,
            original11_text_inputs=[dict(name=p.name, bytes=p.stat().st_size, sha256=pin(p)['sha256']) for p in sorted((packet / 'inputs').iterdir())],
            actual_CPU09_completion_originals=[pin(p) for p in anchors], actual_future_probe=manifest['files'][0],
            required_parser_run_id=bindings.run_id, actual_new_native_parser_accepted=False,
            old_phase_or_hardware_acceptance_inherited=False, required_pending_after_root_admission=PENDING,
            current_core_reference=manifest['current_core']['ROOT_reference'])
        d.pop('actual_admitted_current_manifest', None)
        parser_admission = d['required_native_parser_admission']
        parser_admission['source'] = 'Fresh current ad60 native AST5 originals and ROOT direct review; old parse acceptance is not inherited'
        parser_admission['prepared_parser_descriptor'] = pin(descriptor_path)
        parser_admission['required_fields'].update(prepared_descriptor_sha256=bindings.parser_sha, run_id=bindings.run_id)
        if phase_name != 'names':
            d.update(original_four_payload_stage_descriptor=pin(stage / 'staging-inputs-owned-icd-02.json'),
                required_fresh_staging_original_path=str(bindings.stage_proof),
                required_fresh_staging_ROOT_path=str(bindings.stage_root),
                actual_five_payload_stage_ROOT='pending fresh actual staging ROOT review',
                original_private_payload_stage=copy.deepcopy(manifest['original_private_payload_stage']),
                fresh_five_payload_staging_original_proof_required=True)
        write(packet / descriptor_name, d)
        before = templates[str(old / old_host_name(phase_name))].decode()
        phase_host = packet / new_host_name(bindings.run_id, phase_name)
        save_script(phase_host, before, bindings.change(before))
        pp = bindings.data(json.loads(templates[str(old / f'prepared-i386-{phase_name}-01.json')]))
        pp.update(ready=False, runner_source=RUNNER, core_source=identity['source'], core_ci_run=identity['run'],
            local_inputs=[dict(pin(p), transfer=p.name != 'phase-inputs-original.json') for p in sorted((packet / 'inputs').iterdir())]
                + [dict(pin(p), transfer=False) for p in [packet / descriptor_name, descriptor_path, *anchors]],
            orchestrator_sha256=pin(phase_host)['sha256'], recommended_output=str(bindings.outputs[phase_name]),
            actual_new_native_parser_accepted=False, required_parser_run_id=bindings.run_id,
            five_hardware_payloads_reused_immutable=False, fresh_current_five_payload_stage_required=phase_name != 'names')
        write(packet / f'prepared-i386-{phase_name}-01.json', pp)
        a = bindings.data(json.loads(templates[str(old / f'{phase_name}-authorization.template.json')]))
        a.update(authorized=False, descriptor_sha256=pin(packet / descriptor_name)['sha256'],
            orchestrator_sha256=pin(phase_host)['sha256'], manifest_sha256='pending actual fresh ROOT ready manifest',
            parser_root_review_sha256='pending actual current ad60 AST5 ROOT review',
            latest_accepted_release='pending actual latest ROOT-reviewed explicit release')
        if phase_name != 'names':
            a['staging_proof_sha256'] = 'pending actual fresh current-core five-payload staging proof'
        write(packet / f'{phase_name}-authorization.template.json', a)
        before = templates[str(old / 'execute-owned-current-phase-01.py')].decode()
        after = bindings.change(before)
        after = once(after, checked_host_hash(after), pin(phase_host)['sha256'])
        save_script(packet / 'execute-owned-current-phase-01.py', before, after)
        before = templates[str(old / 'review-current-phase-originals-01.py')].decode()
        save_script(packet / 'review-current-phase-originals-01.py', before,
                    bindings.phase_reader(before, pin(old_parser / 'phase-parser-inputs-01.json')['sha256']))
        result, plan = run_default(phase_host)
        require(plan['expected_known_host_operations'] == 7, 'Unchanged actual USER transport boundary required')
        if phase_name in ('offscreen', 'present'):
            require(plan['offscreen_pixels_required'] == 448
                    and plan['screen_pixels_required'] == (64 if phase_name == 'present' else 0),
                    'Actual public pixel requirements must remain unchanged')
        local_results.append((result, plan))

    final = dict(schema='system-d3d8-current-core-descendant-handoffs-v1', prepared=True, ready=False,
        source_commit=identity['source'], ci_run=identity['run'], current_core_reference=manifest['current_core']['ROOT_reference'],
        input_phase_packet=pin(phase / 'prepared-current-core-phase-source-01.json'),
        input_stage_packet=pin(stage / 'current-core-staging-final-review-descriptor-01.json'),
        native_probe_source=PROBE, native_setup_source=SETUP, native_cpu_source=CPU, runner_source=RUNNER,
        parser_descriptor=pin(descriptor_path), parser_host=pin(host), parser_output=str(bindings.parser_output),
        admitted_manifest_future_path=str(bindings.admitted_manifest), stage_proof_future_path=str(bindings.stage_proof),
        stage_ROOT_future_path=str(bindings.stage_root), packets={p: str(bindings.packets[p]) for p in PHASES},
        outputs={p: str(bindings.outputs[p]) for p in PHASES}, HAL_ROOT_future_path=str(bindings.hal_root),
        offscreen_ROOT_future_path=str(bindings.offscreen_root),
        local_default_plan_originals=[r for r, _ in local_results],
        generated_source_files=[pin(p) for p in sorted(out.rglob('*')) if p.is_file() and p.suffix in ('.py', '.ps1', '.json', '.diff')],
        actual_target_calls=0, fresh_native_AST5_accepted=False, fresh_names_HAL_offscreen_Present_accepted=False,
        hardware_acceptance_inherited=False, old_core_or_HAL_reused=False,
        next_steps=['ROOT review of new descendant source and binding diffs',
            'fresh native AST5/Add-Type originals and direct ROOT parser review',
            'fresh five-payload stage originals and direct ROOT stage review',
            'fresh limited USER names originals', 'fresh held-frontend system HAL originals',
            'fresh public system CreateDevice/offscreen448 originals', 'fresh Present64 desktop originals'])
    final_path = out / 'prepared-current-core-descendant-handoffs-01.json'
    write(final_path, final)
    return pin(final_path)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--phase-packet', type=Path)
    parser.add_argument('--staging-packet', type=Path)
    parser.add_argument('--out', type=Path)
    args = parser.parse_args()
    templates = frozen_templates()
    previews = preview_bindings(templates)
    if not any((args.phase_packet, args.staging_packet, args.out)):
        print(json.dumps(dict(prepared_generator=True, ready=False,
            requires='actual admitted current-core phase and fresh five-payload stage packets',
            frozen_templates=len(templates), binding_source_previews=len(previews),
            core_bytes_or_hash_guessed=False, output_packets_written=0, actual_target_calls=0,
            native_AST_or_runtime_acceptance=False, generator=pin(Path(__file__)), template_lock=pin(LOCK)), indent=2))
        return
    require(all((args.phase_packet, args.staging_packet, args.out)), 'Provide all three phase/staging/out arguments')
    print(json.dumps(materialize(args.phase_packet.resolve(), args.staging_packet.resolve(), args.out.resolve(), templates), indent=2))


if __name__ == '__main__':
    main()
