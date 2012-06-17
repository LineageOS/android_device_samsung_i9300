#
# Copyright (C) 2012 The CyanogenMod Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

DEVICE_PACKAGE_OVERLAYS += device/samsung/i9300/overlay

$(call inherit-product, $(SRC_TARGET_DIR)/product/languages_full.mk)

# The gps config appropriate for this device
$(call inherit-product, device/common/gps/gps_us_supl.mk)

# This device is xhdpi.  However the platform doesn't
# currently contain all of the bitmaps at xhdpi density so
# we do this little trick to fall back to the hdpi version
# if the xhdpi doesn't exist.
PRODUCT_AAPT_CONFIG := normal hdpi xhdpi
PRODUCT_AAPT_PREF_CONFIG := xhdpi

# Init files
PRODUCT_COPY_FILES := \
	device/samsung/i9300/init.bt.rc:root/init.bt.rc \
	device/samsung/i9300/init.smdk4x12.rc:root/init.smdk4x12.rc \
	device/samsung/i9300/init.smdk4x12.usb.rc:root/init.smdk4x12.usb.rc \
	device/samsung/i9300/lpm.rc:root/lpm.rc \
	device/samsung/i9300/ueventd.smdk4x12.rc:root/ueventd.smdk4x12.rc \
	device/samsung/i9300/ueventd.smdk4x12.rc:recovery/root/ueventd.smdk4x12.rc

# Camera FW
PRODUCT_COPY_FILES += \
	device/samsung/i9300/80cfw:system/etc/init.d/80cfw

# Audio
PRODUCT_COPY_FILES += \
	device/samsung/i9300/configs/tinyucm.conf:system/etc/tinyucm.conf

# Vold and Storage
PRODUCT_COPY_FILES += \
	device/samsung/i9300/configs/vold.fstab:system/etc/vold.fstab

# Bluetooth configuration files
PRODUCT_COPY_FILES += \
	system/bluetooth/data/main.le.conf:system/etc/bluetooth/main.conf

# Wifi
PRODUCT_COPY_FILES += \
	device/samsung/i9300/configs/wpa_supplicant.conf:system/etc/wifi/wpa_supplicant.conf

PRODUCT_PROPERTY_OVERRIDES += \
	wifi.interface=wlan0 \
	wifi.supplicant_scan_interval=15

# Gps
PRODUCT_COPY_FILES += \
	device/samsung/i9300/configs/gps.xml:system/etc/gps.xml

# Packages
PRODUCT_PACKAGES := \
	audio.a2dp.default \
	camera.exynos4 \
    Camera \
    com.android.future.usb.accessory \
    GalaxyS3Settings \
    SamsungServiceMode \
    Torch

# HAL
PRODUCT_PACKAGES += \
	nfc.exynos4 \
	lights.exynos4

#	libgralloc_ump \
#	libhwconverter \
#	libfimg \
#	hwcomposer.exynos4
#	libhwjpeg \
#	libhdmi \
#	libfimc \
#	libcec \
#	libddc \
#	libedid \
#	libhdmiclient \
#	libTVOut
#   libtinyalsa \

# NFC
PRODUCT_PACKAGES += \
	libnfc \
	libnfc_jni \
	Nfc \
	Tag

PRODUCT_COPY_FILES += \
	packages/apps/Nfc/migrate_nfc.txt:system/etc/updatecmds/migrate_nfc.txt \
	device/sample/nxp/com.nxp.mifare.xml:system/etc/permissions/com.nxp.mifare.xml \
	frameworks/base/nfc-extras/com.android.nfc_extras.xml:system/etc/permissions/com.android.nfc_extras.xml \
	frameworks/base/data/etc/android.hardware.nfc.xml:system/etc/permissions/android.hardware.nfc.xml

# NFCEE access control
ifeq ($(TARGET_BUILD_VARIANT),user)
	NFCEE_ACCESS_PATH := device/samsung/i9300/nfcee_access.xml
else
	NFCEE_ACCESS_PATH := device/samsung/i9300/nfcee_access_debug.xml
endif

PRODUCT_COPY_FILES += \
	$(NFCEE_ACCESS_PATH):system/etc/nfcee_access.xml

PRODUCT_PACKAGES += \
	com.android.nfc_extras

# MFC API
PRODUCT_PACKAGES += \
    libsecmfcdecapi \
    libsecmfcencapi

# OMX
PRODUCT_PACKAGES += \
    libstagefrighthw \
    libSEC_OMX_Resourcemanager \
    libSEC_OMX_Core \
    libOMX.SEC.AVC.Decoder \
    libOMX.SEC.M4V.Decoder \
    libOMX.SEC.WMV.Decoder \
    libOMX.SEC.AVC.Encoder \
    libOMX.SEC.M4V.Encoder
#   libOMX.SEC.VP8.Decoder

PRODUCT_COPY_FILES += \
	device/samsung/i9300/configs/media_profiles.xml:system/etc/media_profiles.xml

# RIL
PRODUCT_PROPERTY_OVERRIDES += \
	ro.telephony.ril_class=Smdk4210RIL \
	mobiledata.interfaces=pdp0,wlan0,gprs,ppp0

# Filesystem management tools
PRODUCT_PACKAGES += \
	static_busybox \
	make_ext4fs \
	setup_fs

# Live Wallpapers
PRODUCT_PACKAGES += \
	Galaxy4 \
	HoloSpiralWallpaper \
	LiveWallpapers \
	LiveWallpapersPicker \
	MagicSmokeWallpapers \
	NoiseField \
	PhaseBeam \
	VisualizationWallpapers \
	librs_jni

# These are the hardware-specific features
PRODUCT_COPY_FILES += \
	frameworks/base/data/etc/android.hardware.audio.low_latency.xml:system/etc/permissions/android.hardware.audio.low_latency.xml \
	frameworks/base/data/etc/android.hardware.camera.autofocus.xml:system/etc/permissions/android.hardware.camera.autofocus.xml \
	frameworks/base/data/etc/android.hardware.camera.flash-autofocus.xml:system/etc/permissions/android.hardware.camera.flash-autofocus.xml \
	frameworks/base/data/etc/android.hardware.camera.front.xml:system/etc/permissions/android.hardware.camera.front.xml \
	frameworks/base/data/etc/android.hardware.camera.xml:system/etc/permissions/android.hardware.camera.xml \
	frameworks/base/data/etc/android.hardware.location.gps.xml:system/etc/permissions/android.hardware.location.gps.xml \
	frameworks/base/data/etc/android.hardware.location.xml:system/etc/permissions/android.hardware.location.xml \
	frameworks/base/data/etc/android.hardware.sensor.accelerometer.xml:system/etc/permissions/android.hardware.sensor.accelerometer.xml \
	frameworks/base/data/etc/android.hardware.sensor.compass.xml:system/etc/permissions/android.hardware.sensor.compass.xml \
	frameworks/base/data/etc/android.hardware.sensor.gyroscope.xml:system/etc/permissions/android.hardware.sensor.gyroscope.xml \
	frameworks/base/data/etc/android.hardware.sensor.light.xml:system/etc/permissions/android.hardware.sensor.light.xml \
	frameworks/base/data/etc/android.hardware.sensor.proximity.xml:system/etc/permissions/android.hardware.sensor.proximity.xml \
	frameworks/base/data/etc/android.hardware.telephony.gsm.xml:system/etc/permissions/android.hardware.telephony.gsm.xml \
	frameworks/base/data/etc/android.hardware.touchscreen.multitouch.jazzhand.xml:system/etc/permissions/android.hardware.touchscreen.multitouch.jazzhand.xml \
	frameworks/base/data/etc/android.hardware.touchscreen.multitouch.xml:system/etc/permissions/android.hardware.touchscreen.multitouch.xml \
	frameworks/base/data/etc/android.hardware.touchscreen.xml:system/etc/permissions/android.hardware.touchscreen.xml \
	frameworks/base/data/etc/android.hardware.usb.accessory.xml:system/etc/permissions/android.hardware.usb.accessory.xml \
	frameworks/base/data/etc/android.hardware.usb.host.xml:system/etc/permissions/android.hardware.usb.host.xml \
	frameworks/base/data/etc/android.hardware.wifi.direct.xml:system/etc/permissions/android.hardware.wifi.direct.xml \
	frameworks/base/data/etc/android.hardware.wifi.xml:system/etc/permissions/android.hardware.wifi.xml \
	frameworks/base/data/etc/android.software.sip.voip.xml:system/etc/permissions/android.software.sip.voip.xml \
	frameworks/base/data/etc/android.software.sip.xml:system/etc/permissions/android.software.sip.xml \
	frameworks/base/data/etc/handheld_core_hardware.xml:system/etc/permissions/handheld_core_hardware.xml

# Feature live wallpaper
PRODUCT_COPY_FILES += \
	packages/wallpapers/LivePicker/android.software.live_wallpaper.xml:system/etc/permissions/android.software.live_wallpaper.xml

PRODUCT_PROPERTY_OVERRIDES += \
	ro.opengles.version=131072 \
	hwui.render_dirty_regions=false

PRODUCT_TAGS += dalvik.gc.type-precise

# Set default USB interface
PRODUCT_DEFAULT_PROPERTY_OVERRIDES += \
	persist.sys.usb.config=mtp

$(call inherit-product, frameworks/base/build/phone-xhdpi-1024-dalvik-heap.mk)

# Include exynos4 platform specific parts
TARGET_HAL_PATH := hardware/samsung/exynos4/hal
TARGET_OMX_PATH := hardware/samsung/exynos/multimedia/openmax
$(call inherit-product, hardware/samsung/exynos4x12.mk)
$(call inherit-product, vendor/cm/config/nfc_enhanced.mk)
