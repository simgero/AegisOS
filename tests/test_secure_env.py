"""Validate helper archive boundaries before any guest code is executed."""
import hashlib
import importlib.util
import io
from pathlib import Path
import shutil
import struct
import subprocess
import tarfile
import tempfile
import unittest

spec=importlib.util.spec_from_file_location('helper',Path(__file__).resolve().parents[1]/'scripts/qemu-with-secure-env.py')
helper=importlib.util.module_from_spec(spec)
spec.loader.exec_module(helper)


class HelperArchiveTests(unittest.TestCase):
    def fixture(self,folder,extra=None):
        root=Path(folder);images=root/'images';images.mkdir()
        header=bytearray(4096)
        header[:8]=b'VNDRBOOT'
        struct.pack_into('<II',header,8,3,4096)
        struct.pack_into('<I',header,24,4)
        struct.pack_into('<I',header,2096,2112)
        (images/'vendor_boot.img').write_bytes(header+b'BASE')
        archive=root/'secure-env-arm64.tar.gz'
        with tarfile.open(archive,'w:gz') as tar:
            for name in ['init','host/bin/secure_env','lib/ld-musl-aarch64.so.1']:
                item=tarfile.TarInfo(name);item.size=3;item.mode=0o755
                tar.addfile(item,io.BytesIO(b'ELF'))
            if extra:tar.addfile(extra,io.BytesIO(b''))
        (root/'SHA256SUMS').write_text(hashlib.sha256(archive.read_bytes()).hexdigest()+'  '+archive.name+'\n')
        return root,images

    def test_rejects_traversal_even_with_matching_checksum(self):
        with tempfile.TemporaryDirectory() as d:
            root,images=self.fixture(d,tarfile.TarInfo('../escape'))
            with self.assertRaisesRegex(ValueError,'Unsafe'):helper.helper_initrd(root,images)

    def test_rejects_links(self):
        with tempfile.TemporaryDirectory() as d:
            item=tarfile.TarInfo('link');item.type=tarfile.SYMTYPE;item.linkname='/etc/passwd'
            root,images=self.fixture(d,item)
            with self.assertRaisesRegex(ValueError,'unsupported'):helper.helper_initrd(root,images)

    def test_rejects_corruption(self):
        with tempfile.TemporaryDirectory() as d:
            root,images=self.fixture(d)
            with (root/'secure-env-arm64.tar.gz').open('ab') as f:f.write(b'corrupt')
            with self.assertRaisesRegex(ValueError,'checksum'):helper.helper_initrd(root,images)

    @unittest.skipUnless(shutil.which('cpio'),'Independent cpio reader not installed')
    def test_generated_archive_is_readable_by_cpio(self):
        with tempfile.TemporaryDirectory() as d:
            root,images=self.fixture(d)
            data=helper.helper_initrd(root,images)
            self.assertEqual(data[:4],b'BASE')
            result=subprocess.run(['cpio','-it'],input=data[4:],capture_output=True,check=True)
            self.assertIn(b'host/bin/secure_env',result.stdout)
            self.assertIn(b'state/instances/cvd-1/logs',result.stdout)

    @unittest.skipUnless(shutil.which('lz4') and shutil.which('cpio'), 'LZ4 and cpio required')
    def test_legacy_lz4_vendor_stream_does_not_consume_overlay(self):
        with tempfile.TemporaryDirectory() as d:
            root,images=self.fixture(d)
            compressed=subprocess.run(['lz4','-l','-c'],input=b'BASE',
                                      capture_output=True,check=True).stdout
            header=bytearray((images/'vendor_boot.img').read_bytes()[:4096])
            struct.pack_into('<I',header,24,len(compressed))
            (images/'vendor_boot.img').write_bytes(header+compressed)
            data=helper.helper_initrd(root,images)
            self.assertEqual(data[:4],b'BASE')
            result=subprocess.run(['cpio','-it'],input=data[4:],capture_output=True,check=True)
            self.assertIn(b'host/bin/secure_env',result.stdout)


if __name__=='__main__':unittest.main()
