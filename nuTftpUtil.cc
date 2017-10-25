//  
// This file is part of nuTftpServer 
// Copyright (c) Antonino Calderone (antonino.calderone@gmail.com)
// All rights reserved.  
// Licensed under the MIT License. 
// See COPYING file in the project root for full license information.
//


/* -------------------------------------------------------------------------- */

#include "nuSockTool.h"
#include <string>
#include <sstream>

using namespace std;


/* -------------------------------------------------------------------------- */

#include "nuTftpUtil.h"
#include <netinet/in.h>

#define DEFULT_FLAGS 0

const char* tftp_file_mode[] =
{
    "netascii",
    "octet",
    "mail"
};


/* -------------------------------------------------------------------------- */

/**
  TFTP  supports five types of packets, all of which have been mentioned
above:

opcode  operation
  1     Read request (RRQ)
  2     Write request (WRQ)
  3     Data (DATA)
  4     Acknowledgment (ACK)
  5     Error (ERROR)
*/


/* -------------------------------------------------------------------------- */

#define TFTP_OPCODE_SIZE (sizeof(uint16_t))

/*
   Error Codes
   Value     Meaning
   0         Not defined, see error message (if any).
   1         File not found.
   2         Access violation.
   3         Disk full or allocation exceeded.
   4         Illegal TFTP operation.
   5         Unknown transfer ID.
   6         File already exists.
   7         No such user.
 */

const char tftp_error_codes[8][64] = {
    "Generic Error",
    "File not found.",
    "Access violation.",
    "Disk full or allocation exceeded.",
    "Illegal TFTP operation.",
    "Unknown transfer ID.",
    "File already exists.",
    "No such user."
};


/* -------------------------------------------------------------------------- */

const char* tftp_get_last_error_msg(int tftp_last_error)
{
    if (tftp_last_error == TFTP_ERROR__SUCCESS) {
        return "None";
    }
    else {
        switch (tftp_last_error) {
            case TFTP_ERROR__NOT_DEFINED:
            case TFTP_ERROR__FILE_NOT_FOUND:
            case TFTP_ERROR__ACCESS_VIOLATION:
            case TFTP_ERROR__DISK_FULL:
            case TFTP_ERROR__ILLEGAL_OPERATION:
            case TFTP_ERROR__UNKNOWN_TRANSFER_ID:
            case TFTP_ERROR__FILE_ALREADY_EXISTS:
            case TFTP_ERROR__NO_SUCH_USER:
                return tftp_error_codes[tftp_last_error];
        }
    }
    return "Unknown";
}


/* -------------------------------------------------------------------------- */
// TFTP packet parsing/formatting utility functions


tftp_opcode_t tftp_parse_opcode(const char* buffer, uint16_t size)
{
    tftp_opcode_t op_code;

    if (size < TFTP_OPCODE_SIZE) {
        return TFTP_INVALID_OPCODE;
    }

    // Get the op_code field
    memcpy(&op_code, buffer, TFTP_OPCODE_SIZE);
    op_code = (tftp_opcode_t)htons(op_code);

    if (op_code >= TFTP_INVALID_OPCODE) {
        return TFTP_INVALID_OPCODE;
    }

    return op_code;
}


/* -------------------------------------------------------------------------- */

uint16_t tftp_format_ACK_packet(
        tftp_ack_t* packet,
        uint16_t block)
{
    memset(packet, 0, sizeof(tftp_ack_t));

    packet->op_code = htons((uint16_t) TFTP_ACK);
    packet->block = htons(block);

    return sizeof(tftp_ack_t);
}


/* -------------------------------------------------------------------------- */

bool tftp_parse_ACK_packet(
        tftp_ack_t* packet,  //out
        const char* buffer,
        uint16_t size)
{
    tftp_opcode_t op_code;
    uint16_t block;
    int i = 0;

    // Get op-code
    op_code = tftp_parse_opcode(buffer, size);

    if (op_code != TFTP_ACK)
        return false;

    // Invalidate packet
    memset(packet, 0, sizeof(tftp_ack_t));

    i += TFTP_OPCODE_SIZE;
    size -= TFTP_OPCODE_SIZE;
    packet->op_code = ntohs((uint16_t)op_code);

    // Get block
    memcpy(&block, &buffer[i], sizeof(block));
    packet->block = ntohs(block);

    return true;
}


/* -------------------------------------------------------------------------- */

uint16_t tftp_format_DATA_packet(
        tftp_data_t* packet,  // out
        uint16_t block,
        const char* source_data,
        uint16_t size)
{
    uint16_t data_size = 0;

    if (source_data)
        memset(packet, 0, sizeof(tftp_data_t));

    packet->op_code = htons((uint16_t)TFTP_DATA);
    packet->block = htons(block);

    data_size = size < TFTP_MAX_BUFFER_SIZE ? size : TFTP_MAX_BUFFER_SIZE;

    if (source_data)
        memcpy(packet->buffer, source_data, data_size);

    return sizeof(tftp_data_t) - TFTP_MAX_BUFFER_SIZE + data_size;
}


/* -------------------------------------------------------------------------- */

bool tftp_parse_DATA_packet(
        tftp_data_t* packet,  //out
        const char* buffer,
        uint16_t* size) //in=size of udp buffer /out=size of packet data
{
    tftp_opcode_t op_code;
    uint16_t block;
    int i = 0;

    // Get op-code
    op_code = tftp_parse_opcode(buffer, *size);

    if (op_code != TFTP_DATA)
        return false;

    // Invalidate packet
    memset(packet, 0, sizeof(tftp_data_t));

    if (*size < TFTP_OPCODE_SIZE)
        return false;

    i += TFTP_OPCODE_SIZE;
    *size -= TFTP_OPCODE_SIZE;
    packet->op_code = ntohs((uint16_t)op_code);

    if (*size < sizeof(block))
        return false;

    // Get block
    memcpy(&block, &buffer[i], sizeof(block));
    packet->block = ntohs(block);

    i += sizeof(block);
    *size -= sizeof(block);

    memcpy(packet->buffer, &buffer[i], *size);

    return true;
}


/* -------------------------------------------------------------------------- */

//returns false if fails
bool tftp_parse_ERROR_packet(
        tftp_error_t* packet,  //out
        const char* buffer,
        uint16_t* size) //in=size of udp buffer /out=size of error message
{
    tftp_opcode_t op_code;
    uint16_t error_code;
    int i = 0;

    // Get op-code
    op_code = tftp_parse_opcode(buffer, *size);

    if (op_code != TFTP_ERROR)
        return false;

    // Invalidate packet
    memset(packet, 0, sizeof(tftp_error_t));

    if (*size < TFTP_OPCODE_SIZE)
        return false;

    i += TFTP_OPCODE_SIZE;
    *size -= TFTP_OPCODE_SIZE;
    packet->op_code = ntohs((uint16_t)op_code);

    if (*size < sizeof(error_code))
        return false;

    // Get block
    memcpy(&error_code, &buffer[i], sizeof(error_code));
    packet->error_code = ntohs(error_code);

    i += sizeof(error_code);
    *size -= sizeof(error_code);

    *size = *size < TFTP_MAX_ERROR_STRING_LEN ? *size : TFTP_MAX_ERROR_STRING_LEN;

    strncpy(packet->error_string, &buffer[i], *size);

    return true;
}


/* -------------------------------------------------------------------------- */

// return the size of the packet, 0 if error_code is out of range
uint16_t tftp_format_ERROR_packet(
        tftp_error_t* packet,  // out
        uint16_t error_code)
{
    const char* error_msg = 0;

    if (error_code > (sizeof(tftp_error_codes) / sizeof(char*)))
        return 0; // bad error code

    memset(packet, 0, sizeof(tftp_error_t));

    packet->op_code = htons((uint16_t)TFTP_ERROR);
    packet->error_code = htons(error_code);

    error_msg = tftp_error_codes[error_code];

    strncpy(packet->error_string, error_msg, TFTP_MAX_ERROR_STRING_LEN - 1);

    return sizeof(tftp_error_t) - TFTP_MAX_ERROR_STRING_LEN + strlen(error_msg) + 1;
}


/* -------------------------------------------------------------------------- */

#define SMALLEST_TFTP_REQUEST_PACKET 10

bool tftp_parse_RQ_packet(
        tftp_request_t* request,  //out
        char* buffer,
        uint16_t size)
{
    int offset = 0, i = 0;
    char c;
    uint16_t op_code;

    // The size cannot be smaller than the SMALLEST_TFTP_REQUEST_PACKET
    if (size < SMALLEST_TFTP_REQUEST_PACKET)
        return false;

    // Invalidate the request
    memset(request, 0, sizeof(tftp_request_t));

    // Get the op_code field
    op_code = tftp_parse_opcode(buffer, size);

    if (op_code == TFTP_INVALID_OPCODE)
        return false;

    offset += sizeof(op_code);
    size -= sizeof(op_code);

    // Starting from offset 1 copy the file name
    // in the field filename of the request
    // POST: size should be a positive value
    //       the filename length shouldn't be greater than TFTP_MAX_FILENAME_SIZE
    while ((c = buffer[offset]) && size && i < TFTP_MAX_FILENAME_SIZE) {
        request->filename[i] = c;
        ++i;      //destination string index
        ++offset; //offset of source buffer
        --size;   //remaining size of source packet
    }

    // Skip the string termination char (NULL)
    --size;
    ++offset;

    if (!size || i >= TFTP_MAX_FILENAME_SIZE)
        return false;

    // Copy the mode string
    i = 0;
    while ((c = buffer[offset]) && size && i < TFTP_MAX_MODESTRING_SIZE) {
        request->mode[i] = c;
        ++i;      //destination string index
        ++offset; //offset of source buffer
        --size;   //remaining size of source packet
    }

    if (!size || i >= TFTP_MAX_MODESTRING_SIZE)
        return false;

    // Resolve the file mode, and validate the request
    request->fmode = INVALID_MODE;
    for (i = 0; i < int(sizeof(tftp_file_mode) / sizeof(const char*)); ++i) {
        if (((tftp_fmode_t)i) == INVALID_MODE)
            return false;

        if (strcmp(request->mode, tftp_file_mode[i]) == 0) {
            request->fmode = (tftp_fmode_t)i;
            break;
        }
    }

    if (request->fmode == INVALID_MODE)
        return false;

    return true;
}


/* -------------------------------------------------------------------------- */

uint16_t tftp_format_RQ_packet(
        char* packet,
        tftp_opcode_t op_code,
        const char* filename,
        tftp_fmode_t fmode)
{
    int packet_size = 0, len = 0;
    const char* mode = 0;
    uint16_t opCode = 0;
    len = strlen(filename);

    //validate the parameters
    if ((op_code != TFTP_RRQ && op_code != TFTP_WRQ) ||
            (fmode != NETASCII && fmode != OCTET) ||
            (len == 0) || (!packet))
    {
        return 0; // not ok
    }

    opCode = htons((uint16_t)op_code);
    memcpy(packet + packet_size, &opCode, sizeof(uint16_t));
    packet_size += sizeof(uint16_t);

    memcpy(packet + packet_size, filename, len + 1);
    packet_size += len + 1;

    mode = tftp_file_mode[(int)fmode];

    len = strlen(mode);
    memcpy(packet + packet_size, mode, len + 1);
    packet_size += len + 1;

    return packet_size;
}


/* -------------------------------------------------------------------------- */

// TFTP packet commuunication utility functions

bool tftp_send_ERROR(
        int sd,
        uint32_t toAddr,
        uint16_t toPort,
        uint16_t error_code)
{
    tftp_error_t tftp_error;
    uint16_t packet_size = 0;
    packet_size = tftp_format_ERROR_packet(&tftp_error, error_code);

    return 0 < nu_sendto(sd,
            (const char*)&tftp_error,
            packet_size,
            DEFULT_FLAGS,
            toAddr,
            toPort);
}


/* -------------------------------------------------------------------------- */

bool tftp_send_DATA(
        int sd,
        uint32_t toAddr,
        uint16_t toPort,
        tftp_data_t* tftp_data_ptr,
        uint16_t size)
{
    return 0 < nu_sendto(sd,
            (const char*)tftp_data_ptr,
            size,
            DEFULT_FLAGS,
            toAddr,
            toPort);
}


/* -------------------------------------------------------------------------- */

bool tftp_send_ACK(
        int sd,
        uint32_t toAddr,
        uint16_t toPort,
        uint16_t block)
{
    tftp_ack_t tftp_ack;

    uint16_t packet_size = 0;
    packet_size = tftp_format_ACK_packet(&tftp_ack, block);

    return 0 < nu_sendto(sd,
            (const char*)&tftp_ack,
            packet_size,
            DEFULT_FLAGS,
            toAddr,
            toPort);
}

