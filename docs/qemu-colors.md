# QEMU color output and boot contour quality

29 September 2026. Before the correction, an internal guest screenshot contains
RGB `(196, 241, 90)` and `(243, 244, 246)`, but QMP scanout contains
`(90, 241, 196)` and `(246, 244, 243)`. This is an output-channel mismatch,
not a branding palette change. It affects all composed content.

The pinned ranchu guest composer allocates RGBA scanout buffers and composes
RGBA bytes. Its existing `display_framebuffer_format` property was not consumed
by this source version. The graphics registration step adds support for BGRA
allocation plus one RGBA-to-BGRA conversion at final guest composition. The
local virtio-2D QEMU configuration selects BGRA. The default RGBA path is
preserved; empty compositions do not swap an already-converted buffer again.
Both upstream source hashes are checked, unrelated edits are rejected, and
full builds verify the installed patch after compilation.

Boot frames are rendered at 3x resolution and downsampled with Lanczos to
720x720; logo geometry, layout, colors and angular speed remain unchanged.
The rounded square has a 90-degree period, so 126 frames at 30 fps replace
four repeated periods (504 frames). A full mathematical turn still takes
16.8 seconds. Packaging checks the archive size before replacing an artifact.

## Verified runtime result

Full build `5a01e7cfbc8db1d8b1625c38574c2205df94c708`, release
`aosp-20260929T150357Z-5a01e7cf-0abe7af0`, was uploaded, downloaded and
checksum/AVB-checked. Separate profile `graphics-5a01e7cf` boots with
`sys.boot_completed=1` and SELinux Enforcing.

The real QMP boot frame has 3,263 exact brand-green pixels `(196,241,90)`,
zero swapped-green pixels `(90,241,196)`, and 26,009 exact shield-white pixels
`(243,244,246)`. The internal screenshot independently contains exact green.
The 720x1280 screen was visually reviewed for smooth contours.

A normal lock-screen comparison has mean absolute channel error 0.457/255;
98.53% of compared pixels agree within two levels in every channel. Exact
pixel identity is not claimed: screenshot and display composition have small
rounding differences. The swapped-channel hypothesis has 5.185/255 mean
error, over eleven times higher. Screenshots transferred through the existing
development console were checked against guest SHA-256; no ADB key was added.

Evidence is in `out/full-build-5a01e7cf/visible-1/display-check.json`, alongside
guest PNGs and QMP PPM/PNG captures. Boot playback was stopped after testing;
the normal visible VM remains running. The previous profiles are preserved.
Shutdown uses the new archive but has not been separately visually accepted.
Older images do not contain the composer correction, even with the BGRA
property. Future full builds run the hash-pinned graphics registration step.
