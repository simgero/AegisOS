# Standalone QEMU has no RootCanal / Bluetooth HCI peer on hvc5.
# Set before inheritance: upstream then omits com.google.cf.bt and declares
# android.hardware.bluetooth unavailable instead of starting a crashing HAL.
BOARD_HAVE_BLUETOOTH := false

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

# First identity integration: the GNU/Linux lifecycle coordinator is not installed yet.
# The service refuses any other/unspecified mode instead of claiming a complete runtime logout.
PRODUCT_SYSTEM_PROPERTIES += ro.aegis.runtime.mode=absent

PRODUCT_PACKAGES += \
    aegis \
    aegis-identity-service
# Append after the common jars regardless of product makefile inheritance order.
PRODUCT_SYSTEM_SERVER_JARS_EXTRA += aegis-identity-service

# Keep this private boot-time service list in framework-res itself. Inherited Cuttlefish
# RROs remain enabled; the pinned core/phone overlays do not replace this resource.
DEVICE_PACKAGE_OVERLAYS += device/aegis/qemu_arm64/overlay
PRODUCT_ENFORCE_RRO_EXCLUDED_OVERLAYS += device/aegis/qemu_arm64/overlay
