import importlib.util
from pathlib import Path
import subprocess
import tempfile
import unittest
import xml.etree.ElementTree as ET


ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('kernel_sources', ROOT / 'scripts/kernel/source_manifest.py')
sources = importlib.util.module_from_spec(spec)
spec.loader.exec_module(sources)


class SourcePinsTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.manifest = self.root / 'manifest.xml'
        self.tree = ET.parse(ROOT / 'kernel/manifest.xml')
        self.tree.write(self.manifest)

    def save(self):
        self.tree.write(self.manifest)

    def test_pins_include_matching_base_modules_and_tools(self):
        pins = sources.projects(self.manifest)
        self.assertEqual(len(pins), 40)
        self.assertEqual(pins['common'], '50eb8d5d443b43f38d6e72f005f1b8601ac88a05')
        self.assertEqual(pins['common-modules/virtual-device'], 'b972a07579955c4a6757e46899040922f60ef06c')
        self.assertEqual(pins['build/kernel'], 'a02488f06e024e940862a398ad7c38ea1279014e')

    def test_rejects_floating_revision(self):
        self.tree.find('project').set('revision', 'main')
        self.save()
        with self.assertRaisesRegex(ValueError, 'unpinned'):
            sources.projects(self.manifest)

    def test_rejects_escaping_project(self):
        self.tree.find('project').set('path', '../unrelated')
        self.save()
        with self.assertRaisesRegex(ValueError, 'Unsafe'):
            sources.projects(self.manifest)

    def test_rejects_silent_override(self):
        ET.SubElement(self.tree.getroot(), 'extend-project', name='kernel/common', revision='main')
        self.save()
        with self.assertRaisesRegex(ValueError, 'directive'):
            sources.projects(self.manifest)

    def test_rejects_different_upstream(self):
        self.tree.find('remote').set('fetch', 'https://example.invalid/')
        self.save()
        with self.assertRaisesRegex(ValueError, 'upstream'):
            sources.projects(self.manifest)

    def test_actual_checkouts_reject_dirty_or_wrong_commit(self):
        manifest = ET.Element('manifest')
        ET.SubElement(manifest, 'remote', name='aosp', fetch='https://android.googlesource.com/')
        ET.SubElement(manifest, 'default', remote='aosp')
        workspace = self.root / 'workspace'
        for name in ('common', 'common-modules/virtual-device', 'build/kernel'):
            path = workspace / name
            path.mkdir(parents=True)
            subprocess.run(['git', 'init', '-q', '--template=', str(path)], check=True)
            (path / 'source').write_text('original\n')
            subprocess.run(['git', '-C', str(path), 'add', 'source'], check=True)
            subprocess.run(['git', '-C', str(path), '-c', 'user.name=Test',
                            '-c', 'user.email=test@example.invalid', '-c', 'commit.gpgsign=false',
                            'commit', '-qm', 'fixture'], check=True)
            revision = subprocess.check_output(['git', '-C', str(path), 'rev-parse', 'HEAD'], text=True).strip()
            ET.SubElement(manifest, 'project', name='kernel/' + name, path=name, revision=revision)
        ET.ElementTree(manifest).write(self.manifest)
        sources.check_tree(self.manifest, workspace)
        (workspace / 'common/source').write_text('changed\n')
        with self.assertRaisesRegex(ValueError, 'local changes'):
            sources.check_tree(self.manifest, workspace)
        (workspace / 'common/source').write_text('original\n')
        manifest.find('project').set('revision', '0' * 40)
        ET.ElementTree(manifest).write(self.manifest)
        with self.assertRaisesRegex(ValueError, 'revision differs'):
            sources.check_tree(self.manifest, workspace)


if __name__ == '__main__':
    unittest.main()
