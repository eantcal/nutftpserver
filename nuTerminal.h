//  
// This file is part of nuTftpServer 
// Copyright (c) Antonino Calderone (antonino.calderone@gmail.com)
// All rights reserved.  
// Licensed under the MIT License. 
// See COPYING file in the project root for full license information.
//


/* -------------------------------------------------------------------------- */

#ifndef __NUTERMINAL_H__
#define __NUTERMINAL_H__


/* -------------------------------------------------------------------------- */

#  define AT_CLEANSCREEN   "\x1B[2J"
#  define AT_ERASELINE     "\x1B[K"
#  define AT_DISABLE_ALL   "\x1B[0m"
#  define AT_BOLD_ON       "\x1B[1m"
#  define AT_UNDERSCORE_ON "\x1B[4m"
#  define AT_BLINK_ON      "\x1B[5m"
#  define AT_REVERSEVID_ON "\x1B[7m"
#  define AT_CONCEALED_ON  "\x1B[8m"

#  define AT_TEXT_BLACK    "\x1B[30m"
#  define AT_TEXT_RED      "\x1B[31m"
#  define AT_TEXT_GREEN    "\x1B[32m"
#  define AT_TEXT_YELLOW   "\x1B[33m"
#  define AT_TEXT_BLUE     "\x1B[34m"
#  define AT_TEXT_MAGENTA  "\x1B[35m"
#  define AT_TEXT_CYAN     "\x1B[36m"
#  define AT_TEXT_WHITE    "\x1B[37m"

#  define AT_BG_BLACK      "\x1B[40m"
#  define AT_BG_RED        "\x1B[41m"
#  define AT_BG_GREEN      "\x1B[42m"
#  define AT_BG_YELLOW     "\x1B[43m"
#  define AT_BG_BLUE       "\x1B[44m"
#  define AT_BG_MAGENTA    "\x1B[45m"
#  define AT_BG_CYAN       "\x1B[46m"
#  define AT_BG_WHITE      "\x1B[47m"


/* -------------------------------------------------------------------------- */

#ifndef TERMINAL_OUTPUT
#define TERMINAL_OUTPUT printf
#endif


/* -------------------------------------------------------------------------- */

#endif

