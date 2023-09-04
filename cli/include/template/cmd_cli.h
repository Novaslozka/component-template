#ifndef CMD_CLI_H_
#define CMD_CLI_H_

#include <service/service_com.h>

#if !defined(STATIC_BUILD)
/*!
    \brief Register client commands.
    This signature is mandatory for VCOM plugins!
*/
void register_cli_cmds(void);
#endif

/*!
    \brief Register client commands.
    This function can be used when multiple statically linked client libraries are used.
    Note that the name has to be unique for the client!
*/
void register_cli_example_cmds(void);

#endif