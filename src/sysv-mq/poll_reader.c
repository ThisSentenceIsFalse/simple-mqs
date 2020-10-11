
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifndef DEFAULT_BUF_LEN
#define DEFAULT_BUF_LEN (8192 - sizeof(long))
#endif

/*
 * Doesn't matter what the path is as long as it is "accessible"
 */
#ifndef DEFAULT_QPATH
#define DEFAULT_QPATH "./";
#endif

/*
 * Should always be greater than 0, man ftok
 */
#ifndef DEFAULT_PROJ_ID
#define DEFAULT_PROJ_ID 1
#endif

#ifndef DEFAULT_R_PERM
#define DEFAULT_R_PERM (S_IWUSR | S_IRUSR | S_IRGRP)
#endif

enum MSG_TYPE {
    /*
     * Should always be greater than 0, man msgsnd
     */
    MSG_CHUNK = 1,
    MSG_SEP
};

struct msg {
    long msg_type;
    char msg_text[DEFAULT_BUF_LEN];
};

char info_str[1<<10];
struct msg recv_msg_buffer;

int main(int argc, char *argv[]) {
    
    char * const qpath = DEFAULT_QPATH;
    int const proj_id = DEFAULT_PROJ_ID;

    key_t qkey = ftok(qpath, proj_id);

    if (qkey < 0) {
        sprintf(info_str, "Could not get key for mq %s:%d", qpath, proj_id);
        perror(info_str);

        goto err;
    }

    int permflg = DEFAULT_R_PERM;
    int const qid = msgget(qkey, permflg | IPC_CREAT);

    if (qid < 0) {
        sprintf(info_str, "Could not create mq for qid %d", qid);
        perror(info_str);

        goto err;
    }

    fprintf(stderr, "Got mq key %d\n", qid);
    fprintf(stderr, "Receiving messages...\n");

    do {
        ssize_t const bytes_read = msgrcv(qid, (void *)&recv_msg_buffer, 
                                          DEFAULT_BUF_LEN, 0, IPC_NOWAIT);

        if (bytes_read < 0) {
            if (errno == ENOMSG) {
                fprintf(stderr, "No more messages in the queue\n");

                break;
            } else {
                sprintf(info_str, "Error reqding from queue %d", qid);
                perror(info_str);

                goto err;
            }
        }

        fprintf(stderr, "Read message: %zu bytes, type %d \n", bytes_read, recv_msg_buffer.msg_type);

        size_t const msg_written = fwrite((void *)recv_msg_buffer.msg_text, bytes_read, 1, stdout);

        if (msg_written != 1) {
            perror("Error writing to stdout");

            goto err;
        }
    } while (true);

    return EXIT_SUCCESS;

err:
    fprintf(stderr, "Program encountered errors, exiting\n");

    return EXIT_FAILURE;
}
