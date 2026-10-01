"""Explicit ARM64 guest execution on Apple Silicon or Linux, including TCG."""
import os
import platform


def execution(accel='auto', system=None, machine=None, kvm=None):
    system = system or platform.system()
    machine = machine or platform.machine()
    if system not in ('Darwin', 'Linux'):
        raise ValueError('QEMU guests require macOS or Linux')
    arm = machine in ('arm64', 'aarch64')
    kvm = os.access('/dev/kvm', os.R_OK | os.W_OK) if kvm is None else kvm
    if accel == 'auto':
        accel = 'hvf' if system == 'Darwin' and arm else (
            'kvm' if system == 'Linux' and arm and kvm else 'tcg')
    if accel == 'hvf' and not (system == 'Darwin' and arm):
        raise ValueError('ARM64 HVF requires Apple Silicon')
    if accel == 'kvm' and not (system == 'Linux' and arm and kvm):
        raise ValueError('ARM64 KVM requires an ARM64 Linux host and accessible /dev/kvm')
    if accel not in ('hvf', 'kvm', 'tcg'):
        raise ValueError('Unsupported accelerator')
    # AOSP's boot_devices binding names the low PCI ECAM node. TCG's max CPU
    # otherwise moves that node to 4010000000.pcie on this machine version.
    board = ('virt-11.1' if system == 'Darwin' else 'virt-10.2,highmem-ecam=off')
    return ['-machine', board + ',gic-version=3',
            '-accel', 'tcg,thread=multi' if accel == 'tcg' else accel,
            '-cpu', 'max' if accel == 'tcg' else 'host']
