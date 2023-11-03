/*
 * Copyright (c) 2013-2021 Satlab A/S <satlab@satlab.com>
 * All rights reserved. Do not distribute without permission.
 */

#ifndef _SL_BTP_CLIENT_H_
#define _SL_BTP_CLIENT_H_

#include <stdint.h>
#include <stdbool.h>

#include <csp/csp.h>

#include <btp/types.h>

/**
 * Magic offset value to indicate next available block in
 * btp_client_get_blocks/btp_client_send_blocks offset argument.  
 */
#define BTP_OFFSET_NEXT	(UINT32_MAX)

/** BTP context struct */
struct btp_context {
	uint8_t host;
	uint8_t port;
	uint32_t timeout;
	unsigned int attempts;
	bool complete;
	bool raw;
	enum btp_xfer_state state;
	csp_conn_t *conn;
	int fd;
	char filename[BLOB_NAME_SIZE];
	char filename_partial[BLOB_NAME_SIZE + 5];
	uint32_t blocks;
	uint32_t block_size;
	uint32_t size;
	uint32_t progress;
	uint32_t checksum;
	uint32_t next;
	uint32_t bits;
	uint8_t *block_status;
};

/**
 * @brief Progress callback function
 *
 * This function is repeatedly called by the BTP stack when transferring data,
 * and can be used to show the transfer progress.
 *
 * @param btp BTP context
 * @param arg Pointer value passed through btp_client_get_blocks/send_blocks.
 */
typedef void (*progress_cb)(struct btp_context *btp, void *arg);

/**
 * @brief Open new BTP connection to host
 *
 * @param host CSP host to connect to
 * @param port CSP port on host with BTP server
 * @param timeout Timeout in milliseconds
 * @param opts Extra CSP connection options
 * @param attempts Number of times to try each BTP operation
 *
 * @return A btp_context object on success, NULL on failure
 */
struct btp_context *btp_client_connect(uint8_t host,
				       uint8_t port,
				       uint32_t timeout,
				       uint32_t opts,
				       unsigned int attempts);

/**
 * @brief Close an active BTP connection
 *
 * @param btp An open btp_context
 *
 * @return 0 on success, error code on failure
 */
int btp_client_disconnect(struct btp_context *btp);

/**
 * @brief Begin upload of a file
 *
 * @param btp An open btp_context from btp_client_connect
 * @param backend Backend to upload to
 * @param remotepath Absolute path where file should be uploaded
 * @param localpath Absolute path to the file to upload
 * @param block_size Number of bytes to upload per block
 * @param force If true, force upload even if transfer is already complete
 *
 * @return 0 on success, error code on failure
 */
int btp_client_upload(struct btp_context *btp,
		      char *backend,
		      char *remotepath,
		      char *localpath,
		      uint8_t block_size,
		      uint32_t timeout_csum,
		      uint32_t timeout_server,
		      bool force);

/**
 * @brief Begin download of a file
 *
 * @param btp An open btp_context from btp_client_connect
 * @param backend Backend to download from
 * @param remotepath Absolute path to remote file to download
 * @param localpath Absolute path where downloaded file should be stored
 * @param block_size Number of bytes to download per block
 * @param force If true, force download even if transfer is already complete
 *
 * @return 0 on success, error code on failure
 */
int btp_client_download(struct btp_context *btp,
		        char *backend,
			char *remotepath,
			char *localpath,
			uint8_t block_size,
		        uint32_t timeout_csum,
			uint32_t timeout_server,
			bool force);

/**
 * @brief Update upload status
 *
 * @param btp An open btp_context from btp_client_connect
 *
 * @return 0 on success, error code on failure
 */
int btp_client_request_status(struct btp_context *btp);

/**
 * @brief Send download status
 *
 * @param btp An open btp_context from btp_client_connect
 *
 * @return 0 on success, error code on failure
 */
int btp_client_send_status(struct btp_context *btp);

/**
 * @brief Request blocks in download
 *
 * @param btp An open btp_context from btp_client_connect
 * @param offset Start from block offset
 * @param count Number of blocks to download
 * @param cb Progress callback function
 * @param cbarg Argument passed to callback
 *
 * @return 0 on success, error code on failure
 */
int btp_client_get_blocks(struct btp_context *btp, unsigned int offset,
			  unsigned int count, progress_cb cb,
			  void *cbarg);

/**
 * @brief Send blocks in upload
 *
 * @param btp An open btp_context from btp_client_connect
 * @param offset Start from block offset
 * @param count Number of blocks to upload
 * @param cb Progress callback function
 * @param cbarg Argument passed to callback
 *
 * @return 0 on success, error code on failure
 */
int btp_client_send_blocks(struct btp_context *btp, unsigned int offset,
			   unsigned int count, progress_cb cb,
			   void *cbarg);

/**
 * @brief Finish active transfer
 *
 * @param btp An open btp_context from btp_client_connect
 *
 * @return 0 on success, error code on failure
 */
int btp_client_complete(struct btp_context *btp);

/**
 * @brief Test is tranfer is finished
 *
 * @param btp An open btp_context from btp_client_connect
 *
 * @return true if transfer is complete, false otherwise
 */
bool btp_client_finished(struct btp_context *btp);

/**
 * @brief Remove file
 *
 * @param btp An open btp_context from btp_client_connect
 * @param backend Backend to remove file on
 * @param path Absolute path to remote file to remove
 *
 * @return 0 on success, error code on failure
 */
int btp_client_remove(struct btp_context *btp, char *backend,
		      char *path);

/**
 * @brief Move file
 *
 * @param btp An open btp_context from btp_client_connect
 * @param backend Backend to move file on
 * @param frompath Absolute path to file to move
 * @param topath Absolute path where file should be moved
 *
 * @return 0 on success, error code on failure
 */
int btp_client_move(struct btp_context *btp, char *backend,
		    char *frompath, char *topath);

/**
 * @brief Copy file
 *
 * @param btp An open btp_context from btp_client_connect
 * @param backend Backend to copy file on
 * @param frompath Absolute path to file to copy
 * @param topath Absolute path where file should be copied
 *
 * @return 0 on success, error code on failure
 */
int btp_client_copy(struct btp_context *btp, char *backend,
		    char *frompath, char *topath);

/**
 * @brief List files
 *
 * @param btp An open btp_context from btp_client_connect
 * @param count Maximum number of files to list
 * @param backend Backend to list files on
 * @param name Absolute path to list
 *
 * @return 0 on success, error code on failure
 */
int btp_client_list(struct btp_context *btp, uint16_t count,
		    char *backend, char *name);

/**
 * @brief Create shell connection
 *
 * @param btp An open btp_context from btp_client_connect
 * @param count Maximum number of reply packets
 * @param cmd Command to execute. Use cmd[0] = '\0' for interactive mode
 * @param raw If true, transmit one packet per character
 *
 * @return 0 on success, error code on failure
 */
int btp_client_shell(struct btp_context *btp, uint16_t count,
		     char *cmd, bool raw);

#endif /* _BTP_CLIENT_H_ */
