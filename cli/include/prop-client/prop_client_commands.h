/*
 * Copyright (c) 2016-2021 Satlab A/S <satlab@satlab.com>
 * All rights reserved. Do not distribute without permission.
 */

/**
 * @file
 *
 * This file contains slash (https://github.com/satlab/slash) debug commands
 * for the remote property functions. The SL_PROP_CLIENT_COMMANDS_GENERATE macro is used by subsystem command implementations to generate subcommands with the correct
 */

#ifndef _SL_PROP_CLIENT_COMMANDS_H_
#define _SL_PROP_CLIENT_COMMANDS_H_

#include <stdint.h>

#include <prop-client/prop_proto.h>

/** Slash property command context */
struct sl_prop_client_command_context {
	uint8_t *node;
	uint8_t port;
	uint32_t *timeout;
	const struct prop_subsys_spec *spec;
};

#endif /* _SL_PROP_CLIENT_COMMANDS_H_ */
