#!/usr/bin/env python3
"""Install pinned vold completion checks; source integration only, never a runtime test."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import subprocess
import tempfile

spec=importlib.util.spec_from_file_location('storage_integration',Path(__file__).with_name('register-runtime-storage.py'))
common=importlib.util.module_from_spec(spec);spec.loader.exec_module(common)
checked_path=common.checked_path;write_atomic=common.write_atomic;digest=common.digest
encoded=common.encoded;replace_once=common.replace_once
ORIGINALS={'KeyUtil.cpp','KeyUtil.h','FsCrypt.cpp','VoldNativeService.cpp'}
HEADER='AegisConfirmedEviction.h'
SOURCE='packages/aegis/identity/runtime/fscrypt_eviction.h'
MARKER='out/aegis-vold-eviction/sources.json'
COMMIT='11760521a2e86389c56620ec50e780e8b17e40ca'


def patch_key_util(data):
    text=data.decode()
    text=replace_once(text,'#include "KeyUtil.h"','#include "KeyUtil.h"\n#include "AegisConfirmedEviction.h"')
    anchor='bool evictKey(const std::string& mountpoint, const EncryptionPolicy& policy) {'
    addition='''// AEGIS: normal return requires the kernel to confirm that no decrypted inodes remain.
bool evictKeyConfirmed(const std::string& mountpoint, const EncryptionPolicy& policy) {
    const std::lock_guard<std::mutex> lock(fscrypt_keyring_mutex);
    android::base::unique_fd fd(open(mountpoint.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC));
    if (fd == -1) return false;
    fscrypt_key_specifier spec = {};
    if (!buildKeySpecifier(&spec, policy)) { errno = EINVAL; return false; }
    bool complete = aegis::fscrypt_eviction::Confirm([&](uint32_t* status) {
        fscrypt_get_key_status_arg arg = {};
        arg.key_spec = spec;
        if (ioctl(fd, FS_IOC_GET_ENCRYPTION_KEY_STATUS, &arg) != 0) return -1;
        *status = arg.status;
        return 0;
    }, [&](uint32_t* flags) {
        fscrypt_remove_key_arg arg = {};
        arg.key_spec = spec;
        if (ioctl(fd, FS_IOC_REMOVE_ENCRYPTION_KEY, &arg) != 0) return -1;
        *flags = arg.removal_status_flags;
        return 0;
    });
    if (complete) LOG(DEBUG) << "AEGIS fscrypt eviction confirmed: " << keyrefstring(policy.key_raw_ref);
    return complete;
}

'''
    return replace_once(text,anchor,addition+anchor).encode()


def patch_fscrypt(data):
    text=data.decode()
    text=replace_once(text,'#include "KeyUtil.h"','#include "KeyUtil.h"\n#include "AegisConfirmedEviction.h"')
    text=replace_once(text,'std::map<userid_t, UserPolicies> s_ce_policies;',
        'std::map<userid_t, UserPolicies> s_ce_policies;\n'
        '// Retained policies may be partially evicted; never report them as unlocked.\n'
        'std::set<userid_t> s_ce_locking;')
    begin=text.index('static bool evict_user_keys(')
    end=text.index('\n}\n',begin)+2
    text=text[:begin]+'''static bool evict_user_keys(std::map<userid_t, UserPolicies>& policy_map, userid_t user_id) {
    return aegis::fscrypt_eviction::EvictUser(policy_map, user_id,
            [](const std::string& volume, const EncryptionPolicy& policy) {
                return android::vold::evictKeyConfirmed(BuildDataPath(volume), policy);
            });
}'''+text[end:]
    text=replace_once(text,'    success &= evict_user_keys(s_ce_policies, user_id);\n    success &= evict_user_keys(s_de_policies, user_id);',
        '    // Do not destroy key files while any volume still has decrypted inodes.\n'
        '    if (!fscrypt_lock_ce_storage(user_id)) return false;\n'
        '    if (!evict_user_keys(s_de_policies, user_id)) return false;')
    text=replace_once(text,'    return evict_user_keys(s_ce_policies, user_id);',
        '    s_ce_locking.insert(user_id);\n'
        '    bool complete = evict_user_keys(s_ce_policies, user_id);\n'
        '    if (complete) s_ce_locking.erase(user_id);\n'
        '    return complete;')
    text=replace_once(text, '        user_ids.push_back(user_id);',
        '        // Pending policies are retained for eviction, not usable CE.\n'
        '        if (!s_ce_locking.count(user_id)) user_ids.push_back(user_id);')
    for signature in ('bool fscrypt_create_user_keys(userid_t user_id, bool ephemeral) {',
                      'bool fscrypt_unlock_ce_storage(userid_t user_id, const std::vector<uint8_t>& secret) {'):
        text=replace_once(text,signature,signature+'\n    if (s_ce_locking.count(user_id)) { errno = EBUSY; return false; }')
    signature='bool fscrypt_prepare_user_storage(const std::string& volume_uuid, userid_t user_id, int flags) {'
    text=replace_once(text,signature,signature+'''
    if ((flags & android::os::IVold::STORAGE_FLAG_CE) && s_ce_locking.count(user_id)) {
        errno = EBUSY;
        return false;
    }''')
    return text.encode()


def patched(originals, header):
    h=originals['KeyUtil.h'].decode()
    anchor='bool evictKey(const std::string& mountpoint, const android::fscrypt::EncryptionPolicy& policy);'
    h=replace_once(h,anchor,anchor+'\n\n// Confirms the final kernel state; EBUSY retains retryable policy metadata.\nbool evictKeyConfirmed(const std::string& mountpoint, const android::fscrypt::EncryptionPolicy& policy);')
    service=replace_once(originals['VoldNativeService.cpp'].decode(),
        '    return translateBool(fscrypt_lock_ce_storage(userId));',
        '    if (fscrypt_lock_ce_storage(userId)) return binder::Status::ok();\n'
        '    return binder::Status::fromServiceSpecificError(errno ? errno : EIO);')
    service=replace_once(service, '    return translateBool(fscrypt_destroy_user_keys(userId));',
        '    if (fscrypt_destroy_user_keys(userId)) return binder::Status::ok();\n'
        '    return binder::Status::fromServiceSpecificError(errno ? errno : EIO);')
    return {'KeyUtil.cpp':patch_key_util(originals['KeyUtil.cpp']),'KeyUtil.h':h.encode(),
            'FsCrypt.cpp':patch_fscrypt(originals['FsCrypt.cpp']),
            'VoldNativeService.cpp':service.encode(),HEADER:header}


def validate(record):
    if (not isinstance(record,dict) or record.get('schema')!=1
        or record.get('status')!='VOLD_SOURCES_PREPARED_NOT_TESTED'
        or record.get('commit')!=COMMIT or set(record.get('inputs',{}))!=ORIGINALS
        or set(record.get('outputs',{}))!=ORIGINALS|{HEADER}
        or any(not isinstance(x,str) or not re.fullmatch('[0-9a-f]{64}',x)
               for x in [*record['inputs'].values(),*record['outputs'].values()])):
        raise ValueError('Invalid vold ownership receipt')


def verify(aosp,record):
    validate(record)
    for name,sha in record['outputs'].items():
        if digest(checked_path(Path(aosp),'system/vold/'+name).read_bytes())!=sha:
            raise ValueError('Integrated vold source changed')


def prepare(project,aosp,originals=None,pins=None):
    project,aosp=Path(project).absolute(),Path(aosp).absolute()
    base=checked_path(aosp,'system/vold/.anchor').parent
    if originals is None:
        commit=subprocess.check_output(['git','-C',str(base),'rev-parse','HEAD'],text=True).strip()
        if commit!=COMMIT:raise ValueError('Unexpected vold Git baseline')
        originals={n:subprocess.check_output(['git','-C',str(base),'show','HEAD:'+n]) for n in sorted(ORIGINALS)}
    if pins is None:pins=json.loads(checked_path(project,'runtime/aosp-vold-hooks.json').read_bytes())
    if (pins.get('schema')!=1 or pins.get('commit')!=COMMIT or set(pins.get('files',{}))!=ORIGINALS
        or set(originals)!=ORIGINALS or any(digest(originals[n])!=pins['files'][n] for n in ORIGINALS)):
        raise ValueError('Vold Git blobs do not match pins')
    outputs=patched(originals,checked_path(project,SOURCE).read_bytes())
    record={'schema':1,'status':'VOLD_SOURCES_PREPARED_NOT_TESTED','commit':COMMIT,
            'inputs':pins['files'],'outputs':{n:digest(b) for n,b in outputs.items()}}
    marker=checked_path(aosp,MARKER);previous=json.loads(marker.read_bytes()) if marker.exists() else None
    if previous:
        validate(previous)
        if previous['inputs']!=record['inputs']:raise ValueError('Vold ownership baseline changed')
    changes={};before={}
    for n,data in outputs.items():
        path=checked_path(aosp,'system/vold/'+n);current=path.read_bytes() if path.exists() else None
        if current==data:continue
        allowed=current is None if n==HEADER else current==originals[n]
        if previous and current is not None:allowed |= digest(current)==previous['outputs'][n]
        if not allowed:raise ValueError('Unmanaged vold changes; preserving them')
        changes[n]=data;before[n]=current
    if changes:
        parent=checked_path(aosp,'out/aegis-vold-eviction/backups/.anchor').parent;parent.mkdir(parents=True,exist_ok=True)
        backup=Path(tempfile.mkdtemp(prefix='install-',dir=parent))
        for n,data in before.items():
            if data is not None:(backup/n).write_bytes(data)
        (backup/'intent.json').write_bytes(encoded(record))
        for n,data in changes.items():write_atomic(checked_path(aosp,'system/vold/'+n),data)
    verify(aosp,record);write_atomic(marker,encoded(record));return record


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--aosp',required=True,type=Path);parser.add_argument('--project',type=Path)
    parser.add_argument('--receipt',required=True,type=Path);parser.add_argument('--verify',action='store_true')
    args=parser.parse_args()
    if args.verify:verify(args.aosp,json.loads(args.receipt.read_bytes()))
    else:write_atomic(args.receipt,encoded(prepare(args.project,args.aosp)))
