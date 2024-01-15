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
#include <time.h>
#include <pcap.h>
#include <fgen_common.h>        // for FGEN_USED, fgen_countof
#include <fgen_log.h>
#include <fgen_stdio.h>
#include <bits/getopt_core.h>        // for optind
#include <fgen.h>
#include <fgen_strings.h>

#include "fgen_test.h"

#define MAX_FGEN_STRINGS 32
static pcap_t *pcap;
static pcap_dumper_t *pcap_dumper;
static char pcap_filename[512];
static char fgen_filename[512];
static int pkt_string_cnt;
static int verbose;

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
static char *pkt_strings[MAX_FGEN_STRINGS];
// clang-format on

static int
_open_pcap(void)
{
    pcap = pcap_open_dead(DLT_EN10MB, 65535);

    unlink(pcap_filename);

    pcap_dumper = pcap_dump_open(pcap, pcap_filename);

    chmod(pcap_filename, 0666);

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

    if (strlen(fgen_filename) > 0) {
        fgen_printf("  [magenta]Loading file[] '[orange]%s[]'\n", fgen_filename);
        if (fgen_load_file(fg, fgen_filename) < 0)
            FGEN_ERR_GOTO(leave, "Failed to load the fgen file\n");
    } else {
        fgen_printf("  [magenta]Loading [orange]Default [magenta]Frames[]\n");
        if (fgen_load_strings(fg, default_strings, fgen_countof(default_strings)) < 0)
            FGEN_ERR_GOTO(leave, "Failed to load fgen strings\n");
    }
    fgen_printf("  [magenta]Found [orange]%d [magenta]packets[]\n", fgen_fcnt(fg));

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

            pcap_dump((char *)pcap_dumper, &pcap_hdr, fbuf_mtod(f, char *));
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
        pcap_dump_close(pcap_dumper);
        pcap_close(pcap);
    }

    fgen_decode_destroy(dc);
    fgen_destroy(fg);
    return 0;

leave:
    if (create_pcap) {
        if (pcap_dumper)
            pcap_dump_close(pcap_dumper);
        if (pcap)
            pcap_close(pcap);
    }
    fgen_destroy(fg);
    return -1;
}

int
main(int argc, char **argv)
{
    tst_info_t *tst;
    bool create_pcap = false;
    int opt;
    char **argvopt;
    int option_index, flags;
    static const struct option lgopts[] = {{NULL, 0, 0, 0}};

    argvopt = argv;

    optind  = 0;
    flags   = 0;
    verbose = 0;
    while ((opt = getopt_long(argc, argvopt, "VvDf:p::e:", lgopts, &option_index)) != EOF) {
        switch (opt) {
        case 'p':
            create_pcap = true;
            if (!optarg)
                optarg = (char *)(uintptr_t) "frame-generator.pcap";
            strlcpy(pcap_filename, optarg, sizeof(pcap_filename));
            break;
        case 'f':
            strlcpy(fgen_filename, optarg, sizeof(fgen_filename));
            break;
        case 'V':
            flags |= FGEN_VERBOSE;
            break;
        case 'D':
            flags |= FGEN_DUMP_DATA;
            break;
        case 'v':
            verbose = 1;
            break;
        default:
            break;
        }
    }

    tst = tst_start("Frame Generator (fgen)");

    fgen_start(tst, create_pcap, flags);

    tst_end(tst, TST_PASSED);

    if (pkt_strings[0] != (char *)(uintptr_t)default_strings) {
        for (int i = 0; i < pkt_string_cnt; i++)
            free(pkt_strings[i]);
    }

    return 0;
}
