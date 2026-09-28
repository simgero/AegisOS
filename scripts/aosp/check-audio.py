#!/usr/bin/env python3
"""Check installed policy includes and the selected upstream audio APEX contract."""
import argparse
import subprocess
import xml.etree.ElementTree as ET


def check(policy, manifest):
    # Resolve against the installed directory: a source-tree include is not
    # evidence that PRODUCT_COPY_FILES delivered it to the vendor partition.
    expanded = subprocess.check_output(['xmllint', '--nonet', '--xinclude', str(policy)])
    root = ET.fromstring(expanded)
    names = [module.attrib['name'] for module in root.findall('./modules/module')]
    if len(names) != len(set(names)) or 'primary' not in names:
        raise ValueError('Missing primary or duplicate audio module')
    configured = {'default' if name == 'primary' else name for name in names}
    declared = set()
    for hal in ET.parse(manifest).getroot().findall('hal'):
        if hal.get('format') == 'aidl' and hal.findtext('name') == 'android.hardware.audio.core':
            for fqname in hal.findall('fqname'):
                if fqname.text and fqname.text.startswith('IModule/'):
                    declared.add(fqname.text.removeprefix('IModule/'))
    if not declared or configured != declared:
        raise ValueError(f'Audio policy/APEX module mismatch: configured={sorted(configured)}, '
                         f'declared={sorted(declared)}')
    print('Installed audio policy matches selected APEX modules: ' + ', '.join(sorted(declared)))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('policy')
    parser.add_argument('manifest')
    args = parser.parse_args()
    check(args.policy, args.manifest)
