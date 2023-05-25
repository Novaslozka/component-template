// static library headers
#include <client/client.h>
#include <server/server.h>
#include <server/server_packets.h>

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

// use memory COM emulation
extern service_com_t backend_memcom;
service_com_t* com = &backend_memcom;

// flag to signal server startup is finished
volatile bool server_ready = false;

// which COM ports to use for server and client
// memcom addresses ports as integers, ports N and N-1 are connected together
#define COM_PORT_CLIENT         (0)
#define COM_PORT_SERVER         (1)

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

void thread_server(void* arg) {
  service_com_conf_t conf = { .speed = 0 };

  // open a file for logging
  fs->init();
  service_fs_file_handle_t hfile = fs->file_handle_malloc();
  fs->open(hfile, "autotest/logs/logfile_server.txt", fs->mode.WRONLY | fs->mode.CREAT | fs->mode.APPEND);

  service_com_handle_t hcom = com->com_handle_malloc();
  int port = COM_PORT_SERVER;
  com->open(hcom, (service_com_device_t)&port, conf);

  // buffer for incoming commands
  const size_t buff_size = 64;
  uint8_t buff[buff_size];

  server_ready = true;

  // process commands
  bool keep_running = true;
  while(keep_running) {
    // read data from COM port
    size_t len = com->available(hcom);
    if(len > 0) {
      com->read(hcom, (void*)buff, len);
    } else {
      os->thread_delay(1);
    }

    if(len > 1) {
      // log everything
      log_to_file(hfile, buff, len);

      int ret = server_process_cmd(hcom, buff, len);
      if(ret == SERVER_CMD_KILL) {
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

void thread_client(void* arg) {
  service_com_conf_t conf = { .speed = 0 };

  service_com_handle_t hcom = com->com_handle_malloc();
  int port = COM_PORT_CLIENT;
  com->open(hcom, (service_com_device_t)&port, conf);

  // wait until the server is ready
  while(!server_ready) {
    os->thread_delay(100);
  }

  // send some server requests
  for(uint8_t i = 0; i < 10; i++) {
    // send a ping
    client_cmd_send(hcom, SERVER_CMD_PING, NULL, 0);

    // try to get a reply
    uint8_t ping_buff[16];
    if(client_rpl_get(hcom, ping_buff, strlen(SERVER_CMD_PING_RPL)) > 0) {
      printf("Ping #%d got reply: %s\n", i, ping_buff);
    }
    os->thread_delay(10);
  }

  com->close(hcom);
  com->com_handle_free(hcom);
}

int main(int argc, char *argv[]) {
  // set backends for client and server
  client_set_backends(fs, os, com);
  server_set_backends(fs, os, com);

  // set up server thread to handle communicaton coming from the client
  service_os_thread_handle_t th_server;
  os->thread_create(&th_server, NULL, thread_server, NULL);

  // set up client thread to do the testing
  service_os_thread_handle_t th_client;
  os->thread_create(&th_client, NULL, thread_client, NULL);

  // wait for exit
  os->thread_wait_for(th_client);
  return(0);
}