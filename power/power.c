/*
 * Copyright (C) 2016 The CyanogenMod Project <http://www.cyanogenmod.org>
 *           (C) 2017 The LineageOS Project
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
#define MINMAX_CPU_PATH "/sys/power/"

#define US_TO_NS (1000L)

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

static int current_power_profile = -1;
static bool is_low_power = false;
static bool is_vsync_active = false;

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

static int sysfs_write_long(char *path, long value) {
    char buf[80];
    snprintf(buf, 80, "%ld", value);
    return sysfs_write_str(path, buf);
}

#ifdef LOG_NDEBUG
#define WRITE_PEGASUSQ_PARAM(profile, param) do { \
    ALOGV("%s: WRITE_PEGASUSQ_PARAM(profile=%d, param=%s): new val => %d", __func__, profile, #param, profiles[profile].param); \
    sysfs_write_int(PEGASUSQ_PATH #param, profiles[profile].param); \
} while (0)
#define WRITE_LOW_POWER_PARAM(profile, param) do { \
    ALOGV("%s: WRITE_LOW_POWER_PARAM(profile=%d, param=%s): new val => %d", \
            __func__, profile, #param, profiles_low_power[profile].param); \
    sysfs_write_int(PEGASUSQ_PATH #param, profiles_low_power[profile].param); \
} while (0)
#define WRITE_PEGASUSQ_VALUE(param, value) do { \
    ALOGV("%s: WRITE_PEGASUSQ_VALUE(param=%s, value=%d)", __func__, #param, value); \
    sysfs_write_int(PEGASUSQ_PATH #param, value); \
} while (0)
#define WRITE_MINMAX_CPU(param, value) do { \
    ALOGV("%s: WRITE_MINMAX_CPU(param=%s, value=%d)", __func__, #param, value); \
    sysfs_write_int(MINMAX_CPU_PATH #param, value); \
} while(0)
#else
#define WRITE_PEGASUSQ_PARAM(profile, param) sysfs_write_int(PEGASUSQ_PATH #param, profiles[profile].param)
#define WRITE_LOW_POWER_PARAM(profile, param) sysfs_write_int(PEGASUSQ_PATH #param, profiles_low_power[profile].param)
#define WRITE_PEGASUSQ_VALUE(param, value)   sysfs_write_int(PEGASUSQ_PATH #param, value)
#define WRITE_MINMAX_CPU(param, value) sysfs_write_int(MINMAX_CPU_PATH #param, value)
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
    WRITE_MINMAX_CPU(cpufreq_max_limit, profiles[profile].max_freq);
    WRITE_MINMAX_CPU(cpufreq_min_limit, profiles[profile].min_freq);
    WRITE_PEGASUSQ_PARAM(profile, up_threshold);
    WRITE_PEGASUSQ_PARAM(profile, down_differential);
    WRITE_PEGASUSQ_PARAM(profile, min_cpu_lock);
    WRITE_PEGASUSQ_PARAM(profile, max_cpu_lock);
    WRITE_PEGASUSQ_PARAM(profile, cpu_down_rate);
    WRITE_PEGASUSQ_PARAM(profile, sampling_rate);
    WRITE_PEGASUSQ_PARAM(profile, io_is_busy);
    WRITE_PEGASUSQ_PARAM(profile, boost_freq);
    WRITE_PEGASUSQ_PARAM(profile, boost_mincpus);

    current_power_profile = profile;

    ALOGV("%s: %d", __func__, profile);

}

static void boost(long boost_time) {
#ifdef USE_PEGASUSQ_BOOSTING
    if (is_vsync_active || !check_governor()) return;
    if (boost_time == -1) {
        sysfs_write_int(PEGASUSQ_PATH "boost_lock_time", -1);
    } else {
        sysfs_write_long(PEGASUSQ_PATH "boost_lock_time", boost_time);
    }
#endif
}

static void end_boost() {
#ifdef USE_PEGASUSQ_BOOSTING
    if (is_vsync_active || !check_governor()) return;
    sysfs_write_int(PEGASUSQ_PATH "boost_lock_time", 0);
#endif
}

static void set_low_power(bool low_power) {
    if (!is_profile_valid(current_power_profile)) {
        ALOGV("%s: current_power_profile not set yet", __func__);
        return;
    }

    if (!check_governor()) return;

    if (is_low_power == low_power) return;

    if (low_power) {
        end_boost();

        WRITE_LOW_POWER_PARAM(current_power_profile, hotplug_freq_1_1);
        WRITE_LOW_POWER_PARAM(current_power_profile, hotplug_freq_2_0);
        WRITE_LOW_POWER_PARAM(current_power_profile, hotplug_freq_2_1);
        WRITE_LOW_POWER_PARAM(current_power_profile, hotplug_freq_3_0);
        WRITE_LOW_POWER_PARAM(current_power_profile, hotplug_freq_3_1);
        WRITE_LOW_POWER_PARAM(current_power_profile, hotplug_freq_4_0);
        WRITE_LOW_POWER_PARAM(current_power_profile, hotplug_rq_1_1);
        WRITE_LOW_POWER_PARAM(current_power_profile, hotplug_rq_2_0);
        WRITE_LOW_POWER_PARAM(current_power_profile, hotplug_rq_2_1);
        WRITE_LOW_POWER_PARAM(current_power_profile, hotplug_rq_3_0);
        WRITE_LOW_POWER_PARAM(current_power_profile, hotplug_rq_3_1);
        WRITE_LOW_POWER_PARAM(current_power_profile, hotplug_rq_4_0);
        WRITE_MINMAX_CPU(cpufreq_max_limit, profiles_low_power[current_power_profile].max_freq);
        WRITE_MINMAX_CPU(cpufreq_min_limit, profiles_low_power[current_power_profile].min_freq);
        WRITE_LOW_POWER_PARAM(current_power_profile, up_threshold);
        WRITE_LOW_POWER_PARAM(current_power_profile, down_differential);
        WRITE_LOW_POWER_PARAM(current_power_profile, min_cpu_lock);
        WRITE_LOW_POWER_PARAM(current_power_profile, max_cpu_lock);
        WRITE_LOW_POWER_PARAM(current_power_profile, cpu_down_rate);
        WRITE_LOW_POWER_PARAM(current_power_profile, sampling_rate);
        WRITE_LOW_POWER_PARAM(current_power_profile, io_is_busy);
        is_low_power = true;
    } else {
        WRITE_PEGASUSQ_PARAM(current_power_profile, hotplug_freq_1_1);
        WRITE_PEGASUSQ_PARAM(current_power_profile, hotplug_freq_2_0);
        WRITE_PEGASUSQ_PARAM(current_power_profile, hotplug_freq_2_1);
        WRITE_PEGASUSQ_PARAM(current_power_profile, hotplug_freq_3_0);
        WRITE_PEGASUSQ_PARAM(current_power_profile, hotplug_freq_3_1);
        WRITE_PEGASUSQ_PARAM(current_power_profile, hotplug_freq_4_0);
        WRITE_PEGASUSQ_PARAM(current_power_profile, hotplug_rq_1_1);
        WRITE_PEGASUSQ_PARAM(current_power_profile, hotplug_rq_2_0);
        WRITE_PEGASUSQ_PARAM(current_power_profile, hotplug_rq_2_1);
        WRITE_PEGASUSQ_PARAM(current_power_profile, hotplug_rq_3_0);
        WRITE_PEGASUSQ_PARAM(current_power_profile, hotplug_rq_3_1);
        WRITE_PEGASUSQ_PARAM(current_power_profile, hotplug_rq_4_0);
        WRITE_MINMAX_CPU(cpufreq_max_limit, profiles[current_power_profile].max_freq);
        WRITE_MINMAX_CPU(cpufreq_min_limit, profiles[current_power_profile].min_freq);
        WRITE_PEGASUSQ_PARAM(current_power_profile, up_threshold);
        WRITE_PEGASUSQ_PARAM(current_power_profile, down_differential);
        WRITE_PEGASUSQ_PARAM(current_power_profile, min_cpu_lock);
        WRITE_PEGASUSQ_PARAM(current_power_profile, max_cpu_lock);
        WRITE_PEGASUSQ_PARAM(current_power_profile, cpu_down_rate);
        WRITE_PEGASUSQ_PARAM(current_power_profile, sampling_rate);
        WRITE_PEGASUSQ_PARAM(current_power_profile, io_is_busy);
        WRITE_PEGASUSQ_PARAM(current_power_profile, boost_freq);
        WRITE_PEGASUSQ_PARAM(current_power_profile, boost_mincpus);

        is_low_power = false;
    }
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

/*
 * The setInteractive function performs power management actions upon the
 * system entering interactive state (that is, the system is awake and ready
 * for interaction, often with UI devices such as display and touchscreen
 * enabled) or non-interactive state (the system appears asleep, display
 * usually turned off).  The non-interactive state is usually entered after a
 * period of inactivity, in order to conserve battery power during such
 * inactive periods.
 *
 * Typical actions are to turn on or off devices and adjust cpufreq parameters.
 * This function may also call the appropriate interfaces to allow the kernel
 * to suspend the system to low-power sleep state when entering non-interactive
 * state, and to disallow low-power suspend when the system is in interactive
 * state.  When low-power suspend state is allowed, the kernel may suspend the
 * system whenever no wakelocks are held.
 *
 * on is non-zero when the system is transitioning to an interactive / awake
 * state, and zero when transitioning to a non-interactive / asleep state.
 *
 * This function is called to enter non-interactive state after turning off the
 * screen (if present), and called to enter interactive state prior to turning
 * on the screen.
 */
static void power_set_interactive(__attribute__((unused)) struct power_module *module, int on) {
    if (!is_profile_valid(current_power_profile)) {
        ALOGD("%s: no power profile selected", __func__);
        return;
    }

    if (!check_governor()) return;
    ALOGV("%s: setting interactive => %d", __func__, on);
    pthread_mutex_lock(&lock);
    set_low_power(!on);
    pthread_mutex_unlock(&lock);
}

/*
 * The powerHint function is called to pass hints on power requirements, which
 * may result in adjustment of power/performance parameters of the cpufreq
 * governor and other controls.
 *
 * The possible hints are:
 *
 * POWER_HINT_VSYNC
 *
 *     Foreground app has started or stopped requesting a VSYNC pulse
 *     from SurfaceFlinger.  If the app has started requesting VSYNC
 *     then CPU and GPU load is expected soon, and it may be appropriate
 *     to raise speeds of CPU, memory bus, etc.  The data parameter is
 *     non-zero to indicate VSYNC pulse is now requested, or zero for
 *     VSYNC pulse no longer requested.
 *
 * POWER_HINT_INTERACTION
 *
 *     User is interacting with the device, for example, touchscreen
 *     events are incoming.  CPU and GPU load may be expected soon,
 *     and it may be appropriate to raise speeds of CPU, memory bus,
 *     etc.  The data parameter is unused.
 *
 * POWER_HINT_LOW_POWER
 *
 *     Low power mode is activated or deactivated. Low power mode
 *     is intended to save battery at the cost of performance. The data
 *     parameter is non-zero when low power mode is activated, and zero
 *     when deactivated.
 *
 * POWER_HINT_CPU_BOOST
 *
 *     An operation is happening where it would be ideal for the CPU to
 *     be boosted for a specific duration. The data parameter is an
 *     integer value of the boost duration in microseconds.
 */
static void power_hint(__attribute__((unused)) struct power_module *module, power_hint_t hint, void *data) {
    int32_t val;

    if (hint == POWER_HINT_SET_PROFILE) {
        ALOGV("%s: set profile %d", __func__, *(int32_t *)data);
        pthread_mutex_lock(&lock);
        if (is_vsync_active) {
            is_vsync_active = false;
            end_boost();
        }
        set_power_profile(*(int32_t *)data);
        pthread_mutex_unlock(&lock);
    }

    if (current_power_profile == PROFILE_POWER_SAVE) return;

    pthread_mutex_lock(&lock);

    switch (hint) {
        case POWER_HINT_INTERACTION:
            ALOGV("%s: interaction", __func__);
            val = *(int32_t *)data;
            if (val != 0) {
                boost(val * US_TO_NS);
            } else {
                boost(profiles[current_power_profile].interaction_boost_time);
            }
            break;
        case POWER_HINT_LAUNCH:
            ALOGV("%s: launch", __func__);
            boost(profiles[current_power_profile].launch_boost_time);
            break;
/*       case POWER_HINT_VSYNC:
            if (*(int32_t *)data) {
                ALOGV("%s: vsync", __func__);
                boost(-1);
                is_vsync_active = true;
            } else {
                is_vsync_active = false;
                end_boost();
            }
            break;*/
        case POWER_HINT_CPU_BOOST:
            ALOGV("%s: cpu_boost", __func__);
            boost((*(int32_t *)data) * US_TO_NS);
            break;
    }

    pthread_mutex_unlock(&lock);
}

/*
 * (*getFeature) is called to get the current value of a particular
 * feature or capability from the hardware or PowerHAL
 */
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
