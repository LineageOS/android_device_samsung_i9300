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
#include <dirent.h>
#include <linux/ioctl.h>
#include <linux/input.h>
#include <linux/uinput.h>

#define LOG_TAG "smdk4x12_sensors"
#include <utils/Log.h>

#include "smdk4x12_sensors.h"

void input_event_set(struct input_event *event, int type, int code, int value)
{
	if (event == NULL)
		return;

	memset(event, 0, sizeof(struct input_event));

	event->type = type,
	event->code = code;
	event->value = value;

	gettimeofday(&event->time, NULL);
}

long int timestamp(struct timeval *time)
{
	if (time == NULL)
		return -1;

	return time->tv_sec * 1000000000LL + time->tv_usec * 1000;
}

long int input_timestamp(struct input_event *event)
{
	if (event == NULL)
		return -1;

	return timestamp(&event->time);
}

int uinput_rel_create(const char *name)
{
	struct uinput_user_dev uinput_dev;
	int uinput_fd;
	int rc;

	if (name == NULL)
		return -1;

	uinput_fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
	if (uinput_fd < 0) {
		ALOGE("%s: Unable to open uinput device", __func__);
		goto error;
	}

	memset(&uinput_dev, 0, sizeof(uinput_dev));

	strncpy(uinput_dev.name, name, sizeof(uinput_dev.name));
	uinput_dev.id.bustype	= BUS_I2C;
	uinput_dev.id.vendor	= 0;
	uinput_dev.id.product	= 0;
	uinput_dev.id.version	= 0;

	rc = 0;
	rc |= ioctl(uinput_fd, UI_SET_EVBIT, EV_REL);
	rc |= ioctl(uinput_fd, UI_SET_RELBIT, REL_X);
	rc |= ioctl(uinput_fd, UI_SET_RELBIT, REL_Y);
	rc |= ioctl(uinput_fd, UI_SET_RELBIT, REL_Z);
	rc |= ioctl(uinput_fd, UI_SET_EVBIT, EV_SYN);

	if (rc < 0) {
		ALOGE("%s: Unable to set uinput bits", __func__);
		goto error;
	}

	rc = write(uinput_fd, &uinput_dev, sizeof(uinput_dev));
	if (rc < 0) {
		ALOGE("%s: Unable to write uinput device", __func__);
		goto error;
	}

	rc = ioctl(uinput_fd, UI_DEV_CREATE);
	if (rc < 0) {
		ALOGE("%s: Unable to create uinput device", __func__);
		goto error;
	}

	usleep(3000);

	return uinput_fd;

error:
	if (uinput_fd >= 0)
		close(uinput_fd);

	return -1;
}

void uinput_destroy(int uinput_fd)
{
	if (uinput_fd < 0)
		return;

	ioctl(uinput_fd, UI_DEV_DESTROY);
}

int input_open(char *name)
{
	DIR *d;
	struct dirent *di;

	char input_name[80] = { 0 };
	char path[PATH_MAX];
	char *c;
	int fd;
	int rc;

	if (name == NULL)
		return -EINVAL;

	d = opendir("/dev/input");
	if (d == NULL)
		return -1;

	while ((di = readdir(d))) {
		if (di == NULL || strcmp(di->d_name, ".") == 0 || strcmp(di->d_name, "..") == 0)
			continue;

		snprintf(path, PATH_MAX, "/dev/input/%s", di->d_name);
		fd = open(path, O_RDONLY | O_NONBLOCK);
		if (fd < 0)
			continue;

		rc = ioctl(fd, EVIOCGNAME(sizeof(input_name) - 1), &input_name);
		if (rc < 0)
			continue;

		c = strstr((char *) &input_name, "\n");
		if (c != NULL)
			*c = '\0';

		if (strcmp(input_name, name) == 0)
			return fd;
		else
			close(fd);
	}

	return -1;
}

int sysfs_path_prefix(char *name, char *path_prefix)
{
	DIR *d;
	struct dirent *di;

	char input_name[80] = { 0 };
	char path[PATH_MAX];
	char *c;
	int fd;

	if (name == NULL || path_prefix == NULL)
		return -EINVAL;

	d = opendir("/sys/class/input");
	if (d == NULL)
		return -1;

	while ((di = readdir(d))) {
		if (di == NULL || strcmp(di->d_name, ".") == 0 || strcmp(di->d_name, "..") == 0)
			continue;

		snprintf(path, PATH_MAX, "/sys/class/input/%s/name", di->d_name);

		fd = open(path, O_RDONLY);
		if (fd < 0)
			continue;

		read(fd, &input_name, sizeof(input_name));
		close(fd);

		c = strstr((char *) &input_name, "\n");
		if (c != NULL)
			*c = '\0';

		if (strcmp(input_name, name) == 0) {
			snprintf(path_prefix, PATH_MAX, "/sys/class/input/%s", di->d_name);
			return 0;
		}
	}

	return -1;
}

int sysfs_value_read(char *path)
{
	char buffer[100];
	int value;
	int fd = -1;
	int rc;

	if (path == NULL)
		return -1;

	fd = open(path, O_RDONLY);
	if (fd < 0)
		goto error;

	rc = read(fd, &buffer, sizeof(buffer));
	if (rc <= 0)
		goto error;

	value = atoi(buffer);
	goto complete;

error:
	value = -1;

complete:
	if (fd >= 0)
		close(fd);

	return value;
}

int sysfs_value_write(char *path, int value)
{
	char buffer[100];
	int fd = -1;
	int rc;

	if (path == NULL)
		return -1;

	fd = open(path, O_WRONLY);
	if (fd < 0)
		goto error;

	snprintf((char *) &buffer, sizeof(buffer), "%d\n", value);

	rc = write(fd, buffer, strlen(buffer));
	if (rc < (int) strlen(buffer))
		goto error;

	rc = 0;
	goto complete;

error:
	rc = -1;

complete:
	if (fd >= 0)
		close(fd);

	return rc;
}

int sysfs_string_read(char *path, char *buffer, size_t length)
{
	int fd = -1;
	int rc;

	if (path == NULL || buffer == NULL || length == 0)
		return -1;

	fd = open(path, O_RDONLY);
	if (fd < 0)
		goto error;

	rc = read(fd, buffer, length);
	if (rc <= 0)
		goto error;

	rc = 0;
	goto complete;

error:
	rc = -1;

complete:
	if (fd >= 0)
		close(fd);

	return rc;
}

int sysfs_string_write(char *path, char *buffer, size_t length)
{
	int fd = -1;
	int rc;

	if (path == NULL || buffer == NULL || length == 0)
		return -1;

	fd = open(path, O_WRONLY);
	if (fd < 0)
		goto error;

	rc = write(fd, buffer, length);
	if (rc <= 0)
		goto error;

	rc = 0;
	goto complete;

error:
	rc = -1;

complete:
	if (fd >= 0)
		close(fd);

	return rc;
}
