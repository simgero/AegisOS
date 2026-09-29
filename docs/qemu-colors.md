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

Runtime acceptance of the new build is pending. Older images do not contain
the composer correction, even when launched with the BGRA property.
