#ifndef CMD_CLI_H_
#define CMD_CLI_H_

#include <service/service_com.h>

/*!
    \brief Register client commands.
    This signature is mandatory for VCOM plugins!
*/
void register_cli_cmds(void);

#endif