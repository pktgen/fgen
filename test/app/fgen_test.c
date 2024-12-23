/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2019-2021 Intel Corporation
 */

#include <stdio.h>             // for size_t, EOF, NULL
#include <getopt.h>            // for getopt_long, option
#include <fgen_mmap.h>         // for MMAP_HUGEPAGE_4KB, MMAP_HUGEPAGE_2MB
#include <tst_info.h>          // for tst_cleanup, tst_error, tst_end, tst_s...
#include <unistd.h>            // for getpagesize
#include <sys/stat.h>          // for chmod
#include <bsd/string.h>        // for strlcpy
#include <string.h>            // for strlen
#include <time.h>
#include <pcap.h>
#include <fgen_common.h>        // for FGEN_USED, fgen_countof
#include <fgen_log.h>
#include <fgen_stdio.h>
#include <bits/getopt_core.h>        // for optind
#include <fgen.h>
#include <fgen_strings.h>
#include <fgen_version.h>

#include "fgen_test.h"

#define MAX_FGEN_STRINGS 16
#define MAX_FGEN_FILES 16
typedef struct {
    char *fgen_strings[MAX_FGEN_STRINGS];
    char *fgen_files[MAX_FGEN_FILES];
    int fgen_string_cnt;
    int fgen_file_cnt;
    pcap_t *pcap;
    pcap_dumper_t *pcap_dumper;
    char *pcap_filename;
    int verbose;
} test_info_t;

static test_info_t *info;

// clang-format off
static const char *default_strings[] = {
    "Frame0 := Ether( dst=00:01:02:03:04:05 )/"
        "IPv4(dst=1.2.3.4)/"
        "UDP(sport=5678, dport=1234)/"
        "TSC()/"
        "Payload(size=32, fill=0xaa)",
    "Frame1 := Ether( dst=00:01:02:03:04:05 )/"
        "IPv4(dst=1.2.3.4, src=5.6.7.8)/"
        "UDP(sport=0x1234, dport=1234)/"
        "TSC()/"
        "Payload(fill=0xbb)",
    "Frame2 := Ether(dst=00:11:22:33:44:55, src=01:ff:ff:ff:ff:ff )/"
        "Dot1q(vlan=0x322, cfi=1, prio=7)/"
        "IPv4(dst=1.2.3.4)/"
        "UDP(sport=5678)/"
        "Payload(size=128)",
    "Frame3:=Ether(src=2201:2203:4405)/"
        "Dot1ad(vlan=0x22, cfi=1, prio=7)/"
        "Dot1ad(vlan=0x33, cfi=1, prio=7)/"
        "IPv4(dst=1.2.3.4)/"
        "TCP(sport=0x5678)/"
        "TSC()",
    "Frame4:=Ether(src=2201:2203:4405)/"
        "Dot1Q(vlan=0x22, cfi=1, prio=7)/"
        "Dot1ad(vlan=0x33, cfi=1, prio=7)/"
        "IPv4(dst=1.2.3.4)/"
        "TCP(sport=0x5678)/"
        "TSC()",
};

static const char *pkt_data_string = {
    "3C FD FE E4 34 C0 3C FD FE E4 38 40 08 00 45 00 "
    "00 72 B3 0F 00 00 40 11 3A 45 C6 12 00 01 C6 12 "
    "01 01 04 D2 16 2E 00 5E BE 84 6B 6C 6D 6E 6F 70 "
    "E8 A7 59 CF 57 E2 03 00 54 73 74 61 6D 70 32 32 "
    "61 62 63 64 65 66 67 68 69 6A 6B 6C 6D 6E 6F 70 "
    "71 72 73 74 75 76 77 78 79 7A 30 31 32 33 34 35 "
    "61 62 63 64 65 66 67 68 69 6A 6B 6C 6D 6E 6F 70 "
    "71 72 73 74 75 76 77 78 79 7A 30 31 32 33 34 35 "
};
// clang-format on

static int
add_file(char *filename)
{
    if (info->fgen_file_cnt > MAX_FGEN_FILES)
        return -1;
    info->fgen_files[info->fgen_file_cnt++] = strdupa(filename);

    return 0;
}

static int
add_string(char *str)
{
    if (info->fgen_string_cnt > MAX_FGEN_STRINGS)
        return -1;
    info->fgen_strings[info->fgen_string_cnt++] = strdupa(str);

    return 0;
}

static int
_open_pcap(void)
{
    info->pcap = pcap_open_dead(DLT_EN10MB, 65535);

    unlink(info->pcap_filename);

    info->pcap_dumper = pcap_dump_open(info->pcap, info->pcap_filename);

    chmod(info->pcap_filename, 0666);

    return 0;
}

static int
fgen_start(tst_info_t *tst __fgen_unused, bool create_pcap, int flags)
{
    fgen_t *fg                  = NULL;
    struct pcap_pkthdr pcap_hdr = {0};
    frame_t *f                  = NULL;

    fg = fgen_create(flags);
    if (!fg)
        FGEN_ERR_GOTO(leave, "Failed to create frame generator object\n");

    if (info->fgen_file_cnt > 0) {
        for(int i = 0; i < info->fgen_file_cnt; i++) {
            fgen_printf("  [magenta]Loading file[] '[orange]%d[]' [magenta]files[]\n", info->fgen_file_cnt);
            if (fgen_load_files(fg, info->fgen_files, info->fgen_file_cnt) < 0)
                FGEN_ERR_GOTO(leave, "Failed to load the fgen file\n");
        }
    }
    if (info->fgen_string_cnt > 0) {
        fgen_printf("  [magenta]Loading [orange]%d [magenta]frames[]\n", info->fgen_string_cnt);
        if (fgen_load_strings(fg, info->fgen_strings, info->fgen_string_cnt) < 0)
            FGEN_ERR_GOTO(leave, "Failed to load fgen strings\n");
    }
    fgen_printf("  [magenta]Found [orange]%d [magenta]frames[]\n", fgen_fcnt(fg));

    if (create_pcap && _open_pcap() < 0)
        FGEN_ERR_GOTO(leave, "Failed to create PCAP file\n");

    fgen_decode_t *dc = fgen_decode_create();
    if (!dc)
        goto leave;

    fgen_printf("\n");
    TAILQ_FOREACH (f, &fg->head, next) {
        if (create_pcap) {
            pcap_hdr.ts.tv_sec  = 0;
            pcap_hdr.ts.tv_usec = 0;
            pcap_hdr.len        = fbuf_data_len(f);
            pcap_hdr.caplen     = fbuf_data_len(f);

            pcap_dump((char *)info->pcap_dumper, &pcap_hdr, fbuf_mtod(f, char *));
        }

        if (fgen_decode(dc, fbuf_mtod(f, void *), fbuf_data_len(f), 0) < 0)
            goto leave;

        fgen_print_string(f->name, fgen_decode_text(dc));
    }

    frame_t *r = fgen_find_frame(fg, "Frame0");
    if (!r)
        FGEN_ERR_GOTO(leave, "Failed to find Frame0\n");

    if (fgen_decode_string(pkt_data_string, fbuf_mtod(r, uint8_t *), fbuf_data_len(r)) < 0)
        goto leave;
    if (fgen_decode(dc, fbuf_mtod(r, void *), fbuf_data_len(r), 0) < 0)
        goto leave;
    fgen_print_string(r->name, fgen_decode_text(dc));

    if (create_pcap) {
        pcap_dump_close(info->pcap_dumper);
        pcap_close(info->pcap);
    }

    fgen_decode_destroy(dc);
    fgen_destroy(fg);
    return 0;

leave:
    if (create_pcap) {
        if (info->pcap_dumper)
            pcap_dump_close(info->pcap_dumper);
        if (info->pcap)
            pcap_close(info->pcap);
    }
    fgen_destroy(fg);
    return -1;
}

static void
usage(char *argv0)
{
    printf("usage: %s [options]\n", argv0);
    printf("\n");
    printf("options:\n");
    printf("  -h, --help\n");
    printf("  -V, --verbose\n");
    printf("  -D, --dump\n");
    printf("  -f, --fgen-file <file>     # can have multiple times\n");
    printf("  -s, --fgen-string <string> # can have multiple times\n");
    printf("  -p, --pcap <filename>      # optional <filename> will default to 'frame-generator.pcap'\n");
    printf(" Note: -f and -s are not mutually exclusive, if no -f/-s then use internal defaults\n");
    printf("       Max number of files is %d\n", MAX_FGEN_FILES);
    printf("       Max number of strings is %d\n", MAX_FGEN_STRINGS);
    printf("\n");
}

int
main(int argc, char **argv)
{
    tst_info_t *tst;
    bool create_pcap = false;
    int opt;
    char **argvopt;
    int option_index, flags;
    // clang-format off
    static const struct option lgopts[] = {
        {"pcap", required_argument, NULL, 'p'},
        {"fgen-file", required_argument, NULL, 'f'},
        {"fgen-string", required_argument, NULL, 's'},
        {"verbose", no_argument, NULL, 'V'},
        {"dump", no_argument, NULL, 'D'},
        {"help", no_argument, NULL, 'h'},
        {NULL, 0, 0, 0}
    };
    // clang-format on

    fgen_printf("Version: %s\n", fgen_version());

    info = calloc(1, sizeof(test_info_t));
    if (!info) {
        printf("unable to allocate memory for internal structure\n");
        exit(-1);
    }

    argvopt = argv;

    optind  = 0;
    flags   = 0;
    info->verbose = 0;
    while ((opt = getopt_long(argc, argvopt, "hVvDf:s:p::", lgopts, &option_index)) != EOF) {
        switch (opt) {
        case 'f':
            if (add_file(optarg) < 0)
                printf("too many fgen files > %d\n", MAX_FGEN_FILES);
            break;
        case 's':
            if (add_string(optarg) < 0)
                printf("too many fgen strings > %d\n", MAX_FGEN_STRINGS);
            break;
        case 'p':
            create_pcap = true;
            if (!optarg)
                optarg = (char *)(uintptr_t) "frame-generator.pcap";
            info->pcap_filename = strdupa(optarg);
            break;
        case 'V':
            flags |= FGEN_VERBOSE;
            break;
        case 'D':
            flags |= FGEN_DUMP_DATA;
            break;
        case 'v':
            info->verbose = 1;
            break;
        case 'h':
            usage(argv[0]);
            return 0;
        default:
            break;
        }
    }

    if (info->fgen_file_cnt == 0 && info->fgen_string_cnt == 0) {
        for (int i = 0; i < fgen_countof(default_strings); i++)
            add_string((char *)(uintptr_t)default_strings[i]);
    }

    tst = tst_start("Frame Generator (fgen)");

    fgen_start(tst, create_pcap, flags);

    tst_end(tst, TST_PASSED);

    return 0;
}
