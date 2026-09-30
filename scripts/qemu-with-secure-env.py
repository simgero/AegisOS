#!/usr/bin/env python3
"""Bounded local Android + Linux secure_env diagnostic using original AOSP HALs.

Both VMs run on the Mac with HVF. By default both disks are ephemeral; an
explicit paired profile enables persistent Android and helper state together.
No host directory sharing is enabled. Guest networking is off by default;
--network user explicitly adds outbound QEMU user-mode networking to Android.
An optional loopback-only serial channel can carry authenticated ADB traffic.
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
import signal
import uuid


def cpio_entry(name, mode, data, inode):
    name = name.encode() + b'\0'
    fields = [inode,mode,0,0,1,0,len(data),0,0,0,0,len(name),0]
    result = b'070701' + ''.join(f'{x:08x}' for x in fields).encode() + name
    result += b'\0' * (-len(result) % 4)
    result += data
    return result + b'\0' * (-len(data) % 4)


def helper_initrd(assets, images, profile_id=None, require_persistent=False):
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
    protocol = None
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
                if name=='etc/aegis-helper-protocol': protocol=data
            else:
                raise ValueError('Helper archive contains unsupported file type')
            extra.extend(cpio_entry(name,mode,data,index))
    if not {'init','host/bin/secure_env','lib/ld-musl-aarch64.so.1'} <= seen:
        raise ValueError('Incomplete helper archive')
    if (profile_id or require_persistent) and protocol != b'persistent-state-v1\n':
        raise ValueError('This helper does not support persistent paired state')
    if profile_id:
        if str(uuid.UUID(profile_id)) != profile_id:
            raise ValueError('Invalid profile identity')
        if 'etc/aegis-profile-id' in seen:
            raise ValueError('Helper archive must not provide a profile identity')
        if 'etc' not in seen:
            seen.add('etc');extra.extend(cpio_entry('etc',0o040755,b'',len(seen)))
        seen.add('etc/aegis-profile-id')
        extra.extend(cpio_entry('etc/aegis-profile-id',0o100600,
                                (profile_id+'\n').encode(),len(seen)))
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


def interrupt(*_):
    raise KeyboardInterrupt()


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('images',type=Path)
    p.add_argument('helper_assets',type=Path)
    p.add_argument('disk',type=Path)
    p.add_argument('bootconfig',type=Path)
    p.add_argument('output',type=Path)
    p.add_argument('--seconds',type=int,default=180,help='0 keeps the VMs running until the QEMU window is closed')
    p.add_argument('--display',choices=['none','cocoa'],default='none')
    p.add_argument('--framebuffer-format',choices=['rgba','bgra'],
                   help='Override the local compositor framebuffer format for diagnostics')
    p.add_argument('--pointer',choices=['mouse','tablet'],default='mouse',
                   help='Relative mouse for Android cursor input; tablet retained for diagnostics')
    p.add_argument('--adb-port',type=int,default=0,
                   help='Optional 127.0.0.1 TCP endpoint for a guest hvc17-to-adbd bridge')
    p.add_argument('--network',choices=['none','user'],default='none',
                   help='Optional Android user-mode networking; no host port forwards; helper remains offline')
    p.add_argument('--profile',type=Path,help='Paired persistent Android and TPM profile')
    p.add_argument('--create-profile',action='store_true',help='Explicitly provision a NEW profile')
    args=p.parse_args()
    if not 0<=args.seconds<=600: p.error('Duration must be 0–600 seconds')
    if args.adb_port and not 1024<=args.adb_port<=65535: p.error('ADB port must be 1024–65535')
    if args.create_profile and not args.profile: p.error('--create-profile requires --profile')
    # Termination enters the same cleanup path as Ctrl-C.
    signal.signal(signal.SIGTERM,interrupt)
    if args.profile:
        import qemu_profile
        args.profile=args.profile.absolute()
        if args.create_profile:
            helper_initrd(args.helper_assets.resolve(),args.images.resolve(),require_persistent=True)
            qemu_profile.create(args.profile,args.images,args.disk,args.bootconfig,args.helper_assets)
        with qemu_profile.locked(args.profile):
            manifest=qemu_profile.validate(args.profile,args.images,args.disk,args.bootconfig,args.helper_assets)
            run(args,manifest)
    else:
        run(args)


def run(args,manifest=None):
    images=args.images.resolve();output=args.output.resolve()
    initrd=helper_initrd(args.helper_assets.resolve(),images,
                         profile_id=manifest['profile_id'] if manifest else None)
    output.mkdir(parents=True,exist_ok=False)
    (output/'helper-initrd.img').write_bytes(initrd)
    local_config=Path(__file__).resolve().parents[1]/'tools/qemu/local.bootconfig'
    local_text=local_config.read_text()
    if args.framebuffer_format:
        import re
        local_text=re.sub(r'(?m)^androidboot.hardware.hwcomposer.display_framebuffer_format=.*$',
                          'androidboot.hardware.hwcomposer.display_framebuffer_format='+args.framebuffer_format,
                          local_text)
    (output/'bootconfig').write_text(args.bootconfig.read_text().rstrip()+'\n'+local_text)
    subprocess.run([sys.executable,str(Path(__file__).with_name('qemu-init.py')),
                    str(images),str(output/'android'),'--disk',str(args.disk.resolve()),
                    '--bootconfig',str(output/'bootconfig'),'--prepare-only'],check=True)
    android=shlex.split((output/'android/command.txt').read_text())
    if manifest:
        android.remove('-snapshot')
        drive=android.index('-drive')+1
        android[drive]=f'file={args.profile / "android.qcow2"},format=qcow2,if=none,id=android'
        (output/'profile-path.txt').write_text(str(args.profile)+'\n')
    android[android.index('-display')+1]='cocoa,zoom-to-fit=on' if args.display=='cocoa' else args.display
    android += ['-device','virtio-gpu-pci,xres=720,yres=1280','-device','virtio-keyboard-pci',
                '-device',f'virtio-{args.pointer}-pci','-device','virtio-serial-pci,id=serial,max_ports=31']
    if args.network=='user':
        # Keep -net none to suppress implicit legacy NICs; this pair is explicit.
        # Cuttlefish renames eth0 and explicitly restricts eth1. Reserve these
        # two slots without backends; Android's ordinary Ethernet manager owns
        # eth2, including DHCP, DNS, default routing and network validation.
        # Only eth2 can carry traffic; no NIC publishes a host port.
        android += ['-device','virtio-net-pci,id=aegis-reserved0,mac=52:54:00:ae:61:00',
                    '-device','virtio-net-pci,id=aegis-reserved1,mac=52:54:00:ae:61:02',
                    '-netdev','user,id=aegis-net,ipv6=off',
                    '-device','virtio-net-pci,id=aegis-nic,netdev=aegis-net,mac=52:54:00:ae:61:01']
    (output/'network-mode.txt').write_text(args.network+'\n')
    helper=['qemu-system-aarch64','-machine','virt-11.1,gic-version=3','-accel','hvf',
            '-cpu','host','-smp','2','-m','1024','-nodefaults','-display','none','-net','none',
            '-no-reboot','-serial','stdio','-monitor','none','-kernel',str(images/'kernel'),
            '-initrd',str(output/'helper-initrd.img'),'-append',
            'console=ttyAMA0 earlycon=pl011,0x09000000 panic=1 printk.devkmsg=on',
            '-device','virtio-serial-pci,id=serial,max_ports=4']
    if manifest:
        helper += ['-drive',f'file={args.profile / "secure-env.ext4"},format=raw,if=none,id=state',
                   '-device','virtio-blk-pci,drive=state']
    processes=[]
    with tempfile.TemporaryDirectory(prefix='aegis-tee-',dir='/tmp') as folder:
        console=Path(folder)/'console.sock'
        (output/'console-path.txt').write_text(str(console)+'\n')
        qmp=Path(folder)/'android-qmp.sock'
        android += ['-qmp',f'unix:{qmp},server=on,wait=off']
        (output/'qmp-path.txt').write_text(str(qmp)+'\n')
        sockets={name:Path(folder)/(name+'.sock') for name in ['keymint','gatekeeper','keymaster','oemlock']}
        for index,(name,path) in enumerate(sockets.items()):
            helper += ['-chardev',f'socket,id=h{index},path={path},server=on,wait=off',
                       '-device',f'virtconsole,bus=serial.0,chardev=h{index}']
        mapping={3:'keymaster',4:'gatekeeper',10:'oemlock',11:'keymint'}
        for index in range(18 if args.adb_port else 17):
            if index in mapping:
                backend=f'socket,id=h{index},path={sockets[mapping[index]]}'
            elif index==1:
                backend=f'socket,id=h{index},path={console},server=on,wait=off'
            elif index==2:
                backend=f'file,id=h{index},path={output / "logcat.log"}'
            elif index==17:
                backend=f'socket,id=h17,host=127.0.0.1,port={args.adb_port},server=on,wait=off'
            else:
                backend=f'null,id=h{index}'
            android += ['-chardev',backend,'-device',f'virtconsole,bus=serial.0,chardev=h{index}']
        if args.adb_port:
            (output/'adb-address.txt').write_text(f'127.0.0.1:{args.adb_port}\n')
        (output/'helper-command.txt').write_text(shlex.join(helper)+'\n')
        (output/'android-command.txt').write_text(shlex.join(android)+'\n')
        with (output/'helper.log').open('w') as hostlog,(output/'android.log').open('w') as guestlog:
            try:
                host=subprocess.Popen(helper,stdin=subprocess.PIPE,stdout=hostlog,stderr=subprocess.STDOUT);processes.append(host)
                deadline=time.monotonic()+30
                while not all(path.exists() for path in sockets.values()):
                    if host.poll() is not None or time.monotonic()>deadline:
                        raise RuntimeError('Helper failed before opening channels; inspect helper.log')
                    time.sleep(.1)
                if manifest:
                    while 'AEGIS_HELPER_READY' not in (output/'helper.log').read_text(errors='replace'):
                        if host.poll() is not None or time.monotonic()>deadline:
                            raise RuntimeError('Persistent helper not ready; Android was not started')
                        time.sleep(.1)
                guest=subprocess.Popen(android,stdout=guestlog,stderr=subprocess.STDOUT);processes.append(guest)
                deadline=time.monotonic()+args.seconds if args.seconds else None
                while deadline is None or time.monotonic()<deadline:
                    if host.poll() is not None or guest.poll() is not None: break
                    time.sleep(.5)
            finally:
                if manifest and len(processes)>1 and processes[1].poll() is None:
                    # Request Android's normal shutdown, keeping its KeyMint peer alive.
                    import socket
                    try:
                        with socket.socket(socket.AF_UNIX) as connection:
                            connection.settimeout(2);connection.connect(str(console))
                            connection.sendall(b'\nsu 0 setprop sys.powerctl shutdown\n')
                        processes[1].wait(timeout=45)
                    except (OSError,subprocess.TimeoutExpired):
                        print('WARNING: Android shutdown was not confirmed; preserving disks for recovery.',file=sys.stderr)
                for process in reversed(processes[1:]): stop(process)
                if manifest and processes and processes[0].poll() is None:
                    try:
                        processes[0].stdin.write(b'poweroff\n');processes[0].stdin.flush()
                        processes[0].wait(timeout=20)
                    except (OSError,subprocess.TimeoutExpired):
                        print('WARNING: helper shutdown was not confirmed; preserving state for recovery.',file=sys.stderr)
                for process in processes[:1]: stop(process)
                if manifest:
                    clean='AEGIS_HELPER_SHUTDOWN_CLEAN' in (output/'helper.log').read_text(errors='replace')
                    (output/'helper-shutdown.txt').write_text('clean\n' if clean else 'unconfirmed\n')
    print(f'Both test VMs stopped. Logs: {output}')
    print('A complete Android boot must be confirmed separately; process exit is not success.')


if __name__=='__main__':
    main()
