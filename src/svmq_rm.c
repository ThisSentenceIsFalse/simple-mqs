#include <sys/msg.h>
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

    struct msqid_ds buf;
    int rm_res = msgctl(mqid, IPC_RMID, NULL);

    if (rm_res == -1) {
        sprintf(error_str, "Could not remove queue %zd", qkey);
        perror(error_str);

        goto err;
    }

    fprintf(stderr, "Successfully removed queue %zd\n", qkey);

	return EXIT_SUCCESS;

err:

    return EXIT_FAILURE;
}
