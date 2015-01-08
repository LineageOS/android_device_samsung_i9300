/*
 * Copyright (C) 2012-2013, The CyanogenMod Project
 * Copyright (C) 2015 The OmniROM Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_NDEBUG 1

#define LOG_PARAMETERS 0
#define LOG_TAG "CameraWrapper"

#include <cutils/log.h>
#include <utils/threads.h>
#include <utils/String8.h>
#include <hardware/hardware.h>
#include <hardware/camera.h>
#include <camera/Camera.h>
#include <camera/CameraParameters.h>

static android::Mutex gCameraWrapperLock;
static camera_module_t *gVendorModule = 0;

bool preview_running = false;

static int camera_device_open(const hw_module_t *module, const char *name,
                              hw_device_t **device);
static int camera_device_close(hw_device_t *device);
static int camera_get_number_of_cameras(void);
static int camera_get_camera_info(int camera_id, struct camera_info *info);

static struct hw_module_methods_t camera_module_methods = {
    .open = camera_device_open
};

camera_module_t HAL_MODULE_INFO_SYM = {
    .common = {
        .tag = HARDWARE_MODULE_TAG,
        .module_api_version = CAMERA_MODULE_API_VERSION_1_0,
        .hal_api_version = HARDWARE_HAL_API_VERSION,
        .id = CAMERA_HARDWARE_MODULE_ID,
        .name = "i9300 Camera Wrapper",
        .author = "The OmniROM Project",
        .methods = &camera_module_methods,
        .dso = NULL, /* fix compilation warnings */
        .reserved = { 0 }, /* fix compilation warnings */
    },
    .get_number_of_cameras = camera_get_number_of_cameras,
    .get_camera_info = camera_get_camera_info,
    .set_callbacks = NULL, /* fix compilation warnings, unused in API V1 */
    .get_vendor_tag_ops = NULL, /* fix compilation warnings, unused in API V1 */
    .open_legacy = NULL, /* fix compilation warnings, unused in API V1 */
    .reserved = { 0 }, /* fix compilation warnings */
};

typedef struct wrapper_camera_device {
    camera_device_t base;
    int id;
    camera_device_t *vendor;
} wrapper_camera_device_t;

#define VENDOR_CALL(device, func, ...) ({ \
    wrapper_camera_device_t *__wrapper_dev = (wrapper_camera_device_t*) device; \
    __wrapper_dev->vendor->ops->func(__wrapper_dev->vendor, ##__VA_ARGS__); \
})

#define CAMERA_ID(device) (((wrapper_camera_device_t *)(device))->id)

static int check_vendor_module()
{
    int rv = 0;
    ALOGV("%s", __FUNCTION__);

    if (gVendorModule) {
        ALOGV("already got vendor camera module");
        return 0;
    }

    rv = hw_get_module("vendor-camera", (const hw_module_t **) &gVendorModule);

    if (rv)
        ALOGE("%s: failed to open vendor camera module", __FUNCTION__);

    return rv;
}

const static char *iso_values[] = {"auto,ISO100,ISO200,ISO400,ISO800", "auto"};

static char *camera_fixup_getparams(int id, const char *settings)
{
    android::CameraParameters params;
    params.unflatten(android::String8(settings));

    // fix params here
    params.set(android::CameraParameters::KEY_SUPPORTED_ISO_MODES,
               iso_values[id]);
    params.set(android::CameraParameters::KEY_VIDEO_SNAPSHOT_SUPPORTED, "true");

    android::String8 strParams = params.flatten();
    char *ret = strdup(strParams.string());

    ALOGV("%s: get parameters fixed up", __FUNCTION__);
    return ret;
}

char *camera_fixup_setparams(int id, const char *settings)
{
    android::CameraParameters params;
    params.unflatten(android::String8(settings));

    // fix params here
    if (params.get("iso")) {
        const char *isoMode = params.get(android::CameraParameters::KEY_ISO_MODE);

        if (strcmp(isoMode, "ISO100") == 0)
            params.set(android::CameraParameters::KEY_ISO_MODE, "100");
        else if (strcmp(isoMode, "ISO200") == 0)
            params.set(android::CameraParameters::KEY_ISO_MODE, "200");
        else if (strcmp(isoMode, "ISO400") == 0)
            params.set(android::CameraParameters::KEY_ISO_MODE, "400");
        else if (strcmp(isoMode, "ISO800") == 0)
            params.set(android::CameraParameters::KEY_ISO_MODE, "800");
    }

    params.set(android::CameraParameters::KEY_CITYID, 0);

    android::String8 strParams = params.flatten();
    char *ret = strdup(strParams.string());

    ALOGV("%s: set parameters fixed up", __FUNCTION__);
    return ret;
}

/*******************************************************************
 * implementation of camera_device_ops functions
 *******************************************************************/

int camera_set_preview_window(struct camera_device *device,
                              struct preview_stream_ops *window)
{
    ALOGV("%s->%08X->%08X", __FUNCTION__, (uintptr_t)device,
          (uintptr_t)(((wrapper_camera_device_t *)device)->vendor));

    if (!device)
        return -EINVAL;

    return VENDOR_CALL(device, set_preview_window, window);
}

void camera_set_callbacks(struct camera_device *device,
        camera_notify_callback notify_cb,
        camera_data_callback data_cb,
        camera_data_timestamp_callback data_cb_timestamp,
        camera_request_memory get_memory,
        void *user)
{
	ALOGV("%s->%08X->%08X", __FUNCTION__, (uintptr_t)device,
          (uintptr_t)(((wrapper_camera_device_t *)device)->vendor));

    if (!device)
        return;

    VENDOR_CALL(device, set_callbacks, notify_cb, data_cb, data_cb_timestamp,
                get_memory, user);
}

void camera_enable_msg_type(struct camera_device *device, int32_t msg_type)
{
	ALOGV("%s->%08X->%08X", __FUNCTION__, (uintptr_t)device,
          (uintptr_t)(((wrapper_camera_device_t *)device)->vendor));

    if (!device)
        return;

    VENDOR_CALL(device, enable_msg_type, msg_type);
}

void camera_disable_msg_type(struct camera_device *device, int32_t msg_type)
{
	ALOGV("%s->%08X->%08X", __FUNCTION__, (uintptr_t)device,
          (uintptr_t)(((wrapper_camera_device_t *)device)->vendor));

    if (!device)
        return;

    VENDOR_CALL(device, disable_msg_type, msg_type);
}

int camera_msg_type_enabled(struct camera_device *device, int32_t msg_type)
{
    ALOGV("%s->%08X->%08X", __FUNCTION__, (uintptr_t)device,
          (uintptr_t)(((wrapper_camera_device_t *)device)->vendor));

    if (!device)
        return 0;

    return VENDOR_CALL(device, msg_type_enabled, msg_type);
}

int camera_start_preview(struct camera_device *device)
{
    int rc;

    ALOGV("%s->%08X->%08X", __FUNCTION__, (uintptr_t)device,
          (uintptr_t)(((wrapper_camera_device_t *)device)->vendor));

    if (!device)
        return -EINVAL;

    rc = VENDOR_CALL(device, start_preview);
    if (rc)
        preview_running = true;

    return rc;
}

void camera_stop_preview(struct camera_device *device)
{
    ALOGV("%s->%08X->%08X", __FUNCTION__, (uintptr_t)device,
          (uintptr_t)(((wrapper_camera_device_t *)device)->vendor));

    if (!device)
        return;

    // Workaround for camera freezes
    VENDOR_CALL(device, send_command, 7, 0, 0);

    VENDOR_CALL(device, stop_preview);

    preview_running = false;
}

int camera_preview_enabled(struct camera_device *device)
{
    ALOGV("%s->%08X->%08X", __FUNCTION__, (uintptr_t)device,
          (uintptr_t)(((wrapper_camera_device_t *)device)->vendor));

    if (!device)
        return -EINVAL;

    return VENDOR_CALL(device, preview_enabled);
}

int camera_store_meta_data_in_buffers(struct camera_device *device, int enable)
{
    ALOGV("%s->%08X->%08X", __FUNCTION__, (uintptr_t)device,
          (uintptr_t)(((wrapper_camera_device_t *)device)->vendor));

    if (!device)
        return -EINVAL;

    return VENDOR_CALL(device, store_meta_data_in_buffers, enable);
}

int camera_start_recording(struct camera_device *device)
{
    ALOGV("%s->%08X->%08X", __FUNCTION__, (uintptr_t)device,
          (uintptr_t)(((wrapper_camera_device_t *)device)->vendor));

    if (!device)
        return EINVAL;

    return VENDOR_CALL(device, start_recording);
}

void camera_stop_recording(struct camera_device *device)
{
    ALOGV("%s->%08X->%08X", __FUNCTION__, (uintptr_t)device,
          (uintptr_t)(((wrapper_camera_device_t *)device)->vendor));

    if (!device)
        return;

    VENDOR_CALL(device, stop_recording);
}

int camera_recording_enabled(struct camera_device *device)
{
    ALOGV("%s->%08X->%08X", __FUNCTION__, (uintptr_t)device,
          (uintptr_t)(((wrapper_camera_device_t *)device)->vendor));

    if (!device)
        return -EINVAL;

    return VENDOR_CALL(device, recording_enabled);
}

void camera_release_recording_frame(struct camera_device *device,
                                    const void *opaque)
{
    ALOGV("%s->%08X->%08X", __FUNCTION__, (uintptr_t)device,
          (uintptr_t)(((wrapper_camera_device_t *)device)->vendor));

    if (!device)
        return;

    VENDOR_CALL(device, release_recording_frame, opaque);
}

int camera_auto_focus(struct camera_device *device)
{
    ALOGV("%s->%08X->%08X", __FUNCTION__, (uintptr_t)device,
          (uintptr_t)(((wrapper_camera_device_t *)device)->vendor));

    if (!device)
        return -EINVAL;

    return VENDOR_CALL(device, auto_focus);
}

int camera_cancel_auto_focus(struct camera_device *device)
{
    ALOGV("%s->%08X->%08X", __FUNCTION__, (uintptr_t)device,
          (uintptr_t)(((wrapper_camera_device_t *)device)->vendor));

    if (!device)
        return -EINVAL;

    if (preview_running)
        return VENDOR_CALL(device, cancel_auto_focus);

    return 0;
}

int camera_take_picture(struct camera_device *device)
{
    ALOGV("%s->%08X->%08X", __FUNCTION__, (uintptr_t)device,
          (uintptr_t)(((wrapper_camera_device_t *)device)->vendor));

    if (!device)
        return -EINVAL;

    return VENDOR_CALL(device, take_picture);
}

int camera_cancel_picture(struct camera_device *device)
{
    ALOGV("%s->%08X->%08X", __FUNCTION__, (uintptr_t)device,
          (uintptr_t)(((wrapper_camera_device_t *)device)->vendor));

    if (!device)
        return -EINVAL;

    return VENDOR_CALL(device, cancel_picture);
}

int camera_set_parameters(struct camera_device *device, const char *params)
{
    ALOGV("%s->%08X->%08X", __FUNCTION__, (uintptr_t)device,
          (uintptr_t)(((wrapper_camera_device_t *)device)->vendor));

    if (!device)
        return -EINVAL;

#if LOG_PARAMETERS
    ALOGV("Before fixup:");
    __android_log_write(ANDROID_LOG_VERBOSE, LOG_TAG, params);
#endif

    char *tmp = NULL;
    tmp = camera_fixup_setparams(CAMERA_ID(device), params);

#if LOG_PARAMETERS
    ALOGV("After fixup:");
    __android_log_write(ANDROID_LOG_VERBOSE, LOG_TAG, tmp);
#endif

    int ret = VENDOR_CALL(device, set_parameters, tmp);
    return ret;
}

char* camera_get_parameters(struct camera_device *device)
{
    ALOGV("%s->%08X->%08X", __FUNCTION__, (uintptr_t)device,
          (uintptr_t)(((wrapper_camera_device_t *)device)->vendor));

    if (!device)
        return NULL;

    char *params = VENDOR_CALL(device, get_parameters);

#if LOG_PARAMETERS
    ALOGV("Before fixup:");
    __android_log_write(ANDROID_LOG_VERBOSE, LOG_TAG, params);
#endif

    char *tmp = camera_fixup_getparams(CAMERA_ID(device), params);
    VENDOR_CALL(device, put_parameters, params);
    params = tmp;

#if LOG_PARAMETERS
    ALOGV("After fixup:");
    __android_log_write(ANDROID_LOG_VERBOSE, LOG_TAG, tmp);
#endif

    return params;
}

static void camera_put_parameters(struct camera_device *device, char *params)
{
    ALOGV("%s->%08X->%08X", __FUNCTION__, (uintptr_t)device,
          (uintptr_t)(((wrapper_camera_device_t *)device)->vendor));

    if (params)
        free(params);
}

int camera_send_command(struct camera_device *device, int32_t cmd, int32_t arg1, int32_t arg2)
{
    ALOGV("%s->%08X->%08X", __FUNCTION__, (uintptr_t)device,
          (uintptr_t)(((wrapper_camera_device_t *)device)->vendor));

    if (!device)
        return -EINVAL;

    return VENDOR_CALL(device, send_command, cmd, arg1, arg2);
}

void camera_release(struct camera_device *device)
{
    ALOGV("%s->%08X->%08X", __FUNCTION__, (uintptr_t)device,
          (uintptr_t)(((wrapper_camera_device_t *)device)->vendor));

    if (!device)
        return;

    VENDOR_CALL(device, release);
}

int camera_dump(struct camera_device *device, int fd)
{
    if (!device)
        return -EINVAL;

    return VENDOR_CALL(device, dump, fd);
}

#ifdef HEAPTRACKER
extern "C" void heaptracker_free_leaked_memory(void);
#endif

int camera_device_close(hw_device_t *device)
{
    int ret = 0;
    wrapper_camera_device_t *wrapper_dev = NULL;

    ALOGV("%s", __FUNCTION__);

    android::Mutex::Autolock lock(gCameraWrapperLock);

    if (!device) {
        ret = -EINVAL;
        goto done;
    }

    wrapper_dev = (wrapper_camera_device_t *) device;

    ALOGI("closing camera device with id %d", wrapper_dev->id);

    wrapper_dev->vendor->common.close((hw_device_t *) wrapper_dev->vendor);

    if (wrapper_dev->base.ops)
        free(wrapper_dev->base.ops);

    free(wrapper_dev);

done:
#ifdef HEAPTRACKER
    heaptracker_free_leaked_memory();
#endif

    ALOGI("camera device closed");

    return ret;
}

/*******************************************************************
 * implementation of camera_module functions
 *******************************************************************/

/* open device handle to one of the cameras
 *
 * assume camera service will keep singleton of each camera
 * so this function will always only be called once per camera instance
 */

int camera_device_open(const hw_module_t *module, const char *name,
                       hw_device_t **device)
{
    int rv = 0;
    int num_cameras = 0;
    int cameraid;
    wrapper_camera_device_t *camera_device = NULL;
    camera_device_ops_t *camera_ops = NULL;

    android::Mutex::Autolock lock(gCameraWrapperLock);

    ALOGV("%s", __FUNCTION__);

    if (name != NULL) {
        if (check_vendor_module())
            return -EINVAL;

        cameraid = atoi(name);
        num_cameras = gVendorModule->get_number_of_cameras();

        if (cameraid > num_cameras) {
            ALOGE("%s: camera service provided out of bounds camera id (id = %d, num supported = %d)",
                    __FUNCTION__, cameraid, num_cameras);
            rv = -EINVAL;
            goto fail;
        }

        camera_device = (wrapper_camera_device_t *) malloc(sizeof(*camera_device));
        if (!camera_device) {
            ALOGE("%s: camera_device allocation fail", __FUNCTION__);
            rv = -ENOMEM;
            goto fail;
        }

        memset(camera_device, 0, sizeof(*camera_device));
        camera_device->id = cameraid;

        rv = gVendorModule->common.methods->open(
                (const hw_module_t *) gVendorModule, name,
                (hw_device_t **) &(camera_device->vendor));
        if (rv) {
            ALOGE("%s: vendor camera open fail", __FUNCTION__);
            goto fail;
        }

        ALOGV("%s: got vendor camera device 0x%08X",
                __FUNCTION__, (uintptr_t) (camera_device->vendor));

        camera_ops = (camera_device_ops_t *) malloc(sizeof(*camera_ops));
        if (!camera_ops) {
            ALOGE("%s: camera_ops allocation fail", __FUNCTION__);
            rv = -ENOMEM;
            goto fail;
        }

        memset(camera_ops, 0, sizeof(*camera_ops));

        camera_device->base.common.tag = HARDWARE_DEVICE_TAG;
        camera_device->base.common.version = CAMERA_DEVICE_API_VERSION_1_0;
        camera_device->base.common.module = (hw_module_t *) module;
        camera_device->base.common.close = camera_device_close;
        camera_device->base.ops = camera_ops;

        camera_ops->set_preview_window = camera_set_preview_window;
        camera_ops->set_callbacks = camera_set_callbacks;
        camera_ops->enable_msg_type = camera_enable_msg_type;
        camera_ops->disable_msg_type = camera_disable_msg_type;
        camera_ops->msg_type_enabled = camera_msg_type_enabled;
        camera_ops->start_preview = camera_start_preview;
        camera_ops->stop_preview = camera_stop_preview;
        camera_ops->preview_enabled = camera_preview_enabled;
        camera_ops->store_meta_data_in_buffers = camera_store_meta_data_in_buffers;
        camera_ops->start_recording = camera_start_recording;
        camera_ops->stop_recording = camera_stop_recording;
        camera_ops->recording_enabled = camera_recording_enabled;
        camera_ops->release_recording_frame = camera_release_recording_frame;
        camera_ops->auto_focus = camera_auto_focus;
        camera_ops->cancel_auto_focus = camera_cancel_auto_focus;
        camera_ops->take_picture = camera_take_picture;
        camera_ops->cancel_picture = camera_cancel_picture;
        camera_ops->set_parameters = camera_set_parameters;
        camera_ops->get_parameters = camera_get_parameters;
        camera_ops->put_parameters = camera_put_parameters;
        camera_ops->send_command = camera_send_command;
        camera_ops->release = camera_release;
        camera_ops->dump = camera_dump;

        *device = &camera_device->base.common;
    }

    ALOGI("camera device with id %d opened", camera_device->id);

    return rv;

fail:
    if (camera_device) {
        free(camera_device);
        camera_device = NULL;
    }

    if (camera_ops) {
        free(camera_ops);
        camera_ops = NULL;
    }

    *device = NULL;

    return rv;
}

int camera_get_number_of_cameras(void)
{
    ALOGV("%s", __FUNCTION__);

    if (check_vendor_module())
        return 0;

    return gVendorModule->get_number_of_cameras();
}

int camera_get_camera_info(int camera_id, struct camera_info *info)
{
    ALOGV("%s", __FUNCTION__);

    if (check_vendor_module())
        return 0;

    return gVendorModule->get_camera_info(camera_id, info);
}
