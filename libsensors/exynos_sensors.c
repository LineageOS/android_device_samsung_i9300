/*
 * Copyright (C) 2013 Paul Kocialkowski
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <fcntl.h>
#include <errno.h>
#include <poll.h>



#include <sys/select.h>



#include <hardware/sensors.h>
#include <hardware/hardware.h>

#define LOG_TAG "exynos_sensors"
#include <utils/Log.h>

#include "exynos_sensors.h"

/*
 * Sensors list
 */

struct sensor_t exynos_sensors[] = {
	{ "LSM330DLC Acceleration Sensor", "STMicroelectronics", 1, SENSOR_TYPE_ACCELEROMETER,
		SENSOR_TYPE_ACCELEROMETER, 19.61f, 0.0096f, 0.23f, 10000, 0, 0, {}, },
	{ "AKM8975 Magnetic Sensor", "Asahi Kasei", 1, SENSOR_TYPE_MAGNETIC_FIELD,
		SENSOR_TYPE_MAGNETIC_FIELD, 2000.0f, 0.06f, 6.8f, 10000, 0, 0, {}, },
	{ "Orientation Sensor", "Exynos Sensors", 1, SENSOR_TYPE_ORIENTATION,
		SENSOR_TYPE_ORIENTATION, 360.0f, 0.1f, 0.0f, 10000, 0, 0, {}, },
	{ "CM36651 Light Sensor", "Capella", 1, SENSOR_TYPE_LIGHT,
		SENSOR_TYPE_LIGHT, 121240.0f, 1.0f, 0.2f, 0, 0, 0, {}, },
	{ "CM36651 Proximity Sensor", "Capella", 1, SENSOR_TYPE_PROXIMITY,
		SENSOR_TYPE_PROXIMITY, 8.0f, 8.0f, 1.3f, 0, 0, 0, {}, },
	{ "LSM330DLC Gyroscope Sensor", "STMicroelectronics", 1, SENSOR_TYPE_GYROSCOPE,
		SENSOR_TYPE_GYROSCOPE, 8.73f, 0.00031f, 6.1f, 5000, 0, 0, {}, },
	{ "LPS331AP Pressure Sensor", "STMicroelectronics", 1, SENSOR_TYPE_PRESSURE,
		SENSOR_TYPE_PRESSURE, 1260.0f, 0.00024f, 0.045f, 40000, 0, 0, {}, },
};

int exynos_sensors_count = sizeof(exynos_sensors) / sizeof(struct sensor_t);

struct exynos_sensors_handlers *exynos_sensors_handlers[] = {
	&lsm330dlc_acceleration,
	&akm8975,
	&orientation,
	&cm36651_proximity,
	&cm36651_light,
	&lsm330dlc_gyroscope,
	&lps331ap,
};

int exynos_sensors_handlers_count = sizeof(exynos_sensors_handlers) /
	sizeof(struct exynos_sensors_handlers *);

/*
 * Exynos Sensors
 */

int exynos_sensors_activate(struct sensors_poll_device_t *dev, int handle, int enabled)
{
	struct exynos_sensors_device *device;
	int i;

	ALOGD("%s(%p, %d, %d)", __func__, dev, handle, enabled);

	if (dev == NULL)
		return -EINVAL;

	device = (struct exynos_sensors_device *) dev;

	if (device->handlers == NULL || device->handlers_count <= 0)
		return -EINVAL;

	for (i = 0; i < device->handlers_count; i++) {
		if (device->handlers[i] == NULL)
			continue;

		if (device->handlers[i]->handle == handle) {
			if (enabled && device->handlers[i]->activate != NULL) {
				device->handlers[i]->needed |= EXYNOS_SENSORS_NEEDED_API;
				if (device->handlers[i]->needed == EXYNOS_SENSORS_NEEDED_API)
					return device->handlers[i]->activate(device->handlers[i]);
				else
					return 0;
			} else if (!enabled && device->handlers[i]->deactivate != NULL) {
				device->handlers[i]->needed &= ~EXYNOS_SENSORS_NEEDED_API;
				if (device->handlers[i]->needed == 0)
					return device->handlers[i]->deactivate(device->handlers[i]);
				else
					return 0;
			}
		}
	}

	return -1;
}

int exynos_sensors_set_delay(struct sensors_poll_device_t *dev, int handle, int64_t ns)
{
	struct exynos_sensors_device *device;
	int i;

	ALOGD("%s(%p, %d, %ld)", __func__, dev, handle, (long int) ns);

	if (dev == NULL)
		return -EINVAL;

	device = (struct exynos_sensors_device *) dev;

	if (device->handlers == NULL || device->handlers_count <= 0)
		return -EINVAL;

	for (i = 0; i < device->handlers_count; i++) {
		if (device->handlers[i] == NULL)
			continue;

		if (device->handlers[i]->handle == handle && device->handlers[i]->set_delay != NULL)
			return device->handlers[i]->set_delay(device->handlers[i], (long int) ns);
	}

	return 0;
}

int exynos_sensors_poll(struct sensors_poll_device_t *dev,
	struct sensors_event_t* data, int count)
{
	struct exynos_sensors_device *device;
	int i, j;
	int c, n;
	int poll_rc, rc;

//	ALOGD("%s(%p, %p, %d)", __func__, dev, data, count);

	if (dev == NULL)
		return -EINVAL;

	device = (struct exynos_sensors_device *) dev;

	if (device->handlers == NULL || device->handlers_count <= 0 ||
		device->poll_fds == NULL || device->poll_fds_count <= 0)
		return -EINVAL;

	n = 0;

	do {
		poll_rc = poll(device->poll_fds, device->poll_fds_count, n > 0 ? 0 : -1);
		if (poll_rc < 0)
			return -1;

		for (i = 0; i < device->poll_fds_count; i++) {
			if (!(device->poll_fds[i].revents & POLLIN))
				continue;

			for (j = 0; j < device->handlers_count; j++) {
				if (device->handlers[j] == NULL || device->handlers[j]->poll_fd != device->poll_fds[i].fd || device->handlers[j]->get_data == NULL)
					continue;

				rc = device->handlers[j]->get_data(device->handlers[j], &data[n]);
				if (rc < 0) {
					device->poll_fds[i].revents = 0;
					poll_rc = -1;
				} else {
					n++;
					count--;
				}
			}
		}
	} while ((poll_rc > 0 || n < 1) && count > 0);

	return n;
}

/*
 * Interface
 */

int exynos_sensors_close(hw_device_t *device)
{
	struct exynos_sensors_device *exynos_sensors_device;
	int i;

	ALOGD("%s(%p)", __func__, device);

	if (device == NULL)
		return -EINVAL;

	exynos_sensors_device = (struct exynos_sensors_device *) device;

	if (exynos_sensors_device->poll_fds != NULL)
		free(exynos_sensors_device->poll_fds);

	for (i = 0; i < exynos_sensors_device->handlers_count; i++) {
		if (exynos_sensors_device->handlers[i] == NULL || exynos_sensors_device->handlers[i]->deinit == NULL)
			continue;

		exynos_sensors_device->handlers[i]->deinit(exynos_sensors_device->handlers[i]);
	}

	free(device);

	return 0;
}

int exynos_sensors_open(const struct hw_module_t* module, const char *id,
	struct hw_device_t** device)
{
	struct exynos_sensors_device *exynos_sensors_device;
	int p, i;

	ALOGD("%s(%p, %s, %p)", __func__, module, id, device);

	if (module == NULL || device == NULL)
		return -EINVAL;

	exynos_sensors_device = (struct exynos_sensors_device *)
		calloc(1, sizeof(struct exynos_sensors_device));
	exynos_sensors_device->device.common.tag = HARDWARE_DEVICE_TAG;
	exynos_sensors_device->device.common.version = 0;
	exynos_sensors_device->device.common.module = (struct hw_module_t *) module;
	exynos_sensors_device->device.common.close = exynos_sensors_close;
	exynos_sensors_device->device.activate = exynos_sensors_activate;
	exynos_sensors_device->device.setDelay = exynos_sensors_set_delay;
	exynos_sensors_device->device.poll = exynos_sensors_poll;
	exynos_sensors_device->handlers = exynos_sensors_handlers;
	exynos_sensors_device->handlers_count = exynos_sensors_handlers_count;
	exynos_sensors_device->poll_fds = (struct pollfd *)
		calloc(1, exynos_sensors_handlers_count * sizeof(struct pollfd));

	p = 0;
	for (i = 0; i < exynos_sensors_handlers_count; i++) {
		if (exynos_sensors_handlers[i] == NULL || exynos_sensors_handlers[i]->init == NULL)
			continue;

		exynos_sensors_handlers[i]->init(exynos_sensors_handlers[i], exynos_sensors_device);
		if (exynos_sensors_handlers[i]->poll_fd >= 0) {
			exynos_sensors_device->poll_fds[p].fd = exynos_sensors_handlers[i]->poll_fd;
			exynos_sensors_device->poll_fds[p].events = POLLIN;
			p++;
		}
	}

	exynos_sensors_device->poll_fds_count = p;

	*device = &(exynos_sensors_device->device.common);

	return 0;
}

int exynos_sensors_get_sensors_list(struct sensors_module_t* module,
	const struct sensor_t **sensors_p)
{
	ALOGD("%s(%p, %p)", __func__, module, sensors_p);

	if (sensors_p == NULL)
		return -EINVAL;

	*sensors_p = exynos_sensors;
	return exynos_sensors_count;
}

struct hw_module_methods_t exynos_sensors_module_methods = {
	.open = exynos_sensors_open,
};

struct sensors_module_t HAL_MODULE_INFO_SYM = {
	.common = {
		.tag = HARDWARE_MODULE_TAG,
		.version_major = 1,
		.version_minor = 0,
		.id = SENSORS_HARDWARE_MODULE_ID,
		.name = "Exynos Sensors",
		.author = "Paul Kocialkowski",
		.methods = &exynos_sensors_module_methods,
	},
	.get_sensors_list = exynos_sensors_get_sensors_list,
};
