//  
// This file is part of nuTftpServer 
// Copyright (c) Antonino Calderone (antonino.calderone@gmail.com)
// All rights reserved.  
// Licensed under the MIT License. 
// See COPYING file in the project root for full license information.
//


/* -------------------------------------------------------------------------- */

#ifndef __TFTP_SERVER_H__
#define __TFTP_SERVER_H__


/* -------------------------------------------------------------------------- */

#include "nuTftpUtil.h"


/* -------------------------------------------------------------------------- */

#define TFTP_SERVER_PORT 69       //!< standard TFTP port
#define TFTP_MAX_CONNECTION 16

#define TFTP_RECV_TIMEOUT 1       //!< secs
#define TFTP_RECV_ATTEMPTS 2

//!max number of tftpd daemons that is possible to run
//!(that's different than number of sessions!!!)
//!Each tftpd daemon should be started with a different port of
//!service, that for default is 69, and each tftpd daemon can have
#define TFTPD_IPC_POOL_SIZE 3 


/* -------------------------------------------------------------------------- */

typedef void* TFTPD_HANDLE;


/* -------------------------------------------------------------------------- */

/**
 * This function starts the TFTP server
 *
 *  @param task_prio: [in] intial priority of the tftpd task
 *  @param max_sessions: [in] max tftp concurrent sessions 
 *                       (it should be less or equal than TFTP_MAX_CONNECTION)
 *  @param r_path: [in] volume/path of the directory where tftpd downloads the
 *                 files got by a GET command is performed
 *  @param w_path: [in] volume/path of the directory from where tftpd uploads the
 *                 files sent after a PUT command is performed
 * @param connectionID: [in] A pointer to the connection id handle returned 
 * @param port_of_service: [in] normaly it should be 69
 * @param trace_level: [in] 0 disable, 1 error, 2 warning, 3 debug, 4 pedantic
 * @return TFTPD_HANDLE: if function successes, returns a non-zero handle
 */
TFTPD_HANDLE tftp_start_server(
        int task_prio,
        int max_sessions,
        const char* r_path,
        const char* w_path,
        unsigned short port_of_service,
        int trace_level);


/* -------------------------------------------------------------------------- */

/**
 * This function starts the TFTP server
 *
 * NOTE:                                                                      
 *  - the handle must be a valid TFTPD_HANDLE
 *
 *  @param handle: [in] handle of a tftpd server
 *  @return none
 */
void tftp_stop_server(TFTPD_HANDLE handle);


/* -------------------------------------------------------------------------- */

/**
 * This function returns the count of active sessions that are running 
 *
 * NOTE:                                                                      
 *  - the handle must be a valid TFTPD_HANDLE
 *
 *  @param handle: [in] handle of a tftpd server
 *  @return unsigned int: count of active sessions
 */
unsigned int tftp_get_opened_sessions_count(TFTPD_HANDLE handle);


/* -------------------------------------------------------------------------- */

/**
 * This function returns true if the tftpd is running
 * Be carefull that the handle is valid. Invalid value of it can
 * cause an exception.
 *
 * NOTE:                                                                      
 *  - the handle must be a valid TFTPD_HANDLE
 *
 *  @param handle: [in] handle of a tftpd server
 *  @return bool: true, if tftp server is up
 */
bool tftp_is_server_running(TFTPD_HANDLE handle);


/* -------------------------------------------------------------------------- */

/**
 * This function returns true if the tftp_stop_server had been previously invoked 
 *
 * NOTE:                                                                      
 *  - the handle must be a valid TFTPD_HANDLE
 *
 *  @param handle: [in] handle of a tftpd server
 *  @return bool: true, if tftp_stop_server had been previously invoked, false else
 */
bool tftp_stop_cmd_issued(TFTPD_HANDLE handle);


/* -------------------------------------------------------------------------- */

/**
 * This function returns the TFTP last error code 
 *
 * NOTE:                                                                      
 *  - the handle must be a valid TFTPD_HANDLE
 *
 *  @param handle: [in] handle of a tftpd server
 *  @return int: error code
 */
int tftp_get_last_server_error_code(TFTPD_HANDLE handle);


/* -------------------------------------------------------------------------- */

#endif

