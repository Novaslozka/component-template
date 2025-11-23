#include "template/client_csp.h"

// SALLY libraries
#include <service/service_err_codes.h>
#include <csp/csp.h>

// lpldgen-generated packet descriptions
#include <pld_driver_template.h>

#include "packets.h"

// some timeout value for the transactions
#define TIMEOUT_MS    3000

// usually, these would be implemented with lp_generic_txn structures,
// and cmd_lp_generic_txn_addr() function calls,
// but since those are in the commands library (which is not strictly needed here),
// we can do a bit of reimplemetnation here

int16_t client_ping(uint8_t node, char* rpl) {
  // allocate buffers for the raw request and reply
  uint8_t req_buff[DRIVER_TEMPLATE_PING_BIN_SIZE] = { 0 };
  uint8_t rpl_buff[DRIVER_TEMPLATE_PONG_BIN_SIZE] = { 0 };

  // statically build the request
  const struct Driver_Template_Ping req_pld = {
    .pld_id = DRIVER_TEMPLATE_PING_ID,
  };
  pld_error_t ret = Driver_Template_Ping_build_s(&req_pld, req_buff, DRIVER_TEMPLATE_PING_BIN_SIZE);
  if(ret != PLD_OK) {
    return(SERVICE_ERR_PAYLOADS(ret));
  }

  // perform the transaction
  ret = csp_transaction(CSP_PRIO_NORM, node, DRIVER_TEMPLATE_PING_DST_PORT, TIMEOUT_MS, 
                        req_buff, DRIVER_TEMPLATE_PING_BIN_SIZE, 
                        rpl_buff, DRIVER_TEMPLATE_PONG_BIN_SIZE);
  if(!ret) {
    return(SERVICE_ERR_TIMEOUT);
  }

  // parse the reply
  struct Driver_Template_Pong rpl_pld;
  ret = Driver_Template_Pong_parse_s(rpl_buff, DRIVER_TEMPLATE_PONG_BIN_SIZE, &rpl_pld);
  if(ret != PLD_OK) {
    return(SERVICE_ERR_PAYLOADS(ret));
  }

  // copy to the output buffer and we're done
  strncpy(rpl, (char*)rpl_pld.text, DRIVER_TEMPLATE_PONG_BIN_SIZE - 1);
  return(SERVICE_ERR_NONE);
}

int16_t client_echo(uint8_t node, char* out, char* in) {
  // do some basic validation, because this packet has variable length
  size_t out_len = strlen(out);
  if(out_len > DRIVER_TEMPLATE_CMD_ECHO_MAX_LEN) {
    return(SERVICE_ERR_INVALID_ARG);
  }

  // allocate buffers with the maximum possible size
  uint8_t req_buff[DRIVER_TEMPLATE_ECHO_REQ_BIN_SIZE(DRIVER_TEMPLATE_CMD_ECHO_MAX_LEN)] = { 0 };
  uint8_t rpl_buff[DRIVER_TEMPLATE_ECHO_RPL_BIN_SIZE(DRIVER_TEMPLATE_CMD_ECHO_MAX_LEN)] = { 0 };

  // this is a little gcc-only trick, by creating a new structure with a statically-allocated array,
  // we avoid having to dynamically allocate buffer for teh tail array, or do some wild casting
  struct req_pack_t {
    struct Driver_Template_Echo_Req pld;
    char text[DRIVER_TEMPLATE_CMD_ECHO_MAX_LEN + 1];
  } req_pack = {
    .pld = {
      .pld_id = DRIVER_TEMPLATE_ECHO_REQ_ID,
      .text_len = strlen(out),
    },
    .text = { 0 },
  };

  // copy the string to echo
  strncpy((char*)req_pack.pld.text, out, DRIVER_TEMPLATE_CMD_ECHO_MAX_LEN);

  // now build the packet
  pld_error_t ret = Driver_Template_Echo_Req_build_s(&req_pack.pld, req_buff, DRIVER_TEMPLATE_ECHO_REQ_BIN_SIZE(req_pack.pld.text_len));
  if(ret != PLD_OK) {
    return(SERVICE_ERR_PAYLOADS(ret));
  }

  // perform the transaction (pretend like we don't know what will be the reply size)
  int csp_ret = csp_transaction(CSP_PRIO_NORM, node, DRIVER_TEMPLATE_ECHO_REQ_DST_PORT, TIMEOUT_MS, 
                        req_buff, DRIVER_TEMPLATE_ECHO_REQ_BIN_SIZE(req_pack.pld.text_len), 
                        rpl_buff, -1);
  if(!csp_ret) {
    return(SERVICE_ERR_TIMEOUT);
  }

  // parse the reply
  struct rpl_pack_t {
    struct Driver_Template_Echo_Rpl pld;
    char text[DRIVER_TEMPLATE_CMD_ECHO_MAX_LEN + 1];
  } rpl_pack = {
    .pld = {
      .pld_id = DRIVER_TEMPLATE_ECHO_RPL_ID,
      .text_len = 0,
    },
    .text = { 0 },
  };
  ret = Driver_Template_Echo_Rpl_parse_s(rpl_buff, DRIVER_TEMPLATE_ECHO_RPL_BIN_SIZE(csp_ret - 1), &rpl_pack.pld);
  if(ret != PLD_OK) {
    return(SERVICE_ERR_PAYLOADS(ret));
  }

  // copy the received string adn we're done
  strncpy(in, (char*)rpl_pack.pld.text, DRIVER_TEMPLATE_CMD_ECHO_MAX_LEN);
  return(SERVICE_ERR_NONE);
}

int16_t client_kill_server(uint8_t node) {
  // allocate buffers for the raw request only, since there is no reply
  uint8_t req_buff[DRIVER_TEMPLATE_KILL_BIN_SIZE] = { 0 };

  // statically build the request
  const struct Driver_Template_Kill req_pld = {
    .pld_id = DRIVER_TEMPLATE_KILL_ID,
  };
  pld_error_t ret = Driver_Template_Kill_build_s(&req_pld, req_buff, DRIVER_TEMPLATE_KILL_BIN_SIZE);
  if(ret != PLD_OK) {
    return(SERVICE_ERR_PAYLOADS(ret));
  }

  // perform the transaction, without waiting for reply
  ret = csp_transaction(CSP_PRIO_NORM, node, DRIVER_TEMPLATE_KILL_DST_PORT, TIMEOUT_MS, 
                        req_buff, DRIVER_TEMPLATE_PING_BIN_SIZE, 
                        NULL, 0);
  if(!ret) {
    return(SERVICE_ERR_TIMEOUT);
  }
  
  return(SERVICE_ERR_NONE);
}

/* 
// if you alread have libcommands and libconsole dependencies covered
// (which will be the case when writing VCOM plugins),
// you can use the following functions which are a lot simpler

int16_t client_ping(uint8_t node, char* rpl)  {
  const struct lp_generic_txn txn = {
    .req_spec = PldSpec_Driver_Template_Ping,
    .resp_spec = PldSpec_Driver_Template_Pong,
    .timeout = 0,
  };

  return(cmd_lp_generic_txn_addr(&txn, node));
}

int16_t client_echo(uint8_t node, char* out, char* in) {
  const struct lp_generic_txn txn = {
    .req_spec = PldSpec_Driver_Template_Echo_Req,
    .resp_spec = PldSpec_Driver_Template_Echo_Rpl,
    .timeout = 0,
  };

  return(cmd_lp_generic_txn_addr(&txn, node));
}

int16_t client_kill_server(uint8_t node) {
  const struct lp_generic_txn txn = {
    .req_spec = PldSpec_Driver_Template_Kill,
    .resp_spec = NULL,
    .timeout = 0,
  };

  return(cmd_lp_generic_txn_addr(&txn, node));
}
*/
