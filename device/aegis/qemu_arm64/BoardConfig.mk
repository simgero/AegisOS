# Reuse the version-pinned ARM64 virtual-device kernel/modules and board policy.
# Required includes deliberately fail when the upstream source sync is incomplete.
include device/google/cuttlefish/vsoc_arm64_only/BoardConfig.mk

# Extend the platform-side policy. No vendor HAL or general app permission is added.
SYSTEM_EXT_PRIVATE_SEPOLICY_DIRS += device/aegis/qemu_arm64/sepolicy/private
