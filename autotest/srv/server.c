// static library headers
#include <srv/server.h>

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
  fs->init(NULL);
  service_fs_file_handle_t hfile = fs->file_handle_malloc();
  fs->open(hfile, "autotest/logs/logfile.txt", fs->mode.WRONLY | fs->mode.CREAT | fs->mode.APPEND);

  // open a COM port
  service_com_handle_t hcom = com->com_handle_malloc();
  service_com_conf_t conf = {
    .speed = 460800,
  };
  com->open(hcom, (service_com_device_t)arg, &conf);

  // buffer for incoming commands
  uint8_t buff[64] = { 0 };

  // process commands
  bool keep_running = true;
  while(keep_running) {
    // read data from COM port
    size_t len = com->read(hcom, (void*)buff, 64);

    if(len > 1) {
      // log everything
      log_to_file(hfile, buff, len);

      int ret = server_process_cmd(hcom, buff, len);
      if(ret == DRIVER_TEMPLATE_CMD_KILL) {
        keep_running = false;
      }
    }
  }

  // all done, clean up
  com->close(hcom);
  com->com_handle_free(hcom);
  fs->close(hfile);
  fs->file_handle_free(hfile);
}

int main(int argc, char *argv[]) {
  const char* port = "dev/ttyS0";

  server_set_backends(fs, os, com);

  // set up a thread to handle incoming communicaton
  service_os_thread_handle_t thread = NULL;
  os->thread_create(&thread, NULL, thread_com, (void*)port);

  // wait for exit
  os->thread_wait_for(thread);
  return(0);
}
