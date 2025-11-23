#ifndef DRIVER_TEMPLATE_SERVER_CSP_H
#define DRIVER_TEMPLATE_SERVER_CSP_H

/*!
  \file server_csp.h 
  \brief This template header contains commands for simulating server
  processing and responses. This is to be used in testing to simulate the device.
  This version processes packets sent via CSP.
*/

#include <csp/csp.h>
#include <service/service.h>

/*!  
  \brief This function processes packets sent via CSP.
  It is called by SALLY after packet has been received,
  or a connection has been opened (depending on the configuration of the service)
  \param packet When called, this will point to the received CSP packet.
  NOTE: It is up to the caller to dispose of the packet with csp_buffer_free(packet)!
  \param conn When called, this will point to the opened connection.
*/
void server_process_packet(csp_packet_t *packet, csp_conn_t* conn);

/*!  
  \brief This registers a user function callback to the server.
  This allows the server to call user functions.
  \param cb Callback to register
  \param index Registered callback index (0 to SERVICE_REG_CB_MAX)
*/
void server_register_callback(cb_execution_func_t cb, int index);

#endif
