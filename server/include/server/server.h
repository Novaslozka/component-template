#ifndef _DRIVER_TEMPLATE_SERVER_H
#define _DRIVER_TEMPLATE_SERVER_H

// SALLY libraries
#include <service/service.h>
#include <service/service_filesystem.h>
#include <service/service_com.h>

void server_set_backends(service_fs_t* fs, service_os_t* os, service_com_t* com);
int server_process_cmd(service_com_handle_t hcom, uint8_t* buff, size_t len);

#endif
