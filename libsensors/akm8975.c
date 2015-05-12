/*
 * Copyright (C) 2013 Paul Kocialkowski <contact@paulk.fr>
 * Copyright (C) 2012 Asahi Kasei Microdevices Corporation, Japan
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

#include <hardware/sensors.h>
#include <hardware/hardware.h>

#define LOG_TAG "smdk4x12_sensors"
#include <utils/Log.h>

#include "smdk4x12_sensors.h"
#include "ak8975.h"
#include "ak8975-reg.h"

#include <AKFS_Compass.h>
#include <AKFS_FileIO.h>

#define AKFS_CONFIG_PATH	"/data/misc/akfs.txt"
#define AKFS_PAT		PAT3

struct akm8975_data {
	struct smdk4x12_sensors_handlers *orientation_sensor;

	AK8975PRMS akfs_params;
	sensors_vec_t magnetic;

	long int delay;
	int device_fd;
	int uinput_fd;

	pthread_t thread;
	pthread_mutex_t mutex;
	int thread_continue;
};

int akfs_get_magnetic_field(struct akm8975_data *akm8975_data, short *magnetic_data)
{
	AK8975PRMS *params;
	int rc;

	if (akm8975_data == NULL || magnetic_data == NULL)
		return -EINVAL;

	params = &akm8975_data->akfs_params;

	// Decomposition
	// Sensitivity adjustment, i.e. multiply ASA, is done in this function.
	rc = AKFS_DecompAK8975(magnetic_data, 1, &params->mi_asa, AKFS_HDATA_SIZE, params->mfv_hdata);
	if (rc == AKFS_ERROR) {
		ALOGE("Failed to decomp!");
		return -1;
	}

	// Adjust coordination
	rc = AKFS_Rotate(params->m_hpat, &params->mfv_hdata[0]);
	if (rc == AKFS_ERROR) {
		ALOGE("Failed to rotate!");
		return -1;
	}

	// AOC for magnetometer
	// Offset estimation is done in this function
	AKFS_AOC(&params->m_aocv, params->mfv_hdata, &params->mfv_ho);

	// Subtract offset
	// Then, a magnetic vector, the unit is uT, is stored in mfv_hvbuf.
	rc = AKFS_VbNorm(AKFS_HDATA_SIZE, params->mfv_hdata, 1,
		&params->mfv_ho, &params->mfv_hs, AK8975_HSENSE_TARGET,
		AKFS_HDATA_SIZE, params->mfv_hvbuf);
	if (rc == AKFS_ERROR) {
		ALOGE("Failed to normalize!");
		return -1;
	}

	// Averaging
	rc = AKFS_VbAve(AKFS_HDATA_SIZE, params->mfv_hvbuf, CSPEC_HNAVE_V, &params->mfv_hvec);
	if (rc == AKFS_ERROR) {
		ALOGE("Failed to average!");
		return -1;
	}

	akm8975_data->magnetic.x = params->mfv_hvec.u.x;
	akm8975_data->magnetic.y = params->mfv_hvec.u.y;
	akm8975_data->magnetic.z = params->mfv_hvec.u.z;

	return 0;
}

int akfs_init(struct akm8975_data *akm8975_data, char *asa, AKFS_PATNO pat)
{
	AK8975PRMS *params;

	if (akm8975_data == NULL || asa == NULL)
		return -EINVAL;

	params = &akm8975_data->akfs_params;

	memset(params, 0, sizeof(AK8975PRMS));

	// Sensitivity
	params->mfv_hs.u.x = AK8975_HSENSE_DEFAULT;
	params->mfv_hs.u.y = AK8975_HSENSE_DEFAULT;
	params->mfv_hs.u.z = AK8975_HSENSE_DEFAULT;
	params->mfv_as.u.x = AK8975_ASENSE_DEFAULT;
	params->mfv_as.u.y = AK8975_ASENSE_DEFAULT;
	params->mfv_as.u.z = AK8975_ASENSE_DEFAULT;

	// Initialize variables that initial value is not 0.
	params->mi_hnaveV = CSPEC_HNAVE_V;
	params->mi_hnaveD = CSPEC_HNAVE_D;
	params->mi_anaveV = CSPEC_ANAVE_V;
	params->mi_anaveD = CSPEC_ANAVE_D;

	// Copy ASA values
	params->mi_asa.u.x = asa[0];
	params->mi_asa.u.y = asa[1];
	params->mi_asa.u.z = asa[2];

	// Copy layout pattern
	params->m_hpat = pat;

	return 0;
}

void *akm8975_thread(void *thread_data)
{
	struct smdk4x12_sensors_handlers *handlers = NULL;
	struct akm8975_data *data = NULL;
	struct input_event event;
	struct timeval time;
	char i2c_data[SENSOR_DATA_SIZE] = { 0 };
	short magnetic_data[3];
	short mode;
	long int before, after;
	int diff;
	int device_fd;
	int uinput_fd;
	int rc;

	if (thread_data == NULL)
		return NULL;

	handlers = (struct smdk4x12_sensors_handlers *) thread_data;
	if (handlers->data == NULL)
		return NULL;

	data = (struct akm8975_data *) handlers->data;

	device_fd = data->device_fd;
	if (device_fd < 0)
		return NULL;

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

			mode = AK8975_MODE_SNG_MEASURE;
			rc = ioctl(device_fd, ECS_IOCTL_SET_MODE, &mode);
			if (rc < 0) {
				ALOGE("%s: Unable to set akm8975 mode", __func__);
				return NULL;
			}

			memset(&i2c_data, 0, sizeof(i2c_data));
			rc = ioctl(device_fd, ECS_IOCTL_GETDATA, &i2c_data);
			if (rc < 0) {
				ALOGE("%s: Unable to get akm8975 data", __func__);
				return NULL;
			}

			if (!(i2c_data[0] & 0x01)) {
				ALOGE("%s: akm8975 data is not ready", __func__);
				continue;
			}

			if (i2c_data[7] & (1 << 2) || i2c_data[7] & (1 << 3)) {
				ALOGE("%s: akm8975 data read error or overflow", __func__);
				continue;
			}

			magnetic_data[0] = (short) (i2c_data[2] << 8) | (i2c_data[1]);
			magnetic_data[1] = (short) (i2c_data[4] << 8) | (i2c_data[3]);
			magnetic_data[2] = (short) (i2c_data[6] << 8) | (i2c_data[5]);

			rc = akfs_get_magnetic_field(data, (short *) &magnetic_data);
			if (rc < 0) {
				ALOGE("%s: Unable to get AKFS magnetic field", __func__);
				continue;
			}

			input_event_set(&event, EV_REL, REL_X, (int) (data->magnetic.x * 1000));
			write(uinput_fd, &event, sizeof(event));
			input_event_set(&event, EV_REL, REL_Y, (int) (data->magnetic.y * 1000));
			write(uinput_fd, &event, sizeof(event));
			input_event_set(&event, EV_REL, REL_Z, (int) (data->magnetic.z * 1000));
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

int akm8975_init(struct smdk4x12_sensors_handlers *handlers,
	struct smdk4x12_sensors_device *device)
{
	struct akm8975_data *data = NULL;
	pthread_attr_t thread_attr;
	char i2c_data[RWBUF_SIZE] = { 0 };
	short mode;
	int device_fd = -1;
	int uinput_fd = -1;
	int input_fd = -1;
	int rc;
	int i;

	ALOGD("%s(%p, %p)", __func__, handlers, device);

	if (handlers == NULL || device == NULL)
		return -EINVAL;

	data = (struct akm8975_data *) calloc(1, sizeof(struct akm8975_data));

	for (i = 0; i < device->handlers_count; i++) {
		if (device->handlers[i] == NULL)
			continue;

		if (device->handlers[i]->handle == SENSOR_TYPE_ORIENTATION)
			data->orientation_sensor = device->handlers[i];
	}

	device_fd = open("/dev/akm8975", O_RDONLY);
	if (device_fd < 0) {
		ALOGE("%s: Unable to open device", __func__);
		goto error;
	}

	mode = AK8975_MODE_POWER_DOWN;
	rc = ioctl(device_fd, ECS_IOCTL_SET_MODE, &mode);
	if (rc < 0) {
		ALOGE("%s: Unable to set akm8975 mode", __func__);
		goto error;
	}

	mode = AK8975_MODE_FUSE_ACCESS;
	rc = ioctl(device_fd, ECS_IOCTL_SET_MODE, &mode);
	if (rc < 0) {
		ALOGE("%s: Unable to set akm8975 mode", __func__);
		goto error;
	}

	i2c_data[0] = 3;
	i2c_data[1] = AK8975_FUSE_ASAY;
	rc = ioctl(device_fd, ECS_IOCTL_READ, &i2c_data);
	if (rc < 0) {
		ALOGE("%s: Unable to read akm8975 FUSE data", __func__);
		goto error;
	}

	ALOGD("AKM8975 ASA (Sensitivity Adjustment) values are: (%d, %d, %d)",
		i2c_data[1], i2c_data[2], i2c_data[3]);

	rc = akfs_init(data, &i2c_data[1], AKFS_PAT);
	if (rc < 0) {
		ALOGE("%s: Unable to init AKFS", __func__);
		goto error;
	}

	i2c_data[0] = 1;
	i2c_data[1] = AK8975_REG_WIA;
	rc = ioctl(device_fd, ECS_IOCTL_READ, &i2c_data);
	if (rc < 0) {
		ALOGE("%s: Unable to read akm8975 FUSE data", __func__);
		goto error;
	}

	ALOGD("AKM8975 WIA (Device ID) value is: 0x%x", i2c_data[1]);

	mode = AK8975_MODE_POWER_DOWN;
	rc = ioctl(device_fd, ECS_IOCTL_SET_MODE, &mode);
	if (rc < 0) {
		ALOGE("%s: Unable to set akm8975 mode", __func__);
		goto error;
	}

	uinput_fd = uinput_rel_create("magnetic");
	if (uinput_fd < 0) {
		ALOGD("%s: Unable to create uinput", __func__);
		goto error;
	}

	input_fd = input_open("magnetic");
	if (input_fd < 0) {
		ALOGE("%s: Unable to open magnetic input", __func__);
		goto error;
	}

	data->thread_continue = 1;

	pthread_mutex_init(&data->mutex, NULL);
	pthread_mutex_lock(&data->mutex);

	pthread_attr_init(&thread_attr);
	pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_DETACHED);

	rc = pthread_create(&data->thread, &thread_attr, akm8975_thread, (void *) handlers);
	if (rc < 0) {
		ALOGE("%s: Unable to create akm8975 thread", __func__);
		pthread_mutex_destroy(&data->mutex);
		goto error;
	}

	data->device_fd = device_fd;
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

	if (device_fd >= 0)
		close(device_fd);

	handlers->poll_fd = -1;
	handlers->data = NULL;

	return -1;
}

int akm8975_deinit(struct smdk4x12_sensors_handlers *handlers)
{
	struct akm8975_data *data = NULL;
	short mode;
	int rc;

	ALOGD("%s(%p)", __func__, handlers);

	if (handlers == NULL || handlers->data == NULL)
		return -EINVAL;

	data = (struct akm8975_data *) handlers->data;

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

	mode = AK8975_MODE_POWER_DOWN;
	rc = ioctl(data->device_fd, ECS_IOCTL_SET_MODE, &mode);
	if (rc < 0)
		ALOGE("%s: Unable to set akm8975 mode", __func__);

	if (data->device_fd >= 0)
		close(data->device_fd);
	data->device_fd = -1;

	free(handlers->data);
	handlers->data = NULL;

	return 0;
}

int akm8975_activate(struct smdk4x12_sensors_handlers *handlers)
{
	struct akm8975_data *data;
	AK8975PRMS *akfs_params;
	int rc;

	ALOGD("%s(%p)", __func__, handlers);

	if (handlers == NULL || handlers->data == NULL)
		return -EINVAL;

	data = (struct akm8975_data *) handlers->data;
	akfs_params = &data->akfs_params;

	// Read settings from a file
	rc = AKFS_LoadParameters(akfs_params, AKFS_CONFIG_PATH);
	if (rc != AKM_SUCCESS)
		ALOGE("%s: Unable to read AKFS parameters", __func__);

	// Initialize buffer
	AKFS_InitBuffer(AKFS_HDATA_SIZE, akfs_params->mfv_hdata);
	AKFS_InitBuffer(AKFS_HDATA_SIZE, akfs_params->mfv_hvbuf);
	AKFS_InitBuffer(AKFS_ADATA_SIZE, akfs_params->mfv_adata);
	AKFS_InitBuffer(AKFS_ADATA_SIZE, akfs_params->mfv_avbuf);

	// Initialize for AOC
	AKFS_InitAOC(&akfs_params->m_aocv);
	// Initialize magnetic status
	akfs_params->mi_hstatus = 0;

	handlers->activated = 1;
	pthread_mutex_unlock(&data->mutex);

	return 0;
}

int akm8975_deactivate(struct smdk4x12_sensors_handlers *handlers)
{
	struct akm8975_data *data;
	AK8975PRMS *akfs_params;
	int device_fd;
	short mode;
	int empty;
	int rc;
	int i;

	ALOGD("%s(%p)", __func__, handlers);

	if (handlers == NULL || handlers->data == NULL)
		return -EINVAL;

	data = (struct akm8975_data *) handlers->data;
	akfs_params = &data->akfs_params;

	device_fd = data->device_fd;
	if (device_fd < 0)
		return -1;

	empty = 1;

	for (i = 0; i < 3; i++) {
		if (akfs_params->mfv_ho.v[i] != 0) {
			empty = 0;
			break;
		}
	}

	if (!empty) {
		// Write settings to a file
		rc = AKFS_SaveParameters(akfs_params, AKFS_CONFIG_PATH);
		if (rc != AKM_SUCCESS)
			ALOGE("%s: Unable to write AKFS parameters", __func__);
	}

	mode = AK8975_MODE_POWER_DOWN;
	rc = ioctl(device_fd, ECS_IOCTL_SET_MODE, &mode);
	if (rc < 0)
		ALOGE("%s: Unable to set akm8975 mode", __func__);

	handlers->activated = 0;

	return 0;
}

int akm8975_set_delay(struct smdk4x12_sensors_handlers *handlers, long int delay)
{
	struct akm8975_data *data;

	ALOGD("%s(%p, %ld)", __func__, handlers, delay);

	if (handlers == NULL || handlers->data == NULL)
		return -EINVAL;

	data = (struct akm8975_data *) handlers->data;

	data->delay = delay;

	return 0;
}

float akm8975_convert(int value)
{
	return (float) value / 1000.0f;
}

int akm8975_get_data(struct smdk4x12_sensors_handlers *handlers,
	struct sensors_event_t *event)
{
	struct akm8975_data *data;
	struct input_event input_event;
	int input_fd;
	int rc;

//	ALOGD("%s(%p, %p)", __func__, handlers, event);

	if (handlers == NULL || handlers->data == NULL || event == NULL)
		return -EINVAL;

	data = (struct akm8975_data *) handlers->data;

	input_fd = handlers->poll_fd;
	if (input_fd < 0)
		return -1;

	memset(event, 0, sizeof(struct sensors_event_t));
	event->version = sizeof(struct sensors_event_t);
	event->sensor = handlers->handle;
	event->type = handlers->handle;

	event->magnetic.status = SENSOR_STATUS_ACCURACY_MEDIUM;

	do {
		rc = read(input_fd, &input_event, sizeof(input_event));
		if (rc < (int) sizeof(input_event))
			break;

		if (input_event.type == EV_REL) {
			switch (input_event.code) {
				case REL_X:
					event->magnetic.x = akm8975_convert(input_event.value);
					break;
				case REL_Y:
					event->magnetic.y = akm8975_convert(input_event.value);
					break;
				case REL_Z:
					event->magnetic.z = akm8975_convert(input_event.value);
					break;
				default:
					continue;
			}
		} else if (input_event.type == EV_SYN) {
			if (input_event.code == SYN_REPORT)
				event->timestamp = input_timestamp(&input_event);
		}
	} while (input_event.type != EV_SYN);

	if (data->orientation_sensor != NULL)
		orientation_fill(data->orientation_sensor, NULL, &event->magnetic);

	return 0;
}

struct smdk4x12_sensors_handlers akm8975 = {
	.name = "AKM8975",
	.handle = SENSOR_TYPE_MAGNETIC_FIELD,
	.init = akm8975_init,
	.deinit = akm8975_deinit,
	.activate = akm8975_activate,
	.deactivate = akm8975_deactivate,
	.set_delay = akm8975_set_delay,
	.get_data = akm8975_get_data,
	.activated = 0,
	.needed = 0,
	.poll_fd = -1,
	.data = NULL,
};
