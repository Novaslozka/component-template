#ifndef _DRIVER_TEMPLATE_SERVER_H
#define _DRIVER_TEMPLATE_SERVER_H

// this file defines the communication interface between client and server
// usually, this is an implementation of the device ICD
#define SERVER_CMD_LINE_END         "\r\n"
#define SERVER_CMD_PING             (0x00)
#define SERVER_CMD_PING_RPL         "Hello World!" SERVER_CMD_LINE_END
#define SERVER_CMD_ECHO             (0x01)
#define SERVER_CMD_KILL             (0x02)

#endif
