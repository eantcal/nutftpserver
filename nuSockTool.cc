//  
// This file is part of nuTftpServer 
// Copyright (c) Antonino Calderone (antonino.calderone@gmail.com)
// All rights reserved.  
// Licensed under the MIT License. 
// See COPYING file in the project root for full license information.
//


/* -------------------------------------------------------------------------- */

#include "nuSockTool.h"
#include <netinet/in.h>
#include <assert.h>


/* -------------------------------------------------------------------------- */

int nu_create()
{
    return socket(AF_INET, SOCK_DGRAM, 0);
}


/* -------------------------------------------------------------------------- */

void nu_free_sock(int sd)
{
    if (sd>0) {
        close(sd);
    }
}


/* -------------------------------------------------------------------------- */

int nu_sendto(int sd,
        const char* buf,
        int len,
        int flags,
        unsigned long destIp,
        unsigned short port)
{
    struct sockaddr_in remote_host = {0};

    remote_host.sin_addr.s_addr = htonl(destIp);
    remote_host.sin_family = AF_INET;
    remote_host.sin_port = htons(port);

    return sendto(sd,
            buf,
            len,
            flags,
            (struct sockaddr*) & remote_host,
            sizeof(remote_host));
}


/* -------------------------------------------------------------------------- */

int nu_recvfrom(
        int sd,
        char *buf,
        int len,
        int flags,
        unsigned int *fromAddr,
        unsigned short* fromPort)

{
    struct sockaddr_in from;
    socklen_t ret_val;
    socklen_t fromlen = sizeof(from);

    memset(&from, 0, sizeof(from));
    ret_val = recvfrom(sd, buf, len, flags, (struct sockaddr*) & from, &fromlen);
    *fromAddr = htonl(from.sin_addr.s_addr);
    *fromPort = htons(from.sin_port);

    return int(ret_val);
}


/* -------------------------------------------------------------------------- */

int nu_recvfrom_timeout(
        int sd,
        char *buf,
        int len,
        int flags,
        unsigned int *fromAddr,
        unsigned short* fromPort,
        struct timeval* timeout)
{
    //   #define MAX_ATTEMPTS 1
    fd_set readMask;
    long nd;

    unsigned int _fromAddr = 0;
    unsigned short _fromPort = 0;
    int ret_val = 0;

    // Add socket to receive select mask
    FD_ZERO(&readMask);
    FD_SET(sd, &readMask);

    do
    {
        nd = select(FD_SETSIZE, &readMask, (fd_set *)0, (fd_set *)0, timeout);
    } 
    while (!FD_ISSET(sd, &readMask) && nd > 0);

    if (nd > 0) // ...Receive actvity !
    {
        ret_val = nu_recvfrom(sd, buf, len, flags, &_fromAddr, &_fromPort);

        if ((ret_val <= 0) || ((_fromAddr == 0) && (_fromPort == 0)))
        {
            return ret_val;
        }
        else if ((_fromAddr == *fromAddr || *fromAddr == 0) &&  // 0->ANY_ADDR
                (_fromPort == *fromPort || *fromPort == 0))   // 0->ANY_PORT
        {
            *fromPort = _fromPort;
            *fromAddr = _fromAddr;
            return ret_val;
        }
    }
    else if (nd < 0) // Select failure...
    {
        return -1; // error
    }

    return 0; // timeout
}


/* -------------------------------------------------------------------------- */

int nu_bind_and_getprt(
        int sd,
        unsigned short* port_ptr)
{
    struct sockaddr_in sin;
    int err_code;
    socklen_t address_len = 0;

    memset(&sin, 0, sizeof(sin));

    sin.sin_family = AF_INET;
    sin.sin_port = htons(*port_ptr);
    sin.sin_addr.s_addr = INADDR_ANY;

    err_code = bind(sd, (struct sockaddr *) & sin, sizeof(sin));

    if (*port_ptr == 0) {
        address_len = sizeof(sin);
        getsockname(sd, (struct sockaddr *) &sin, &address_len);
        *port_ptr = ntohs(sin.sin_port);
    }

    return err_code == 0;
}


/* -------------------------------------------------------------------------- */

int nu_bind_port(int sd, unsigned short port)
{
    return nu_bind_and_getprt(sd, &port);
}


/* -------------------------------------------------------------------------- */


