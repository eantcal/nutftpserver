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

#include "nuTftpServer.h"
#include "nuCriticalSection.h"
#include <signal.h>
#include <errno.h>
#include <stdint.h>


/* -------------------------------------------------------------------------- */

#define MAX_FRAME_SIZE 1500

#define PATH_SEPARATOR_CHAR '/'

extern const char* tftp_file_mode[];

#define TFTP_OPCODE_SIZE (sizeof(uint16_t))

/* -------------------------------------------------------------------------- */

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
extern const char tftp_error_codes[8][64];


/* -------------------------------------------------------------------------- */

// Active connections list management functions
static
nu::critical_section active_connection_list_cs = "active_connection_list";

static void active_connection_list__delete(int index);
static void active_connection_list__invalidate();
static int active_connection_list__search_for(uint32_t fromAddr, uint16_t fromPort);
static int active_connection_list__insert(uint32_t fromAddr, uint16_t fromPort);
static void active_connection_list__show();

/* -------------------------------------------------------------------------- */
// TFTPD IPC's functions

// tftp_start_server function creates and
// passes an instance of the following structure to
// the tftp_server thread
typedef struct _IPC_thread_param
{
    int max_sessions;
    char r_path[PATH_MAX];
    char w_path[PATH_MAX];
    int tftpd;
    int task_prio;
    unsigned short port_of_service;
    int opened_sessions;
    bool tftp_server_running;
    bool ipc_used;
    bool stop_cmd_issued;
    int last_err_code;
    int tid;

}
IPC_thread_param;

static void tftpd_init_ipc(void);
static IPC_thread_param* tftpd_get_ipc(void);
static void tftpd_free_ipc(IPC_thread_param* ipc);

#define TFTP_THREAD_PARAM_T void*


/* -------------------------------------------------------------------------- */

// Thread functions prototypes
void* tftp_server(TFTP_THREAD_PARAM_T arg);
void* tftp_RRQ_session_thread(TFTP_THREAD_PARAM_T arg);
void* tftp_WRQ_session_thread(TFTP_THREAD_PARAM_T arg);

typedef struct _tftp_session_param
{
    char r_path[PATH_MAX];
    char w_path[PATH_MAX];
    uint32_t fromAddr = 0;
    uint16_t fromPort = 0;
    char frame[MAX_FRAME_SIZE];
    uint16_t frame_size = 0;

    IPC_thread_param* server_ipc = 0;
    int session_index = 0;
    bool used = false;
}
tftp_session_param;


/* -------------------------------------------------------------------------- */

tftp_session_param tftp_session_param_table[TFTP_MAX_CONNECTION << 1];


/* -------------------------------------------------------------------------- */

static tftp_session_param* release_session_param(void)
{
    static int current = 0;
    tftp_session_param* ptr = 0;

    int i = 0;

    do {
        ptr = &tftp_session_param_table[current];
        current = (current + 1) % 
            (sizeof(tftp_session_param_table) / sizeof(tftp_session_param));

        ++i;
    } 
    while (ptr->used == true && i < TFTP_MAX_CONNECTION);

    ptr->used = true;

    return ptr;
}


/* -------------------------------------------------------------------------- */

static void free_session(tftp_session_param* ptr)
{
    ptr->used = false;
}


/* -------------------------------------------------------------------------- */

int t_start(
        unsigned long& tid,
        void* thread_proc,
        unsigned long targs[],
        bool detach = true)
{
    // Create and start the thread
    pthread_t my_tid;

    int result = 
        pthread_create(&my_tid, NULL, (void*(*)(void*)) thread_proc, (void*)targs[0]);

    if (result != 0) 
    {
        NU_TRACE("[TFTP]", NU_TM_TFTP, NU_TL_ERR, 
                "pthread_create error=%i errno=%i", result, errno);
        perror("pthread_create");
        result = errno;
    }
    else if (detach && (result = pthread_detach(my_tid)) != 0) 
    {
        NU_TRACE("[TFTP]", NU_TM_TFTP, NU_TL_ERR, 
                "pthread_detach error=%i errno=%i", result, errno);
        perror("pthread_detach");
        result = errno;
    }

    tid = (unsigned long)my_tid;

    return result;
}


/* -------------------------------------------------------------------------- */

TFTPD_HANDLE tftp_start_server(
        int task_prio,
        int max_sessions,
        const char* r_path,
        const char* w_path,
        unsigned short port_of_service,
        int traceLevel)
{
    unsigned long targs[4] = { 0 };
    unsigned long tid = 0;
    unsigned long err_code = 0;
    IPC_thread_param* ipc;

    if (max_sessions <= 0 ||
            !r_path ||
            !w_path ||
            port_of_service == 0)
    {
        NU_TRACE("[TFTP]", NU_TM_TFTP, NU_TL_ERR,
                "tftp_start_server: bad parameters line=%i", __LINE__);

        return 0; // error, no ipc
    }

    int tftpd = nu_create();

    if (tftpd < 0) {
        NU_TRACE("[TFTP]", NU_TM_TFTP, NU_TL_ERR,
                "tftp_start_server::nu_create error line=%i", __LINE__);

        return 0; // error, no ipc
    }

    ipc = tftpd_get_ipc();

    memset(ipc, 0, sizeof(IPC_thread_param));

    strncpy(ipc->r_path, r_path, PATH_MAX);
    strncpy(ipc->w_path, w_path, PATH_MAX);
    ipc->max_sessions = max_sessions;
    ipc->tftpd = tftpd;
    ipc->task_prio = task_prio;
    ipc->port_of_service = port_of_service;
    ipc->last_err_code = TFTP_ERROR__SUCCESS;

    targs[0] = (unsigned long)ipc;
    err_code = t_start(/*in/out*/ tid, (void*)tftp_server, targs, false);
    ipc->tid = tid;

    if (err_code != CALL_SUCCESS) {
        NU_TRACE("[TFTP]", NU_TM_TFTP, NU_TL_ERR,
                "tftp_start_server::t_start = 0x%x line=%i errno=%i",
                int(err_code), int(__LINE__), errno);

        nu_free_sock(tftpd);
        tftpd_free_ipc(ipc);

        return 0; // error, no ipc
    }

    return (TFTPD_HANDLE)ipc;
}


/* -------------------------------------------------------------------------- */

void tftp_stop_server(TFTPD_HANDLE handle)
{
    IPC_thread_param* ipc = (IPC_thread_param*)handle;

    // Force tftp server to exit by while loop
    ipc->stop_cmd_issued = true;
    close(ipc->tftpd);
}


/* -------------------------------------------------------------------------- */

unsigned int tftp_get_opened_sessions_count(TFTPD_HANDLE handle)
{
    IPC_thread_param* ipc = (IPC_thread_param*)handle;
    return ipc->opened_sessions;
}


/* -------------------------------------------------------------------------- */

bool tftp_is_server_running(TFTPD_HANDLE handle)
{
    IPC_thread_param* ipc = (IPC_thread_param*)handle;
    return ipc->tftp_server_running;
}


/* -------------------------------------------------------------------------- */

bool tftp_stop_cmd_issued(TFTPD_HANDLE handle)
{
    IPC_thread_param* ipc = (IPC_thread_param*)handle;
    return ipc->stop_cmd_issued;
}


/* -------------------------------------------------------------------------- */

int tftp_get_last_server_error_code(TFTPD_HANDLE handle)
{
    IPC_thread_param* ipc = (IPC_thread_param*)handle;
    return ipc->last_err_code;
}


/* -------------------------------------------------------------------------- */

void* tftp_server(TFTP_THREAD_PARAM_T arg)
{
    IPC_thread_param* ipc;
    int recv_size = 0;
    char buf[TFTP_MAX_BUFFER_SIZE];
    uint32_t fromAddr;
    uint16_t fromPort;
    tftp_session_param* session_param;
    tftp_opcode_t opcode;
    unsigned long targs[4] = { 0 };
    unsigned long tid = 0;
    unsigned long err_code;
    int index = 0;

    ipc = (IPC_thread_param*)arg;

    ipc->tftp_server_running = true;
    ipc->stop_cmd_issued = false;

    NU_TRACE("[TFTP]", NU_TM_TFTP, NU_TL_DBG,
            "tftp server started on port %i (0x%x)",
            ipc->port_of_service,
            ipc->port_of_service);

    // Bind on TFTP_SERVER_PORT

    active_connection_list__invalidate();

    if (!nu_bind_port(ipc->tftpd, ipc->port_of_service)) {
        NU_TRACE("[TFTP]", NU_TM_TFTP, NU_TL_ERR,
                "tftp_server bind failed");
    }
    else while (true) {
        recv_size = nu_recvfrom(ipc->tftpd,
                buf,
                TFTP_MAX_BUFFER_SIZE,
                0,   // flags
                &fromAddr,
                &fromPort);

        if (recv_size <= 0) {
            NU_TRACE("[TFTP]", NU_TM_TFTP, NU_TL_ERR,
                    "tftp_server recv fails: disconnetting...");

            break; // disconnect
        }


        if ((index = active_connection_list__search_for(fromAddr, fromPort)) >= 0) {
            //Port already present
            NU_TRACE("[TFTP]", NU_TM_TFTP, NU_TL_WRN,
                    "tftp_server: connection present %x-%i ", fromAddr, fromPort);

            if (ipc->opened_sessions == 0) 
                active_connection_list__invalidate();
            else 
                active_connection_list__show();

            continue;
        }

        opcode = tftp_parse_opcode(buf, recv_size);

        if ((opcode == TFTP_RRQ) || (opcode == TFTP_WRQ)) {
            index = active_connection_list__insert(fromAddr, fromPort);

            if (index < 0 || ipc->opened_sessions >= ipc->max_sessions) {
                NU_TRACE("[TFTP]", NU_TM_TFTP, NU_TL_WRN,
                        "tftp_server request ignore, max worker count reached (%i)",
                        ipc->opened_sessions);

                continue;
            }

            session_param = release_session_param();
            memset(session_param, 0, sizeof(tftp_session_param));
            session_param->fromAddr = fromAddr;
            session_param->fromPort = fromPort;
            strcpy(session_param->w_path, ipc->w_path);
            strcpy(session_param->r_path, ipc->r_path);
            memcpy(session_param->frame, buf, recv_size);
            session_param->frame_size = recv_size;
            targs[0] = (unsigned long)session_param;
            session_param->server_ipc = ipc;
            session_param->session_index = index;

            err_code = t_start(tid,
                    (void*)(opcode == TFTP_RRQ ?
                        tftp_RRQ_session_thread :
                        tftp_WRQ_session_thread),
                    targs);

            if (err_code != CALL_SUCCESS) {
                NU_TRACE("[TFTP]", NU_TM_TFTP, NU_TL_ERR,
                        "tftp_server::t_start = 0x%x line=%i", err_code, __LINE__);

                free_session(session_param);

                active_connection_list__delete(index);
            }
        }
    }


    NU_TRACE("[TFTP]", NU_TM_TFTP, NU_TL_WRN, "tftp server stopped !");

    if (!ipc->stop_cmd_issued) 
        ipc->last_err_code = TFTP_ERROR__NOT_DEFINED;

    nu_free_sock(ipc->tftpd);
    ipc->tftpd = 0;
    ipc->tftp_server_running = false;
    tftpd_free_ipc(ipc);

    exit(0);

    return 0;
}


/* -------------------------------------------------------------------------- */

void* tftp_RRQ_session_thread(TFTP_THREAD_PARAM_T arg)
{
    tftp_request_t tftp_request;
    tftp_data_t tftp_data;
    tftp_ack_t tftp_ack;

    int reading_sector_size = 0;
    int tftpd_session = -1;
    uint16_t last_ack_block = 0;
    FILE* file = 0;
    char file_path[PATH_MAX + 1] = { 0 };
    int sector_size = 0;
    char frame[MAX_FRAME_SIZE] = { 0 };
    uint16_t block_index = 0;

    //Get session parameters
    tftp_session_param* session_param = (tftp_session_param*)arg;

    try {

        //Increment the session number
        session_param->server_ipc->opened_sessions++;

        NU_TRACE("[TFTP]", NU_TM_TFTP, NU_TL_DBG,
                "tftp_RRQ_session_thread+ (sessions = %i)",
                session_param->server_ipc->opened_sessions);

        //Create a new socket for this task
        tftpd_session = nu_create();

        //Obtain an unused port number and bind it to the socket
        uint16_t bindPort = 0;
        nu_bind_and_getprt(tftpd_session, &bindPort);

        //Parse the RRQ packet
        if (tftp_parse_RQ_packet(
                    &tftp_request, 
                    session_param->frame, 
                    session_param->frame_size)) 
        {
            //We are able to transmit only binary files
            if (tftp_request.fmode != OCTET && tftp_request.fmode != NETASCII) {
                tftp_send_ERROR(tftpd_session,
                        session_param->fromAddr,
                        session_param->fromPort,
                        TFTP_ERROR__ILLEGAL_OPERATION);

                NU_TRACE("[TFTP]", NU_TM_TFTP, NU_TL_ERR,
                        "%s TFTP_ERROR__ILLEGAL_OPERATION errno=%d", file_path, errno);

                session_param->server_ipc->last_err_code = TFTP_ERROR__ILLEGAL_OPERATION;

                throw 0;
            }

            //Compose complete path of the file requested
            strncpy(file_path, session_param->r_path, PATH_MAX);
            int path_len = strlen(file_path);

            char separator[2] = { PATH_SEPARATOR_CHAR };
            if (path_len > 0 && file_path[path_len] != PATH_SEPARATOR_CHAR)
                strcat(file_path, separator);

            strcat(file_path, tftp_request.filename);

            //Try to open the file
            file = fopen(file_path, "rb");

            NU_TRACE("[TFTP]", NU_TM_TFTP, NU_TL_DBG,
                    "tftp_RRQ_session_thread: (uploading %s)", file_path);

            if (!file) {
                tftp_send_ERROR(tftpd_session,
                        session_param->fromAddr,
                        session_param->fromPort,
                        TFTP_ERROR__FILE_NOT_FOUND);

                NU_TRACE("[TFTP]", NU_TM_TFTP, NU_TL_ERR,
                        "%s TFTP_ERROR__FILE_NOT_FOUND errno=%d", file_path, errno);

                session_param->server_ipc->last_err_code = TFTP_ERROR__FILE_NOT_FOUND;

                throw 0;
            }

            //Calculate the size of the file
            int file_size = 0;

            if (fseek(file, 0L, SEEK_END) == 0) { // is it OK ?  
                //Yes, calculate the size of the file
                file_size = ftell(file);
            }

            if (file_size < 0) {
                tftp_send_ERROR(tftpd_session,
                        session_param->fromAddr,
                        session_param->fromPort,
                        TFTP_ERROR__ACCESS_VIOLATION);

                NU_TRACE("[TFTP]", NU_TM_TFTP, NU_TL_ERR,
                        "%s TFTP_ERROR__ACCESS_VIOLATION errno=%d", file_path, errno);

                session_param->server_ipc->last_err_code = TFTP_ERROR__ACCESS_VIOLATION;

                throw 0;
            }

            fseek(file, 0, SEEK_SET);

            //Calculate the count of the blocks to transmit
            int block_tot = (file_size / TFTP_MAX_BUFFER_SIZE) + 1;

            //Transmit each block
            for (int i = 0; i < block_tot; ++i) {
                // Read a block within the file
                int last_position = ftell(file);

                reading_sector_size = i < (block_tot - 1) ?
                    TFTP_MAX_BUFFER_SIZE :
                    file_size % TFTP_MAX_BUFFER_SIZE;

                if (reading_sector_size) {
                    if (fread(tftp_data.buffer, reading_sector_size, 1, file)) {
                        sector_size = ftell(file) - last_position;
                    }
                    else {
                        tftp_send_ERROR(tftpd_session,
                                session_param->fromAddr,
                                session_param->fromPort,
                                TFTP_ERROR__ACCESS_VIOLATION);

                        NU_TRACE("[TFTP]", NU_TM_TFTP, NU_TL_ERR,
                                "%s TFTP_ERROR__ACCESS_VIOLATION 2 errno=%d", file_path, errno);

                        session_param->server_ipc->last_err_code = TFTP_ERROR__ACCESS_VIOLATION;

                        throw 0;
                    }
                }
                else {
                    sector_size = 0; // the size of the file is divisble for TFTP_MAX_BUFFER_SIZE
                }

                // Format a tfpt_data packet
                sector_size = tftp_format_DATA_packet(&tftp_data, ++block_index, 0, sector_size);

                bool packet_acknowledged = false;
                bool wait_for_valid_ack = false;

                //For a max number of the attemps, try to send the block
                for (int attempt = 0; attempt < TFTP_RECV_ATTEMPTS; ++attempt) {
                    // Send the packet
                    if (!wait_for_valid_ack) {
                        if (!tftp_send_DATA(tftpd_session,
                                    session_param->fromAddr,
                                    session_param->fromPort,
                                    &tftp_data,
                                    sector_size))
                        {
                            throw 0;
                        }
                    }

                    //Wait for an ack message
                    struct timeval timeout = {0};
                    timeout.tv_usec = 0;
                    timeout.tv_sec = TFTP_RECV_TIMEOUT;

                    int ack_size = nu_recvfrom_timeout(tftpd_session,
                            frame, MAX_FRAME_SIZE, 0,
                            &session_param->fromAddr,
                            &session_param->fromPort,
                            &timeout);

                    if (ack_size <= 0) {
                        NU_TRACE("[TFTP]", NU_TM_TFTP, NU_TL_WRN,
                                "tftp_RRQ_session_thread: error or timeout");

                        break;
                    }

                    //Ack was received, parse and validate it
                    if (ack_size > 0) {
                        if (tftp_parse_ACK_packet(&tftp_ack, frame, (uint16_t)ack_size)) {
                            NU_TRACE("[TFTP]", NU_TM_TFTP, NU_TL_PED,
                                    "tftp_RRQ_session_thread:ACK %i", tftp_ack.block);

                            if (tftp_ack.block != block_index) {
                                NU_TRACE("[TFTP]", NU_TM_TFTP, NU_TL_WRN,
                                        "tftp_RRQ_session_thread: bad block %i!=ack block %i",
                                        block_index,
                                        tftp_ack.block);

                                // if you receive an ack of a block already acknowledged,
                                // return to the receive fase
                                wait_for_valid_ack = tftp_ack.block <= last_ack_block;
                                continue;
                            }

                            last_ack_block = tftp_ack.block; // last valid acknowledged packet
                            packet_acknowledged = true;
                            break; // no error, break "attempt" loop
                        }
                        else {
                            NU_TRACE("[TFTP]", NU_TM_TFTP, NU_TL_WRN,
                                    "tftp_RRQ_session_thread: ACK sending fails");

                        }
                    }
                    else if (ack_size == 0) {
                        NU_TRACE("[TFTP]", NU_TM_TFTP, NU_TL_WRN,
                                "tftp_RRQ_session_thread: no ACK, last block = %i",
                                tftp_ack.block);
                        break; /// TEMP
                    }
                    else {
                        break; // error in the communication
                    }
                } // end of "for loop"

                if (!packet_acknowledged) {
                    tftp_send_ERROR(tftpd_session,
                            session_param->fromAddr,
                            session_param->fromPort,
                            TFTP_ERROR__NOT_DEFINED);

                    NU_TRACE("[TFTP]", NU_TM_TFTP, NU_TL_WRN,
                            "%s !packet_acknowledged TFTP_ERROR__NOT_DEFINED errno=%d", 
                            file_path, errno);

                    session_param->server_ipc->last_err_code = TFTP_ERROR__NOT_DEFINED;

                    NU_TRACE("[TFTP]", NU_TM_TFTP, NU_TL_ERR, "RRQ operation stopped");

                    throw 0;
                }

            }

        }
        else {
            throw 0;
        }
    }
    catch (int) {
    }

    //Free all allocated resources
    active_connection_list__show();
    active_connection_list__delete(session_param->session_index);
    active_connection_list__show();
    nu_free_sock(tftpd_session);
    session_param->server_ipc->opened_sessions--;

    NU_TRACE("[TFTP]", NU_TM_TFTP, NU_TL_DBG,
            "tftp_RRQ_session_thread- (sessions = %i)",
            session_param->server_ipc->opened_sessions);

    free_session(session_param);

    if (file) 
        fclose(file);

    return 0;
}


/* -------------------------------------------------------------------------- */

void* tftp_WRQ_session_thread(TFTP_THREAD_PARAM_T arg)
{
    tftp_request_t tftp_request;
    tftp_data_t tftp_data;

    int data_size = 0;
    int tftpd_session = -1;
    char file_path[PATH_MAX + 1] = { 0 };
    char frame[MAX_FRAME_SIZE] = { 0 };
    bool packet_received = false;
    bool operation_completed = false;
    int attempt = 0;

    //Get session parameters
    tftp_session_param* session_param = (tftp_session_param*)arg;
    FILE * file = nullptr;

    try {
        //Increment the session number
        session_param->server_ipc->opened_sessions++;

        NU_TRACE("[TFTP]", NU_TM_TFTP, NU_TL_DBG,
                "tftp_WRQ_session_thread+ (sessions = %i)",
                session_param->server_ipc->opened_sessions);

        //Obtain an unused port number and bind it to the socket
        tftpd_session = nu_create();
        uint16_t bindPort = 0;
        nu_bind_and_getprt(tftpd_session, &bindPort);

        //Parse the WRQ packet
        if (tftp_parse_RQ_packet(&tftp_request,
                    session_param->frame,
                    session_param->frame_size))
        {
            //We are able to receive only binary files
            if (tftp_request.fmode != OCTET && tftp_request.fmode != NETASCII) {
                tftp_send_ERROR(tftpd_session,
                        session_param->fromAddr,
                        session_param->fromPort,
                        TFTP_ERROR__ILLEGAL_OPERATION);

                NU_TRACE("[TFTP]", NU_TM_TFTP, NU_TL_ERR,
                        "%s TFTP_ERROR__ILLEGAL_OPERATION errno=%d", 
                        file_path, errno);

                session_param->server_ipc->last_err_code = 
                    TFTP_ERROR__ILLEGAL_OPERATION;

                throw 0;
            }

            //Compose complete path of the file requested
            strncpy(file_path, session_param->w_path, PATH_MAX);
            int path_len = strlen(file_path);

            char separator[2] = { PATH_SEPARATOR_CHAR };
            if (path_len > 0 && file_path[path_len] != PATH_SEPARATOR_CHAR) 
                strcat(file_path, separator);

            strcat(file_path, tftp_request.filename);

            //Try to open the file, if file exists, over-write it
            file = fopen(file_path, "w+b");

            NU_TRACE("[TFTP]", NU_TM_TFTP, NU_TL_DBG,
                    "tftp_WRQ_session_thread: (downloading %s)", file_path);

            if (!file) {
                tftp_send_ERROR(tftpd_session,
                        session_param->fromAddr,
                        session_param->fromPort,
                        TFTP_ERROR__DISK_FULL);

                NU_TRACE("[TFTP]", NU_TM_TFTP, NU_TL_ERR,
                        "%s TFTP_ERROR__DISK_FULL errno=%d", file_path, errno);

                session_param->server_ipc->last_err_code = TFTP_ERROR__DISK_FULL;

                throw 0;
            }

            uint16_t block_index = 0;

            //Until client send us blocks,
            //ack them and write the content in the file
            do {
                packet_received = false;

                for (attempt = 0; attempt < TFTP_RECV_ATTEMPTS; ++attempt) {
                    //Send ACK (the first ack must be with block number = 0)
                    if (!tftp_send_ACK(tftpd_session,
                                session_param->fromAddr,
                                session_param->fromPort,
                                block_index++)) // block index is 0 for first ack
                    {
                        throw 0;
                    }

                    //Receive a block
                    struct timeval timeout = {0};
                    timeout.tv_usec = 0;
                    timeout.tv_sec = TFTP_RECV_TIMEOUT;

                    data_size = nu_recvfrom_timeout(tftpd_session,
                            frame, MAX_FRAME_SIZE, 0,
                            &session_param->fromAddr,
                            &session_param->fromPort,
                            &timeout);

                    //If OK
                    if (data_size > 0) {
                        //Parse the packet (this should be a DATA packet)
                        if (tftp_parse_DATA_packet(&tftp_data, frame, (uint16_t*)&data_size)) {
                            //Verify if this block is that we are wating for...
                            if (tftp_data.block != block_index) {
                                NU_TRACE("[TFTP]", NU_TM_TFTP, NU_TL_WRN,
                                        "tftp_WRQ_session_thread: block %i!=ack block %i",
                                        block_index,
                                        tftp_data.block);
                                continue;
                            }

                            //data_size value is updated by tftp_parse_DATA_packet
                            //and it should be the size of the block (without the header of
                            //TFTP frame); it's possible that its value is zero, because
                            //the size of the file was divisible by TFTP_MAX_BUFFER_SIZE
                            if (data_size) {
                                //Write the block in the file
                                if (!fwrite(tftp_data.buffer, data_size, 1, file)) 
                                {
                                    tftp_send_ERROR(
                                            tftpd_session,
                                            session_param->fromAddr,
                                            session_param->fromPort,
                                            TFTP_ERROR__DISK_FULL);

                                    NU_TRACE("[TFTP]", NU_TM_TFTP, NU_TL_ERR,
                                            "%s !fwrite TFTP_ERROR__DISK_FULL errno=%d", file_path, errno);

                                    session_param->server_ipc->last_err_code = TFTP_ERROR__DISK_FULL;

                                    throw 0;
                                }

                                //Is it the last one ?
                                if (data_size < TFTP_MAX_BUFFER_SIZE) {
                                    //Ok, all bytes received, operation completed !
                                    operation_completed = true;

                                    //Send ACK
                                    if (!tftp_send_ACK(tftpd_session,
                                                session_param->fromAddr,
                                                session_param->fromPort,
                                                block_index++)) // block index is 0 for first ack
                                    {
                                        tftp_send_ERROR(
                                                tftpd_session,
                                                session_param->fromAddr,
                                                session_param->fromPort,
                                                TFTP_ERROR__NOT_DEFINED);

                                        NU_TRACE("[TFTP]", NU_TM_TFTP, NU_TL_ERR,
                                                "%s !tftp_send_ACK TFTP_ERROR__NOT_DEFINED errno=%d", 
                                                file_path, errno);

                                        session_param->server_ipc->last_err_code = TFTP_ERROR__NOT_DEFINED;

                                        throw 0;
                                    }
                                }
                            } // if (data_size)...

                            packet_received = true;
                            break; // no error, break "attempt" loop

                        } // if 
                        else {
                            NU_TRACE("[TFTP]", NU_TM_TFTP, NU_TL_WRN,
                                    "tftp_WRQ_session_thread: WARNING recv timeout");
                        }
                    }
                    else if (data_size == 0) {
                        NU_TRACE("[TFTP]", NU_TM_TFTP, NU_TL_WRN,
                                "tftp_WRQ_session_thread: no ACK, last block = %i", tftp_data.block);

                        break;
                    }
                    else {
                        break; // error in the communication
                    }

                } // for ...

                if (!packet_received) {
                    tftp_send_ERROR(tftpd_session,
                            session_param->fromAddr,
                            session_param->fromPort,
                            TFTP_ERROR__NOT_DEFINED);

                    NU_TRACE("[TFTP]", NU_TM_TFTP, NU_TL_WRN,
                            "%s !packet_received TFTP_ERROR__NOT_DEFINED errno=%d", 
                            file_path, errno);

                    session_param->server_ipc->last_err_code = TFTP_ERROR__NOT_DEFINED;

                    NU_TRACE("[TFTP]", NU_TM_TFTP, NU_TL_ERR,
                            "tftp_WRQ_session_thread: no ACK. Transfer interrupted");

                    throw 0;
                }

            } 
            while (!operation_completed);

        }
        else {
            throw 0;
        }
    }
    catch (int) {}

    //Free all allocated resources
    active_connection_list__delete(session_param->session_index);
    nu_free_sock(tftpd_session);
    session_param->server_ipc->opened_sessions--;

    NU_TRACE("[TFTP]", NU_TM_TFTP, NU_TL_DBG,
            "tftp_WRQ_session_thread- (sessions = %i)",
            session_param->server_ipc->opened_sessions);

    free_session(session_param);

    if (file) 
        fclose(file);

    return 0;
}


/* -------------------------------------------------------------------------- */

// Active connections list management functions

typedef struct _tftp_connection_t
{
    uint32_t fromAddr;
    uint16_t fromPort;
}
tftp_connection_t;


/* -------------------------------------------------------------------------- */

static tftp_connection_t active_connection_list[TFTP_MAX_CONNECTION];


/* -------------------------------------------------------------------------- */

static void active_connection_list__delete(int index)
{
    nu::autoCs_t acs = active_connection_list_cs;

    NU_TRACE("[TFTP]", NU_TM_TFTP, NU_TL_DBG,
            "active_connection_list__delete %i", index);

    if (index >= 0) {
        active_connection_list[index % TFTP_MAX_CONNECTION].fromAddr = 0;
        active_connection_list[index % TFTP_MAX_CONNECTION].fromPort = 0;
    }
}


/* -------------------------------------------------------------------------- */

static void active_connection_list__invalidate()
{
    nu::autoCs_t acs = active_connection_list_cs;

    int i = 0;
    //Invalidate the list of client ports
    for (i = 0; i < TFTP_MAX_CONNECTION; ++i) {
        active_connection_list[i % TFTP_MAX_CONNECTION].fromAddr = 0;
        active_connection_list[i % TFTP_MAX_CONNECTION].fromPort = 0;
    }
}


/* -------------------------------------------------------------------------- */

static int active_connection_list__search_for(
        uint32_t fromAddr,
        uint16_t fromPort)
{
    nu::autoCs_t acs = active_connection_list_cs;

    int i = 0;
    for (i = 0; i < TFTP_MAX_CONNECTION; ++i)
    {
        if ((active_connection_list[i].fromPort == fromPort) &&
                (active_connection_list[i].fromAddr == fromAddr))
        {
            return i;
        }
    }

    return -1; // invalid index
}


/* -------------------------------------------------------------------------- */

static int active_connection_list__insert(
        uint32_t fromAddr,
        uint16_t fromPort)
{
    nu::autoCs_t acs = active_connection_list_cs;

    NU_TRACE("[TFTP]", NU_TM_TFTP, NU_TL_DBG,
            "active_connection_list__insert %x %i", fromAddr, fromPort);

    int position = -1;
    for (int i = 0; i < TFTP_MAX_CONNECTION; ++i)
    {
        if ((active_connection_list[i].fromPort == 0) &&
                (active_connection_list[i].fromAddr == 0))
        {
            position = i;
            break;
        }
    }

    if (position >= 0)
    {
        active_connection_list[position].fromPort = fromPort;
        active_connection_list[position].fromAddr = fromAddr;
    }

    return position;
}


/* -------------------------------------------------------------------------- */

static void active_connection_list__show()
{
    nu::autoCs_t acs = active_connection_list_cs;

    NU_TRACE("[TFTP]", NU_TM_TFTP, NU_TL_DBG, "active_connection_list__show()");

    for (int i = 0; i < TFTP_MAX_CONNECTION; ++i) {
        if (active_connection_list[i].fromAddr &&
                active_connection_list[i].fromPort)
        {
            NU_TRACE_INF("[TFTP]", "%02i 0x%08x %04i",
                    i,
                    active_connection_list[i].fromAddr,
                    active_connection_list[i].fromPort);
        }
    }
}


/* -------------------------------------------------------------------------- */

// TFTPD IPC's functions
static IPC_thread_param IPC_memory_pool[TFTPD_IPC_POOL_SIZE];


/* -------------------------------------------------------------------------- */

static nu::critical_section IPC_memory_pool_cs = "IPC_memory_pool";


/* -------------------------------------------------------------------------- */

static void tftpd_init_ipc(void)
{
    nu::autoCs_t acs = IPC_memory_pool_cs;

    for (int i = 0; i < TFTPD_IPC_POOL_SIZE; ++i) 
        memset(IPC_memory_pool, 0, sizeof(IPC_thread_param));
}


/* -------------------------------------------------------------------------- */

static IPC_thread_param* tftpd_get_ipc(void)
{
    nu::autoCs_t acs = IPC_memory_pool_cs;

    for (int i = 0; i < TFTPD_IPC_POOL_SIZE; ++i) {
        if (!IPC_memory_pool->ipc_used) {
            IPC_memory_pool[i].ipc_used = true;
            return &IPC_memory_pool[i];
        }
    }

    return 0;
}


/* -------------------------------------------------------------------------- */

static void tftpd_free_ipc(IPC_thread_param* ipc)
{
    nu::autoCs_t acs = IPC_memory_pool_cs;

    for (int i = 0; i < TFTPD_IPC_POOL_SIZE; ++i) {
        if (&IPC_memory_pool[i] == ipc) {
            memset(ipc, 0, sizeof(IPC_thread_param));
            break;
        }
    }
}


/* -------------------------------------------------------------------------- */

int trace_level_signal1(int sig) 
{
    NU_TRACE_INF("[TFTP]", "old trace_level=%i", NU_TRACE_LEVEL);

    ++NU_TRACE_LEVEL;

    if (NU_TRACE_LEVEL > NU_TL_PED) 
        NU_TRACE_LEVEL = NU_TL_PED;

    NU_TRACE_INF("[TFTP]", "new trace_level=%i", NU_TRACE_LEVEL);

    return 0;
}


/* -------------------------------------------------------------------------- */

int trace_level_signal2(int sig) 
{
    NU_TRACE_INF("[TFTP]", "old trace_level=%i", NU_TRACE_LEVEL);

    --NU_TRACE_LEVEL;

    if (int(NU_TRACE_LEVEL) < NU_TL_DIS) 
        NU_TRACE_LEVEL = NU_TL_DIS;

    NU_TRACE_INF("[TFTP]", "new trace_level=%i", NU_TRACE_LEVEL);

    return 0;
}


/* -------------------------------------------------------------------------- */

#define DEFAULT_R_PATH "/tmp";
#define DEFAULT_W_PATH "/tmp";


/* -------------------------------------------------------------------------- */

int main(int argc, char* argv[])
{
    int trace_level = 3;

    signal(SIGPIPE, SIG_IGN);
    //signal(SIGUSR1, (sighandler_t)trace_level_signal1);
    //signal(SIGUSR2, (sighandler_t)trace_level_signal2);

    tftpd_init_ipc();
    active_connection_list__invalidate();

    NU_TRACE_INF("[TFTP]",
            "nuTFTPServer 1.0 - antonino.calderone@gmail.com");

    NU_TRACE_INF("[TFTP]",
            "Usage: %s [GET_DIR] [PUT_DIR] [max_concurrent_sessions] [trace_level]", 
            argv[0]);

    string r_path = DEFAULT_R_PATH;
    string w_path = DEFAULT_W_PATH;
    int max_sessions = TFTP_MAX_CONNECTION;

    if (argc > 1) {
        r_path = argv[1];

        if (argc > 2) {
            w_path = argv[2];

            if (argc > 3) {
                if (argc > 4) 
                    trace_level = atoi(argv[4]);

                int sessions = atoi(argv[3]);

                if (sessions > 0 && sessions <= TFTP_MAX_CONNECTION) {
                    max_sessions = sessions;
                }
                else {
                    NU_TRACE_INF("[TFTP]",
                            "WARNING: max_concurrent_sessions "
                            "%i out of range, default value is used",
                            sessions);
                }
            }
        }
    }

    NU_TRACE_LEVEL = trace_level;
    NU_TRACE_MASK = NU_TM_TFTP;

    if (NU_TRACE_LEVEL < NU_TL_DIS) 
        NU_TRACE_LEVEL = NU_TL_DIS;
    else if (NU_TRACE_LEVEL > NU_TL_PED) 
        NU_TRACE_LEVEL = NU_TL_PED;

    TFTPD_HANDLE handle = tftp_start_server(
            0 /*unused*/,
            max_sessions,
            r_path.c_str(),
            w_path.c_str(),
            69, trace_level);

    NU_TRACE_INF("[TFTP]", "GET_DIR=%s", r_path.c_str());
    NU_TRACE_INF("[TFTP]", "PUT_DIR=%s", w_path.c_str());
    NU_TRACE_INF("[TFTP]", "tmax_concurrent_sessions=%i", max_sessions);
    NU_TRACE_INF("[TFTP]", "trace_level=%i", NU_TRACE_LEVEL);

    while (handle)
        sleep(1);

    return 0;
}

