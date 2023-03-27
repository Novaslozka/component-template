#ifndef _DRIVER_TEMPLATE_CLIENT_H
#define _DRIVER_TEMPLATE_CLIENT_H

// SALLY libraries
#include <service/service.h>
#include <service/service_filesystem.h>
#include <service/service_com.h>

void client_set_backends(service_fs_t* fs, service_os_t* os, service_com_t* com);
void client_cmd_send(service_com_handle_t hcom, uint8_t cmd_id, uint8_t* data, uint8_t data_len);
size_t client_rpl_get(service_com_handle_t hcom, uint8_t* buff, size_t target_len);

#endif
