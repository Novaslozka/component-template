// app headers
#include "server/server.h"
#include "server/server_packets.h"

// standard libraries
#include <string.h>

// SALLY libraries
#include <service/service.h>
#include <service/service_filesystem.h>
#include <service/service_com.h>

// SALLY backends provided by the library user
static service_fs_t* server_fs = NULL;
static service_os_t* server_os = NULL;
static service_com_t* server_com = NULL;

void server_set_backends(service_fs_t* fs, service_os_t* os, service_com_t* com) {
  server_fs = fs;
  server_os = os;
  server_com = com;
}

int server_process_cmd(service_com_handle_t hcom, uint8_t* buff, size_t len) {
  // an example of a very simple processing
  switch(buff[0]) {
    case SERVER_CMD_PING: {
      // on ping, just send the defined reply
      const char* rpl = SERVER_CMD_PING_RPL;
      server_com->write(hcom, (void*)rpl, strlen(rpl));
    } break;

    case SERVER_CMD_ECHO: {
      // on echo, sned everything backeexcept the command byte
      uint8_t* pos = strstr(buff, SERVER_CMD_LINE_END);
      if(pos != NULL) {
        size_t echo_len = pos-buff-1;
        server_com->write(hcom, (void*)&buff[1], echo_len);
        server_com->write(hcom, (void*)SERVER_CMD_LINE_END, strlen(SERVER_CMD_LINE_END));
      }
    } break;

    case SERVER_CMD_KILL:
      // on kill, stop processing 
      return(SERVER_CMD_KILL);

    default:
      break;
  }

  return(0);
}
