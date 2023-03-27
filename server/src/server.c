// app headers
#include "server.h"

// standard libraries
#include <string.h>
#include <stdio.h>

// SALLY libraries
#include <service/service.h>
#include <service/service_filesystem.h>
#include <service/service_com.h>

// SALLY backends - set by CMake
extern service_fs_t FS_BACKEND;
service_fs_t* fs = &FS_BACKEND;
extern service_os_t OS_BACKEND;
service_os_t* os = &OS_BACKEND;
extern service_com_t COM_BACKEND;
service_com_t* com_tty = &COM_BACKEND;

// a helper function to log data to file
void log_to_file(service_fs_file_handle_t file, uint8_t* data, size_t len) {
  char buff[16];
  for(size_t i = 0; i < len; i++) {
    sprintf(buff, "%02X ", data[i]);
    fs->write(file, buff, 3);
  }
  buff[0] = '\n';
  fs->write(file, (void*)buff, 1);
}

void thread_com(void* arg) {
  // open a file for logging
  fs->init();
  service_fs_file_handle_t hfile = fs->file_handle_malloc();
  fs->open(hfile, "logfile.txt", fs->mode.WRONLY | fs->mode.CREAT | fs->mode.APPEND);

  // open a COM port
  service_com_handle_t hcom = com_tty->com_handle_malloc();
  service_com_conf_t conf = {
    .speed = 460800,
  };
  com_tty->open(hcom, (service_com_device_t)arg, conf);

  // buffer for incoming commands
  const size_t buff_size = 64;
  uint8_t buff[buff_size];

  // process commands
  bool keep_running = true;
  while(keep_running) {
    // read data from COM port
    size_t len = com_tty->read(hcom, (void*)buff, buff_size);

    if(len > 1) {
      // log everything
      log_to_file(hfile, buff, len);

      // an example of a very simple processing
      switch(buff[0]) {
        case SERVER_CMD_PING: {
          // on ping, just send the defined reply
          const char* rpl = SERVER_CMD_PING_RPL;
          com_tty->write(hcom, (void*)rpl, strlen(rpl));
        } break;

        case SERVER_CMD_ECHO: {
          // on echo, sned everything backeexcept the command byte
          uint8_t* pos = strstr(buff, SERVER_CMD_LINE_END);
          if(pos != NULL) {
            size_t echo_len = pos-buff-1;
            com_tty->write(hcom, (void*)&buff[1], echo_len);
            com_tty->write(hcom, (void*)SERVER_CMD_LINE_END, strlen(SERVER_CMD_LINE_END));
          }
        } break;

        case SERVER_CMD_KILL:
          // on kill, stop processing 
          keep_running = false;
          break;

        default:
          break;
      }
    }
  }

  // all done, clean up
  com_tty->close(hcom);
  com_tty->com_handle_free(hcom);
  fs->close(hfile);
  fs->file_handle_free(hfile);
}

int main(int argc, char *argv[]) {
  const char* port = "dev/ttyS0";

  // set up a thread to handle incoming communicaton
  service_os_thread_handle_t thread;
  os->thread_create(&thread, NULL, thread_com, (void*)port);

  // wait for exit
  os->thread_wait_for(thread);
  return(0);
}
