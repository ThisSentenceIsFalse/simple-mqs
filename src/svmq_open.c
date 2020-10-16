#include <sys/msg.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>

#ifndef DEFAULT_QPATH
#define DEFAULT_QPATH "./"
#endif

#ifndef DEFAULT_QIDX
#define DEFAULT_QIDX ((char)1)
#endif

static char error_str[1<<12];

struct open_cfg {
    char *path;
    char idx;
    key_t key;
    int excl;
    mode_t mode;
};

bool parse_key(key_t *buf, char *arg) {
    if (strcmp(arg, "private") == 0) {
        *buf = IPC_PRIVATE;

        return true;
    }

    return sscanf(arg, "%jd", buf) == 1;
}

bool parse_mode(mode_t *mode, char *arg) {
    mode_t parsed_mode;

    if (sscanf(arg, "%o", &parsed_mode) == 1) {
        if (parsed_mode < 0700) {
            *mode = parsed_mode;

            return true;
        }
    }

    return false;
}

bool parse_idx(char *idx, char *arg) {
    char parsed_idx;

    if (sscanf(arg, "%c", &idx) == 1) {
        if (parsed_idx != 0) {
            *idx = parsed_idx;

            return true;
        }
    }
    
    return false;

}

struct open_cfg parse_opts(int argc, char *argv[]) {
    struct open_cfg cfg = {
        .path = DEFAULT_QPATH,
        .idx = DEFAULT_QIDX,
        .key = (key_t)-1,
        .excl = 0,
        .mode = S_IWUSR | S_IRUSR | S_IRGRP
    };
    char opt;

    while ((opt = getopt(argc, argv, "p:i:k:m:x")) != -1) {
        switch (opt) {
            case 'p': {
                cfg.path = optarg;

                break;
            }
            case 'i': {
                if (!parse_idx(&cfg.idx, optarg)) {
                    perror("Failed to parse index value");

                    exit(EXIT_FAILURE);
                }

                break;
            }
            case 'k': {
                if (!parse_key(&cfg.key, optarg)) {
                    perror("Failed to parse key value");

                    exit(EXIT_FAILURE);
                }

                break;
            }
            case 'x': {
                cfg.excl = IPC_EXCL;

                break;
            }
            case 'm': {
                if (!parse_mode(&cfg.mode, optarg)) {
                    perror("Failed to parse mode value");

                    exit(EXIT_FAILURE);
                }

                break;
            }
            default: {
                 fprintf(stderr, "TODO: Usage doc\n");

                 exit(EXIT_FAILURE);
            }
        }
    }

    return cfg;
}

key_t get_key(struct open_cfg *cfg) {
    if (cfg->key != (key_t)-1) {
        return cfg->key;
    }

    fprintf(stderr, "Converting %s:%u to queue key...\n", cfg->path, cfg->idx);

    return ftok(cfg->path, cfg->idx);
}

int main(int argc, char *argv[]) {
    struct open_cfg cfg = parse_opts(argc, argv);
    key_t qkey = get_key(&cfg);

    if (qkey == (key_t)-1) {
        perror("Could not obtain queue key");

        exit(EXIT_FAILURE);
    }

    fprintf(stderr, "Obtained key %jd\n", qkey);

    int mqflg = IPC_CREAT | cfg.excl | cfg.mode;
    int mqid = msgget(qkey, mqflg);

    if (mqid == -1) {
        sprintf(error_str, "Could not create queue %zd", qkey);
        perror(error_str);

        exit(EXIT_FAILURE);
    } else {
        fprintf(stderr, "Successfully created queue %zd\n", qkey);
    }

	exit(EXIT_SUCCESS);
}
