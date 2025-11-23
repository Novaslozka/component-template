// static library headers
#include <template/client_csp.h>

// standard libraries
#include <string.h>
#include <stdio.h>

// CSP
#include <csp/csp.h>
#include <csp/drivers/usart.h>

// SALLY libraries
#include <service/service.h>
#include <service/service_filesystem.h>
#include <service/service_com.h>
#include <service/service_err_codes.h>
#include <service-csp/csp_pld_txn.h>

// lpldgen-generated packet descriptions
#include <pld_driver_template.h>

// headers shared for both client and server
#include "packets.h"

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

// CSP interface
static csp_iface_t *csp_if_kiss;

void thread_client(void* arg) {
  fprintf(stdout, "client start\r\n");

  // set CSP configuration
  csp_conf_t csp_conf = { 0 };
  csp_conf_get_defaults(&csp_conf);

  csp_conf.address = CSP_NODE_CLI;
  csp_conf.hostname = "example";
  csp_conf.model = "client";
  csp_conf.revision = GITREV;
  int ret = csp_init(&csp_conf);
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

  // send some server requests
  for(uint8_t i = 0; i < 10; i++) {
    // send a ping
    char ping_buff[16];
    ret = client_ping(CSP_NODE_SRV, ping_buff);
    if(ret == SERVICE_ERR_NONE) {
      fprintf(stdout, "Ping #%d got reply: %s\n", i, ping_buff);
    } else {
      fprintf(stderr, "Ping #%d failed with code %d\r\n", i, ret);
    }
    os->thread_delay(10);
  }

  // send an echo
  char echo_out_buff[] = "Echo this!";
  char echo_in_buff[DRIVER_TEMPLATE_CMD_ECHO_MAX_LEN + 1] = { 0 };
  ret = client_echo(CSP_NODE_SRV, echo_out_buff, echo_in_buff);
  if(ret == SERVICE_ERR_NONE) {
    fprintf(stdout, "Echo got reply: %s\n", echo_in_buff);
  } else {
    fprintf(stderr, "Echo failed with code %d\r\n", ret);
  }
  os->thread_delay(10);

  // send a command to un-alive the server
  client_kill_server(CSP_NODE_SRV);

  // all done, clean up
  fprintf(stdout, "client done\r\n");
}

int main(int argc, char *argv[]) {
  const char* port = "dev/ttyS1";

  // set up a thread to handle CSP communication
  service_os_thread_handle_t thread = NULL;
  os->thread_create(&thread, NULL, thread_client, (void*)port);

  // wait for exit
  os->thread_wait_for(thread);
  return(0);
}
