//  
// This file is part of nuTftpServer 
// Copyright (c) Antonino Calderone (antonino.calderone@gmail.com)
// All rights reserved.  
// Licensed under the MIT License. 
// See COPYING file in the project root for full license information.
//


/* -------------------------------------------------------------------------- */

#ifndef __NU_TFTP_UTIL_H__
#define __NU_TFTP_UTIL_H__

#include <linux/limits.h>
#include <stdint.h>
#include "nuSockTool.h"


/* -------------------------------------------------------------------------- */

typedef uint16_t tftp_fmode_t;
typedef uint16_t tftp_opcode_t;


/* -------------------------------------------------------------------------- */

#define NETASCII            0
#define OCTET               1
#define MAIL                2
#define INVALID_MODE        3

#define TFTP_RRQ            1
#define TFTP_WRQ            2
#define TFTP_DATA           3
#define TFTP_ACK            4
#define TFTP_ERROR          5
#define TFTP_INVALID_OPCODE 6

typedef enum _tftp_error_codes_index_t {
    TFTP_ERROR__NOT_DEFINED,
    TFTP_ERROR__FILE_NOT_FOUND,
    TFTP_ERROR__ACCESS_VIOLATION,
    TFTP_ERROR__DISK_FULL,
    TFTP_ERROR__ILLEGAL_OPERATION,
    TFTP_ERROR__UNKNOWN_TRANSFER_ID,
    TFTP_ERROR__FILE_ALREADY_EXISTS,
    TFTP_ERROR__NO_SUCH_USER
} tftp_error_codes_index_t;

#define TFTP_ERROR__SUCCESS -1


/* -------------------------------------------------------------------------- */

/*
TFTP Formats
 
Type   Op #     Format without header
       2 bytes    string   1 byte     string   1 byte
       -----------------------------------------------
RRQ/  | 01/02 |  Filename  |   0  |    Mode    |   0  |
WRQ    -----------------------------------------------
       2 bytes    2 bytes       n bytes
       ---------------------------------
DATA  | 03    |   Block #  |    Data    |
       ---------------------------------
       2 bytes    2 bytes
       -------------------
ACK   | 04    |   Block #  |
       --------------------
       2 bytes  2 bytes        string    1 byte
       ----------------------------------------
ERROR | 05    |  ErrorCode |   ErrMsg   |   0  |
       ----------------------------------------
*/


/* -------------------------------------------------------------------------- */

#define TFTP_MAX_BUFFER_SIZE 512

typedef struct _tftp_data_t {
    uint16_t op_code;
    uint16_t block;
    char buffer[TFTP_MAX_BUFFER_SIZE];
}
tftp_data_t;


/* -------------------------------------------------------------------------- */

typedef struct _tftp_ack_t {
    uint16_t op_code;
    uint16_t block;
}
tftp_ack_t;


/* -------------------------------------------------------------------------- */

#define TFTP_MAX_ERROR_STRING_LEN 128

typedef struct _tftp_error_t {
    uint16_t op_code;
    uint16_t error_code;
    char error_string[TFTP_MAX_ERROR_STRING_LEN];
}
tftp_error_t; 


/* -------------------------------------------------------------------------- */

#define TFTP_MAX_MODESTRING_SIZE 32
#define TFTP_MAX_FILENAME_SIZE PATH_MAX

typedef struct _tftp_request_t {
    tftp_opcode_t op_code; // RRQ/WRQ
    char filename[TFTP_MAX_FILENAME_SIZE];
    char mode[TFTP_MAX_MODESTRING_SIZE];
    tftp_fmode_t fmode;
}
tftp_request_t;


/* -------------------------------------------------------------------------- */
// TFTP packet parsing/formatting utility functions

tftp_opcode_t tftp_parse_opcode(const char* buffer, uint16_t size);
bool tftp_parse_RQ_packet(tftp_request_t* request, char* buffer, uint16_t size);
bool tftp_parse_ERROR_packet(tftp_error_t* packet, const char* buffer, uint16_t* size);
bool tftp_parse_DATA_packet(tftp_data_t* packet, const char* buffer, uint16_t* size);
bool tftp_parse_ACK_packet(tftp_ack_t* packet, const char* buffer, uint16_t size);
uint16_t tftp_format_ERROR_packet(tftp_error_t* packet, uint16_t error_code);
uint16_t tftp_format_DATA_packet(tftp_data_t* packet, uint16_t block, const char* source_data, uint16_t size);
uint16_t tftp_format_ACK_packet(tftp_ack_t* packet, uint16_t block);
uint16_t tftp_format_RQ_packet(char* packet, tftp_opcode_t op_code, const char* filename, tftp_fmode_t fmode);


/* -------------------------------------------------------------------------- */

// TFTP packet utility functions for communication

bool tftp_send_ERROR(int sd, uint32_t toAddr, uint16_t toPort, uint16_t error_code);
bool tftp_send_DATA(int sd, uint32_t toAddr, uint16_t toPort, tftp_data_t* tftp_data_ptr, uint16_t size);
bool tftp_send_ACK(int sd, uint32_t toAddr, uint16_t toPort, uint16_t block);


/* -------------------------------------------------------------------------- */

// TFTP Error management

const char* tftp_get_last_error_msg(int tftp_last_error);


/* -------------------------------------------------------------------------- */

#endif


