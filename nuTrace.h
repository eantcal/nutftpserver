//  
// This file is part of nuTftpServer 
// Copyright (c) Antonino Calderone (antonino.calderone@gmail.com)
// All rights reserved.  
// Licensed under the MIT License. 
// See COPYING file in the project root for full license information.
//


/* -------------------------------------------------------------------------- */

#ifndef __NU_TRACE_H__
#define __NU_TRACE_H__

#include "nuTerminal.h"
#include <stdio.h>
#include <stdarg.h>


/* -------------------------------------------------------------------------- */

void NU_DUMP_BUFFER(const char* buf, unsigned long size);
void NU_TRACE(const char* signature, unsigned long mask, unsigned long level, const char* format, ...);
void NU_TRACE_INF(const char* signature, const char* format, ...);


/* -------------------------------------------------------------------------- */

//All possible trace levels
enum NU_TRACE_LEVEL_t { 
    NU_TL_DIS, 
    NU_TL_ERR, 
    NU_TL_WRN,
    NU_TL_DBG,
    NU_TL_PED
};


/* -------------------------------------------------------------------------- */

//All traceable components
enum NU_TRACE_MASK_t {
    NU_TM_NONE     = 0x00000000,

    NU_TM_TFTP     = 0x00000001, 
    NU_TM_SOCK     = 0x00000002, 

    NU_TM_ALL      = 0xFFFFFFFF
};


/* -------------------------------------------------------------------------- */

//Initial global trace level
#define NU_INIT_TRACE_LEVEL NU_TL_WRN

//Initial list of components with tracing on
#define NU_INIT_TRACE_MASK  ( NU_TM_ALL )

//Current trace level
extern unsigned long NU_TRACE_LEVEL;
extern unsigned long NU_TRACE_MASK;

/* -------------------------------------------------------------------------- */

#endif /* !__NU_TRACE_H__ */

