#ifndef _DRIVER_TEMPLATE_PACKETS_H
#define _DRIVER_TEMPLATE_PACKETS_H

// this file defines the communication interface between client and server
// usually, this is an implementation of the device ICD
#define DRIVER_TEMPLATE_CMD_LINE_END         "\r\n"
#define DRIVER_TEMPLATE_CMD_PING             (0x00)
#define DRIVER_TEMPLATE_CMD_PING_RPL         "Hello World!" DRIVER_TEMPLATE_CMD_LINE_END
#define DRIVER_TEMPLATE_CMD_ECHO             (0x01)
#define DRIVER_TEMPLATE_CMD_KILL             (0x02)

#endif
