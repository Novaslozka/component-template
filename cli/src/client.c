// app headers
#include "cli/client.h"

// standard libraries
#include <string.h>
#include <stdio.h>

// SALLY libraries
#include <service/service.h>
#include <service/service_filesystem.h>
#include <service/service_com.h>

// headers shared for both client and server
#include "packets.h"

// SALLY backends provided by the library user
static service_fs_t* client_fs = NULL;
static service_os_t* client_os = NULL;
static service_com_t* client_com = NULL;

void client_set_backends(service_fs_t* fs, service_os_t* os, service_com_t* com) {
  client_fs = fs;
  client_os = os;
  client_com = com;
}

// the following command/reply functions are for illustration only!
void client_cmd_send(service_com_handle_t hcom, uint8_t cmd_id, uint8_t* data, uint8_t data_len) {
  uint8_t buff[64] = { 0 };
  buff[0] = cmd_id;

  if((data != NULL) && (data_len > 0)) {
    memcpy(&buff[1], data, data_len);
  }

  memcpy(&buff[1 + data_len], DRIVER_TEMPLATE_CMD_LINE_END, strlen(DRIVER_TEMPLATE_CMD_LINE_END));
  client_com->write(hcom, buff, 1 + data_len + strlen(DRIVER_TEMPLATE_CMD_LINE_END));
}

size_t client_rpl_get(service_com_handle_t hcom, uint8_t* buff, size_t target_len) {
  size_t len = 0;
  while(len < target_len) {
    size_t res = client_com->read(hcom, buff + len, target_len);
    if(res >= 0) {
      len += res;
    } else {
      return(res);
    }
  }
  return(len);
}
