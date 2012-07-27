/*
 * Copyright (C) 2012 The Android Open Source Project
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
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define LOG_TAG "smdk4x12 PowerHAL"
#include <utils/Log.h>

#include <hardware/hardware.h>
#include <hardware/power.h>

static void sysfs_write(char *path, char *s)
{
    char buf[80];
    int len;
    int fd = open(path, O_WRONLY);

    if (fd < 0) {
        strerror_r(errno, buf, sizeof(buf));
        ALOGE("Error opening %s: %s\n", path, buf);
        return;
    }

    len = write(fd, s, strlen(s));
    if (len < 0) {
        strerror_r(errno, buf, sizeof(buf));
        ALOGE("Error writing to %s: %s\n", path, buf);
    }

    close(fd);
}

static char *sysfs_read(char *path)
{
    FILE *fp = fopen(path, "r");
    int len;
    char *string = (char *) malloc(80 * sizeof(char));

    if(fp == NULL) {
      ALOGE("Error opening %s", string);
      return NULL;
    }

    while((len = fgetc(fp)) != EOF)
      sprintf(string + strlen(string), "%c", len);

    fclose(fp);

    return string;
}

static void power_init(struct power_module *module)
{
    // we have actually pre-initialized values in kernel, these definitions are for reference
    //sysfs_write("/sys/devices/system/cpu/cpu0/cpufreq/scaling_max_freq", "1400000");
    //sysfs_write("/sys/devices/system/cpu/cpufreq/pegasusq/sampling_rate_min", "10000");
}

static void power_set_interactive(struct power_module *module, int on)
{
    if(on != 0) // the documntation says if on is non-zero, it's interactive
    {
      if(strcmp(sysfs_read("/sys/devices/system/cpu/cpu0/cpufreq/scaling_max_freq"), "1400000") != 0)
        return;
      else
        sysfs_write("/sys/devices/system/cpu/cpu0/cpufreq/scaling_max_freq", "1400000");
    } else // if device is going to sleep
    {
      if(strcmp(sysfs_read("/sys/devices/system/cpu/cpu0/cpufreq/scaling_max_freq"), "1400000") != 0)
        return;
      else
        sysfs_write("/sys/devices/system/cpu/cpu0/cpufreq/scaling_max_freq", "500000");
    }
    char *governor = sysfs_read("/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor");

    if(strcmp(governor, "pegasusq") == 0) {
      // can i use sgs2 tweaks?
    } else if(strcmp(governor, "interactive") == 0) {
      // has to search tweaks yet
    }
}

static void power_hint(struct power_module *module, power_hint_t hint,
                       void *data) {
    switch (hint) {
    case POWER_HINT_VSYNC:
        break;

    default:
        break;
    }
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
        .name = "Power HAL for the smdk4x12 board",
        .author = "The CyanogenMod Project",
        .methods = &power_module_methods,
    },

    .init = power_init,
    .setInteractive = power_set_interactive,
    .powerHint = power_hint,
};
