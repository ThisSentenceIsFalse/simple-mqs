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
    bool cfg_excl = false;

    if (cfg_key == (key_t)-1) {
        qkey = ftok(DEFAULT_QPATH, DEFAULT_QIDX);

        if (qkey == (key_t)-1) {
            sprintf(error_str, "Could not obtain key %s:%zd\n", DEFAULT_QPATH, DEFAULT_QIDX);
            perror(error_str);

            goto err;
        }
    } else {
        qkey = cfg_key;
    }

    fprintf(stderr, "Obtained key %zd\n", qkey);

    int mqmode = S_IWUSR | S_IRUSR | S_IRGRP;
    int mqflg = IPC_CREAT | IPC_EXCL | mqmode;
    int mqid = msgget(qkey, mqflg);

    if (mqid == -1) {
        if (errno == EEXIST && !cfg_excl) {
            fprintf(stderr, "Queue for key %zd already exists\n", qkey);
        } else {
            sprintf(error_str, "Could not create queue %zd", qkey);
            perror(error_str);

            goto err;
        }

        goto err;
    } else {
        fprintf(stderr, "Successfully created queue %zd\n", qkey);
    }

	return EXIT_SUCCESS;

err:

    return EXIT_FAILURE;
}
