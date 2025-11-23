// static library headers
#include <template/server_csp.h>

// standard libraries
#include <string.h>
#include <stdio.h>

// CSP
#include <csp/csp.h>
#include <csp/drivers/usart.h>

// SALLY libraries
#include <service/service.h>
#include <service/service_filesystem.h>
#include <service/service_err_codes.h>
#include <service-csp/csp_pld_txn.h>

// lpldgen-generated packet descriptions
#include <pld_driver_template.h>

// headers shared for both client and server
#include "packets.h"

// set by build configuration tool
#ifndef GITREV
#define GITREV "no-git"
#endif

// SALLY backends - set by build configuration tool
extern service_os_t OS_BACKEND;
service_os_t* os = &OS_BACKEND;

// CSP KISS interface - it is the simples one to use,
// and we already use an emualted COM port for the COM example
static csp_iface_t *csp_if_kiss;

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
  fprintf(stdout, "server start\r\n");

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

  // set up a simple KISS interface for the client to connect to
  static struct csp_usart_conf csp_kiss_driver;
  csp_kiss_driver.device = (const char*)arg;
  csp_kiss_driver.baudrate = 460800;
  ret = csp_usart_open_and_add_kiss_interface(&csp_kiss_driver, "KISS", &csp_if_kiss);
  if(ret == CSP_ERR_NONE) {
    fprintf(stdout, "CSP opened KISS interface on %s\r\n", csp_kiss_driver.device);
  } else {
    fprintf(stderr, "CSP failed to open %s, code %d\r\n", csp_kiss_driver.device, ret);
    return;
  }

  // set the default route and start router
  (void)csp_route_set(CSP_DEFAULT_ROUTE, csp_if_kiss, CSP_NODE_MAC);
  (void)csp_route_start_task(8192*16, 0);

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

  fprintf(stdout, "server done\r\n");
}

int main(int argc, char *argv[]) {
  const char* port = "dev/ttyS0";

  // set up a thread to handle CSP communication
  service_os_thread_handle_t thread = NULL;
  os->thread_create(&thread, NULL, thread_server, (void*)port);

  // wait for exit
  os->thread_wait_for(thread);
  return(0);
}
