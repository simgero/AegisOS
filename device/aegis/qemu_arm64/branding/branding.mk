# Visual identity only; platform/API versions and package identities stay truthful.
PRODUCT_COPY_FILES += \
    device/aegis/qemu_arm64/branding/bootanimation.zip:$(TARGET_COPY_OUT_PRODUCT)/media/bootanimation.zip \
    device/aegis/qemu_arm64/branding/bootanimation.zip:$(TARGET_COPY_OUT_PRODUCT)/media/bootanimation-dark.zip \
    device/aegis/qemu_arm64/branding/shutdownanimation.zip:$(TARGET_COPY_OUT_PRODUCT)/media/shutdownanimation.zip \
    device/aegis/qemu_arm64/branding/bootanimation.zip:$(TARGET_COPY_OUT_PRODUCT)/media/userspace-reboot.zip
