/*
 * Copyright (c) 2013-2021 Satlab A/S <satlab@satlab.com>
 * All rights reserved. Do not distribute without permission.
 */

#include <btp/crc32.h>

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <inttypes.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <util/crc32c.h>

int btp_crc32c_fd(int fd, uint32_t *result, size_t limit)
{
	int i, ret;
	uint8_t buf[512];
	uint32_t crc, size, remain, bytes;

	ret = lseek(fd, 0, SEEK_END);
	if (ret < 0)
		return ret;

	size = ret;

	if (limit > 0)
		size = limit;

	ret = lseek(fd, 0, SEEK_SET);
	if (ret < 0)
		return -EIO;

	crc = 0;
	for (i = 0; size && i < (size + sizeof(buf) - 1) / sizeof(buf); i++) {
		remain = size - i * sizeof(buf);
		bytes = remain >= sizeof(buf) ? sizeof(buf) : remain;

		if (read(fd, buf, bytes) < bytes)
			return -EIO;

		crc = crc32c_update(crc, buf, bytes);
	}

	if (lseek(fd, 0, SEEK_SET) < 0)
		return -EIO;

	*result = crc;

	return 0;
}
