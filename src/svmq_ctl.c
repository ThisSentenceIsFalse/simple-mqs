#include <sys/msg.h>
#include <sys/stat.h>
#include <errno.h>
#include <grp.h>
#include <pwd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>

#ifndef DEFAULT_QPATH
#define DEFAULT_QPATH "./"
#endif

#ifndef DEFAULT_QIDX
#define DEFAULT_QIDX 1
#endif

static char error_str[1<<12];

enum CHANGES {
    UID_CHG = 1,
    GID_CHG = 1 << 1,
    MODE_CHG = 1 << 2,
    QBYTES_CHG = 1 << 3
};

/*
 * Changing queue UIDs on Linux also affects the GID in ways which may seem
 * counterintuitive. We smooth these quirks by being explicit: set the primary
 * GID of the user when setting the UID. But we also want to support overriding
 * the GID, which can appear before a UID setting on the command line.
 */
struct update_cfg {
    uint8_t set;
    uid_t uid;
    gid_t gid;
    mode_t mode;
    msglen_t qbytes;
};

struct ctl_cfg {
    char *path;
    int8_t idx;
    key_t key;
    struct update_cfg update;
};

typedef struct ctl_cfg ctl_cfg;

static bool parse_key(key_t *buf, char *arg) {
    if (strcmp(arg, "private") == 0) {
        *buf = IPC_PRIVATE;

        return true;
    }

    return sscanf(arg, "%jd", buf) == 1;
}

static mode_t const exec_mask = S_IXUSR | S_IXGRP | S_IXOTH;

static bool parse_mode(mode_t *mode, char *arg) {
    mode_t parsed_mode;

    if (sscanf(arg, "%o", &parsed_mode) == 1) {
        if (((parsed_mode & exec_mask) == 0) && parsed_mode < 0777) {
            *mode = parsed_mode;

            return true;
        }
    }

    return false;
}

static bool parse_idx(int8_t *idx, char *arg) {
    int8_t parsed_idx;

    if (sscanf(arg, "%hhu", &parsed_idx) == 1) {
        if (parsed_idx != 0) {
            *idx = parsed_idx;

            return true;
        }
    }
    
    return false;
}

// TODO: enforce some better checks based on size 
static struct passwd *get_userdata(char *user) {
    uintmax_t uid;

    bool scan_success = sscanf(user, "%ju", &uid) == 1;

    return scan_success ? getpwuid((uid_t) uid) : getpwnam(user);
}

static struct group *get_groupdata(char *group) {
    uintmax_t gid;

    bool scan_success = sscanf(group, "%ju", &gid) == 1;

    return scan_success ? getgrgid((gid_t) gid) : getgrnam(group);
}

static ctl_cfg parse_opts(int argc, char *argv[]) {
    ctl_cfg cfg = {
        .path = DEFAULT_QPATH,
        .idx = DEFAULT_QIDX,
        .key = (key_t)-1,
    };
    char opt;
    bool group_override = false;

    while ((opt = getopt(argc, argv, "p:i:k:u:g:m:")) != -1) {
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
            case 'm': {
                if (!parse_mode(&cfg.update.mode, optarg)) {
                    perror("Failed to parse mode value");

                    exit(EXIT_FAILURE);
                }

                cfg.update.set |= MODE_CHG;

                break;
            }
            case 'u': {
                struct passwd *udata = get_userdata(optarg);

                if (udata == NULL) {
                    perror("Failed to parse user value");

                    exit(EXIT_FAILURE);
                }

                cfg.update.uid = udata->pw_uid;
                cfg.update.set |= UID_CHG;

                break;
            }
            case 'g': {
                struct group *gdata = get_groupdata(optarg);

                if (gdata == NULL) {
                    perror("Failed to parse group value");

                    exit(EXIT_FAILURE);
                }

                cfg.update.gid = gdata->gr_gid;
                cfg.update.set |= GID_CHG;

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


key_t get_key(struct ctl_cfg *cfg) {
    if (cfg->key != (key_t)-1) {
        return cfg->key;
    }

    fprintf(stderr, "Converting %s:%u to queue key...\n", cfg->path, cfg->idx);

    return ftok(cfg->path, cfg->idx);
}

void print_stats(struct msqid_ds *mq_stats) {
    printf("UID: %d\n", mq_stats->msg_perm.uid);
    printf("GID: %d\n", mq_stats->msg_perm.gid);
    printf("CUID: %d\n", mq_stats->msg_perm.cuid);
    printf("CGID: %d\n", mq_stats->msg_perm.cgid);
    printf("Mode: %o\n", mq_stats->msg_perm.mode);
    printf("No. of messages: %ju\n", mq_stats->msg_qnum);
    printf("Queue bytes limit: %ju\n", mq_stats->msg_qbytes);
    printf("PID of last send: %ju\n", mq_stats->msg_lspid);
    printf("Timestamp of last send: %ju\n", mq_stats->msg_stime);
    printf("PID of last receive: %ju\n", mq_stats->msg_lrpid);
    printf("Timestamp of last receive: %ju\n", mq_stats->msg_rtime);
    printf("Last modified: %ju\n", mq_stats->msg_ctime);
}

int main(int argc, char *argv[]) {
    ctl_cfg cfg = parse_opts(argc, argv);
    key_t qkey = get_key(&cfg);

    if (qkey == (key_t)-1) {
        perror("Could not obtain queue key");

        exit(EXIT_FAILURE);
    }

    fprintf(stderr, "Obtained queue key %zd\n", qkey);

    int mqid = msgget(qkey, 0);

    if (mqid == -1) {
        sprintf(error_str, "Could not obtain queue %zd", qkey);
        perror(error_str);

        exit(EXIT_FAILURE);
    }

    struct msqid_ds mq_stats;
    int stat_res = msgctl(mqid, IPC_STAT, &mq_stats);

    if (stat_res == -1) {
        sprintf(error_str, "Could not stat queue %zd", qkey);
        perror(error_str);

        exit(EXIT_FAILURE);
    }

    bool update_req = cfg.update.set != 0;

    if (!update_req) {
        print_stats(&mq_stats);

        exit(EXIT_SUCCESS);
    }

    struct msqid_ds new_stats = mq_stats;
    new_stats.msg_qbytes = 16384;

    struct ipc_perm *new_perms = &new_stats.msg_perm;

    if (cfg.update.set & MODE_CHG) {
        fprintf(stderr, "Setting mode %o\n", cfg.update.mode);

        new_perms->mode = cfg.update.mode;
    }

    if (cfg.update.set & UID_CHG) {
        fprintf(stderr, "Setting user %d\n", cfg.update.uid);

        new_perms->uid = cfg.update.uid;
    }

    if (cfg.update.set & GID_CHG) {
        fprintf(stderr, "Setting group %d\n", cfg.update.gid);

        new_perms->gid = cfg.update.gid;
    }

    int set_res = msgctl(mqid, IPC_SET, &new_stats);

    if (set_res == -1) {
        sprintf(error_str, "Could not set queue %zd", qkey);
        perror(error_str);

        exit(EXIT_FAILURE);
    }

    fprintf(stderr, "Successfully set queue %zd\n", qkey);

	exit(EXIT_SUCCESS);
}
