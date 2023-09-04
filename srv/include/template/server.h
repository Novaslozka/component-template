#ifndef _DRIVER_TEMPLATE_SERVER_H
#define _DRIVER_TEMPLATE_SERVER_H

/*!  \file server.h 
*    \brief This template header contains commands for simulating server
*    processing and responses. This is to be used in testing to simulate the device
*/

// SALLY libraries
#include <service/service.h>
#include <service/service_filesystem.h>
#include <service/service_com.h>

/*!  
*   \brief This function sets the backends to be used for all processing
*   \param fs Pointer to the filesystem backend
*   \param os Pointer to the operating system backend
*   \param com Pointer to the COM port backend
*/
void server_set_backends(service_fs_t* fs, service_os_t* os, service_com_t* com);
/*!  
*   \brief This function contains a very simple processing script to simulate a device
*
*   This should either be extended or changed according to the IDC of the device
*
*   \param hcom COM port handle
*   \param buff Data buffer - Data to be received from the client
*   \param len Length of the data buffer
*/
int server_process_cmd(service_com_handle_t hcom, uint8_t* buff, size_t len);

#endif
