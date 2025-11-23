// static library headers
#include <template/client_csp.h>
#include <template/server_csp.h>

// standard libraries
#include <string.h>
#include <stdio.h>

// CSP
#include <csp/csp.h>

// SALLY libraries
#include <service/service.h>
#include <service/service_err_codes.h>
#include <service/service_com.h>
#include <service-csp/csp_pld_txn.h>

// lpldgen-generated packet descriptions
#include <pld_driver_template.h>

// headers shared for both client and server
#include "packets.h"

// munit testing framework
#include "munit.h"

// CSP addresses - the one for server is in lpldgen, the one for client can be anything else
#define CSP_NODE_SRV        (DRIVER_TEMPLATE_PING_DST_ADDR)
#define CSP_NODE_CLI        (DRIVER_TEMPLATE_PING_DST_ADDR + 1)

// set by build configuration tool
#ifndef GITREV
#define GITREV "no-git"
#endif

// SALLY backends - set by build configuration tool
extern service_os_t OS_BACKEND;
service_os_t* os = &OS_BACKEND;

// flag to signal server startup is finished
volatile bool server_ready = false;

// mutex to protect the flag
service_os_mutex_handle_t mux = NULL;

// this flag will be used to shut down the server thread
static volatile bool server_shutdown_flag = false;

// function to set the flag from server side
// this is a SALLY callback registered later, so the signature is fixed
int server_shutdown(void** args) {
  (void)args;
  server_shutdown_flag = true;
  return(SERVICE_ERR_NONE);
}

void thread_server(void* arg) {
  // set up the server SALLY service configuration
  service_desc_t example_service = {
    // CSP port of the server (defined in lpldgen)
    .port = DRIVER_TEMPLATE_PING_DST_PORT,
    // callback to be executed on packet reception
    .cb_packet_read = server_process_packet,

    // this is strictly not necessary for the purposes of this example,
    // but to make it complete, we use SALLY registered callback to execute a custom function
    // from the server implementation (in this case, shutting down the server thread on kill command)
    .cb_reg = server_register_callback,
    .cbs = { server_shutdown, NULL, NULL, NULL },
    
    // because this example does almost nothing, most of service functionality is not used
    // feel free to dig into the SALLY documentation
    .th_top = SERVICE_THREAD_NONE,
    .th_spawned = SERVICE_THREAD_NONE,
    .log_flag_index = { SERVICE_LOGGING_LEVEL_UNUSED, SERVICE_LOGGING_LEVEL_UNUSED },
    .cb_log_setup = { NULL, NULL },
    .cb_conn_accepted = NULL,
    .cb_init = NULL,
    .cb_init_args = NULL,
  };

  // register callbacks and spawn the service
  service_desc_t* service_list[] = {
    &example_service,
    NULL,
  };
  service_register_csp_callbacks(service_list);
  int ret = service_spawn(&example_service);
  if(ret == SERVICE_ERR_NONE) {
    fprintf(stdout, "Service spawned\r\n");
  } else {
    fprintf(stderr, "Service failed to spawn, code %d\r\n", ret);
    return;
  }
  
  // set CSP configuration
  csp_conf_t csp_conf = { 0 };
  csp_conf_get_defaults(&csp_conf);
  csp_conf.address = DRIVER_TEMPLATE_PING_DST_ADDR; // from lpldgen definition
  csp_conf.hostname = "example";
  csp_conf.model = "server";
  csp_conf.revision = GITREV;
  ret = csp_init(&csp_conf);
  if(ret == CSP_ERR_NONE) {
    fprintf(stdout, "CSP init OK\r\n");
  } else {
    fprintf(stderr, "CSP failed to initialize, code %d\r\n", ret);
    return;
  }

  // since both server and client are running in the same application,
  // we can just use the loopback interface
  csp_iface_t* csp_if_loop = csp_iflist_get_by_name("LOOP");

  // set the default route and start router
  (void)csp_route_set(CSP_DEFAULT_ROUTE, csp_if_loop, CSP_NODE_MAC);
  (void)csp_route_start_task(8192*16, 0);

  os->mutex_lock(mux, 0);
  server_ready = true;
  os->mutex_unlock(mux);

  // start listening for connections
  csp_conn_t *conn = NULL;
  csp_packet_t *packet = NULL;
  csp_socket_t *socket = csp_socket(0);
  (void)csp_bind(socket, CSP_ANY);
  (void)csp_listen(socket, 10);
  fprintf(stdout, "CSP server running and waiting for connections, stop with Ctrl+C\r\n");
  while(!server_shutdown_flag) {
    // check incoming connection
    conn = csp_accept(socket, 1000);

    // only continue if a valid connection is present
    if(conn == NULL) {
      continue;
    }

    fprintf(stdout, "CSP got connection!\r\n");

    // handle services which have a "connection accepted" callback
    if(service_exec_accept_callback(conn)) {
      fprintf(stdout, "CSP connection handled by SALLY callback\r\n");
      continue;
    }

    // read incoming data
    while((packet = csp_read(conn, 0)) != NULL) {
      // handle services which have a "packet read" callback
      int port = csp_conn_dport(conn);
      if(service_exec_read_callback(port, packet, conn)) {
        fprintf(stdout, "CSP packet to port %d handled by SALLY callback\r\n", port);
        continue;
      }

      switch(port) {
        // CSP service handlers (ping, ident, etc.)
        default:
          csp_service_handler(conn, packet);
          break;
      }
    }

    if(conn) {
      csp_close(conn);
    }
  }

  // kill own thread
  os->thread_delete(NULL);
}

static MunitResult
test_ping(const MunitParameter params[], void* fixture) {
  (void)fixture;

  // send a ping and get the reply
  char ping_buff[16] = { 0 };
  int16_t ret = client_ping(CSP_NODE_SRV, ping_buff);

  // check we have the data and that it matches the expected "Hello World!" reply
  munit_assert_int(ret, ==, 0);
  munit_assert_string_equal(ping_buff, DRIVER_TEMPLATE_CMD_PING_RPL);

  return(MUNIT_OK);
}

static MunitResult
test_echo(const MunitParameter params[], void* fixture) {
  (void)fixture;

  // send an echo request and get the reply
  char echo_out_buff[] = "Echo this!";
  char echo_in_buff[DRIVER_TEMPLATE_CMD_ECHO_MAX_LEN + 1] = { 0 };
  int16_t ret = client_echo(CSP_NODE_SRV, echo_out_buff, echo_in_buff);

  // check we have the data and that it matches the expected "Hello World!" reply
  munit_assert_int(ret, ==, 0);
  munit_assert_string_equal(echo_in_buff, echo_out_buff);

  return(MUNIT_OK);
}

static MunitResult
test_kill(const MunitParameter params[], void* fixture) {
  (void)fixture;

  // send the kill command and check the status
  int16_t ret = client_kill_server(CSP_NODE_SRV);
  munit_assert_int(ret, ==, 0);
  return(MUNIT_OK);
}

static void*
test_client_setup(const MunitParameter params[], void* user_data) {
  (void)params;
  (void)user_data;

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

  return(NULL);
}

static void
test_client_teardown(void* fixture) {
  (void)fixture;
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
   // create the mutex
  mux = os->mutex_create();

  // set up server thread to handle communicaton coming from the client
  service_os_thread_handle_t th_server = NULL;
  os->thread_create(&th_server, NULL, thread_server, NULL);

  // run the tests
  return(munit_suite_main(&test_suite, NULL, argc, argv));
}
