#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/stat.h>
#include <sys/types.h>

char info_str[1<<10];

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

struct msg send_msg_buffer;

int main(int argc, char *argv[]) {
    char * const qpath = default_qpath;
    int const proj_id = default_proj_id;

    key_t qkey = ftok(qpath, proj_id);

    if (qkey < 0) {
        sprintf(info_str, "Could not get key for mq %s:%d", qpath, proj_id);
        perror(info_str);

        goto err;
    }

    int permflg = S_IRUSR | S_IWUSR | S_IRGRP;
    int const qid = msgget(qkey, permflg | IPC_CREAT);

    if (qid < 0) {
        sprintf(info_str, "Could not create mq for qid %d", qid);
        perror(info_str);

        goto err;
    }

    fprintf(stderr, "Got mq key %d\n", qid);
    fprintf(stderr, "Sending message...\n");

    char msg_text[] = "A";

    send_msg_buffer.msg_type = MSG_CHUNK;
    strncpy(send_msg_buffer.msg_text, msg_text, MAX_MSG_LEN);
    
    int send_result = msgsnd(qid, (void *)&send_msg_buffer, sizeof(msg_text), 0);

    if (send_result < 0) {
        sprintf(info_str,"Could not send Hello message on queue");
        perror(info_str);

        goto err;
    }

    fprintf(stderr, "Sent message \"%s\"\n", msg_text);

    return EXIT_SUCCESS;

err:
    fprintf(stderr, "Program encountered errors, exiting\n");

    return EXIT_FAILURE;
}
