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

#include <dirent.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
//#define LOG_NDEBUG 0

#define LOG_TAG "Universal5410PowerHAL"
#include <utils/Log.h>

#include <hardware/hardware.h>
#include <hardware/power.h>


struct universal5410_power_module {
    struct power_module base;
    pthread_mutex_t lock;
    const char *touchscreen_power_path;
};

static void sysfs_write(const char *path, char *s)
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

static void init_touchscreen_power_path(struct universal5410_power_module *universal5410)
{
    char buf[80];
    const char dir[] = "/sys/devices/platform/s3c2440-i2c.0/i2c-0/0-0020/input/input1";
    const char filename[] = "enabled";
    DIR *d;
    struct dirent *de;
    char *path;
    int pathsize;

    d = opendir(dir);
    if (d == NULL) {
        strerror_r(errno, buf, sizeof(buf));
        ALOGE("Error opening directory %s: %s\n", dir, buf);
        return;
    }
    while ((de = readdir(d)) != NULL) {
        if (strncmp("input", de->d_name, 5) == 0) {
            pathsize = strlen(dir) + strlen(de->d_name) + sizeof(filename) + 2;
            path = malloc(pathsize);
            if (path == NULL) {
                strerror_r(errno, buf, sizeof(buf));
                ALOGE("Out of memory: %s\n", buf);
                return;
            }
            snprintf(path, pathsize, "%s/%s/%s", dir, de->d_name, filename);
            universal5410->touchscreen_power_path = path;
            goto done;
        }
    }
    ALOGE("Error failed to find input dir in %s\n", dir);
done:
    closedir(d);
}

static void power_init(struct power_module *module)
{
    struct universal5410_power_module *universal5410 = (struct universal5410_power_module *) module;
    struct dirent **namelist;
    int n;

    sysfs_write("/sys/devices/system/cpu/cpufreq/iks-cpufreq/min_freq",
                "250000");

    init_touchscreen_power_path(universal5410);
}

static void power_set_interactive(struct power_module *module, int on)
{
    struct universal5410_power_module *universal5410 = (struct universal5410_power_module *) module;
    char buf[80];
    int ret;

    ALOGV("power_set_interactive: %d\n", on);

    /*
     * Lower maximum frequency when screen is off.  CPU 0 and 1 share a
     * cpufreq policy.
     */
    sysfs_write("/sys/devices/system/cpu/cpu0/cpufreq/scaling_max_freq",
                on ? "1600000" : "800000");

    sysfs_write(universal5410->touchscreen_power_path, on ? "Y" : "N");

    ALOGV("power_set_interactive: %d done\n", on);
}

static void universal5410_power_hint(struct power_module *module, power_hint_t hint,
                             void *data)

static struct hw_module_methods_t power_module_methods = {
    .open = NULL,
};

struct universal5410_power_module HAL_MODULE_INFO_SYM = {
    base: {
        common: {
            tag: HARDWARE_MODULE_TAG,
            module_api_version: POWER_MODULE_API_VERSION_0_2,
            hal_api_version: HARDWARE_HAL_API_VERSION,
            id: POWER_HARDWARE_MODULE_ID,
            name: "Universal5410 Power HAL",
            author: "The Android Open Source Project",
            methods: &power_module_methods,
        },

        init: power_init,
        setInteractive: power_set_interactive,
        powerHint: universal5410_power_hint,
    },

    lock: PTHREAD_MUTEX_INITIALIZER,
};

