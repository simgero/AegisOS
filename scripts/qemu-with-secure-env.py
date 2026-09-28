#!/usr/bin/env python3
"""Bounded local Android + Linux secure_env diagnostic using original AOSP HALs.

Both VMs run on the Mac with HVF. Helper state is ephemeral, Android disk writes
are discarded. No host directory sharing or networking is enabled.
"""
import argparse
import hashlib
import importlib.util
from pathlib import Path
import shlex
import subprocess
import sys
import tarfile
import tempfile
import time


def cpio_entry(name, mode, data, inode):
    name = name.encode() + b'\0'
    fields = [inode,mode,0,0,1,0,len(data),0,0,0,0,len(name),0]
    result = b'070701' + ''.join(f'{x:08x}' for x in fields).encode() + name
    result += b'\0' * (-len(result) % 4)
    result += data
    return result + b'\0' * (-len(data) % 4)


def helper_initrd(assets, images):
    archive = assets/'secure-env-arm64.tar.gz'
    lines = (assets/'SHA256SUMS').read_text().splitlines()
    expected = next(line.split()[0] for line in lines if line.split()[-1] == archive.name)
    h = hashlib.sha256()
    with archive.open('rb') as f:
        for data in iter(lambda:f.read(1024**2),b''): h.update(data)
    if h.hexdigest() != expected:
        raise ValueError('Helper archive checksum mismatch')
    spec = importlib.util.spec_from_file_location('qemu_init',Path(__file__).with_name('qemu-init.py'))
    module = importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
    # The original vendor ramdisk provides modules matching this exact kernel.
    base = module.vendor_ramdisk(images/'vendor_boot.img')
    # Legacy LZ4 has no end marker. Raw cpio appended to that stream is parsed
    # as another compressed block and rejected by the kernel decompressor.
    if base[:4] in (b'\x02\x21\x4c\x18', b'\x04\x22\x4d\x18'):
        base = subprocess.run(['lz4','-d','-c'],input=base,
                              capture_output=True,check=True).stdout
    extra = bytearray()
    seen = set()
    total = 0
    with tarfile.open(archive,'r:gz') as tar:
        for index,item in enumerate(tar,1):
            name = item.name
            if name.startswith('/') or '..' in Path(name).parts or name in seen:
                raise ValueError('Unsafe or duplicate helper archive path')
            seen.add(name)
            if item.isdir():
                data=b'';mode=0o040755
            elif item.isfile():
                total += item.size
                if total > 1024**3: raise ValueError('Helper initrd too large')
                data=tar.extractfile(item).read();mode=0o100000 | (item.mode & 0o777)
            else:
                raise ValueError('Helper archive contains unsupported file type')
            extra.extend(cpio_entry(name,mode,data,index))
    if not {'init','host/bin/secure_env','lib/ld-musl-aarch64.so.1'} <= seen:
        raise ValueError('Incomplete helper archive')
    # Cuttlefish computes instance_dir from root_dir rather than using the
    # legacy instance_dir JSON field. Prepare its actual logging/state paths.
    for name in ['state/instances','state/instances/cvd-1',
                 'state/instances/cvd-1/logs','state/instances/cvd-1/internal']:
        if name not in seen:
            seen.add(name)
            extra.extend(cpio_entry(name,0o040755,b'',len(seen)))
    extra.extend(cpio_entry('TRAILER!!!',0,b'',len(seen)+1))
    return base + b'\0'*(-len(base)%4) + extra


def stop(process):
    if process.poll() is None:
        process.terminate()
        try: process.wait(timeout=5)
        except subprocess.TimeoutExpired: process.kill();process.wait()


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('images',type=Path)
    p.add_argument('helper_assets',type=Path)
    p.add_argument('disk',type=Path)
    p.add_argument('bootconfig',type=Path)
    p.add_argument('output',type=Path)
    p.add_argument('--seconds',type=int,default=180,help='0 keeps the VMs running until the QEMU window is closed')
    p.add_argument('--display',choices=['none','cocoa'],default='none')
    args=p.parse_args()
    if not 0<=args.seconds<=600: p.error('Duration must be 0–600 seconds')
    images=args.images.resolve();output=args.output.resolve()
    initrd=helper_initrd(args.helper_assets.resolve(),images)
    output.mkdir(parents=True,exist_ok=False)
    (output/'helper-initrd.img').write_bytes(initrd)
    local_config=Path(__file__).resolve().parents[1]/'tools/qemu/local.bootconfig'
    (output/'bootconfig').write_text(args.bootconfig.read_text().rstrip()+'\n'+local_config.read_text())
    subprocess.run([sys.executable,str(Path(__file__).with_name('qemu-init.py')),
                    str(images),str(output/'android'),'--disk',str(args.disk.resolve()),
                    '--bootconfig',str(output/'bootconfig'),'--prepare-only'],check=True)
    android=shlex.split((output/'android/command.txt').read_text())
    android[android.index('-display')+1]=args.display
    android += ['-device','virtio-gpu-pci','-device','virtio-keyboard-pci',
                '-device','virtio-tablet-pci','-device','virtio-serial-pci,id=serial,max_ports=31']
    helper=['qemu-system-aarch64','-machine','virt-11.1,gic-version=3','-accel','hvf',
            '-cpu','host','-smp','2','-m','1024','-nodefaults','-display','none','-net','none',
            '-no-reboot','-serial','stdio','-monitor','none','-kernel',str(images/'kernel'),
            '-initrd',str(output/'helper-initrd.img'),'-append',
            'console=ttyAMA0 earlycon=pl011,0x09000000 panic=1 printk.devkmsg=on',
            '-device','virtio-serial-pci,id=serial,max_ports=4']
    processes=[]
    with tempfile.TemporaryDirectory(prefix='aegis-tee-',dir='/tmp') as folder:
        console=Path(folder)/'console.sock'
        (output/'console-path.txt').write_text(str(console)+'\n')
        sockets={name:Path(folder)/(name+'.sock') for name in ['keymint','gatekeeper','keymaster','oemlock']}
        for index,(name,path) in enumerate(sockets.items()):
            helper += ['-chardev',f'socket,id=h{index},path={path},server=on,wait=off',
                       '-device',f'virtconsole,bus=serial.0,chardev=h{index}']
        mapping={3:'keymaster',4:'gatekeeper',10:'oemlock',11:'keymint'}
        for index in range(17):
            if index in mapping:
                backend=f'socket,id=h{index},path={sockets[mapping[index]]}'
            elif index==1:
                backend=f'socket,id=h{index},path={console},server=on,wait=off'
            elif index==2:
                backend=f'file,id=h{index},path={output / "logcat.log"}'
            else:
                backend=f'null,id=h{index}'
            android += ['-chardev',backend,'-device',f'virtconsole,bus=serial.0,chardev=h{index}']
        (output/'helper-command.txt').write_text(shlex.join(helper)+'\n')
        (output/'android-command.txt').write_text(shlex.join(android)+'\n')
        with (output/'helper.log').open('w') as hostlog,(output/'android.log').open('w') as guestlog:
            try:
                host=subprocess.Popen(helper,stdout=hostlog,stderr=subprocess.STDOUT);processes.append(host)
                deadline=time.monotonic()+30
                while not all(path.exists() for path in sockets.values()):
                    if host.poll() is not None or time.monotonic()>deadline:
                        raise RuntimeError('Helper failed before opening channels; inspect helper.log')
                    time.sleep(.1)
                guest=subprocess.Popen(android,stdout=guestlog,stderr=subprocess.STDOUT);processes.append(guest)
                deadline=time.monotonic()+args.seconds if args.seconds else None
                while deadline is None or time.monotonic()<deadline:
                    if host.poll() is not None or guest.poll() is not None: break
                    time.sleep(.5)
            finally:
                for process in reversed(processes): stop(process)
    print(f'Both test VMs stopped. Logs: {output}')
    print('A complete Android boot must be confirmed separately; process exit is not success.')


if __name__=='__main__':
    main()
