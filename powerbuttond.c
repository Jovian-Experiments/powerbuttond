// SPDX-License-Identifier: BSD-2-Clause
//
// Copyright (c) 2023 Valve Software
// Maintainer: Vicki Pfau <vi@endrift.com>
#include <errno.h>
#include <fcntl.h>
#include <glob.h>
#include <libevdev/libevdev.h>
#include <limits.h>
#include <signal.h>
#include <spawn.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

extern char** environ;

void signal_handler(int) {
}

struct libevdev* open_dev(const char* path) {
	int fd = open(path, O_RDONLY | O_NONBLOCK);
	if (fd < 0) {
		return NULL;
	}
	struct libevdev* dev;
	if (libevdev_new_from_fd(fd, &dev) < 0) {
		close(fd);
		return NULL;
	}

	fprintf(stderr, "info: opened '%s'\n", path);
	fprintf(stderr, "    Input device name: \"%s\"\n", libevdev_get_name(dev));
	if (libevdev_has_event_type(dev, EV_KEY) && libevdev_has_event_code(dev, EV_KEY, KEY_POWER)) {
		fprintf(stderr, "\n");
		return dev;
	}

	fprintf(stderr, "    Ignoring device since it does not have KEY_POWER.\n");
	libevdev_free(dev);
	close(fd);
	fprintf(stderr, "\n");
	return NULL;
}

void do_press(const char* type) {
	char steam[PATH_MAX];
	char press[32];
	char* home = getenv("HOME");
	char* const args[] = {steam, "-ifrunning", press, NULL};

	snprintf(steam, sizeof(steam), "%s/.steam/root/ubuntu12_32/steam", home);
	snprintf(press, sizeof(press), "steam://%spowerpress", type);

	pid_t pid;
	if (posix_spawn(&pid, steam, NULL, NULL, args, environ) < 0) {
		return;
	}
	while (true) {
		if (waitpid(pid, NULL, 0) > 0) {
			break;
		}
		if (errno != EINTR && errno != EAGAIN) {
			break;
		}
	}
}

int reset_fds(struct libevdev** evdev_devices, size_t num_devices, fd_set* fds) {
	int maxfd = 0;
	FD_ZERO(fds);

	for (size_t i = 0; i < num_devices; i++) {
		int fd = libevdev_get_fd(evdev_devices[i]);
		FD_SET(fd, fds);
		if (fd > maxfd) {
			maxfd = fd;
		}
	}

	// highest-numbered file descriptor in any of the sets, plus 1.
	return maxfd + 1;
}

int main() {
	int presses_active = 0;
	bool long_press_fired = false;
	size_t num_devices = 0;
	const char* device_path;
	struct libevdev** evdev_devices;
	fd_set fds;
	int maxfd = 0;

	struct sigaction sa = {
		.sa_handler = signal_handler,
		.sa_flags = SA_NOCLDSTOP,
	};
	sigemptyset(&sa.sa_mask);
	sigaction(SIGALRM, &sa, NULL);

	glob_t buf;
	glob("/dev/input/event[0-9]*", 0, NULL, &buf);
	fprintf(stderr, "\n");
	fprintf(stderr, "info: %ld candidate devices...\n", buf.gl_pathc);
	fprintf(stderr, "\n");

	evdev_devices = calloc(buf.gl_pathc, sizeof(struct libevdev*));

	for (size_t i = 0; i < buf.gl_pathc; i++) {
		device_path = buf.gl_pathv[i];

		struct libevdev* dev = open_dev(device_path);
		if (dev) {
			evdev_devices[num_devices] = dev;
			num_devices++;
		}
	}

	evdev_devices = realloc(evdev_devices, num_devices * sizeof(struct libevdev*));
	fprintf(stderr, "Found %ld power button devices.\n", num_devices);

	if (num_devices == 0) {
		fprintf(stderr, "ABORTING!\n");
		return 1;
	}
	fprintf(stderr, "\n");


	while (true) {
		maxfd = reset_fds(evdev_devices, num_devices, &fds);
		// Block until any of the file descriptors are ready
		int res_select = select(maxfd, &fds, NULL, NULL, NULL);

		if (res_select < 0 && errno == EINTR && presses_active > 0) {
			presses_active = 0;
			alarm(0);
			long_press_fired = true;
			do_press("long");
		}

		for (size_t i = 0; i < num_devices; i++) {
			struct libevdev* dev = evdev_devices[i];
			// Pump all events out of the device
			while (libevdev_has_event_pending(dev)) {
				struct input_event ev;
				int res = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_NORMAL, &ev);
				if (res == LIBEVDEV_READ_STATUS_SUCCESS) {
					if (ev.type == EV_KEY && ev.code == KEY_POWER) {
						if (long_press_fired) {
							long_press_fired = false;
						}
						else if (ev.value == 1) {
							presses_active++;
							alarm(1);
						} else if (presses_active > 0) {
							presses_active--;
							if (presses_active == 0) {
								alarm(0);
								do_press("short");
							}
						}
					}
				}
			}
		}
	}
}
