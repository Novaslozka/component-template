#ifndef _DRIVER_TEMPLATE_CLIENT_H
#define _DRIVER_TEMPLATE_CLIENT_H

// SALLY libraries
#include <service/service.h>
#include <service/service_filesystem.h>
#include <service/service_com.h>

// the public client C API
// this should expose all the client functionality
// and therefore, MUST be properly documented by Doxygen!

/*!
  \brief Send a ping to the server, it will reply with "Hello World!\r\n".
  \param hcom COM port handle to use.
  \param rpl Reply buffer to save the reply into.
  \returns 0 for success, negative value for error code.
*/
int16_t client_ping(service_com_handle_t hcom, char* rpl);

/*!
  \brief Send a string to be echoed by the server.
  \param hcom COM port handle to use.
  \param out String to echo, assumed to be NULL-terminated.
  \param in Buffer to save the echoed reply into.
  \returns 0 for success, negative value for error code.
*/
int16_t client_echo(service_com_handle_t hcom, char* out, char* in);

/*!
  \brief Stop the server.
  \param hcom COM port handle to use.
  \returns 0 for success, negative value for error code.
*/
int16_t client_kill_server(service_com_handle_t hcom);

// these are the low-level functions
void client_cmd_send(service_com_handle_t hcom, uint8_t cmd_id, uint8_t* data, uint8_t data_len);
size_t client_rpl_get(service_com_handle_t hcom, uint8_t* buff, size_t target_len);

// this function is necessary to provide proper backends to the client
void client_set_backends(service_fs_t* fs, service_os_t* os, service_com_t* com);

#endif
