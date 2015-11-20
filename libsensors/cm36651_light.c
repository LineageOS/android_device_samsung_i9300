/*
 * Copyright (C) 2013 Paul Kocialkowski <contact@paulk.fr>
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
#include <math.h>
#include <sys/types.h>
#include <linux/ioctl.h>
#include <linux/input.h>

#include <hardware/sensors.h>
#include <hardware/hardware.h>

#define LOG_TAG "smdk4x12_sensors"
#include <utils/Log.h>

#include "smdk4x12_sensors.h"

struct cm36651_light_data {
	char path_enable[PATH_MAX];
	char path_delay[PATH_MAX];
};

int cm36651_light_init(struct smdk4x12_sensors_handlers *handlers,
	struct smdk4x12_sensors_device *device)
{
	struct cm36651_light_data *data = NULL;
	char path[PATH_MAX] = { 0 };
	int input_fd = -1;
	int rc;

	ALOGD("%s(%p, %p)", __func__, handlers, device);

	if (handlers == NULL)
		return -EINVAL;

	data = (struct cm36651_light_data *) calloc(1, sizeof(struct cm36651_light_data));

	input_fd = input_open("light_sensor");
	if (input_fd < 0) {
		ALOGE("%s: Unable to open input", __func__);
		goto error;
	}

	rc = sysfs_path_prefix("light_sensor", (char *) &path);
	if (rc < 0 || path[0] == '\0') {
		ALOGE("%s: Unable to open sysfs", __func__);
		goto error;
	}

	snprintf(data->path_enable, PATH_MAX, "%s/enable", path);
	snprintf(data->path_delay, PATH_MAX, "%s/poll_delay", path);

	handlers->poll_fd = input_fd;
	handlers->data = (void *) data;

	return 0;

error:
	if (data != NULL)
		free(data);

	if (input_fd >= 0)
		close(input_fd);

	handlers->poll_fd = -1;
	handlers->data = NULL;

	return -1;
}

int cm36651_light_deinit(struct smdk4x12_sensors_handlers *handlers)
{
	ALOGD("%s(%p)", __func__, handlers);

	if (handlers == NULL)
		return -EINVAL;

	if (handlers->poll_fd >= 0)
		close(handlers->poll_fd);
	handlers->poll_fd = -1;

	if (handlers->data != NULL)
		free(handlers->data);
	handlers->data = NULL;

	return 0;
}

int cm36651_light_activate(struct smdk4x12_sensors_handlers *handlers)
{
	struct cm36651_light_data *data;
	int rc;

	ALOGD("%s(%p)", __func__, handlers);

	if (handlers == NULL || handlers->data == NULL)
		return -EINVAL;

	data = (struct cm36651_light_data *) handlers->data;

	rc = sysfs_value_write(data->path_enable, 1);
	if (rc < 0) {
		ALOGE("%s: Unable to write sysfs value", __func__);
		return -1;
	}

	handlers->activated = 1;

	return 0;
}

int cm36651_light_deactivate(struct smdk4x12_sensors_handlers *handlers)
{
	struct cm36651_light_data *data;
	int rc;

	ALOGD("%s(%p)", __func__, handlers);

	if (handlers == NULL || handlers->data == NULL)
		return -EINVAL;

	data = (struct cm36651_light_data *) handlers->data;

	rc = sysfs_value_write(data->path_enable, 0);
	if (rc < 0) {
		ALOGE("%s: Unable to write sysfs value", __func__);
		return -1;
	}

	handlers->activated = 1;

	return 0;
}

int cm36651_light_set_delay(struct smdk4x12_sensors_handlers *handlers, long int delay)
{
	struct cm36651_light_data *data;
	int rc;

	ALOGD("%s(%p, %ld)", __func__, handlers, delay);

	if (handlers == NULL || handlers->data == NULL)
		return -EINVAL;

	data = (struct cm36651_light_data *) handlers->data;

	rc = sysfs_value_write(data->path_delay, (int) delay);
	if (rc < 0) {
		ALOGE("%s: Unable to write sysfs value", __func__);
		return -1;
	}

	return 0;
}

float cm36651_light_convert(int white, int green)
{
	float gwrel = 1.0f;
	float aux;
	float r1, r2, r3, r4;

	if (green <= 4)
		return 0.0f;
	else {
		if (white > 0)
			gwrel = (float) green / (float) white;

		r1 = floorf( (float) (pow((double) green, 1.3341) * 0.0258) );

		aux = floorf( ((float) green * 0.18f * 9.44f) / gwrel);
		r2 = aux;
		r3 = aux * 0.77f;

		r4 = floorf( (float) green * ( (gwrel * 1.546) - 0.46) );

		if (gwrel <= 0.5f) {
			return r1;
		} else if (gwrel >= 0.9f) {
			if (white <= 5999)
				return r2;
			else
				return r3;
		} else {
			return r4;
		}
	}
}

int cm36651_light_get_data(struct smdk4x12_sensors_handlers *handlers,
	struct sensors_event_t *event)
{
	struct input_event input_event;
	int input_fd;
	int green = 0;
	int white = 0;
	int rc;

//	ALOGD("%s(%p, %p)", __func__, handlers, event);

	if (handlers == NULL || event == NULL)
		return -EINVAL;

	input_fd = handlers->poll_fd;
	if (input_fd < 0)
		return -EINVAL;

	memset(event, 0, sizeof(struct sensors_event_t));
	event->version = sizeof(struct sensors_event_t);
	event->sensor = handlers->handle;
	event->type = handlers->handle;

	do {
		rc = read(input_fd, &input_event, sizeof(input_event));
		if (rc < (int) sizeof(input_event))
			break;

		if (input_event.type == EV_REL) {
			if (input_event.code == REL_Y)
				green = input_event.value;
			if (input_event.code == REL_MISC)
				white = input_event.value;
		} else if (input_event.type == EV_SYN) {
			if (input_event.code == SYN_REPORT)
				event->timestamp = input_timestamp(&input_event);
		}
	} while (input_event.type != EV_SYN);

	event->light = cm36651_light_convert(white, green);

	return 0;
}

struct smdk4x12_sensors_handlers cm36651_light = {
	.name = "CM36651 Light",
	.handle = SENSOR_TYPE_LIGHT,
	.init = cm36651_light_init,
	.deinit = cm36651_light_deinit,
	.activate = cm36651_light_activate,
	.deactivate = cm36651_light_deactivate,
	.set_delay = cm36651_light_set_delay,
	.get_data = cm36651_light_get_data,
	.activated = 0,
	.needed = 0,
	.poll_fd = -1,
	.data = NULL,
};
