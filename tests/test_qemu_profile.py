"""Exercise real qcow2/ext4 identity and persistence, without starting a guest."""
import importlib.util
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

spec=importlib.util.spec_from_file_location('profile',Path(__file__).resolve().parents[1]/'scripts/qemu_profile.py')
profile=importlib.util.module_from_spec(spec);spec.loader.exec_module(profile)
MKFS=shutil.which('mkfs.ext4') or '/opt/homebrew/opt/e2fsprogs/sbin/mkfs.ext4'


@unittest.skipUnless(shutil.which('qemu-img') and Path(MKFS).is_file(), 'qemu-img and mkfs.ext4 required')
class ProfileTests(unittest.TestCase):
    def setUp(self):
        self.folder=tempfile.TemporaryDirectory()
        self.addCleanup(self.folder.cleanup)
        self.root=Path(self.folder.name)
        self.images=self.root/'images';self.images.mkdir()
        for name in ['kernel','ramdisk.img','vendor_boot.img','vendor-bootconfig.img']:
            (self.images/name).write_bytes(name.encode())
        self.disk=self.root/'base.raw'
        with self.disk.open('wb') as stream:stream.truncate(1024**2)
        self.config=self.root/'config';self.config.write_text('test\n')
        self.assets=self.root/'assets';self.assets.mkdir()
        (self.assets/'secure-env-arm64.tar.gz').write_bytes(b'fixture')
        self.args=(self.images,self.disk,self.config,self.assets)
        self.first=self.root/'first'
        self.manifest=profile.create(self.first,*self.args,mkfs=MKFS)

    def test_identity_survives_overlay_writes_and_reopen(self):
        before=profile.digest(self.disk)
        overlay=self.first/'android.qcow2'
        subprocess.run(['qemu-io','-f','qcow2','-c','write -P 0x5a 0 4096',str(overlay)],
                       check=True,capture_output=True)
        profile.validate(self.first,*self.args)
        subprocess.run(['qemu-io','-f','qcow2','-c','read -P 0x5a 0 4096',str(overlay)],
                       check=True,capture_output=True)
        self.assertEqual(profile.digest(self.disk),before)
        self.assertEqual(profile.ext4_uuid(self.first/'secure-env.ext4'),self.manifest['profile_id'])

    def test_swapped_state_and_android_are_rejected(self):
        second=self.root/'second'
        profile.create(second,*self.args,mkfs=MKFS)
        for name,pattern in [('secure-env.ext4','Helper state'),('android.qcow2','Android overlay')]:
            with self.subTest(name=name):
                original=self.first/name
                backup=self.first/(name+'.saved');original.rename(backup)
                shutil.copyfile(second/name,original)
                with self.assertRaisesRegex(ValueError,pattern):profile.validate(self.first,*self.args)
                original.unlink();backup.rename(original)

    def test_missing_state_is_not_recreated(self):
        state=self.first/'secure-env.ext4';state.unlink()
        with self.assertRaisesRegex(ValueError,'Missing'):profile.validate(self.first,*self.args)
        with self.assertRaises(FileExistsError):profile.create(self.first,*self.args,mkfs=MKFS)
        self.assertFalse(state.exists())

    def test_changed_base_image_is_rejected(self):
        with self.disk.open('r+b') as stream:stream.write(b'changed')
        with self.assertRaisesRegex(ValueError,'inputs changed'):profile.validate(self.first,*self.args)

    def test_lock_prevents_concurrent_use_and_releases(self):
        with profile.locked(self.first):
            with self.assertRaisesRegex(RuntimeError,'already in use'):
                with profile.locked(self.first):self.fail('Lock was bypassed')
        with profile.locked(self.first):profile.validate(self.first,*self.args)


if __name__=='__main__':unittest.main()
