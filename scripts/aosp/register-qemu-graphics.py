#!/usr/bin/env python3
"""Apply a hash-pinned, reversible-source QEMU 2D scanout format fix."""
import argparse
import hashlib
import subprocess
from pathlib import Path

FILES = {
 'DrmSwapchain.cpp': '62442ac30297a8766ec212e87a14f7c8ab4e70fcebb4cd9b06deb608fdbf8e7b',
 'GuestFrameComposer.cpp': '4d3a769edc832b069e53a06a11788b0547d1207014ea31f867cfff5bc8a00b18',
}

def patch(name, text):
    if name == 'DrmSwapchain.cpp':
        text = text.replace('#include <log/log.h>', '#include <log/log.h>\n#include <android-base/properties.h>')
        old = 'width, height, ::android::PIXEL_FORMAT_RGBA_8888, layerCount, usage, &handle,'
        new = '''width, height,
                ::android::base::GetProperty("ro.vendor.hwcomposer.display_framebuffer_format", "rgba") == "bgra"
                    ? ::android::PIXEL_FORMAT_BGRA_8888 : ::android::PIXEL_FORMAT_RGBA_8888,
                layerCount, usage, &handle,'''
    else:
        old = '    DEBUG_LOG("%s display:%" PRIu32 " flushing drm buffer", __FUNCTION__, displayId);'
        new = '''    // Guest composition above is RGBA. QEMU's virtio-2D dumb scanout is
    // BGRA: convert bytes to match the BGRA allocation, without altering
    // application colors or SurfaceFlinger's screenshot representation.
    if (!noOpComposition && ::android::base::GetProperty("ro.vendor.hwcomposer.display_framebuffer_format", "rgba") == "bgra") {
        for (uint32_t y = 0; y < compositionResultBufferHeight; ++y) {
            auto* row = compositionResultBufferData + static_cast<size_t>(y) * compositionResultBufferStride;
            for (uint32_t x = 0; x < compositionResultBufferWidth; ++x) {
                const uint8_t red = row[4 * x];
                row[4 * x] = row[4 * x + 2];
                row[4 * x + 2] = red;
            }
        }
    }

''' + old
    if text.count(old) != 1:
        raise ValueError('Graphics patch anchor changed: ' + name)
    return text.replace(old, new, 1)

def main():
    p = argparse.ArgumentParser()
    p.add_argument('--aosp', type=Path, required=True)
    p.add_argument('--check', action='store_true')
    args = p.parse_args()
    root = args.aosp / 'device/generic/goldfish-opengl'
    updates = []
    for name, expected in FILES.items():
        relative = 'system/hwc3/' + name
        original = subprocess.check_output(['git', '-C', str(root), 'show', 'HEAD:' + relative])
        if hashlib.sha256(original).hexdigest() != expected:
            raise ValueError('Unrecognized upstream graphics source: ' + name)
        path = root / relative
        wanted = patch(name, original.decode()).encode()
        current = path.read_bytes()
        if current not in (original, wanted):
            raise ValueError('Unrelated graphics edits: ' + name)
        if args.check and current != wanted:
            raise ValueError('Graphics fix is not installed: ' + name)
        updates.append((path, wanted))
    if not args.check:
        for path, wanted in updates:
            if path.read_bytes() != wanted:
                temporary = path.with_suffix('.aegis-tmp')
                with temporary.open('xb') as stream:
                    stream.write(wanted)
                temporary.replace(path)
    print('QEMU graphics source verified' if args.check else 'QEMU graphics source prepared')

if __name__ == '__main__':
    main()
