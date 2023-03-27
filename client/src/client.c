// standard libraries
#include <string.h>
#include <stdio.h>

// SALLY libraries
#include <service/service.h>
#include <service/service_filesystem.h>
#include <service/service_com.h>

// server commands
#include "../../server/include/server.h"

// SALLY backends - set by CMake
extern service_fs_t FS_BACKEND;
service_fs_t* fs = &FS_BACKEND;
extern service_os_t OS_BACKEND;
service_os_t* os = &OS_BACKEND;
extern service_com_t COM_BACKEND;
service_com_t* com_tty = &COM_BACKEND;

// the following command/reply functions are for illustration only!
void cmd_send(service_com_handle_t hcom, uint8_t cmd_id, uint8_t* data, uint8_t data_len) {
  const size_t buff_size = 64;
  uint8_t buff[buff_size];
  buff[0] = cmd_id;

  if((data != NULL) && (data_len > 0)) {
    memcpy(&buff[1], data, data_len);
  }

  memcpy(&buff[1 + data_len], SERVER_CMD_LINE_END, strlen(SERVER_CMD_LINE_END));
  com_tty->write(hcom, buff, 1 + data_len + strlen(SERVER_CMD_LINE_END));
}

size_t rpl_get(service_com_handle_t hcom, uint8_t* buff, size_t target_len) {
  size_t len = 0;
  while(len < target_len) {
    size_t res = com_tty->read(hcom, buff + len, target_len);
    if(res >= 0) {
      len += res;
    } else {
      return(res);
    }
  }
  return(len);
}

int main(int argc, char *argv[]) {
  const char* port = "dev/ttyS1";

  // open a COM port
  service_com_handle_t hcom = com_tty->com_handle_malloc();
  service_com_conf_t conf = {
    .speed = 460800,
  };
  com_tty->open(hcom, (service_com_device_t)port, conf);

  // send some server requests
  for(uint8_t i = 0; i < 10; i++) {
    // send a ping
    cmd_send(hcom, SERVER_CMD_PING, NULL, 0);

    // try to get a reply
    uint8_t ping_buff[16];
    if(rpl_get(hcom, ping_buff, strlen(SERVER_CMD_PING_RPL)) > 0) {
      printf("Ping #%d got reply: %s\n", i, ping_buff);
    }
    os->thread_delay(10);
  }

  // send an echo
  const char* msg = "This is an echo message";
  cmd_send(hcom, SERVER_CMD_ECHO, (uint8_t*)msg, strlen(msg));

  // try to get a reply
  uint8_t echo_buff[32];
  if(rpl_get(hcom, echo_buff, strlen(msg) + strlen(SERVER_CMD_LINE_END)) > 0) {
    printf("Echo got reply: %s\n", echo_buff);
  }
  os->thread_delay(10);

  // un-alive the server
  cmd_send(hcom, SERVER_CMD_KILL, NULL, 0);

  // and now yourself
  com_tty->close(hcom);
  com_tty->com_handle_free(hcom);
  return(0);
}
