/*
 * Simple getopt implementation for Windows
 * Copyright (c) 2024 钟芳道 (DJD)
 */

#include "getopt.h"
#include <string.h>
#include <stdio.h>

char *optarg = NULL;
int optind = 1;
int opterr = 1;
int optopt = 0;

static int optpos = 1;

int getopt(int argc, char * const argv[], const char *optstring) {
    if (optind >= argc || argv[optind] == NULL || argv[optind][0] != '-' || strcmp(argv[optind], "-") == 0) {
        return -1;
    }

    if (strcmp(argv[optind], "--") == 0) {
        optind++;
        return -1;
    }

    char opt = argv[optind][optpos];
    if (opt == '\0') {
        optind++;
        optpos = 1;
        return getopt(argc, argv, optstring);
    }

    const char *optchar = strchr(optstring, opt);
    if (optchar == NULL) {
        optopt = opt;
        if (opterr) {
            fprintf(stderr, "%s: invalid option -- '%c'\n", argv[0], opt);
        }
        optpos++;
        if (argv[optind][optpos] == '\0') {
            optind++;
            optpos = 1;
        }
        return '?';
    }

    if (optchar[1] == ':') {
        // Option requires an argument
        if (argv[optind][optpos + 1] != '\0') {
            // Argument is in same argv element
            optarg = &argv[optind][optpos + 1];
            optind++;
            optpos = 1;
        } else {
            // Argument is in next argv element
            optind++;
            if (optind >= argc || argv[optind] == NULL) {
                optopt = opt;
                if (opterr) {
                    fprintf(stderr, "%s: option requires an argument -- '%c'\n", argv[0], opt);
                }
                return '?';
            }
            optarg = argv[optind];
            optind++;
            optpos = 1;
        }
    } else {
        // Option doesn't require an argument
        optarg = NULL;
        optpos++;
        if (argv[optind][optpos] == '\0') {
            optind++;
            optpos = 1;
        }
    }

    return opt;
}

int getopt_long(int argc, char * const argv[], const char *optstring,
                const struct option *longopts, int *longindex) {
    if (optind >= argc || argv[optind] == NULL || argv[optind][0] != '-') {
        return -1;
    }

    if (strcmp(argv[optind], "--") == 0) {
        optind++;
        return -1;
    }

    if (strlen(argv[optind]) >= 2 && argv[optind][1] == '-') {
        // Long option
        const char *optname = &argv[optind][2];
        const char *eq = strchr(optname, '=');
        size_t optnamelen = eq ? (size_t)(eq - optname) : strlen(optname);

        for (int i = 0; longopts[i].name != NULL; i++) {
            if (strncmp(longopts[i].name, optname, optnamelen) == 0 &&
                strlen(longopts[i].name) == optnamelen) {

                if (longindex) *longindex = i;

                if (longopts[i].has_arg == required_argument) {
                    if (eq) {
                        optarg = (char*)(eq + 1);
                    } else {
                        optind++;
                        if (optind >= argc || argv[optind] == NULL) {
                            if (opterr) {
                                fprintf(stderr, "%s: option '--%s' requires an argument\n",
                                       argv[0], longopts[i].name);
                            }
                            return '?';
                        }
                        optarg = argv[optind];
                    }
                } else if (longopts[i].has_arg == optional_argument) {
                    if (eq) {
                        optarg = (char*)(eq + 1);
                    } else {
                        optarg = NULL;
                    }
                } else {
                    optarg = NULL;
                }

                optind++;
                return longopts[i].val;
            }
        }

        if (opterr) {
            fprintf(stderr, "%s: unrecognized option '--%.*s'\n",
                   argv[0], (int)optnamelen, optname);
        }
        optind++;
        return '?';
    }

    // Short option - use regular getopt
    return getopt(argc, argv, optstring);
}