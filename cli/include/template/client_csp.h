#ifndef DRIVER_TEMPLATE_CLIENT_H
#define DRIVER_TEMPLATE_CLIENT_H

/*!
  \file client.h 
  \brief This template header contains template client commands
  This version performs transactions via CSP.
*/

#include <stdint.h>

// the public client C API
// this should expose all the client functionality
// and therefore, MUST be properly documented by Doxygen!

/*!
  \brief Send a ping to the server, it will reply with "Hello World!\r\n".
  \param node Address of the server to send the request to.
  \param rpl Reply buffer to save the reply into.
  \returns 0 for success, negative value for error code.
*/
int16_t client_ping(uint8_t node, char* rpl);

/*!
  \brief Send a string to be echoed by the server.
  \param node Address of the server to send the request to.
  \param out String to echo, assumed to be NULL-terminated.
  \param in Buffer to save the echoed reply into.
  \returns 0 for success, negative value for error code.
*/
int16_t client_echo(uint8_t node, char* out, char* in);

/*!
  \brief Stop the server.
  \param node Address of the server to send the request to.
  \returns 0 for success, negative value for error code.
*/
int16_t client_kill_server(uint8_t node);

#endif
