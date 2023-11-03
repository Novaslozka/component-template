/*
 * Copyright (c) 2013-2021 Satlab A/S <satlab@satlab.com>
 * All rights reserved. Do not distribute without permission.
 */

#ifndef _SL_BTP_BITOPS_H_
#define _SL_BTP_BITOPS_H_

#include <stdint.h>

static inline uint8_t get_bit(uint8_t *buf, uint32_t bitnum)
{
	/* Return the bitnum'th bit in buf */
	return (buf[bitnum/8] >> (7 - bitnum % 8)) & 0x01;
}

static inline uint8_t bit_is_set(uint8_t *buf, uint32_t bitnum)
{
	return get_bit(buf, bitnum);
}

static inline uint8_t bit_is_clear(uint8_t *buf, uint32_t bitnum)
{
	return !get_bit(buf, bitnum);
}

static inline void set_bit(uint8_t *buf, uint32_t bitnum)
{
	buf[bitnum/8] |= (1 << (7 - bitnum % 8));
}

static inline void clear_bit(uint8_t *buf, uint32_t bitnum)
{
	buf[bitnum/8] &= ~(1 << (7 - bitnum % 8));
}

static inline void set_bit_value(uint8_t *buf, uint32_t bitnum, uint8_t value)
{
	if (value)
		set_bit(buf, bitnum);
	else
		clear_bit(buf, bitnum);
}

static inline void copy_bits(uint8_t *dstbuf, uint32_t dstoffset, uint8_t *srcbuf, uint32_t srcoffset, uint32_t count)
{
	int i;

	/* TODO: this should copy as much as possible bytewise */
	for (i = 0; i < count; i++)
		set_bit_value(dstbuf, dstoffset + i, get_bit(srcbuf, srcoffset + i));
}

#endif /* _BTP_BITOPS_H_ */
