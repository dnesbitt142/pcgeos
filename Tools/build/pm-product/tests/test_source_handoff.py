#!/usr/bin/env python3
# PM-BRINGUP [PM-TESTS] ADDED: handoff/configuration checks, not guest execution.
"""Run with unittest discovery as documented in PM-BRINGUP.md (Python 3.10+)."""
from __future__ import annotations
import ast
import hashlib
import importlib.util
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

sys.dont_write_bytecode = True
PACKAGE = Path(__file__).resolve().parents[1]
ROOT = PACKAGE.parents[2]
SCRIPTS = PACKAGE / 'scripts'
META = json.loads((PACKAGE / 'source-manifest.json').read_text())


def load(name: str, path: Path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module
    spec.loader.exec_module(module)
    return module


checker = load('pm_source_checker', SCRIPTS / 'check-source.py')
profile = load('pm_product_profile', SCRIPTS / 'product_config.py')
logcheck = load('pm_log_check', SCRIPTS / 'check-build-log.py')
geode = load('pm_geode_check', SCRIPTS / 'geode.py')


class SourceHandoffTests(unittest.TestCase):
    def test_01_exact_pinned_baseline(self):
        self.assertEqual(META['base_commit'], '30df506fc64720fd82e528acbcb192adbaa48fce')

    def test_02_seventeen_distinct_project_sources(self):
        paths = [e['path'] for e in META['runtime_sources']]
        self.assertEqual(len(paths), 17)
        self.assertEqual(len(set(paths)), 17)

    def test_03_fifteen_modified_and_two_new(self):
        self.assertEqual(sum(e['status'] == 'modified' for e in META['runtime_sources']), 15)
        self.assertEqual({e['path'] for e in META['runtime_sources'] if e['status'] == 'added'},
                         {'Loader32/fault.asm', 'Driver/Video/VGAlike/VGA16/vga16PMPerf.asm'})

    def test_04_current_hashes_and_comment_only_relationship(self):
        self.assertEqual(checker.check(ROOT, META, 'current'), [])

    def test_05_each_project_source_has_explanatory_marker(self):
        for entry in META['runtime_sources']:
            text = (ROOT / entry['path']).read_bytes().decode('latin1')
            self.assertIn('PM-BRINGUP [' + entry['change_id'] + ']', text)
            self.assertIn('Base archive commit: ' + META['base_commit'], text)

    def test_06_declared_line_endings_are_preserved(self):
        for entry in META['runtime_sources']:
            data = (ROOT / entry['path']).read_bytes()
            if entry['line_endings'] == 'CRLF':
                self.assertEqual(data.count(b'\n'), data.count(b'\r\n'), entry['path'])
            else:
                self.assertNotIn(b'\r\n', data, entry['path'])

    def fixture(self, root: Path):
        entry = next(e for e in META['runtime_sources'] if e['status'] == 'modified').copy()
        path = root / entry['path']
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes((ROOT / entry['path']).read_bytes())
        return {'runtime_sources': [entry]}, path

    def test_07_changed_source_is_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp); meta, path = self.fixture(root)
            path.write_bytes(path.read_bytes() + b'UNEXPECTED\n')
            self.assertTrue(checker.check(root, meta, 'current'))

    def test_08_missing_source_is_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp); meta, path = self.fixture(root); path.unlink()
            self.assertEqual(checker.check(root, meta, 'current')[0]['reason'], 'missing source file')

    def test_09_new_path_collision_is_rejected(self):
        entry = next(e for e in META['runtime_sources'] if e['status'] == 'added')
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp); path = root / entry['path']; path.parent.mkdir(parents=True)
            path.write_text('local work\n')
            self.assertTrue(checker.check(root, {'runtime_sources': [entry]}, 'baseline'))

    def test_10_symlinks_are_not_followed(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp); meta, path = self.fixture(root)
            saved = path.with_suffix('.saved'); path.rename(saved); path.symlink_to(saved)
            self.assertEqual(checker.check(root, meta, 'current')[0]['reason'], 'symlink in source path')

    def test_11_checker_never_writes_sources(self):
        before = {e['path']: (ROOT / e['path']).read_bytes() for e in META['runtime_sources']}
        checker.check(ROOT, META, 'current')
        self.assertTrue(all((ROOT / path).read_bytes() == data for path, data in before.items()))

    def test_12_c_comment_filter_does_not_hide_executable_text(self):
        text = b'/* PM-BRINGUP note */ run();\n'
        self.assertEqual(checker.without_annotations(text, '.c'), text)

    def test_13_true_type_resolves_before_outline_access(self):
        text = checker.without_annotations((ROOT / 'Driver/Font/TrueType/Adapter/ttinit.c').read_bytes(), '.c').decode()
        start = text.index('FontsAvailEntry*      availEntries = LMemDeref(')
        block = text[start:]
        self.assertLess(block.index('fontInfo = LMemDerefHandles('), block.index('outlineData = (OutlineDataEntry*)'))
        self.assertLess(block.index('fontInfo = LMemDerefHandles('), block.index('outlineDataEnd = (OutlineDataEntry*)'))

    def test_14_sampling_guard_wraps_unsupported_calls(self):
        text = (ROOT / 'Appl/Perf/calc.asm').read_text()
        block = text.split('ifndef PM_PERF_MINIMAL', 1)[1].split('endif ; not PM_PERF_MINIMAL', 1)[0]
        for name in ('CalcMemoryStatistics', 'PerfCalcPPPStatistics', 'PerfCalcFreeHandles'):
            self.assertIn(name, block)
        self.assertNotIn('call    PerfCalcCPUUsage', block)

    def test_15_pmdiag2_not_unimplemented_pmdiag3(self):
        text = (ROOT / 'Loader32/strings.asm').read_text()
        self.assertIn('"PMDIAG2 error=0x$"', text)
        self.assertNotIn('"PMDIAG3', text)
        self.assertIn('include fault.asm', text)

    def test_16_full_product_spooler_and_perf_startup(self):
        text = profile.product_profile('[ui]\nnoSpooler = true\n')
        self.assertIn('noSpooler = false', text)
        self.assertIn('Desk Accessories\\Perf', text)
        self.assertIn('truetype.geo', text)

    def test_17_profile_keeps_vga_resolution_and_keyboard_mouse(self):
        text = profile.product_profile('')
        self.assertIn('VESA Compatible SuperVGA: 640x480 64K-color', text)
        self.assertIn('kbmouse.geo', text)

    def test_18_ini_editor_preserves_unrelated_content(self):
        text = '[custom]\nsetting = preserve-me\n[ui]\nnoSpooler = true\n'
        changed = profile.product_profile(text)
        self.assertIn('[custom]\nsetting = preserve-me\n', changed)

    def test_19_ini_editor_replaces_whole_braced_value(self):
        text = '[ui]\nexecOnStartup = {\nOldApp\n}\nother = keep\n'
        changed = profile.set_value(text, 'ui', 'execOnStartup', '{\nPerf\n}')
        self.assertNotIn('OldApp', changed)
        self.assertIn('other = keep', changed)
        self.assertIn('{\nPerf\n}', changed)

    def test_20_ini_editor_rejects_duplicate_sections(self):
        with self.assertRaises(ValueError):
            profile.set_value('[ui]\na=1\n[ui]\nb=2\n', 'ui', 'a', '3')

    def test_21_ini_editor_rejects_unclosed_braces(self):
        with self.assertRaises(ValueError):
            profile.set_value('[ui]\na={\nopen\n[next]\nb=1\n', 'ui', 'a', 'x')

    def test_22_manifest_has_eighteen_documented_exclusions(self):
        data = json.loads((PACKAGE / 'source/product-exclusions.json').read_text())
        self.assertEqual(len(data), 18)
        self.assertEqual(len({d['source'].casefold() for d in data}), 18)
        self.assertTrue(all(d['reason'] for d in data))

    def test_23_lowercase_gdi_variant_in_product_manifest(self):
        text = (ROOT / 'Tools/build/product/bbxensem/bbxensem.filetree').read_text()
        self.assertIn('Library/GDI/GenPC/win32{dbcs}/gdi{ec}.geo', text)
        self.assertNotIn('Library/GDI/GenPC/WIN32{dbcs}/gdi{ec}.geo', text)

    def test_24_source_build_wrapper_does_not_reapply_old_patches(self):
        text = (SCRIPTS / 'build.sh').read_text()
        self.assertNotIn('scripts/apply-changes.py', text)
        self.assertIn('PM_PERF_MINIMAL=1', text)
        self.assertIn('rm -f loader.obj', text)
        self.assertIn('/TrueType/ttinit.obj', text)

    def test_25_detects_legacy_make_error_even_without_exit_failure(self):
        text = "pmake: Can't figure out how to make math.ldf. Stop\n"
        self.assertEqual(logcheck.failures(text), [text.strip()])

    def test_26_compiler_warnings_are_not_reported_as_fatal(self):
        self.assertEqual(logcheck.failures('Warning! W303: unused parameter\n'), [])

    def test_27_all_new_python_helpers_parse(self):
        for path in PACKAGE.rglob('*.py'):
            ast.parse(path.read_text(), filename=str(path))

    def test_28_shell_wrapper_syntax(self):
        subprocess.run(['bash', '-n', str(SCRIPTS / 'build.sh')], check=True)

    def test_29_native_fixture_sources_compile_but_are_not_executed(self):
        if not shutil.which('gcc'):
            self.fail('GCC is required to compile the historical native fixture sources')
        with tempfile.TemporaryDirectory() as temp:
            for index, path in enumerate(sorted((PACKAGE / 'tests/native').rglob('*.c'))):
                subprocess.run(['gcc', '-O2', '-Wall', '-Wextra', '-Werror', '-o',
                                str(Path(temp) / f'fixture-{index}'), str(path)], check=True,
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE)

    def test_30_source_handoff_explicitly_does_not_claim_pending_features(self):
        text = (ROOT / 'TechDocs/Markdown/pm-bringup/STATUS.md').read_text()
        self.assertIn('QR-encoded crash record', text)
        self.assertIn('Design discussion only; no implementation.', text)
        self.assertIn('All measurements from original Perf', text)
        self.assertIn('Open; no fix included.', text)

    def test_31_import_check_requires_compatible_protocol(self):
        consumer = {'name': 'app', 'imports': [{'name': 'geos', 'major': 1, 'minor': 2, 'kind': 0}]}
        provider = {'name': 'geos', 'major': 1, 'minor': 2, 'attributes': 0, 'imports': []}
        self.assertEqual(geode.import_issues([consumer, provider]), [])
        provider['minor'] = 1
        self.assertEqual(len(geode.import_issues([consumer, provider])), 1)

    def test_32_compiler_pin_is_the_supplied_archive(self):
        data = json.loads((PACKAGE / 'reports/inputs.json').read_text())
        self.assertEqual(data['openwatcom']['sha256'], '25ce668825a6e145e18d4347675912b98cb0f202d100bdc1a436f6fe69515f40')
        self.assertEqual(len(data['compiler_binaries']), 6)


if __name__ == '__main__':
    unittest.main()
