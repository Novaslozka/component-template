// app headers
#include "template/server_csp.h"

// standard libraries
#include <string.h>

// SALLY libraries
#include <service/service_err_codes.h>
#include <service-csp/csp_pld_txn.h>

// lpldgen-generated packet descriptions
#include <pld_driver_template.h>

// headers shared for both client and server
#include "packets.h"

// forward declarations of functions that we will use to do the actual stuff
static int8_t server_pong(void* vreq, void* vrpl);
static int8_t server_echo(void* vreq, void* vrpl, size_t* rpl_size);
static int8_t server_kill(void* vreq, void* vrpl);

// SALLY registered callback - this allows the server implementation to call functions in the user code
// in the automated test, this will be used to implement the "shutdown" functionality
static cb_execution_func_t server_registered_cbs[SERVICE_REG_CB_MAX];
void server_register_callback(cb_execution_func_t cb, int index) {
  server_registered_cbs[index] = cb;
}

void server_process_packet(csp_packet_t *packet, csp_conn_t* conn) {
  // based on how the service was set up, it will receive either CSP packets or CSP connections (or both)
  // in this case we ignore conections and just process packets
  (void)conn;
  if(!packet) {
    return;
  }

  // it is usually a good idea to do some basic validation here
  if(packet->length < 1) {
    csp_buffer_free(packet);
    return;
  }

  // the rest of this example assumes lpldgen is being used to generate the packet structures
  // if you are not using lpldgen, then feel free to ignore this completely,
  // and just use the CSP packet/connection structs as you see fit
  // just remember to dispose of the packet with csp_buffer_free(packet) when done!

  // SALLY contians a macro that lets yout to statically
  // determine required buffer sizes based on max req/rsp struct size
  uint8_t pld_req_buff[SIZEOF_MAX(struct Driver_Template_Ping a; \
                                  struct Driver_Template_Echo_Req b; \
                                  struct Driver_Template_Kill c;) + DRIVER_TEMPLATE_CMD_ECHO_MAX_LEN];
  uint8_t pld_rsp_buff[SIZEOF_MAX(struct Driver_Template_Pong a; \
                                  struct Driver_Template_Echo_Rpl b;) + DRIVER_TEMPLATE_CMD_ECHO_MAX_LEN];
  
  // this structure will hold information about the transaction
  struct csp_pld_txn_t txn = {
    .pack_req = packet,
    .pld_req = pld_req_buff,
    .pld_rsp = pld_rsp_buff,
  };

  // set transaction properties based on the command ID
  // this is just an example - see the SALLY documentation for details
  switch(packet->data[0]) {
    case DRIVER_TEMPLATE_PING_CMD_ID: {
      // each transaction needs at least the size of the reply, ...
      txn.bin_size_rsp = DRIVER_TEMPLATE_PONG_BIN_SIZE;
      // ... function to be used to parse the request, ...
      txn.req_parse_s = (fn_parse_static_t)Driver_Template_Ping_parse_s;
      // ... function to be used to build a reply, ...
      txn.rsp_build_s = (fn_build_static_t)Driver_Template_Pong_build_s;
      // ... and a callback to a function that will actually do something
      txn.fce = server_pong;
    } break;

    case DRIVER_TEMPLATE_ECHO_REQ_CMD_ID: {
      // sometimes replies may have variable length - this will get set during processing
      txn.bin_size_rsp = 0;
      txn.req_parse_s = (fn_parse_static_t)Driver_Template_Echo_Req_parse_s;
      txn.rsp_build_s = (fn_build_static_t)Driver_Template_Echo_Rpl_build_s;
      txn.fce_grow = server_echo;
    } break;

    case DRIVER_TEMPLATE_KILL_CMD_ID: {
      // and sometimes there might be no reply at all
      txn.bin_size_rsp = 0;
      txn.req_parse_s = (fn_parse_static_t)Driver_Template_Kill_parse_s;
      txn.rsp_build_s = NULL;
      txn.fce = server_kill;
    } break;

    default: {
      fprintf(stderr, "Unknown command 0x%02x!", packet->data[0]);
      csp_buffer_free(packet);
      return;
    }
  }

  // everything is set up - so we can perform the transaction!
  // this will parse the request from the raw data,
  // call the appropriate function from the ones below,
  // build and send the reply, and take care of any errors along the way
  csp_pld_txn(&txn);
}

static int8_t server_pong(void* vreq, void* vrpl) {
  (void)vreq;
  struct Driver_Template_Pong* rpl = (struct Driver_Template_Pong*)vrpl;
  rpl->pld_id = DRIVER_TEMPLATE_PONG_ID;

  // on ping, just send the defined reply
  snprintf((char*)rpl->text, DRIVER_TEMPLATE_PONG_BIN_SIZE - 1, DRIVER_TEMPLATE_CMD_PING_RPL);

  // these functions have to return a status code
  return(SERVICE_ERR_NONE);
}

static int8_t server_echo(void* vreq, void* vrpl, size_t* rpl_size) {
  struct Driver_Template_Echo_Req* req = (struct Driver_Template_Echo_Req*)vreq;
  struct Driver_Template_Echo_Rpl* rpl = (struct Driver_Template_Echo_Rpl*)vrpl;
  rpl->pld_id = DRIVER_TEMPLATE_ECHO_RPL_ID;

  // check the request text length
  if(req->text_len > DRIVER_TEMPLATE_CMD_ECHO_MAX_LEN) {
    return(SERVICE_ERR_INVALID_ARG);
  }

  // on echo, take the request payload and copy it to the reply
  rpl->text_len = req->text_len;
  memcpy(rpl->text, req->text, rpl->text_len);

  // for replies with variable length, we also have to pass the actual reply size
  *rpl_size = DRIVER_TEMPLATE_ECHO_RPL_BIN_SIZE(rpl->text_len);
  return(SERVICE_ERR_NONE);
}

static int8_t server_kill(void* vreq, void* vrpl) {
  // there is nothing in this request and there is no reply
  (void)vreq;
  (void)vrpl;

  // if there is a callback, it will be used to shutdown the server
  if(server_registered_cbs[0]) {
    (void)server_registered_cbs[0](NULL);
  }

  return(SERVICE_ERR_NONE);
}
