# Initial device integration; boot on standalone QEMU is not yet validated.
# Keep upstream partitions, HALs and security policy while bringing up hardware.
$(call inherit-product, device/google/cuttlefish/vsoc_arm64_only/phone/aosp_cf.mk)

PRODUCT_NAME := aegis_qemu_arm64
PRODUCT_DEVICE := aegis_qemu_arm64
PRODUCT_BRAND := AegisOS
PRODUCT_MANUFACTURER := AegisOS
PRODUCT_MODEL := AegisOS QEMU ARM64 Development
