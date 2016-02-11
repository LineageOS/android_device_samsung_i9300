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
#define LOG_TAG "PegasusPowerHAL"

#include <hardware/hardware.h>
#include <hardware/power.h>

#include <stdbool.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

//#define LOG_NDEBUG 0
#include <utils/Log.h>

#include "power.h"

#define PEGASUSQ_PATH "/sys/devices/system/cpu/cpufreq/pegasusq/"

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

static int current_power_profile = -1;

static int sysfs_write_str(char *path, char *s) {
	char buf[80];
	int len;
	int ret = 0;
	int fd;

	fd = open(path, O_WRONLY);
	if (fd < 0) {
		strerror_r(errno, buf, sizeof(buf));
		ALOGE("Error opening %s: %s\n", path, buf);
		return -1 ;
	}

	len = write(fd, s, strlen(s));
	if (len < 0) {
		strerror_r(errno, buf, sizeof(buf));
		ALOGE("Error writing to %s: %s\n", path, buf);
		ret = -1;
	}

	close(fd);

	return ret;
}

static int sysfs_write_int(char *path, int value) {
	char buf[80];
	snprintf(buf, 80, "%d", value);
	return sysfs_write_str(path, buf);
}
#ifdef LOG_NDEBUG
#define WRITE_PEGASUSQ_PARAM(profile, param) do {  \
	ALOGV("%s: WRITE_PEGASUSQ_PARAM(profile=%d, param=%s): new val => %d", __func__, profile, #param, profiles[profile].param); \
	sysfs_write_int(PEGASUSQ_PATH #param, profiles[profile].param); \
} while (0)
#define WRITE_PEGASUSQ_VALUE(param, value) do {  \
	ALOGV("%s: WRITE_PEGASUSQ_VALUE(param=%s, value=%d)", __func__, #param, value); \
	sysfs_write_int(PEGASUSQ_PATH #param, value); \
} while (0)
#else
#define WRITE_PEGASUSQ_PARAM(profile, param) sysfs_write_int(PEGASUSQ_PATH #param, profiles[profile].param)
#define WRITE_PEGASUSQ_VALUE(param, value)   sysfs_write_int(PEGASUSQ_PATH #param, value)
#endif
static bool check_governor() {
	struct stat s;
	int err = stat(PEGASUSQ_PATH, &s);
	if (err != 0) return false;
	return S_ISDIR(s.st_mode);
}

static bool is_profile_valid(int profile) {
	return profile >= 0 && profile < PROFILE_MAX;
}

static void set_power_profile(int profile) {
	if (!is_profile_valid(profile)) {
		ALOGE("%s: unknown profile: %d", __func__, profile);
		return;
	}

	if (!check_governor()) return;

	if (profile == current_power_profile) return;

	WRITE_PEGASUSQ_PARAM(profile, hotplug_freq_1_1);
	WRITE_PEGASUSQ_PARAM(profile, hotplug_freq_2_0);
	WRITE_PEGASUSQ_PARAM(profile, hotplug_freq_2_1);
	WRITE_PEGASUSQ_PARAM(profile, hotplug_freq_3_0);
	WRITE_PEGASUSQ_PARAM(profile, hotplug_freq_3_1);
	WRITE_PEGASUSQ_PARAM(profile, hotplug_freq_4_0);
	WRITE_PEGASUSQ_PARAM(profile, hotplug_rq_1_1);
	WRITE_PEGASUSQ_PARAM(profile, hotplug_rq_2_0);
	WRITE_PEGASUSQ_PARAM(profile, hotplug_rq_2_1);
	WRITE_PEGASUSQ_PARAM(profile, hotplug_rq_3_0);
	WRITE_PEGASUSQ_PARAM(profile, hotplug_rq_3_1);
	WRITE_PEGASUSQ_PARAM(profile, hotplug_rq_4_0);
	WRITE_PEGASUSQ_PARAM(profile, up_threshold);
	WRITE_PEGASUSQ_PARAM(profile, down_differential);
	WRITE_PEGASUSQ_PARAM(profile, min_cpu_lock);
	WRITE_PEGASUSQ_PARAM(profile, max_cpu_lock);
	WRITE_PEGASUSQ_PARAM(profile, cpu_down_rate);
	WRITE_PEGASUSQ_PARAM(profile, sampling_rate);
	WRITE_PEGASUSQ_PARAM(profile, io_is_busy);

	current_power_profile = profile;

	ALOGV("%s: %d", __func__, profile);

}


/*
 * (*init)() performs power management setup actions at runtime
 * startup, such as to set default cpufreq parameters.  This is
 * called only by the Power HAL instance loaded by
 * PowerManagerService.
 */
static void power_init(__attribute__((unused)) struct power_module *module) {
	set_power_profile(PROFILE_BALANCED);
	ALOGV("%s", __func__);
}

static void power_set_interactive(__attribute__((unused)) struct power_module *module, int on) {
	if (!is_profile_valid(current_power_profile)) {
		ALOGD("%s: no power profile selected", __func__);
		return;
	}

	if (!check_governor()) return;
	ALOGV("%s: setting interactive => %d", __func__, on);
	// TODO - for now we'll just set max cores to two
	if (on) {
		WRITE_PEGASUSQ_VALUE(max_cpu_lock, 0);
	} else {
		WRITE_PEGASUSQ_VALUE(max_cpu_lock, 2);
	}
}

static void power_hint(__attribute__((unused)) struct power_module *module, power_hint_t hint, void *data) {
	switch (hint) {
		case POWER_HINT_SET_PROFILE:
			ALOGV("%s: set profile %d", __func__, *(int32_t *)data);
			pthread_mutex_lock(&lock);
			set_power_profile(*(int32_t *)data);
			pthread_mutex_unlock(&lock);
			break;
		default:
			break;
	}
}

static int power_get_feature(__attribute__((unused)) struct power_module *module, feature_t feature) {
	ALOGV("%s: %d", __func__, feature);
	if (feature == POWER_FEATURE_SUPPORTED_PROFILES) {
		return PROFILE_MAX;
	}
	ALOGV("%s: unknown feature %d", __func__, feature);
	return -1;
}

static struct hw_module_methods_t power_module_methods = {
	.open = NULL,
};

struct power_module HAL_MODULE_INFO_SYM = {
	.common = {
		.tag = HARDWARE_MODULE_TAG,
		.module_api_version = POWER_MODULE_API_VERSION_0_2,
		.hal_api_version = HARDWARE_HAL_API_VERSION,
		.id = POWER_HARDWARE_MODULE_ID,
		.name = "smdk4x12 Power HAL",
		.author = "The CyanogenMod Project",
		.methods = &power_module_methods,
	},

	.init = power_init,
	.setInteractive = power_set_interactive,
	.powerHint = power_hint,
	.getFeature = power_get_feature
};
