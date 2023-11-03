/*
 * Copyright (c) 2016-2021 Satlab A/S <satlab@satlab.com>
 * All rights reserved. Do not distribute without permission.
 */

#ifndef _SL_UTIL_STRING_H_
#define _SL_UTIL_STRING_H_

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

int sec_to_time_string(unsigned int sec, char *s, size_t size);

void strhex(const uint8_t *data, size_t length, char *out);

int dehumanize_u64_metric(const char *s, uint64_t *num);

#endif /* _SL_UTIL_STRING_H_ */
