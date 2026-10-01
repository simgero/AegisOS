"""Source ownership/partial-install fixtures; no compilation or kernel-eviction claim."""
import importlib.util,json,tempfile,unittest
from pathlib import Path
from unittest.mock import patch
ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('vold_hooks',ROOT/'scripts/aosp/register-vold-eviction.py')
h=importlib.util.module_from_spec(spec);spec.loader.exec_module(h)

class VoldEvictionHooksTest(unittest.TestCase):
    def setUp(self):
        temp=tempfile.TemporaryDirectory();self.addCleanup(temp.cleanup)
        self.root=Path(temp.name);self.project=self.root/'project';self.aosp=self.root/'aosp'
        self.originals={
            'KeyUtil.cpp':b'#include "KeyUtil.h"\nbool evictKey(const std::string& mountpoint, const EncryptionPolicy& policy) {\n legacy();\n}\n',
            'KeyUtil.h':b'bool evictKey(const std::string& mountpoint, const android::fscrypt::EncryptionPolicy& policy);\n',
            'VoldNativeService.cpp':b'    return translateBool(fscrypt_lock_ce_storage(userId));\n    return translateBool(fscrypt_destroy_user_keys(userId));\n',
            'FsCrypt.cpp':b'''#include "KeyUtil.h"
std::map<userid_t, UserPolicies> s_ce_policies;
static bool evict_user_keys(std::map<userid_t, UserPolicies>& policy_map, userid_t user_id) {
    legacyEraseUnconditionally();
}
bool fscrypt_create_user_keys(userid_t user_id, bool ephemeral) {
}
bool fscrypt_unlock_ce_storage(userid_t user_id, const std::vector<uint8_t>& secret) {
}
bool fscrypt_prepare_user_storage(const std::string& volume_uuid, userid_t user_id, int flags) {
}
bool fscrypt_lock_ce_storage(userid_t user_id) {
    return evict_user_keys(s_ce_policies, user_id);
}
bool destroy() {
    success &= evict_user_keys(s_ce_policies, user_id);
    success &= evict_user_keys(s_de_policies, user_id);
}
'''}
        self.pins={'schema':1,'commit':h.COMMIT,'files':{n:h.digest(b) for n,b in self.originals.items()}}
        source=self.project/h.SOURCE;source.parent.mkdir(parents=True);source.write_bytes(b'inert header fixture\n')
        for n,b in self.originals.items():
            p=self.aosp/'system/vold'/n;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(b)
    def prepare(self):return h.prepare(self.project,self.aosp,self.originals,self.pins)
    def target(self,n):return self.aosp/'system/vold'/n
    def test_installation_is_verified_and_repeatable_with_original_backups(self):
        r=self.prepare();h.verify(self.aosp,r);self.assertEqual(r,self.prepare())
        backups=list((self.aosp/'out/aegis-vold-eviction/backups').iterdir());self.assertEqual(len(backups),1)
        for n,b in self.originals.items():self.assertEqual((backups[0]/n).read_bytes(),b)
        text=self.target('FsCrypt.cpp').read_text()
        self.assertIn('EvictUser(policy_map, user_id',text)
        self.assertNotIn('legacyEraseUnconditionally',text)
        self.assertEqual(text.count('s_ce_locking.count(user_id)'),3)
        self.assertIn('if (!fscrypt_lock_ce_storage(user_id)) return false;',text)
        self.assertIn('if (complete) s_ce_locking.erase(user_id);',text)
        self.assertIn('FS_IOC_GET_ENCRYPTION_KEY_STATUS',self.target('KeyUtil.cpp').read_text())
    def test_unmanaged_change_blocks_every_write(self):
        self.target('VoldNativeService.cpp').write_bytes(b'local work')
        with self.assertRaises(ValueError):self.prepare()
        self.assertEqual(self.target('KeyUtil.cpp').read_bytes(),self.originals['KeyUtil.cpp'])
        self.assertFalse(self.target(h.HEADER).exists())
    def test_changed_pin_or_missing_anchor_is_rejected(self):
        self.pins['files']['KeyUtil.cpp']='0'*64
        with self.assertRaises(ValueError):self.prepare()
        self.pins['files']['KeyUtil.cpp']=h.digest(self.originals['KeyUtil.cpp'])
        self.originals['KeyUtil.cpp']=b'unexpected source';self.pins['files']['KeyUtil.cpp']=h.digest(self.originals['KeyUtil.cpp'])
        with self.assertRaises(ValueError):self.prepare()
        self.assertFalse((self.aosp/'out').exists())
    def test_interrupted_installation_accepts_only_exact_expected_partial_outputs(self):
        real=h.write_atomic;calls=0
        def stop(p,b):
            nonlocal calls
            real(p,b);calls+=1
            if calls==1:raise OSError('interrupted')
        with patch.object(h,'write_atomic',stop):
            with self.assertRaises(OSError):self.prepare()
        self.assertFalse((self.aosp/h.MARKER).exists())
        h.verify(self.aosp,self.prepare())
    def test_owned_update_retains_previous_header_and_detects_later_tampering(self):
        self.prepare();(self.project/h.SOURCE).write_bytes(b'updated fixture')
        r=self.prepare();h.verify(self.aosp,r)
        self.assertTrue(any(p.read_bytes()==b'inert header fixture\n' for p in (self.aosp/'out/aegis-vold-eviction/backups').glob('*/'+h.HEADER)))
        self.target(h.HEADER).write_bytes(b'tampered')
        with self.assertRaises(ValueError):h.verify(self.aosp,r)
        with self.assertRaises(ValueError):self.prepare()
    def test_symlink_destination_and_invalid_receipt_are_rejected(self):
        self.target(h.HEADER).symlink_to(self.project/h.SOURCE)
        with self.assertRaises(ValueError):self.prepare()
        self.target(h.HEADER).unlink();r=self.prepare();r['outputs']['KeyUtil.cpp']='bad'
        (self.aosp/h.MARKER).write_text(json.dumps(r))
        with self.assertRaises(ValueError):self.prepare()
