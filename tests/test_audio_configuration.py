"""Inert XML regressions for the two observed QEMU audio boot blockers."""
import importlib.util
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('check_audio', ROOT/'scripts/aosp/check-audio.py')
audio = importlib.util.module_from_spec(spec)
spec.loader.exec_module(audio)


class AudioConfigurationTests(unittest.TestCase):
    def setUp(self):
        temp = tempfile.TemporaryDirectory()
        self.addCleanup(temp.cleanup)
        self.root = Path(temp.name)
        self.policy = self.root/'audio.xml'
        self.manifest = self.root/'manifest.xml'
        self.policy.write_text('''<audioPolicyConfiguration xmlns:xi="http://www.w3.org/2001/XInclude">
          <modules><module name="primary"/><module name="r_submix"/>
          <xi:include href="bluetooth.xml"/></modules></audioPolicyConfiguration>''')
        self.bluetooth = self.root/'bluetooth.xml'
        self.bluetooth.write_text('<module name="bluetooth"/>')
        self.manifest.write_text('''<manifest><hal format="aidl">
          <name>android.hardware.audio.core</name><version>3</version>
          <fqname>IModule/default</fqname><fqname>IModule/r_submix</fqname>
          <fqname>IModule/bluetooth</fqname><fqname>IConfig/default</fqname>
          </hal></manifest>''')

    def test_complete_installed_policy_matches_audio_apex(self):
        audio.check(self.policy, self.manifest)

    def test_missing_installed_bluetooth_include_is_rejected(self):
        self.bluetooth.unlink()
        with self.assertRaises(subprocess.CalledProcessError):
            audio.check(self.policy, self.manifest)

    def test_valid_xml_without_declared_bluetooth_module_is_rejected(self):
        self.policy.write_text(self.policy.read_text().replace('<xi:include href="bluetooth.xml"/>', ''))
        with self.assertRaisesRegex(ValueError, 'module mismatch'):
            audio.check(self.policy, self.manifest)

    def test_new_unconfigured_apex_module_is_rejected(self):
        self.manifest.write_text(self.manifest.read_text().replace('</hal>', '<fqname>IModule/usb</fqname></hal>'))
        with self.assertRaisesRegex(ValueError, 'module mismatch'):
            audio.check(self.policy, self.manifest)


if __name__ == '__main__':
    unittest.main()
