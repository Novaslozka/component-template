// static library headers
#include <template/client.h>
#include <template/server.h>

// standard libraries
#include <string.h>
#include <stdio.h>

// SALLY libraries
#include <service/service.h>
#include <service/service_filesystem.h>
#include <service/service_com.h>

// headers shared for both client and server
#include "packets.h"

// munit testing framework
#include "munit.h"

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

// mutex to protect the flag
service_os_mutex_handle_t mux = NULL;

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
  fs->init(NULL);
  service_fs_file_handle_t hfile = fs->file_handle_malloc();
  fs->open(hfile, "autotest/logs/logfile_server.txt", fs->mode.WRONLY | fs->mode.CREAT | fs->mode.APPEND);

  service_com_handle_t hcom = com->com_handle_malloc();
  int port = COM_PORT_SERVER;
  com->open(hcom, (service_com_device_t)&port, &conf);

  // buffer for incoming commands
  uint8_t buff[64] = { 0 };

  os->mutex_lock(mux, 0);
  server_ready = true;
  os->mutex_unlock(mux);

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

  // kill own thread
  os->thread_delete(NULL);
}

static MunitResult
test_ping(const MunitParameter params[], void* fixture) {
  service_com_handle_t hcom = (service_com_handle_t)fixture;

  // send a ping and get the reply
  char ping_buff[16] = { 0 };
  int16_t ret = client_ping(hcom, ping_buff);

  // check we have the data and that it matches the expected "Hello World!" reply
  munit_assert_int(ret, ==, 0);
  munit_assert_string_equal(ping_buff, DRIVER_TEMPLATE_CMD_PING_RPL);

  return(MUNIT_OK);
}

static MunitResult
test_echo(const MunitParameter params[], void* fixture) {
  service_com_handle_t hcom = (service_com_handle_t)fixture;

  // send an echo request and get the reply
  char echo_out_buff[] = "Echo this!";
  char echo_in_buff[16] = { 0 };
  int16_t ret = client_echo(hcom, echo_in_buff, echo_out_buff);

  // check we have the data and that it matches the expected "Hello World!" reply
  munit_assert_int(ret, ==, 0);
  munit_assert_string_equal(echo_in_buff, echo_out_buff);

  return(MUNIT_OK);
}

static MunitResult
test_kill(const MunitParameter params[], void* fixture) {
  service_com_handle_t hcom = (service_com_handle_t)fixture;

  // send the kill command and check the status
  int16_t ret = client_kill_server(hcom);
  munit_assert_int(ret, ==, 0);
  return(MUNIT_OK);
}

static void*
test_client_setup(const MunitParameter params[], void* user_data) {
  (void)params;
  (void)user_data;

  client_set_backends(fs, os, com);

  // open port
  service_com_conf_t conf = { .speed = 0 };
  service_com_handle_t hcom = com->com_handle_malloc();
  int port = COM_PORT_CLIENT;
  com->open(hcom, (service_com_device_t)&port, &conf);

  // wait until the server is ready
  bool wait = true;
  while(wait) {
    if(os->mutex_lock(mux, 10) != SERVICE_OS_PASS) {
      continue;
    }

    if(server_ready) {
      wait = false;
    }

    os->mutex_unlock(mux);

  }

  // return the COM handle, this will be provided to test
  return((void*)hcom);
}

static void
test_client_teardown(void* fixture) {
  service_com_handle_t hcom = (service_com_handle_t)fixture;
  com->close(hcom);
  com->com_handle_free(hcom);
}

static MunitTest test_suite_tests[] = {
  // name - test - setup - teardown - options - params
  { (char*) "/driver/ping", test_ping, test_client_setup, test_client_teardown, MUNIT_TEST_OPTION_NONE, NULL },
  { (char*) "/driver/echo", test_echo, test_client_setup, test_client_teardown, MUNIT_TEST_OPTION_NONE, NULL },
  { (char*) "/driver/kill", test_kill, test_client_setup, test_client_teardown, MUNIT_TEST_OPTION_NONE, NULL },
  { NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL }
};

// see https://github.com/nemequ/munit
static const MunitSuite test_suite = {
  // test suite name
  (char*) "top",
  // array of the tests
  test_suite_tests,
  // additional test suites
  NULL,
  // number of iterations per test
  1,
  // defaults options
  MUNIT_SUITE_OPTION_NONE
};

int main(int argc, char *argv[MUNIT_ARRAY_PARAM(argc + 1)]) {
  // set backends for client and server
  server_set_backends(fs, os, com);

  // create the mutex
  mux = os->mutex_create();

  // set up server thread to handle communicaton coming from the client
  service_os_thread_handle_t th_server = NULL;
  os->thread_create(&th_server, NULL, thread_server, NULL);

  // run the tests
  return(munit_suite_main(&test_suite, NULL, argc, argv));
}