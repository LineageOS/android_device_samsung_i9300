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

#define MS_TO_NS (1000000L)

enum {
    PROFILE_POWER_SAVE = 0,
    PROFILE_BALANCED,
    PROFILE_PERFORMANCE,
    PROFILE_MAX
};

typedef struct governor_settings {
    // freq values for core up/down
    int hotplug_freq_1_1;
    int hotplug_freq_2_0;
    int hotplug_freq_2_1;
    int hotplug_freq_3_0;
    int hotplug_freq_3_1;
    int hotplug_freq_4_0;
    // rq sizes for up/down
    int hotplug_rq_1_1;
    int hotplug_rq_2_0;
    int hotplug_rq_2_1;
    int hotplug_rq_3_0;
    int hotplug_rq_3_1;
    int hotplug_rq_4_0;
    // max/min freqs (-1 for default)
    int max_freq;
    int min_freq;
    // load at which to start scaling up
    int up_threshold;
    // higher down_differential == slower downscaling
    int down_differential;
    // min/max num of cpus to have online
    int min_cpu_lock;
    int max_cpu_lock;
    // wait sampling_rate * cpu_down_rate us before trying to downscale
    int cpu_down_rate;
    int sampling_rate; // in microseconds
    int io_is_busy;
    // boosting
    int boost_freq;
    int boost_mincpus;
    long interaction_boost_time;
    long launch_boost_time;
} power_profile;

static power_profile profiles[PROFILE_MAX] = {
    [PROFILE_POWER_SAVE] = {
        .hotplug_freq_1_1 = 800000,
        .hotplug_freq_2_0 = 700000,
        .hotplug_freq_2_1 = 1000000,
        .hotplug_freq_3_0 = 900000,
        .hotplug_freq_3_1 = 1200000,
        .hotplug_freq_4_0 = 1100000,
        .hotplug_rq_1_1 = 200,
        .hotplug_rq_2_0 = 200,
        .hotplug_rq_2_1 = 300,
        .hotplug_rq_3_0 = 300,
        .hotplug_rq_3_1 = 400,
        .hotplug_rq_4_0 = 400,
        .max_freq = 1000000,
        .min_freq = -1,
        .up_threshold = 95,
        .down_differential = 2,
        .min_cpu_lock = 0,
        .max_cpu_lock = 3,
        .cpu_down_rate = 5,
        .sampling_rate = 200000,
        .io_is_busy = 0,
        // nb: boosting power hints are ignored for PROFILE_POWER_SAVE
        .boost_freq = 0,
        .boost_mincpus = 0,
        .interaction_boost_time = 0,
        .launch_boost_time = 0,
    },
    [PROFILE_BALANCED] = {
        .hotplug_freq_1_1 = 500000,
        .hotplug_freq_2_0 = 500000,
        .hotplug_freq_2_1 = 700000,
        .hotplug_freq_3_0 = 700000,
        .hotplug_freq_3_1 = 900000,
        .hotplug_freq_4_0 = 900000,
        .hotplug_rq_1_1 = 150,
        .hotplug_rq_2_0 = 150,
        .hotplug_rq_2_1 = 250,
        .hotplug_rq_3_0 = 250,
        .hotplug_rq_3_1 = 350,
        .hotplug_rq_4_0 = 350,
        .max_freq = -1,
        .min_freq = -1,
        .up_threshold = 90,
        .down_differential = 3,
        .min_cpu_lock = 0,
        .max_cpu_lock = 0,
        .cpu_down_rate = 10,
        .sampling_rate = 200000,
        .io_is_busy = 0,
        .boost_freq = 700000,
        .boost_mincpus = 0,
        .interaction_boost_time = 60 * (MS_TO_NS),
        .launch_boost_time = 2000 * (MS_TO_NS),
    },
    [PROFILE_PERFORMANCE] = {
        .hotplug_freq_1_1 = 500000,
        .hotplug_freq_2_0 = 200000,
        .hotplug_freq_2_1 = 500000,
        .hotplug_freq_3_0 = 200000,
        .hotplug_freq_3_1 = 700000,
        .hotplug_freq_4_0 = 200000,
        .hotplug_rq_1_1 = 150,
        .hotplug_rq_2_0 = 100,
        .hotplug_rq_2_1 = 200,
        .hotplug_rq_3_0 = 150,
        .hotplug_rq_3_1 = 250,
        .hotplug_rq_4_0 = 200,
        .min_freq = -1,
        .max_freq = -1,
        .up_threshold = 80,
        .down_differential = 5,
        .min_cpu_lock = 0,
        .max_cpu_lock = 0,
        .cpu_down_rate = 20,
        .sampling_rate = 200000,
        .io_is_busy = 1,
        .boost_freq = 900000,
        .boost_mincpus = 2,
        .interaction_boost_time = 90 * (MS_TO_NS),
        .launch_boost_time = 3000 * (MS_TO_NS),
    },
};

// for non-interactive profiles we don't need to worry about
// boosting as it (should) only occur while the screen is on
static power_profile profiles_low_power[PROFILE_MAX] = {
    [PROFILE_POWER_SAVE] = {
        .hotplug_freq_1_1 = 800000,
        .hotplug_freq_2_0 = 800000,
        .hotplug_freq_2_1 = 900000,
        .hotplug_freq_3_0 = 900000,
        .hotplug_freq_3_1 = 1200000,
        .hotplug_freq_4_0 = 1100000,
        .hotplug_rq_1_1 = 200,
        .hotplug_rq_2_0 = 200,
        .hotplug_rq_2_1 = 300,
        .hotplug_rq_3_0 = 300,
        .hotplug_rq_3_1 = 400,
        .hotplug_rq_4_0 = 400,
        .max_freq = 900000,
        .min_freq = -1,
        .up_threshold = 95,
        .down_differential = 1,
        .min_cpu_lock = 0,
        .max_cpu_lock = 2,
        .cpu_down_rate = 5,
        .sampling_rate = 200000,
        .io_is_busy = 0,
        .boost_freq = 0,
        .boost_mincpus = 0,
        .interaction_boost_time = 0,
        .launch_boost_time = 0,
    },
    [PROFILE_BALANCED] = {
        .hotplug_freq_1_1 = 700000,
        .hotplug_freq_2_0 = 700000,
        .hotplug_freq_2_1 = 900000,
        .hotplug_freq_3_0 = 900000,
        .hotplug_freq_3_1 = 1100000,
        .hotplug_freq_4_0 = 1100000,
        .hotplug_rq_1_1 = 150,
        .hotplug_rq_2_0 = 150,
        .hotplug_rq_2_1 = 250,
        .hotplug_rq_3_0 = 250,
        .hotplug_rq_3_1 = 350,
        .hotplug_rq_4_0 = 350,
        .max_freq = 1200000,
        .min_freq = -1,
        .up_threshold = 90,
        .down_differential = 2,
        .min_cpu_lock = 0,
        .max_cpu_lock = 0,
        .cpu_down_rate = 8,
        .sampling_rate = 200000,
        .io_is_busy = 0,
        .boost_freq = 0,
        .boost_mincpus = 0,
        .interaction_boost_time = 0,
        .launch_boost_time = 0,
    },
    [PROFILE_PERFORMANCE] = {
        .hotplug_freq_1_1 = 800000,
        .hotplug_freq_2_0 = 500000,
        .hotplug_freq_2_1 = 800000,
        .hotplug_freq_3_0 = 500000,
        .hotplug_freq_3_1 = 1000000,
        .hotplug_freq_4_0 = 700000,
        .hotplug_rq_1_1 = 150,
        .hotplug_rq_2_0 = 100,
        .hotplug_rq_2_1 = 200,
        .hotplug_rq_3_0 = 150,
        .hotplug_rq_3_1 = 250,
        .hotplug_rq_4_0 = 200,
        .min_freq = -1,
        .max_freq = -1,
        .up_threshold = 85,
        .down_differential = 5,
        .min_cpu_lock = 0,
        .max_cpu_lock = 0,
        .cpu_down_rate = 15,
        .sampling_rate = 200000,
        .io_is_busy = 1,
        .boost_freq = 0,
        .boost_mincpus = 0,
        .interaction_boost_time = 0,
        .launch_boost_time = 0,
    },
};

