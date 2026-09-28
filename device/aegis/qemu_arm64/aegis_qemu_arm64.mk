# Standalone QEMU has no RootCanal / Bluetooth HCI peer on hvc5.
# Set before inheritance: upstream then omits com.google.cf.bt and declares
# android.hardware.bluetooth unavailable instead of starting a crashing HAL.
BOARD_HAVE_BLUETOOTH := false

# Keep upstream partitions and security policy while adapting virtual hardware.
$(call inherit-product, device/google/cuttlefish/vsoc_arm64_only/phone/aosp_cf.mk)

PRODUCT_NAME := aegis_qemu_arm64
# Must match the leaf directory used by AOSP's BoardConfig discovery.
PRODUCT_DEVICE := qemu_arm64
PRODUCT_BRAND := AegisOS
PRODUCT_MANUFACTURER := AegisOS
PRODUCT_MODEL := AegisOS QEMU ARM64 Development

# Development access still requires an explicitly authorized ADB host key.
PRODUCT_SYSTEM_PROPERTIES += ro.adb.secure=1
