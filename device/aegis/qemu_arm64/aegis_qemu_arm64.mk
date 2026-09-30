# Standalone QEMU has no RootCanal / Bluetooth HCI peer on hvc5.
# Set before inheritance: upstream then omits com.google.cf.bt and declares
# android.hardware.bluetooth unavailable instead of starting a crashing HAL.
BOARD_HAVE_BLUETOOTH := false

# No modem peer exists in the standalone launcher. Cuttlefish otherwise starts
# a radio HAL with no backend while PhoneGlobals waits for IRadioModem/slot1.
# Use upstream's vendor switch plus the effective feature exclusions below.
TARGET_NO_TELEPHONY := true

# The inherited audio APEX still declares IModule/bluetooth. Keep the complete
# upstream audio policy and copy its Bluetooth audio configuration explicitly:
# disabling the HCI controller otherwise omits that file. The software audio
# module is separate from the absent HCI HAL; Bluetooth stays unavailable to apps.
LOCAL_AUDIO_PRODUCT_COPY_FILES := \
    device/google/cuttlefish/shared/config/audio/policy/audio_policy_configuration.xml:$(TARGET_COPY_OUT_VENDOR)/etc/audio_policy_configuration.xml \
    frameworks/av/services/audiopolicy/config/bluetooth_with_le_audio_policy_configuration_7_0.xml:$(TARGET_COPY_OUT_VENDOR)/etc/bluetooth_with_le_audio_policy_configuration_7_0.xml \
    device/google/cuttlefish/shared/config/audio/policy/primary_audio_policy_configuration.xml:$(TARGET_COPY_OUT_VENDOR)/etc/primary_audio_policy_configuration.xml \
    hardware/interfaces/audio/aidl/default/audio_effects_config.xml:$(TARGET_COPY_OUT_VENDOR)/etc/audio_effects_config.xml

# Keep upstream partitions and security policy while adapting virtual hardware.
$(call inherit-product, device/google/cuttlefish/vsoc_arm64_only/phone/aosp_cf.mk)

# These Cuttlefish radios have no peer in the standalone QEMU launcher. AOSP
# applies unavailable-feature declarations after reading all partitions/APEXes.
# In particular nfc.any gates both the persistent NFC app and NfcService startup.
PRODUCT_COPY_FILES += \
    device/aegis/qemu_arm64/permissions/unavailable-radios.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/aegis-unavailable-radios.xml

PRODUCT_NAME := aegis_qemu_arm64
# Must match the leaf directory used by AOSP's BoardConfig discovery.
PRODUCT_DEVICE := qemu_arm64
PRODUCT_BRAND := AegisOS
PRODUCT_MANUFACTURER := AegisOS
PRODUCT_MODEL := AegisOS QEMU ARM64 Development

# Development access still requires an explicitly authorized ADB host key.
PRODUCT_SYSTEM_PROPERTIES += ro.adb.secure=1

# A normal source/component selection has no active Linux runtime. A full image
# may opt in only through the build worker's checked base AND kernel receipts.
# These generated includes are outside the project and replaced on registration.
ifeq ($(wildcard device/aegis/qemu_arm64/aegis-runtime-base.mk),)
PRODUCT_SYSTEM_PROPERTIES += ro.aegis.runtime.mode=absent
else
ifeq ($(wildcard device/aegis/qemu_arm64/aegis-runtime-kernel.mk),)
$(error AEGIS managed runtime requires a checked runtime kernel selection)
endif
PRODUCT_SYSTEM_PROPERTIES += ro.aegis.runtime.mode=managed-v1
PRODUCT_PACKAGES += aegis-runtime-broker aegis-runtime-setup aegis-runtime-init aegis-package-prepare \
    aegis-package-plan aegis-package-network aegis-package-execute aegis-package-publish
# Add one vendor profile; AOSP loads it after retaining all platform profiles.
# JoinCgroup deliberately ignores cgroup v2 in this pin. WriteFile is the
# supported profile action, run by Init before dropping to the broker domain.
PRODUCT_COPY_FILES += \
    device/aegis/qemu_arm64/runtime-task-profiles.json:$(TARGET_COPY_OUT_VENDOR)/etc/task_profiles.json
# Namespace and package helpers deliberately live in verified /system.
# Keep generic_system's artifact-path checks; allow only these exact outputs.
PRODUCT_ARTIFACT_PATH_REQUIREMENT_ALLOWED_LIST += \
    system/bin/aegis-runtime-init \
    system/bin/aegis-runtime-setup \
    system/bin/aegis-package-prepare \
    system/bin/aegis-package-plan \
    system/bin/aegis-package-network \
    system/bin/aegis-package-execute \
    system/bin/aegis-package-publish
endif

PRODUCT_PACKAGES += \
    aegis \
    aegis-identity-service \
    libaegis_terminal_jni \
    AegisQemuHardwareOverlay
# Install AOSP's generated numeric-account registries for the reserved runtime IDs.
# These are platform resource names, not personal accounts or authentication data.
PRODUCT_PACKAGES += passwd_vendor group_vendor passwd_system_ext group_system_ext
# Append after the common jars. The explicit partition prefix also selects the
# system-server classpath and expected dexpreopt artifact locations.
PRODUCT_SYSTEM_SERVER_JARS_EXTRA += system_ext:aegis-identity-service

# Keep this private boot-time service list in framework-res itself. Inherited Cuttlefish
# RROs remain enabled; the pinned core/phone overlays do not replace this resource.
# Cellular booleans also need AegisQemuHardwareOverlay: the inherited vendor
# phone RRO overrides config_sms_capable after framework-res has been built.
DEVICE_PACKAGE_OVERLAYS += device/aegis/qemu_arm64/overlay
PRODUCT_ENFORCE_RRO_EXCLUDED_OVERLAYS += device/aegis/qemu_arm64/overlay

include device/aegis/qemu_arm64/branding/branding.mk

# Present only for an explicitly selected, checked shared-base generation.
# The guarded selection above also installs the corresponding trusted services.
-include device/aegis/qemu_arm64/aegis-runtime-base.mk
