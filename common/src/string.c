/*
 * Copyright (c) 2016-2021 Satlab A/S <satlab@satlab.com>
 * All rights reserved. Do not distribute without permission.
 */

#include <util/string.h>

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <errno.h>
#include <ctype.h>

#include <util/util.h>

#define GIGA 1000000000
#define MEGA 1000000
#define KILO 1000

int sec_to_time_string(unsigned int sec, char *s, size_t size)
{
	int ret;
	unsigned int hour, min;

	hour = sec / 3600;
	sec -= hour * 3600;
	min  = sec / 60;
	sec -= min * 60;

	ret = snprintf(s, size, "%02u:%02u:%02u", hour, min, sec);
	if (ret >= size)
		return -ENOSPC;

	s[size - 1] = '\0';

	return 0;
}

void strhex(const uint8_t *data, size_t length, char *out)
{
	size_t i;

	for (i = 0; i < length; i++)
		sprintf(&out[i * 2], "%02hhx", data[i]);
}

static unsigned long long div_table[] = {
	1,
	10,
	100,
	1000,
	10000,
	100000,
	1000000,
	10000000,
	100000000,
	1000000000,
	10000000000
};

int dehumanize_u64_metric(const char *s, uint64_t *num)
{
	char *decimal_point, *prefix;
	unsigned long long mult, val = 0, decimal = 0, div;
	size_t decimals = 0;

	/* strtoull accepts negative numbers as a negation,
	 * so check here to avoid that */
	if (strchr(s, '-'))
		return -EINVAL;

	val = strtoull(s, &prefix, 10);

	if (*prefix == '.') {
		decimal_point = prefix;
		decimal = strtoull(decimal_point + 1, &prefix, 10);
		decimals = prefix - decimal_point - 1;
		if (decimals >= ARRAY_SIZE(div_table))
			return -E2BIG;
	} else if (prefix == s) {
		return -EINVAL;
	}

	switch (tolower(*prefix)) {
	case 'g':
		mult = GIGA;
		break;
	case 'm':
		mult = MEGA;
		break;
	case 'k':
		mult = KILO;
		break;
	case '\0':
		mult = 1;
		break;
	default:
		return -EINVAL;
	}

	div = div_table[decimals];

	/* Round half up */
	*num = val * mult + (decimal * mult + div / 2) / div;

	return (0);
}

int humanize_u64(uint64_t num, char *s, size_t len)
{
	if (num >= GIGA)
		snprintf(s, len, "%fG", (double)num/GIGA);
	else if (num >= MEGA)
		snprintf(s, len, "%fM", (double)num/MEGA);
	else if (num >= KILO)
		snprintf(s, len, "%fk", (double)num/KILO);
	else
		snprintf(s, len, "%"PRIu64, num);

	return 0;
}
