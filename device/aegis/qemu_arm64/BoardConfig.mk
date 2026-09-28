# A selected, inspected runtime kernel overrides all three upstream input paths
# before inheritance. A normal product registration removes this generated file.
-include device/aegis/qemu_arm64/aegis-runtime-kernel.mk

# Reuse the version-pinned ARM64 virtual-device kernel/modules and board policy.
# Required includes deliberately fail when the upstream source sync is incomplete.
include device/google/cuttlefish/vsoc_arm64_only/BoardConfig.mk

# Extend the platform-side policy. No vendor HAL or general app permission is added.
SYSTEM_EXT_PRIVATE_SEPOLICY_DIRS += device/aegis/qemu_arm64/sepolicy/private

# Reserve every runtime app-ID via AOSP's normal duplicate/range-checked registry.
# This grants no capability and starts no process; per-user mappings are separate.
TARGET_FS_CONFIG_GEN += device/aegis/qemu_arm64/runtime-ids.fs
