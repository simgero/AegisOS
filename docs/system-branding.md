# AegisOS system branding

The QEMU product uses the approved ægis.os Brand Guide 06 artwork. This first
pass supplies a light boot screen, a subtle three-dot activity loop, the same
screen during shutdown/userspace restart, the brand wallpaper, an AegisOS
default device name, and neutral system-version labels in English and German.
The activity indicator does not report numeric boot progress.

## Build integration

`device/aegis/qemu_arm64/branding/branding.mk` installs the uncompressed animation
archive under `/product/media`, including the dark-theme filename so selecting
a dark boot theme does not fall back to the platform animation. The first pass
intentionally uses the same light design for both themes. The archive contains
36 frames at 24 fps; its interruptible loop stops when the system requests it.

The existing static product overlay supplies the framework default wallpaper,
SettingsProvider device-name defaults and Settings labels. The actual platform
release number, SDK level, package names and legal notices are unchanged. A
displayed system version of 16 still refers to the platform, not a claimed
AegisOS release 16. Other locales and remaining setup/application strings have
not yet been comprehensively rebranded.

Existing saved wallpaper and device-name preferences are preserved. The new
defaults apply on fresh data; changing an existing user's background is a
separate user preference. The default lock screen follows the system wallpaper
unless a separate lock wallpaper was selected.

## Artwork and regeneration

The logo and wallpaper were copied unchanged from
`website-brand-20260928/output-v6/brand-assets`. The build consumes only the
checked-in files under `device`, without depending on that local website folder.

`scripts/branding/boot.html` is the editable boot layout. Render with Node,
Playwright, installed Chrome and Arial available:

```sh
node scripts/branding/render.cjs
python3 scripts/branding/package.py
```

Set `NODE_PATH` to the installed Playwright modules if necessary. PNGs are
rendered into ignored `out/branding/part0`. The packager uses ZIP_STORED and fixed
ZIP metadata. The normal AOSP build needs neither a browser nor these tools.

## Validation and remaining device checks

Locally checked: visual frame inspection, all 36 PNGs decoding at 720 × 720,
ZIP integrity/uncompressed entries, XML parsing, and existing product installer
tests. Source resource names and boot-file precedence were checked against the
builder's pinned AOSP sources. No bootanimation APEX was present in that build;
such an APEX would take priority over `/product/media` and needs rechecking if
the product selection changes.

The branded image still requires its own AOSP build and QEMU verification:

1. Confirm packaged product animation files match the source archive.
2. Record boot animation and verify the normal boot completes.
3. On fresh data, verify wallpaper, device name and Settings in German/English.
4. On existing data, verify chosen wallpaper/device name remain unchanged.
5. Check shutdown and restart, and confirm no blocking animation segment.

This is the system branding pass, not a custom launcher, setup wizard or desktop.
