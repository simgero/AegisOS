# AegisOS system branding

## Approved boot animation

The production archive contains the approved dark design: exact black
`#000000`, light shield and wordmark, and a green square. Solid phone, tablet
and notebook shapes morph into the shield; their navigation apertures shrink
closed geometrically. The notebook base is part of its morphing silhouette.
The central green square and the wordmark remain visible throughout.

The one-time opening includes the gentle acceleration into the loop. At 30 fps,
158 opening frames are followed by 126 looping frames (4.2 seconds per square
period, unchanged 16.8 seconds per full rotation). The loop continues from the intro's phase and repeats without an
extra pause. Both parts use Android's interruptible `p` mode: finishing system
boot takes precedence over completing the opening or another loop.

## Build integration

`device/aegis/qemu_arm64/branding/branding.mk` installs `bootanimation.zip` as
`/product/media/bootanimation.zip`, `bootanimation-dark.zip` and
`userspace-reboot.zip`. Both display themes intentionally use the approved OLED
black design. Shutdown installs a separate loop-only `shutdownanimation.zip`;
it does not replay the device construction.

Existing static overlays supply the framework default wallpaper, device-name
defaults and Settings labels. Saved user preferences remain intact. Platform
release numbers, SDK levels, package identities and legal notices stay truthful.
This is not a custom launcher or desktop.

## Source and regeneration

The self-contained approved source is `scripts/branding/boot-preview.html`.
`scripts/branding/motion-preview.html` retains the interactive shape study;
`logo-static.svg` and `svg-review.html` retain the static approval proof.
The runtime SVG construction enforces mirror symmetry and uses regular centered
polygons. These later revisions intentionally supersede the original raster
animation and its three-dot activity indicator.

Render with Node, Playwright, Chrome and Arial available; package with Python
and Pillow:

```sh
node scripts/branding/render.cjs
python3 scripts/branding/package.py
```

Set `NODE_PATH` to the installed Playwright modules if necessary. Frames and
render metadata are written to ignored `out/branding/production`. Rendering
hides preview controls and uses explicit timestamps, not wall-clock playback.
The packager checks the frame inventory, dimensions and black corners, then
performs lossless RGB PNG optimisation. ZIP entries are stored uncompressed
with fixed metadata, as required by Android. Each archive remains under the
8-MiB build-input limit. The AOSP build consumes these checked-in archives and
needs neither a browser nor Pillow.

## Verification status

The local animation package is built and validated: 284 PNG frames, 720 × 720,
30 fps, exact black backgrounds, ZIP integrity and stored compression. The
shutdown archive contains the 126 loop frames. Product installer regression
checks cover copying nested product assets.

Full image `5a01e7cf` was built, uploaded and verified, then booted in a separate
local QEMU profile with SELinux Enforcing. Its QMP screenshot contains the exact
brand green `(196, 241, 90)` and shield white `(243, 244, 246)`. The earlier
red/blue swap is corrected in the guest compositor. See [color evidence](qemu-colors.md).
Frames are rendered at 3x resolution and downsampled for smoother contours.

For device verification, compare the installed archive hashes, check for a
higher-priority bootanimation APEX, record construction/loop/boot completion,
and confirm that the animation exits when Android finishes booting. Check
shutdown independently. Earlier AOSP source inspection found no selected
bootanimation APEX; revisit that if product selection changes.
