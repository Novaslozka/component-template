/*
 * Copyright (c) 2013-2021 Satlab A/S <satlab@satlab.com>
 * All rights reserved. Do not distribute without permission.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <locale.h>
#include <langinfo.h>
#include <errno.h>
#include <pthread.h>
#include <termios.h>
#include <dirent.h>

#include <btp/bitops.h>
#include <btp/client.h>
#include <btp/types.h>
#include <btp/error.h>
#include <btp/crc32.h>

#include <csp/csp.h>
#include <csp/csp_endian.h>

#define MAP_SYMBOL_OK	"+"
#define MAP_SYMBOL_MISS	"-"
static const char *map_symbol_ok = MAP_SYMBOL_OK;
static const char *map_symbol_miss = MAP_SYMBOL_MISS;

#ifndef BITS_PER_BYTE
#define BITS_PER_BYTE	(8)
#endif

//#define DEBUG
#ifdef DEBUG
#define pr_debug(...) do { fprintf(stderr, __VA_ARGS__ ); } while(0)
#else
#define pr_debug(...) do {} while (0)
#endif

static char const * strperm(uint32_t type, uint32_t mode) {
	static char local_buff[16] = {0};
	int i = 0;
	
	/* Entry type */
	if ((type & DT_DIR) == DT_DIR) local_buff[i] = 'd';
	else local_buff[i] = '-';
	i++;

	/* user permissions */
	if ((mode & S_IRUSR) == S_IRUSR) local_buff[i] = 'r';
	else local_buff[i] = '-';
	i++;
	if ((mode & S_IWUSR) == S_IWUSR) local_buff[i] = 'w';
	else local_buff[i] = '-';
	i++;
	if ((mode & S_IXUSR) == S_IXUSR) local_buff[i] = 'x';
	else local_buff[i] = '-';
	i++;

	/* group permissions */
	if ((mode & S_IRGRP) == S_IRGRP) local_buff[i] = 'r';
	else local_buff[i] = '-';
	i++;
	if ((mode & S_IWGRP) == S_IWGRP) local_buff[i] = 'w';
	else local_buff[i] = '-';
	i++;
	if ((mode & S_IXGRP) == S_IXGRP) local_buff[i] = 'x';
	else local_buff[i] = '-';
	i++;

	/* other permissions */
	if ((mode & S_IROTH) == S_IROTH) local_buff[i] = 'r';
	else local_buff[i] = '-';
	i++;
	if ((mode & S_IWOTH) == S_IWOTH) local_buff[i] = 'w';
	else local_buff[i] = '-';
	i++;
	if ((mode & S_IXOTH) == S_IXOTH) local_buff[i] = 'x';
	else local_buff[i] = '-';
	return local_buff;
}

static void btp_client_update_bounds(struct btp_context *btp, uint32_t block)
{
	/*
	 * The blocks are marked as received in a bitfield.
	 * - 'next' marks the next packet we expect to receive.
	 * - 'bits' marks the block following the largest block
	 *   we have ever seen.
	 *
	 * [X|X|X|X| | |X| |X|X| | | | | ]
	 *          |	        |
	 *          +- next     +- bits
	 *
	 * Where 'X' marks a received block. Both 'next' and 'bits'
	 * are initialized to 0.
	 *
	 * The difference between between 'bits' and 'next' can not
	 * be larger than the number of bits in btp_statusrep.bitfield,
	 * hence the somewhat complex update bounds function.
	 */

	assert(btp->next <= btp->bits);

	/* Mark packet received */
	set_bit(btp->block_status, block);
	
	/* Update maximum missing packet */
	if (block >= btp->bits &&
			((btp->bits - btp->next) < (BTP_BITFIELD_LENGTH * BITS_PER_BYTE)))
		btp->bits = block + 1;

	/* Update 'next' mark if we have received all packets so far */
	if (block == btp->next)
		btp->next = block + 1;
	
	/* Update 'next' mark up to next missing packet */
	while (bit_is_set(btp->block_status, btp->next))
		btp->next++;

	/* Update 'bits' mark up to */
	while (bit_is_set(btp->block_status, btp->bits) &&
			(btp->bits - btp->next < BTP_BITFIELD_LENGTH * BITS_PER_BYTE ||
			btp->next > btp->bits))
		btp->bits++;

	if (btp->next == btp->blocks)
		btp->complete = true;

	assert(btp->next <= btp->bits);
}

bool btp_client_finished(struct btp_context *btp)
{
	if (!btp || (btp->state != STATE_UPLOAD && btp->state != STATE_DOWNLOAD))
		return BTP_EINVAL;

	return btp->complete;
}

struct btp_context *btp_client_connect(uint8_t host,
				       uint8_t port,
				       uint32_t timeout,
				       uint32_t opts,
				       unsigned int attempts)
{
	struct btp_context *btp;

	btp = calloc(1, sizeof(*btp));
	if (!btp)
		return NULL;

	btp->conn = csp_connect(CSP_PRIO_NORM, host, port, timeout, opts);
	if (!btp->conn) {
		free(btp);
		return NULL;
	}

	btp->host = host;
	btp->port = port;
	btp->timeout = timeout;
	btp->attempts = attempts;
	btp->complete = false;
	btp->state = STATE_CONNECTED;
	btp->fd = -1;

	pr_debug("connected to %hhu:%hhu\n", host, port);

	return btp;
}

int btp_client_disconnect(struct btp_context *btp)
{
	if (!btp || btp->state == STATE_IDLE)
		return BTP_EINVAL;

	csp_close(btp->conn);
	free(btp->block_status);
	free(btp);

	return BTP_EOK;
}

int btp_client_upload(struct btp_context *btp,
		      char *backend,
		      char *remotepath,
		      char *localpath,
		      uint8_t block_size,
		      uint32_t timeout_csum,
		      uint32_t timeout_server,
		      bool force)
{
	int i, ret;
	struct btp_upreq req;
	struct btp_uprep rep;
	uint32_t checksum;

	if (!btp || btp->state != STATE_CONNECTED)
		return BTP_EINVAL;

	req.type = BTP_UP_REQUEST;
	strncpy(req.backend, backend, sizeof(req.backend) - 1);
	req.backend[sizeof(req.backend) - 1] = '\0';
	strncpy(req.name, remotepath, sizeof(req.name) - 1);
	req.name[sizeof(req.name) - 1] = '\0';
	req.block_size = block_size;
	req.timeout = csp_htole32(timeout_server);
	req.force = force;
	
	btp->fd = open(localpath, O_RDONLY);
	if (btp->fd < 0)
		return BTP_ENOENT;

	btp->size = lseek(btp->fd, 0, SEEK_END);
	lseek(btp->fd, 0, SEEK_SET);

	if (btp_crc32c_fd(btp->fd, &checksum, 0) < 0)
		return BTP_EIO;

	btp->checksum = checksum;
	btp->block_size = block_size;
	btp->blocks = (btp->size + btp->block_size - 1) / btp->block_size;
	btp->block_status = calloc(1, (btp->blocks + BITS_PER_BYTE - 1) / BITS_PER_BYTE);

	if (!btp->block_status)
		return BTP_ENOMEM;

	req.checksum = csp_htole32(btp->checksum);
	req.size = csp_htole32(btp->size);

	pr_debug("upload file %s of size %u bytes with checksum %08x\n", localpath, btp->size, btp->checksum);

	for (i = 0; i < btp->attempts; i++) {
		ret = csp_transaction_persistent(btp->conn, timeout_csum, &req, sizeof(req), &rep, sizeof(rep));
		if (ret != 0)
			break;
	}

	if (ret == 0)
		return BTP_ETIMEDOUT;

	if (rep.type != BTP_UP_REPLY)
		return BTP_EPROTO;

	if (rep.err != BTP_EOK)
		return rep.err;

	btp->state = STATE_UPLOAD;

	if (csp_letoh32(rep.size) == btp->size && csp_letoh32(rep.checksum) == btp->checksum)
		btp->complete = true;

	btp->next = 0;
	btp->bits = 0;

	return BTP_EOK;
}

int btp_client_download(struct btp_context *btp,
			char *backend,
			char *remotepath,
			char *localpath,
			uint8_t block_size,
			uint32_t timeout_csum,
			uint32_t timeout_server,
			bool force)
{
	int i, ret;
	char c;
	struct btp_dwreq req;
	struct btp_dwrep rep;
	uint32_t cursize, checksum;
	mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;

	if (!btp || btp->state != STATE_CONNECTED)
		return BTP_EINVAL;

	strncpy(btp->filename, localpath, sizeof(btp->filename) - 1);
	btp->filename[sizeof(btp->filename) - 1] = '\0';

	req.type = BTP_DW_REQUEST;
	strncpy(req.backend, backend, sizeof(req.backend) - 1);
	req.backend[sizeof(req.backend) - 1] = '\0';
	strncpy(req.name, remotepath, sizeof(req.name) - 1);
	req.name[sizeof(req.name) - 1] = '\0';
	req.block_size = block_size;
	req.timeout = csp_htole32(timeout_server);

	/* Deprecated fields */
	req.deprecated1 = 0;
	req.deprecated2 = 0;

	pr_debug("download file %s\n", remotepath);

	for (i = 0; i < btp->attempts; i++) {
		ret = csp_transaction_persistent(btp->conn, timeout_csum, &req, sizeof(req), &rep, sizeof(rep));
		if (ret != 0)
			break;
	}

	if (ret == 0)
		return BTP_ETIMEDOUT;

	if (rep.type != BTP_DW_REPLY)
		return BTP_EPROTO;

	if (rep.err != BTP_EOK)
		return rep.err;

	btp->state = STATE_DOWNLOAD;
	btp->size = csp_letoh32(rep.size);
	btp->checksum = csp_letoh32(rep.checksum);
	btp->block_size = block_size;
	btp->blocks = (btp->size + btp->block_size - 1) / btp->block_size;
	btp->next = 0;
	btp->bits = 0;

	btp->block_status = calloc(1, (btp->blocks + BITS_PER_BYTE - 1) / BITS_PER_BYTE);
	if (!btp->block_status)
		return BTP_ENOMEM;

	if (force)
		unlink(btp->filename);
	
	/* Check if file was already downloaded */
	btp->fd = open(btp->filename, O_RDWR);
	if (btp->fd >= 0) {
		cursize = lseek(btp->fd, 0, SEEK_END);
		lseek(btp->fd, 0, SEEK_SET);

		if (cursize != btp->size)
			return BTP_EEXIST;

		if (btp_crc32c_fd(btp->fd, &checksum, 0) < 0)
			return BTP_EIO;

		if (checksum != btp->checksum)
			return BTP_EEXIST;

		btp->complete = true;
		return BTP_EOK;
	}

	/* Check for partial transfer */
	snprintf(btp->filename_partial,
		 sizeof(btp->filename_partial) - 1,
		 "%s.btp",
		 btp->filename);
	btp->filename_partial[sizeof(btp->filename_partial) - 1] = '\0';

	if (force)
		unlink(btp->filename_partial);

	btp->fd = open(btp->filename_partial, O_RDWR);
	if (btp->fd >= 0) {
		/* Partial transfer */
		cursize = lseek(btp->fd, 0, SEEK_END);
		lseek(btp->fd, 0, SEEK_SET);

		if (cursize != btp->size + btp->blocks) {
			printf("%s has wrong size for this block size (expected %"PRIu32" bytes)\n",
			       btp->filename_partial, btp->size + btp->blocks);
			return BTP_EEXIST;
		}

		lseek(btp->fd, btp->size, SEEK_SET);
		for (i = 0; i < btp->blocks; i++) {
			if (read(btp->fd, &c, 1) != 1 || (c != '+' && c != '-')) {
				printf("%s has wrong format\n", btp->filename_partial);
				return BTP_EINVAL;
			} else {
				if (c == '+')
					btp_client_update_bounds(btp, i);
			}
		}
		lseek(btp->fd, 0, SEEK_SET);

		if (btp_crc32c_fd(btp->fd, &checksum, btp->size) < 0)
			return BTP_EIO;

		if (checksum == btp->checksum)
			btp->complete = true;

		return BTP_EOK;
	}

	/* New transfer */
	btp->fd = open(btp->filename_partial, O_RDWR | O_CREAT, mode);
	if (btp->fd < 0)
		return BTP_EIO;

	lseek(btp->fd, btp->size, SEEK_SET);
	for (i = 0; i < btp->blocks; i++) {
		if (write(btp->fd, map_symbol_miss, 1) != 1)
			return BTP_EIO;
	}
	lseek(btp->fd, 0, SEEK_SET);

	return BTP_EOK;
}

int btp_client_request_status(struct btp_context *btp)
{
	int i, ret;
	struct btp_stat_pushrequest req;
	struct btp_stat_pushreply rep;

	if (!btp || btp->state != STATE_UPLOAD)
		return BTP_EINVAL;

	req.type = BTP_STAT_PUSH_REQUEST;

	for (i = 0; i < btp->attempts; i++) {
		ret = csp_transaction_persistent(btp->conn, btp->timeout, &req, sizeof(req), &rep, sizeof(rep));
		if (ret != 0)
			break;
	}

	if (ret == 0)
		return BTP_ETIMEDOUT;

	if (rep.type != BTP_STAT_PUSH_REPLY)
		return BTP_EPROTO;

	if (rep.err != BTP_EOK)
		return rep.err;	

	if (btp->next > 0)
		btp->progress += (csp_letoh32(rep.next) - btp->next) * btp->block_size;

	btp->next = csp_letoh32(rep.next);
	btp->bits = csp_letoh32(rep.bits);
	copy_bits(btp->block_status, btp->next, rep.bitfield, 0, btp->bits - btp->next);

#ifdef DEBUG
	int j;
	printf("Received status: G=%"PRIu32" B=%"PRIu32" I=[", btp->next, btp->bits);
	for (j = 0; j < (btp->bits - btp->next); j++)
		printf("%c", get_bit(btp->block_status, btp->next + j) ? 'X' : '.');
	printf("]\n");
#endif

	if (csp_letoh32(rep.next) == btp->blocks)
		btp->complete = true;
	
	return BTP_EOK;
}

int btp_client_send_status(struct btp_context *btp)
{
	int i, ret;
	struct btp_stat_pullrequest req;
	struct btp_stat_pullreply rep;

	if (!btp || btp->state != STATE_DOWNLOAD)
		return BTP_EINVAL;

	req.type = BTP_STAT_PULL_REQUEST;
	req.next = csp_htole32(btp->next);
	req.bits = csp_htole32(btp->bits);
	
	memset(req.bitfield, 0, sizeof(req.bitfield));
	copy_bits(req.bitfield, 0, btp->block_status, btp->next, btp->bits - btp->next);

#ifdef DEBUG
	int j;
	pr_debug("Sending status: G=%"PRIu32" B=%"PRIu32" I=[", btp->next, btp->bits);
	for (j = 0; j < (btp->bits - btp->next); j++)
		printf("%c", get_bit(btp->block_status, btp->next + j) ? 'X' : '.');
	pr_debug("]\n");
#endif

	for (i = 0; i < btp->attempts; i++) {
		ret = csp_transaction_persistent(btp->conn, btp->timeout, &req, sizeof(req), &rep, sizeof(rep));
		if (ret != 0)
			break;
	}

	if (ret == 0)
		return BTP_ETIMEDOUT;

	return rep.err;
}

int btp_client_get_blocks(struct btp_context *btp, uint32_t offset, uint32_t count, progress_cb cb, void *cbarg)
{
	int i, j, timeout_count;
	uint32_t remain, bytes;
	csp_packet_t *packet;
	struct btp_blockreq req;
	struct btp_blockrep *rep;
	bool received_data = false;

	if (!btp || btp->state != STATE_DOWNLOAD)
		return BTP_EINVAL;

	req.type = BTP_BLOCK_REQUEST;
	req.offset = csp_htole32(offset);
	req.count = csp_htole32(count);

	for (i = 0; i < btp->attempts && !received_data; i++) {
		pr_debug("sending get blocks\n");
		if (csp_transaction_persistent(btp->conn, btp->timeout, &req, sizeof(req), NULL, 0) == 0)
			return BTP_ETIMEDOUT;

		timeout_count = 0;

		for (j = 0; j < count; j++) {
			packet = csp_read(btp->conn, btp->timeout);

			if (!packet) {
				pr_debug("waiting for more packets\n");
				timeout_count++;
				if (timeout_count >= 5)
					break;
				continue;
			}

			received_data = true;

			rep = (struct btp_blockrep *) packet->data;

			if (rep->type != BTP_BLOCK_REPLY) {
				pr_debug("not block reply\n");
				csp_buffer_free(packet);
				return BTP_EPROTO;
			}

			if (rep->err != BTP_EOK) {
				pr_debug("not EOK\n");
				csp_buffer_free(packet);
				return rep->err;
			}

			if (csp_letoh32(rep->block) < btp->next) {
				pr_debug("block (%"PRIu32") < next (%"PRIu32")\n", csp_letoh32(rep->block), btp->next);
				csp_buffer_free(packet);
				continue;
			}

			if (lseek(btp->fd, csp_letoh32(rep->block) * btp->block_size, SEEK_SET) < 0) {
				printf("lseek failed\n");
				return BTP_EIO;
			}

			remain = btp->size - csp_letoh32(rep->block) * btp->block_size;
			bytes = remain >= btp->block_size ? btp->block_size : remain;

			if (write(btp->fd, rep->data, bytes) < bytes) {
				printf("Failed to write data\n");
				return BTP_EIO;
			}

			if (lseek(btp->fd, btp->size + csp_letoh32(rep->block), SEEK_SET) < 0) {
				printf("lseek map failed\n");
				return BTP_EIO;
			}

			if (write(btp->fd, map_symbol_ok, 1) != 1) {
				printf("Failed to write map\n");
				return BTP_EIO;
			}

			btp->progress += bytes;

			pr_debug("received block %"PRIu32"\n", csp_letoh32(rep->block));
			btp_client_update_bounds(btp, csp_letoh32(rep->block));

			if (csp_letoh32(rep->seq) == count - 1) {
				csp_buffer_free(packet);
				break;
			}

			csp_buffer_free(packet);

			if (cb != NULL)
				cb(btp, cbarg);

			if (btp_client_finished(btp))
				break;
		}
	}

	return received_data ? BTP_EOK : BTP_ETIMEDOUT;
}

static int btp_client_read_block(struct btp_context *btp, uint32_t block, uint8_t *buf, uint32_t bytes)
{
	if (block > btp->blocks)
		return BTP_EINVAL;

	if (lseek(btp->fd, block * btp->block_size, SEEK_SET) < 0)
		return BTP_EIO;	

	if (read(btp->fd, buf, bytes) != bytes)
		return BTP_EIO;

	return BTP_EOK;
}

int btp_client_send_blocks(struct btp_context *btp, uint32_t offset, uint32_t count, progress_cb cb, void *cbarg)
{
	int i;
	bool only_missing = false;
	uint32_t remain, bytes;
	struct btp_blockrep *rep;
	csp_packet_t *packet;

	if (!btp || btp->state != STATE_UPLOAD)
		return BTP_EINVAL;

	if (offset == BTP_OFFSET_NEXT) {
		offset = btp->next;
		only_missing = true;
	}

	for (i = 0; i < count && offset < btp->blocks; i++) {
		packet = csp_buffer_get(sizeof(*rep) + btp->block_size);
		if (!packet) {
			pr_debug("Failed to get buffer\n");
			return BTP_ENOMEM;
		}

		rep = (struct btp_blockrep *) packet->data;
		rep->type = BTP_BLOCK_REPLY;
		rep->err = BTP_EOK;
		rep->seq = csp_htole32(i);
		rep->total = csp_htole32(count);
		rep->block = csp_htole32(offset++);

		/* Skip forward to next missing block */
		while (only_missing && bit_is_set(btp->block_status, offset) &&
		       offset + 1 < btp->blocks)
			offset++;

		remain = btp->size - csp_letoh32(rep->block) * btp->block_size;
		bytes = remain >= btp->block_size ? btp->block_size : remain;

		if (btp_client_read_block(btp, csp_letoh32(rep->block), rep->data, bytes) == BTP_EOK) {
			packet->length = sizeof(*rep) + bytes;
			pr_debug("sending block %"PRIu32"\n", csp_letoh32(rep->block));
			if (!csp_send(btp->conn, packet, btp->timeout)) {
				pr_debug("Failed to send block %"PRIu32"\n", csp_letoh32(rep->block));
				csp_buffer_free(packet);
				return BTP_ETIMEDOUT;
			}
		} else {
			pr_debug("Failed to read block %"PRIu32"\n", csp_letoh32(rep->block));
			csp_buffer_free(packet);
			return BTP_EIO;
		}
	}
	
	return BTP_EOK;
}

int btp_client_list(struct btp_context *btp, uint16_t count, char *backend, char *name)
{
	int i;
	struct btp_listreq req;
	struct btp_listrep rep;
	struct btp_entryreq *ereq;
	struct btp_entryrep *erep;
	csp_packet_t *packet;
	char datestring[256];
	struct tm *tm;

	if (!btp || btp->state != STATE_CONNECTED)
		return BTP_EINVAL;

	req.type = BTP_LIST_REQUEST;
	strncpy(req.backend, backend, sizeof(req.backend) - 1);
	req.backend[sizeof(req.backend) - 1] = '\0';
	strncpy(req.name, name, sizeof(req.name) - 1);
	req.name[sizeof(req.name) - 1] = '\0';
	req.count = csp_htole16(count);

	if (csp_transaction_persistent(btp->conn, btp->timeout, &req, sizeof(req), &rep, sizeof(rep)) == 0)
		return BTP_ETIMEDOUT;

	if (rep.err != BTP_EOK)
		return rep.err;

	printf("total %d\n", rep.entries);

	for (i = 0; i < rep.entries; i++) {
		packet = csp_read(btp->conn, btp->timeout);
		if (!packet)
			return BTP_ETIMEDOUT;

		erep = (struct btp_entryrep *) packet->data;

		if (erep->type != BTP_ENTRY_REPLY) {
			csp_buffer_free(packet);
			return BTP_EPROTO;
		}

		time_t ttt = csp_letoh32(erep->mtime);
		tm = localtime(&ttt);
		strftime(datestring, sizeof(datestring) - 1, "%Y-%m-%d %H:%M", tm);
		datestring[sizeof(datestring) - 1] = '\0';

		printf("%s", strperm(csp_letoh32(erep->etype), csp_letoh32(erep->mode)));
		printf(" %3d", csp_letoh16(erep->nlink));
		printf(" %-4d", csp_letoh16(erep->uid));
		printf(" %-4d", csp_letoh16(erep->gid));
		printf(" %10"PRIu32"", csp_letoh32(erep->size));
		printf(" %s %s\n", datestring, erep->name);

		csp_buffer_free(packet);

		if (i && (i + 1) % count == 0) {
			packet = csp_buffer_get(sizeof(*ereq));
			if (!packet)
				return BTP_ENOMEM;

			ereq = (struct btp_entryreq *) packet->data;

			ereq->type = BTP_ENTRY_REQUEST;
			ereq->count = csp_htole16(count);

			packet->length = sizeof(*ereq);
			
			if (!csp_send(btp->conn, packet, btp->timeout)) {
				csp_buffer_free(packet);
				return BTP_ETIMEDOUT;
			}
		}
	}

	return rep.err;
}

int btp_client_remove(struct btp_context *btp, char *backend, char *path)
{
	struct btp_removereq req;
	struct btp_removerep rep;

	if (!btp || btp->state != STATE_CONNECTED)
		return BTP_EINVAL;
	
	req.type = BTP_REMOVE_REQUEST;
	strncpy(req.backend, backend, sizeof(req.backend) - 1);
	req.backend[sizeof(req.backend) - 1] = '\0';
	strncpy(req.name, path, sizeof(req.name) - 1);
	req.name[sizeof(req.name) - 1] = '\0';

	if (csp_transaction_persistent(btp->conn, btp->timeout, &req, sizeof(req), &rep, sizeof(rep)) == 0)
		return BTP_ETIMEDOUT;

	return rep.err;
}

int btp_client_move(struct btp_context *btp, char *backend, char *frompath, char *topath)
{
	struct btp_movereq req;
	struct btp_moverep rep;

	if (!btp || btp->state != STATE_CONNECTED)
		return BTP_EINVAL;

	req.type = BTP_MOVE_REQUEST;
	strncpy(req.backend, backend, sizeof(req.backend) - 1);
	req.backend[sizeof(req.backend) - 1] = '\0';
	strncpy(req.from, frompath, sizeof(req.from) - 1);
	req.from[sizeof(req.from) - 1] = '\0';
	strncpy(req.to, topath, sizeof(req.to) - 1);
	req.to[sizeof(req.to) - 1] = '\0';

	if (csp_transaction_persistent(btp->conn, btp->timeout, &req, sizeof(req), &rep, sizeof(rep)) == 0)
		return BTP_ETIMEDOUT;

	return rep.err;
}

int btp_client_copy(struct btp_context *btp, char *backend, char *frompath, char *topath)
{
	struct btp_copyreq req;
	struct btp_copyrep rep;

	if (!btp || btp->state != STATE_CONNECTED)
		return BTP_EINVAL;

	req.type = BTP_COPY_REQUEST;
	strncpy(req.backend, backend, sizeof(req.backend) - 1);
	req.backend[sizeof(req.backend) - 1] = '\0';
	strncpy(req.from, frompath, sizeof(req.from) - 1);
	req.from[sizeof(req.from) - 1] = '\0';
	strncpy(req.to, topath, sizeof(req.to) - 1);
	req.to[sizeof(req.to) - 1] = '\0';

	if (csp_transaction_persistent(btp->conn, btp->timeout, &req, sizeof(req), &rep, sizeof(rep)) == 0)
		return BTP_ETIMEDOUT;

	return rep.err;
}

struct thread_args {
	struct btp_context *btp;
	uint32_t timeout;
	uint32_t count;
};

static void tty_mode(struct btp_context *btp, bool raw)
{
	struct termios current;

	if (btp->raw == raw)
		return;

	if (tcgetattr(STDIN_FILENO, &current) < 0)
		return;

	current.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
	current.c_oflag &= ~(OPOST);
	current.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);

	current.c_cc[VMIN] = 20;
	current.c_cc[VTIME] = 2;

	/* put terminal in raw mode after flushing */
	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &current) < 0)
		return;

	btp->raw = true;
}

static void *reader(void *params)
{
	struct thread_args *args;
	csp_packet_t *packet;
	struct btp_shellreq req;
	struct btp_shellrep *rep;
	int count;
	
	args = (struct thread_args *) params;

	count = args->count;

	while (args->count == 0 || count--) {
		packet = csp_read(args->btp->conn, args->timeout);
		if (!packet) {
			printf("\nconnection timed out\n");
			break;
		}

		rep = (struct btp_shellrep *) packet->data;

		if (rep->err != BTP_EOK) {
			csp_buffer_free(packet);
			break;
		}

		tty_mode(args->btp, rep->flags & SHELL_FLAG_CHAR);

		printf("%s", rep->output);
		fflush(stdout);

		csp_buffer_free(packet);
	}

	if (count < 0)
		printf("\n\n... (remaing output truncated)\n");

	req.type = BTP_SHELL_REQUEST;
	req.count = 0;
	req.flags = SHELL_FLAG_DONE;
	req.input[0] = '\0';
	if (csp_transaction_persistent(args->btp->conn, args->timeout, &req, shell_request_size(1), NULL, 0) == 0)
		printf("could not send DONE packet\n");

	pthread_exit(NULL);
}

static void *writer(void *params)
{
	int r;
	struct thread_args *args;
	struct btp_shellreq req;

	args = (struct thread_args *) params;

	while (1) {
		r = read(STDIN_FILENO, req.input, sizeof(req.input) - 1);
		if (r < 1) {
			req.flags = SHELL_FLAG_DONE;
			r = 0;
		}
		req.input[r] = '\0';
		req.type = BTP_SHELL_REQUEST;
		req.count = args->count;
		if (csp_transaction_persistent(args->btp->conn, args->timeout,
					       &req, shell_request_size(r), NULL, 2) == 0)
			break;
	}

	pthread_exit(NULL);
}

int btp_client_shell(struct btp_context *btp, uint16_t count, char *cmd, bool raw)
{
	int ret = BTP_ETIMEDOUT, tret = 0;
	struct btp_shellreq req;
	pthread_t treader, twriter;
	struct thread_args threadargs;
	struct termios orig_termios[3];

	tret += tcgetattr(STDIN_FILENO, &orig_termios[0]);
	tret += tcgetattr(STDOUT_FILENO, &orig_termios[1]);
	tret += tcgetattr(STDERR_FILENO, &orig_termios[2]);

	if (tret < 0)
		return BTP_EINVAL;

	req.flags = 0;
	if (raw) {
		tty_mode(btp, true);
		req.flags = SHELL_FLAG_CHAR;
	}

	strncpy(req.input, cmd, sizeof(req.input) - 1);
	req.input[sizeof(req.input) - 1] = '\0';
	req.type = BTP_SHELL_REQUEST;
	req.count = csp_htole16(count);
	if (csp_transaction_persistent(btp->conn, btp->timeout, &req, shell_request_size(strlen(req.input)+3), NULL, 0) == 0)
		goto out;

	threadargs.btp = btp;
	threadargs.timeout = btp->timeout;
	threadargs.count = count;

	if (pthread_create(&treader, NULL, reader, &threadargs) < 0)
		goto out;

	if (pthread_create(&twriter, NULL, writer, &threadargs) < 0)
		goto out;

	pthread_join(treader, NULL);
	pthread_cancel(twriter);

	ret = BTP_EOK;

out:
	tret += tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios[0]);
	tret += tcsetattr(STDOUT_FILENO, TCSAFLUSH, &orig_termios[1]);
	tret += tcsetattr(STDERR_FILENO, TCSAFLUSH, &orig_termios[2]);

	if (tret < 0)
		return BTP_EINVAL;

	return ret;
}

int btp_client_complete(struct btp_context *btp)
{
	struct btp_completerequest req;
	struct btp_completereply rep;
	uint32_t checksum;

	if (!btp || (btp->state != STATE_UPLOAD && btp->state != STATE_DOWNLOAD))
		return BTP_EINVAL;

	req.type = BTP_COMPLETE_REQUEST;

	/* Truncate */
	if (btp->state == STATE_DOWNLOAD && btp->complete) {
		if (ftruncate(btp->fd, btp->size) < 0)
			return BTP_EIO;

		if (btp_crc32c_fd(btp->fd, &checksum, 0) < 0)
			return BTP_EIO;

		fsync(btp->fd);
		close(btp->fd);

		if (strlen(btp->filename_partial) > 0) {
			if (rename(btp->filename_partial, btp->filename) < 0)
				return BTP_EIO;
		}

		if (btp->checksum != checksum)
			return BTP_ESTALE;
	}

	if (csp_transaction_persistent(btp->conn, btp->timeout, &req, sizeof(req), &rep, sizeof(rep)) == 0)
		return BTP_ETIMEDOUT;

	return BTP_EOK;
}
