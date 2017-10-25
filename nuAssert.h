//  
// This file is part of nuTftpServer 
// Copyright (c) Antonino Calderone (antonino.calderone@gmail.com)
// All rights reserved.  
// Licensed under the MIT License. 
// See COPYING file in the project root for full license information.
//


/* -------------------------------------------------------------------------- */

#ifndef __NU_ASSERT_H__
#define __NU_ASSERT_H__


/* -------------------------------------------------------------------------- */

#include <assert.h>
#include "nuTerminal.h"

#define NU_ASSERT_MESSAGE "NU ASSERT FAILED AT LINE %i of FILE %s\n"


/* -------------------------------------------------------------------------- */

#define NU_ASSERT(x) if(!(x)){\
    TERMINAL_OUTPUT(\
            AT_BG_RED "" \
            AT_TEXT_WHITE "" NU_ASSERT_MESSAGE\
            AT_DISABLE_ALL "",__LINE__, __FILE__);\
    assert(x);\
}


/* -------------------------------------------------------------------------- */

#endif /* ! __NU_ASSERT_H__ */

