#include <sys/msg.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#ifndef DEFAULT_QPATH
#define DEFAULT_QPATH "./"
#endif

#ifndef DEFAULT_QIDX
#define DEFAULT_QIDX 1
#endif

static char error_str[1<<12];

int main(int argc, char *argv[]) {
    key_t qkey, cfg_key = (key_t)-1;

    if (cfg_key == (key_t)-1) {
        qkey = ftok(DEFAULT_QPATH, DEFAULT_QIDX);

        if (qkey == (key_t)-1) {
            perror("Could not obtain the key");

            goto err;
        }
    } else {
        qkey = cfg_key;
    }

    fprintf(stderr, "Obtained key %zd\n", qkey);

    int mqid = msgget(qkey, 0);

    if (mqid == -1) {
        sprintf(error_str, "Could not obtain queue %zd", qkey);
        perror(error_str);

        goto err;
    }

    struct msqid_ds mq_stats;
    int stat_res = msgctl(mqid, IPC_STAT, &mq_stats);

    if (stat_res == -1) {
        sprintf(error_str, "Could not stat queue %zd", qkey);
        perror(error_str);

        goto err;
    }

    bool update_req = true;

    if (!update_req) {
        printf("UID: %d\n", mq_stats.msg_perm.uid);
        printf("GID: %d\n", mq_stats.msg_perm.gid);
        printf("CUID: %d\n", mq_stats.msg_perm.cuid);
        printf("CGID: %d\n", mq_stats.msg_perm.cgid);
        printf("Mode: %o\n", mq_stats.msg_perm.mode);
        printf("No. of messages: %ju\n", mq_stats.msg_qnum);
        printf("Queue bytes limit: %ju\n", mq_stats.msg_qbytes);
        printf("PID of last send: %ju\n", mq_stats.msg_lspid);
        printf("Timestamp of last send: %ju\n", mq_stats.msg_stime);
        printf("PID of last receive: %ju\n", mq_stats.msg_lrpid);
        printf("Timestamp of last receive: %ju\n", mq_stats.msg_rtime);
        printf("Last modified: %ju\n", mq_stats.msg_ctime);

        return EXIT_SUCCESS;
    }

    struct msqid_ds new_stats = mq_stats;
    new_stats.msg_qbytes = 16384;

    struct ipc_perm *new_perms = &(new_stats.msg_perm);
    new_perms->mode  = S_IWUSR | S_IRUSR | S_IWGRP | S_IRGRP;
    new_perms->uid = mq_stats.msg_perm.uid;
    new_perms->gid = mq_stats.msg_perm.gid;

    int set_res = msgctl(mqid, IPC_SET, &new_stats);

    if (set_res == -1) {
        sprintf(error_str, "Could not set queue %zd", qkey);
        perror(error_str);

        goto err;
    }

    fprintf(stderr, "Successfully set queue %zd\n", qkey);

	return EXIT_SUCCESS;

err:

    return EXIT_FAILURE;
}
