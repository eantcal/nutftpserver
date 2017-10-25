//  
// This file is part of nuTftpServer 
// Copyright (c) Antonino Calderone (antonino.calderone@gmail.com)
// All rights reserved.  
// Licensed under the MIT License. 
// See COPYING file in the project root for full license information.
//


/* -------------------------------------------------------------------------- */

#include "nuTrace.h"
#include <string>


/* -------------------------------------------------------------------------- */

using namespace std;


/* -------------------------------------------------------------------------- */

static
char NU_TRACE_LEVEL_desc[][32] = {
    AT_DISABLE_ALL "DIS",
    AT_TEXT_WHITE "" AT_BG_RED "ERR" AT_DISABLE_ALL "",
    AT_TEXT_BLACK "" AT_BG_YELLOW "WRN" AT_DISABLE_ALL "",
    AT_TEXT_WHITE "" AT_BG_BLUE "DBG" AT_DISABLE_ALL "",
    AT_TEXT_BLACK "" AT_BG_GREEN "PED" AT_DISABLE_ALL ""
};


/* -------------------------------------------------------------------------- */

const char* nuGetTraceLevelDesc(int level) 
{
    if (level >= 0 && level <= 
            int(sizeof(NU_TRACE_LEVEL_desc) / sizeof(NU_TRACE_LEVEL_desc[0]))) 
    {
        return NU_TRACE_LEVEL_desc[level];
    }

    return "";
}


/* -------------------------------------------------------------------------- */

void NU_TRACE(const char* signature, unsigned long mask, unsigned long level, const char* format, ...)
{
    va_list par_list;

    if (NU_TRACE_LEVEL >= level && ((mask & NU_TRACE_MASK) == mask)) {
        va_start(par_list, format);

        TERMINAL_OUTPUT(AT_DISABLE_ALL);
        TERMINAL_OUTPUT(AT_REVERSEVID_ON);
        TERMINAL_OUTPUT("%s",signature);
        TERMINAL_OUTPUT(AT_BOLD_ON "[%u]" AT_DISABLE_ALL "%s>", 
                unsigned(level), (const char*)NU_TRACE_LEVEL_desc[level]);

        vprintf(format, par_list);

        TERMINAL_OUTPUT(AT_DISABLE_ALL "\r\n");

        va_end(par_list);
    }
}


/* -------------------------------------------------------------------------- */

void NU_TRACE_INF(const char* signature, const char* format, ...)
{
    va_list par_list;

    va_start(par_list, format);

    TERMINAL_OUTPUT(AT_DISABLE_ALL);
    TERMINAL_OUTPUT(AT_REVERSEVID_ON);
    TERMINAL_OUTPUT("%s",signature);

    TERMINAL_OUTPUT(AT_BOLD_ON "[*]" AT_DISABLE_ALL ""
            AT_TEXT_BLACK "" AT_BG_WHITE "INF"
            AT_DISABLE_ALL ">");

    vprintf(format, par_list);

    TERMINAL_OUTPUT(AT_DISABLE_ALL "\r\n");

    va_end(par_list);
}


/* -------------------------------------------------------------------------- */

unsigned long NU_TRACE_LEVEL = NU_INIT_TRACE_LEVEL;
unsigned long NU_TRACE_MASK = NU_INIT_TRACE_MASK;

