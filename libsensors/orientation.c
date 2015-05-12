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
#include <stddef.h>
#include <fcntl.h>
#include <errno.h>
#include <math.h>
#include <linux/ioctl.h>
#include <linux/uinput.h>
#include <linux/input.h>

#include <hardware/sensors.h>
#include <hardware/hardware.h>

#define LOG_TAG "smdk4x12_sensors"
#include <utils/Log.h>

#include "smdk4x12_sensors.h"

struct orientation_data {
	struct smdk4x12_sensors_handlers *acceleration_sensor;
	struct smdk4x12_sensors_handlers *magnetic_sensor;

	sensors_vec_t orientation;
	sensors_vec_t acceleration;
	sensors_vec_t magnetic;

	long int delay;
	int uinput_fd;

	pthread_t thread;
	pthread_mutex_t mutex;
	int thread_continue;
};

static float rad2deg(float v)
{
	return (v * 180.0f / 3.1415926535f);
}

static float vector_scalar(sensors_vec_t *v, sensors_vec_t *d)
{
	return v->x * d->x + v->y * d->y + v->z * d->z;
}

static float vector_length(sensors_vec_t *v)
{
	return sqrtf(vector_scalar(v, v));
}

void orientation_calculate(sensors_vec_t *a, sensors_vec_t *m, sensors_vec_t *o)
{
	float azimuth, pitch, roll;
	float la, sinp, cosp, sinr, cosr, x, y;

	if (a == NULL || m == NULL || o == NULL)
		return;

	la = vector_length(a);
	pitch = asinf(-(a->y) / la);
	roll = asinf((a->x) / la);

	sinp = sinf(pitch);
	cosp = cosf(pitch);
	sinr = sinf(roll);
	cosr = cosf(roll);

	y = -(m->x) * cosr + m->z * sinr;
	x = m->x * sinp * sinr + m->y * cosp + m->z * sinp * cosr;
	azimuth = atan2f(y, x);

	o->azimuth = rad2deg(azimuth);
	o->pitch = rad2deg(pitch);
	o->roll = rad2deg(roll);

	if (o->azimuth < 0)
		o->azimuth += 360.0f;
}

void *orientation_thread(void *thread_data)
{
	struct smdk4x12_sensors_handlers *handlers = NULL;
	struct orientation_data *data = NULL;
	struct input_event event;
	struct timeval time;
	long int before, after;
	int diff;
	int uinput_fd;

	if (thread_data == NULL)
		return NULL;

	handlers = (struct smdk4x12_sensors_handlers *) thread_data;
	if (handlers->data == NULL)
		return NULL;

	data = (struct orientation_data *) handlers->data;

	uinput_fd = data->uinput_fd;
	if (uinput_fd < 0)
		return NULL;

	while (data->thread_continue) {
		pthread_mutex_lock(&data->mutex);
		if (!data->thread_continue)
			break;

		while (handlers->activated) {
			gettimeofday(&time, NULL);
			before = timestamp(&time);

			orientation_calculate(&data->acceleration, &data->magnetic, &data->orientation);

			input_event_set(&event, EV_REL, REL_X, (int) (data->orientation.azimuth * 1000));
			write(uinput_fd, &event, sizeof(event));
			input_event_set(&event, EV_REL, REL_Y, (int) (data->orientation.pitch * 1000));
			write(uinput_fd, &event, sizeof(event));
			input_event_set(&event, EV_REL, REL_Z, (int) (data->orientation.roll * 1000));
			write(uinput_fd, &event, sizeof(event));
			input_event_set(&event, EV_SYN, 0, 0);
			write(uinput_fd, &event, sizeof(event));

			gettimeofday(&time, NULL);
			after = timestamp(&time);

			diff = (int) (data->delay - (after - before)) / 1000;
			if (diff <= 0)
				continue;

			usleep(diff);
		}
	}

	return NULL;
}

int orientation_fill(struct smdk4x12_sensors_handlers *handlers,
	sensors_vec_t *acceleration, sensors_vec_t *magnetic)
{
	struct orientation_data *data;

//	ALOGD("%s(%p, %p, %p)", __func__, handlers, acceleration, magnetic);

	if (handlers == NULL || handlers->data == NULL)
		return -EINVAL;

	data = (struct orientation_data *) handlers->data;

	if (acceleration != NULL) {
		data->acceleration.x = acceleration->x;
		data->acceleration.y = acceleration->y;
		data->acceleration.z = acceleration->z;
	}

	if (magnetic != NULL) {
		data->magnetic.x = magnetic->x;
		data->magnetic.y = magnetic->y;
		data->magnetic.z = magnetic->z;
	}

	return 0;
}

int orientation_init(struct smdk4x12_sensors_handlers *handlers,
	struct smdk4x12_sensors_device *device)
{
	struct orientation_data *data = NULL;
	pthread_attr_t thread_attr;
	int uinput_fd = -1;
	int input_fd = -1;
	int rc;
	int i;

	ALOGD("%s(%p, %p)", __func__, handlers, device);

	if (handlers == NULL || device == NULL)
		return -EINVAL;

	data = (struct orientation_data *) calloc(1, sizeof(struct orientation_data));

	for (i = 0; i < device->handlers_count; i++) {
		if (device->handlers[i] == NULL)
			continue;

		if (device->handlers[i]->handle == SENSOR_TYPE_ACCELEROMETER)
			data->acceleration_sensor = device->handlers[i];
		else if (device->handlers[i]->handle == SENSOR_TYPE_MAGNETIC_FIELD)
			data->magnetic_sensor = device->handlers[i];
	}

	if (data->acceleration_sensor == NULL || data->magnetic_sensor == NULL) {
		ALOGE("%s: Missing sensors for orientation", __func__);
		goto error;
	}

	uinput_fd = uinput_rel_create("orientation");
	if (uinput_fd < 0) {
		ALOGD("%s: Unable to create uinput", __func__);
		goto error;
	}

	input_fd = input_open("orientation");
	if (input_fd < 0) {
		ALOGE("%s: Unable to open orientation input", __func__);
		goto error;
	}

	data->thread_continue = 1;

	pthread_mutex_init(&data->mutex, NULL);
	pthread_mutex_lock(&data->mutex);

	pthread_attr_init(&thread_attr);
	pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_DETACHED);

	rc = pthread_create(&data->thread, &thread_attr, orientation_thread, (void *) handlers);
	if (rc < 0) {
		ALOGE("%s: Unable to create orientation thread", __func__);
		pthread_mutex_destroy(&data->mutex);
		goto error;
	}

	data->uinput_fd = uinput_fd;
	handlers->poll_fd = input_fd;
	handlers->data = (void *) data;

	return 0;

error:
	if (data != NULL)
		free(data);

	if (uinput_fd >= 0)
		close(uinput_fd);

	if (input_fd >= 0)
		close(input_fd);

	handlers->poll_fd = -1;
	handlers->data = NULL;

	return -1;
}

int orientation_deinit(struct smdk4x12_sensors_handlers *handlers)
{
	struct orientation_data *data;

	ALOGD("%s(%p)", __func__, handlers);

	if (handlers == NULL || handlers->data == NULL)
		return -EINVAL;

	data = (struct orientation_data *) handlers->data;

	handlers->activated = 0;
	data->thread_continue = 0;
	pthread_mutex_unlock(&data->mutex);

	pthread_mutex_destroy(&data->mutex);

	if (data->uinput_fd >= 0) {
		uinput_destroy(data->uinput_fd);
		close(data->uinput_fd);
	}
	data->uinput_fd = -1;

	if (handlers->poll_fd >= 0)
		close(handlers->poll_fd);
	handlers->poll_fd = -1;

	free(handlers->data);
	handlers->data = NULL;

	return 0;
}

int orientation_activate(struct smdk4x12_sensors_handlers *handlers)
{
	struct orientation_data *data;

	ALOGD("%s(%p)", __func__, handlers);

	if (handlers == NULL || handlers->data == NULL)
		return -EINVAL;

	data = (struct orientation_data *) handlers->data;

	if (data->acceleration_sensor == NULL || data->magnetic_sensor == NULL)
		return -1;

	data->acceleration_sensor->needed |= SMDK4x12_SENSORS_NEEDED_ORIENTATION;
	if (data->acceleration_sensor->needed == SMDK4x12_SENSORS_NEEDED_ORIENTATION)
		data->acceleration_sensor->activate(data->acceleration_sensor);

	data->magnetic_sensor->needed |= SMDK4x12_SENSORS_NEEDED_ORIENTATION;
	if (data->magnetic_sensor->needed == SMDK4x12_SENSORS_NEEDED_ORIENTATION)
		data->magnetic_sensor->activate(data->magnetic_sensor);

	handlers->activated = 1;
	pthread_mutex_unlock(&data->mutex);

	return 0;
}

int orientation_deactivate(struct smdk4x12_sensors_handlers *handlers)
{
	struct orientation_data *data;

	ALOGD("%s(%p)", __func__, handlers);

	if (handlers == NULL || handlers->data == NULL)
		return -EINVAL;

	data = (struct orientation_data *) handlers->data;

	if (data->acceleration_sensor == NULL || data->magnetic_sensor == NULL)
		return -1;

	data->acceleration_sensor->needed &= ~(SMDK4x12_SENSORS_NEEDED_ORIENTATION);
	if (data->acceleration_sensor->needed == 0)
		data->acceleration_sensor->deactivate(data->acceleration_sensor);

	data->magnetic_sensor->needed &= ~(SMDK4x12_SENSORS_NEEDED_ORIENTATION);
	if (data->magnetic_sensor->needed == 0)
		data->magnetic_sensor->deactivate(data->magnetic_sensor);

	handlers->activated = 0;

	return 0;
}

int orientation_set_delay(struct smdk4x12_sensors_handlers *handlers,
	long int delay)
{
	struct orientation_data *data;

	ALOGD("%s(%p)", __func__, handlers);

	if (handlers == NULL || handlers->data == NULL)
		return -EINVAL;

	data = (struct orientation_data *) handlers->data;

	if (data->acceleration_sensor == NULL || data->magnetic_sensor == NULL)
		return -1;

	if (data->acceleration_sensor->needed == SMDK4x12_SENSORS_NEEDED_ORIENTATION)
		data->acceleration_sensor->set_delay(data->acceleration_sensor, delay);

	if (data->magnetic_sensor->needed == SMDK4x12_SENSORS_NEEDED_ORIENTATION)
		data->magnetic_sensor->set_delay(data->magnetic_sensor, delay);

	data->delay = delay;

	return 0;
}

float orientation_convert(int value)
{
	return (float) value / 1000.0f;
}

int orientation_get_data(struct smdk4x12_sensors_handlers *handlers,
	struct sensors_event_t *event)
{
	struct input_event input_event;
	int input_fd = -1;
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

	event->orientation.status = SENSOR_STATUS_ACCURACY_MEDIUM;

	do {
		rc = read(input_fd, &input_event, sizeof(input_event));
		if (rc < (int) sizeof(input_event))
			break;

		if (input_event.type == EV_REL) {
			switch (input_event.code) {
				case REL_X:
					event->orientation.azimuth = orientation_convert(input_event.value);
					break;
				case REL_Y:
					event->orientation.pitch = orientation_convert(input_event.value);
					break;
				case REL_Z:
					event->orientation.roll = orientation_convert(input_event.value);
					break;
				default:
					continue;
			}
		} else if (input_event.type == EV_SYN) {
			if (input_event.code == SYN_REPORT)
				event->timestamp = input_timestamp(&input_event);
		}
	} while (input_event.type != EV_SYN);

	return 0;
}

struct smdk4x12_sensors_handlers orientation = {
	.name = "Orientation",
	.handle = SENSOR_TYPE_ORIENTATION,
	.init = orientation_init,
	.deinit = orientation_deinit,
	.activate = orientation_activate,
	.deactivate = orientation_deactivate,
	.set_delay = orientation_set_delay,
	.get_data = orientation_get_data,
	.activated = 0,
	.needed = 0,
	.poll_fd = -1,
	.data = NULL,
};
