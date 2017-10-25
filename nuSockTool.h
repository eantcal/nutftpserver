//  
// This file is part of nuTftpServer 
// Copyright (c) Antonino Calderone (antonino.calderone@gmail.com)
// All rights reserved.  
// Licensed under the MIT License. 
// See COPYING file in the project root for full license information.
//


/* -------------------------------------------------------------------------- */

#ifndef __NUSOCKTOOL_H__
#define __NUSOCKTOOL_H__


/* -------------------------------------------------------------------------- */

#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CALL_SUCCESS 0


/* -------------------------------------------------------------------------- */

/**
 * Returns an instance of a int structure
 * @return int : a socket decriptor
 */
int nu_create();


/* -------------------------------------------------------------------------- */

/**
 * Free an instance of a int strcture
 * @param sd: [in] a socket descriptor
 */
void nu_free_sock(int sd);


/* -------------------------------------------------------------------------- */

/**
 * Binds a port to a socket
 * @param sock_ptr: [in] socket descriptor
 * @param port: [in] port
 * @return int: If no error occurs, returns TRUE. Otherwise, it returns FALSE
 */
int nu_bind_port(int sd, unsigned short port);


/* -------------------------------------------------------------------------- */

/**
 * Binds a port to a socket (allow to obtain a port from o/s)
 *
 * @param sd:       [in] a socked descriptor
 * @param port_ptr: [in/out] a pointer to the variable that must contain the port value. 
 *                  If the port value is zero, a unused value is assigned by o/s
 *
 * @return int: If no error occurs, returns TRUE. Otherwise, it returns FALSE
 */

int nu_bind_and_getprt(int sd, unsigned short* port_ptr);


/* -------------------------------------------------------------------------- */

/**
 * Send datagram to a remote host
 *
 * @param sock_ptr: [in/out] a pointer to a int structure
 * @param buf: [in] buffer to send
 * @param len: [in] length of the buffer to send
 * @param flags: [in] indicator specifying the way in which the call is made
 * @param destIp: [in] address of the remote host
 * @param port: [in] port of the remote host
 *
 * @return int: If no error occurs, nu_sendto returns the total number of bytes sent. 
 *              Otherwise, -1 is returned
 */
int nu_sendto(
        int sd,
        const char* buf,
        int len,
        int flags,
        unsigned long destIp,
        unsigned short port);


/* -------------------------------------------------------------------------- */

/**
 * Receives data from a connected socket
 * @param sd:  [in] a socket descriptor
 * @param buf: [out] for the incoming data
 * @param len: [in] length of the buffer 
 * @param flags: [in] indicator specifying the way in which the call is made
 * @param fromAddr: [out] address of the sender host
 * @param fromPort: [out] destination port
 *
 * @return int: If no error occurs, nu_recv returns the number of bytes received. 
 *              If the connection has been gracefully closed, the return value is zero. 
 *              Otherwise, -1 is returned
 */
int nu_recvfrom(
        int sd,
        char *buf,
        int len,
        int flags,
        unsigned int *fromAddr,
        unsigned short* fromPort);


/* -------------------------------------------------------------------------- */

/**
 * Receives data from a connected socket
 * If the socket is blocking type, the function returns olse if the
 * timeout happens
 * @param sd: [in] socket descriptor
 * @param buf: [out] for the incoming data
 * @param len: [in] length of the buffer 
 * @param flags: [in] indicator specifying the way in which the call is made
 * @param fromAddr: [out] address of the sender host
 * @param fromPort: [out] destination port
 * @param timeout: [in] timeout
 *
 * @return int: If no error occurs, nu_recv returns the number of bytes received. 
 *              If the connection has been gracefully closed, the return value is zero. 
 *              Otherwise, -1 is returned
 */
int nu_recvfrom_timeout(
        int sd,
        char *buf,
        int len,
        int flags,
        unsigned int *fromAddr,
        unsigned short* fromPort,
        struct timeval* timeout );


#endif // __NUSOCKTOOL_H__
