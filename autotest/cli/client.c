// static library headers
#include <cli/client.h>

// standard libraries
#include <string.h>
#include <stdio.h>

// SALLY libraries
#include <service/service.h>
#include <service/service_filesystem.h>
#include <service/service_com.h>

// headers shared for both client and server
#include "packets.h"

// SALLY backends - set by CMake
extern service_fs_t FS_BACKEND;
service_fs_t* fs = &FS_BACKEND;
extern service_os_t OS_BACKEND;
service_os_t* os = &OS_BACKEND;
extern service_com_t COM_BACKEND;
service_com_t* com = &COM_BACKEND;

int main(int argc, char *argv[]) {
  const char* port = "dev/ttyS1";

  client_set_backends(fs, os, com);

  // open a COM port
  service_com_handle_t hcom = com->com_handle_malloc();
  service_com_conf_t conf = {
    .speed = 460800,
  };
  com->open(hcom, (service_com_device_t)port, conf);

  // send some server requests
  for(uint8_t i = 0; i < 10; i++) {
    // send a ping
    client_cmd_send(hcom, DRIVER_TEMPLATE_CMD_PING, NULL, 0);

    // try to get a reply
    uint8_t ping_buff[16];
    if(client_rpl_get(hcom, ping_buff, strlen(DRIVER_TEMPLATE_CMD_PING_RPL)) > 0) {
      printf("Ping #%d got reply: %s\n", i, ping_buff);
    }
    os->thread_delay(10);
  }

  // send an echo
  const char* msg = "This is an echo message";
  client_cmd_send(hcom, DRIVER_TEMPLATE_CMD_ECHO, (uint8_t*)msg, strlen(msg));

  // try to get a reply
  uint8_t echo_buff[32];
  if(client_rpl_get(hcom, echo_buff, strlen(msg) + strlen(DRIVER_TEMPLATE_CMD_LINE_END)) > 0) {
    printf("Echo got reply: %s\n", echo_buff);
  }
  os->thread_delay(10);

  // un-alive the server
  client_cmd_send(hcom, DRIVER_TEMPLATE_CMD_KILL, NULL, 0);

  // and now yourself
  com->close(hcom);
  com->com_handle_free(hcom);
  return(0);
}
