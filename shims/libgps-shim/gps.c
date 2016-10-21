/*
 * Copyright (C) 2016 The CyanogenMod Project <http://www.cyanogenmod.org>
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
#define LOG_TAG "libgps-shim"

#include <utils/Log.h>
#include <hardware/gps.h>

#include <dlfcn.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "gps.h"
#define REAL_GPS_PATH "/system/lib/hw/gps.exynos4.vendor.so"

const GpsInterface* (*vendor_get_gps_interface)(struct gps_device_t* dev);
const void* (*vendor_get_extension)(const char* name);
void (*vendor_set_ref_location)(const AGpsRefLocationNoLTE *agps_reflocation, size_t sz_struct);

void shim_set_ref_location(const AGpsRefLocation *agps_reflocation, size_t sz_struct) {
	// this is mildly ugly
	AGpsRefLocationNoLTE vendor_ref;
	if (sizeof(AGpsRefLocationNoLTE) < sz_struct) {
		ALOGE("%s: AGpsRefLocation is too small, bailing out!", __func__);
		return;
	}
	ALOGE("%s: shimming AGpsRefLocation", __func__);
	// the two structs are identical, so this is ok
	memcpy(&vendor_ref, agps_reflocation, sizeof(AGpsRefLocationNoLTE));
	vendor_set_ref_location(&vendor_ref, sizeof(AGpsRefLocationNoLTE));
	// copy it back
	memcpy(agps_reflocation, &vendor_ref, sizeof(AGpsRefLocationNoLTE));
}

const void* shim_get_extension(const char* name) {
	ALOGE("%s(%s)", __func__, name);
	if (strcmp(name, AGPS_RIL_INTERFACE) == 0) {
		// RIL interface
		AGpsRilInterface *ril = (AGpsRilInterface*)vendor_get_extension(name);
		// now we shim the ref_location callback
		ALOGE("%s: shimming RIL ref_location callback", __func__);
		vendor_set_ref_location = ril->set_ref_location;
		ril->set_ref_location = shim_set_ref_location;
		return ril;
	} else {
		return vendor_get_extension(name);
	}
}

const GpsInterface* shim_get_gps_interface(struct gps_device_t* dev) {
	ALOGE("%s: shimming vendor get_extension", __func__);
	GpsInterface *halInterface = vendor_get_gps_interface(dev);

	vendor_get_extension = halInterface->get_extension;
	halInterface->get_extension = &shim_get_extension;

	return halInterface;
}

static int open_gps(const struct hw_module_t* module, char const* name,
		struct hw_device_t** device) {
	void *realGpsLib;
	int gpsHalResult;
	struct hw_module_t *realHalSym;

	struct gps_device_t **gps = (struct gps_device_t **)device;

	realGpsLib = dlopen(REAL_GPS_PATH, RTLD_LOCAL);
	if (!realGpsLib) {
		ALOGE("Failed to load real GPS HAL '" REAL_GPS_PATH "': %s", dlerror());
		return -EINVAL;
	}

	realHalSym = (struct hw_module_t*)dlsym(realGpsLib, HAL_MODULE_INFO_SYM_AS_STR);
	if (!realHalSym) {
		ALOGE("Failed to locate the GPS HAL module sym: '" HAL_MODULE_INFO_SYM_AS_STR "': %s", dlerror());
		goto dl_err;
	}

	int result = realHalSym->methods->open(realHalSym, name, device);
	if (result < 0) {
		ALOGE("Real GPS open method failed: %d", result);
		goto dl_err;
	}
	ALOGE("Successfully loaded real GPS hal, shimming get_gps_interface...");
	// now, we shim hw_device_t
	vendor_get_gps_interface = (*gps)->get_gps_interface;
	(*gps)->get_gps_interface = &shim_get_gps_interface;

	return 0;
dl_err:
	dlclose(realGpsLib);
	return -EINVAL;
}

static struct hw_module_methods_t gps_module_methods = {
	.open = open_gps
};

struct hw_module_t HAL_MODULE_INFO_SYM = {
	.tag = HARDWARE_MODULE_TAG,
	.module_api_version = 1,
	.hal_api_version = 0,
	.id = GPS_HARDWARE_MODULE_ID,
	.name = "BCM451x GPS shim",
	.author = "The CyanogenMod Project",
	.methods = &gps_module_methods
};
