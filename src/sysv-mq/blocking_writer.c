#include <errno.h>
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

#ifndef DEFAULT_W_PERM
#define DEFAULT_W_PERM (S_IWUSR | S_IRUSR | S_IRGRP)
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

char info_str[1 << 10];
struct msg send_msg_buffer;

/*
 * It should fit in both DEFAULT_BUF_LEN and the system MSGMAX.
 * Implementations differ in their support for determining
 * these values. Linux offers a configurable value in
 * "/proc/sys/kernel/msgmax"
 */
size_t get_msg_max() {
    return DEFAULT_BUF_LEN;
}

int main(int argc, char *argv[]) {
    char * const qpath = DEFAULT_QPATH;
    int const proj_id = DEFAULT_PROJ_ID;

    /*
     * This is what we'll use for splitting input
     */ 
    size_t const msg_max = get_msg_max();
    
    key_t qkey = ftok(qpath, proj_id);

    if (qkey < 0) {
        sprintf(info_str, "Could not get key for mq %s:%d", qpath, proj_id);
        perror(info_str);

        goto err;
    }

    int permflg = DEFAULT_W_PERM;
    int const qid = msgget(qkey, permflg | IPC_CREAT);

    if (qid < 0) {
        sprintf(info_str, "Could not create mq for qid %d", qid);
        perror(info_str);

        goto err;
    }

    fprintf(stderr, "Got mq key %d\n", qid);
    fprintf(stderr, "Sending message...\n");

    size_t msg_bytes;

    while (msg_bytes = fread(send_msg_buffer.msg_text, 1, msg_max, stdin)) {
        if (msg_bytes < msg_max && ferror(stdin) != 0) {
            sprintf(info_str, "Bad read from stdin");
            perror(info_str);

            goto err;
        }

        send_msg_buffer.msg_type = MSG_CHUNK;

        int send_result = msgsnd(qid, (void *)&send_msg_buffer, msg_bytes, 0);

        if (send_result < 0) {
            sprintf(info_str,"Could not send message chunk on queue");
            perror(info_str);

            goto err;
        }
    }

    fprintf(stderr, "Sent message chunk\n");

    return EXIT_SUCCESS;

err:
    fprintf(stderr, "Program encountered errors, exiting\n");

    return EXIT_FAILURE;
}
