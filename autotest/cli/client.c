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
  com->open(hcom, (service_com_device_t)port, &conf);

  // send some server requests
  for(uint8_t i = 0; i < 10; i++) {
    // send a ping
    uint8_t ping_buff[16];
    if(client_ping(hcom, ping_buff) == 0) {
      printf("Ping #%d got reply: %s\n", i, ping_buff);
    }
    os->thread_delay(10);
  }

  // send an echo
  char echo_out_buff[] = "Echo this!";
  char echo_in_buff[32] = { 0 };
  if(client_echo(hcom, echo_out_buff, echo_in_buff) == 0) {
    printf("Echo got reply: %s\n", echo_in_buff);
  }
  os->thread_delay(10);

  // send a command to un-alive the server
  client_kill_server(hcom);

  // and now yourself
  com->close(hcom);
  com->com_handle_free(hcom);
  return(0);
}
