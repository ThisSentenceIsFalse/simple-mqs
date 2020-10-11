
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/stat.h>
#include <sys/types.h>

char info_str[1<<10];

/*
 * Doesn't matter what the path is as long as it is "accessible"
 */
char *const default_qpath = "./";

/*
 * Should always be greater than 0, man ftok
 */
int const default_proj_id = 1;

enum MSG_TYPE {
    /*
     * Should always be greater than 0, man msgsnd
     */
    MSG_CHUNK = 1,
    MSG_SEP
};

#define MAX_MSG_LEN (8192 - sizeof(long))

struct msg {
    long msg_type;
    char msg_text[MAX_MSG_LEN];
};

struct msg recv_msg_buffer;

typedef struct msg msg;

int main(int argc, char *argv[]) {
    
    char * const qpath = default_qpath;
    int const proj_id = default_proj_id;

    key_t qkey = ftok(qpath, proj_id);

    if (qkey < 0) {
        sprintf(info_str, "Could not get key for mq %s:%d", qpath, proj_id);
        perror(info_str);

        goto err;
    }

    int permflg = S_IRGRP;
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
                                          MAX_MSG_LEN, 0, IPC_NOWAIT);

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
