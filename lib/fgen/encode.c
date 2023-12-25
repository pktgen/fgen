/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023-2024 Intel Corporation
 */

#include <stdint.h>        // for uint32_t, uint16_t, int32_t, uint8_t
#include <stdbool.h>
#include <netinet/in.h>        // for ntohs, htonl, htons

#include <fgen_common.h>
#include <fgen_log.h>
#include <fgen_strings.h>
#include <net/fgen_ether.h>
#include <net/fgen_ip.h>
#include <net/fgen_udp.h>
#include <net/fgen_tcp.h>
#include <net/fgen_vxlan.h>

#include "fgen.h"
#include "decode.h"

extern int _encode_frame(fgen_t *fg, frame_t *f);

#define FGEN_LOPT(_p, _i)                                  \
    ({                                                     \
        if ((_i) >= FGEN_MAX_LAYERS)                       \
            FGEN_ERR_RET("Invalid layer index %d\n", (_i)); \
        &(_p)->opts[(_i)];                                 \
    })

#define STRTOL(_v)                                              \
    ({                                                          \
        long _x;                                                \
        errno = 0;                                              \
        _x    = strtol((_v), NULL, 0);                          \
        if (errno)                                              \
            FGEN_ERR_RET("Unable to parse number '%s'\n", (_v)); \
        _x;                                                     \
    })

static inline const char *
parser_type(opt_type_t typ)
{
    static const char *ptypes[] = FGEN_TYPE_STRINGS;

    if (typ >= 0 && typ <= FGEN_TYPE_COUNT)
        return ptypes[typ];
    return "Unknown type";
}

static inline int
next_layer(fgen_t *fg, frame_t *f, int idx)
{
    ftable_t *t;

    if (idx >= fg->num_layers)
        FGEN_ERR_RET("Next layer %d >= %d\n", idx, fg->num_layers);

    t = fg->opts[idx].tbl;

    return (t && t->fn) ? t->fn(fg, f, idx) : -1;
}

static int
_encode_opts(char *str, char **toks, int nb_toks)
{
    int len;

    str = strtrimset(strtrim(str), "()");

    len = strlen(str);
    if (len)
        return fgen_strtok(str, ", ", toks, nb_toks);

    return 0;
}

static int
_encode_vars(char *str, char **vars, int nb_vars)
{
    int len, cnt;

    str = strtrimset(strtrim(str), "()");

    len = strlen(str);
    if (len) {
        cnt = fgen_strtok(str, "= ", vars, nb_vars);
        return (cnt < 0 || cnt != 2) ? -1 : cnt;
    }

    return 0;
}

static inline int
parser_kvp(char *param, const char **kvps, int len, char **val)
{
    char *kvp[FGEN_MAX_KVP_TOKENS] = {0};

    if (_encode_vars(param, kvp, fgen_countof(kvp)) > 0) {
        for (int i = 0; i < len; i++) {
            if (strncasecmp(kvp[0], kvps[i], strlen(kvps[i])) == 0) {
                *val = kvp[1];
                return i;
            }
        }
    }
    *val = (kvp[0]) ? kvp[0] : (char *)(uintptr_t) "Unknown_key";
    return -1;
}

static int
_encode_ether(fgen_t *fg, frame_t *f, int lidx)
{
    struct ether_header *eth;
    fopt_t *opt = FGEN_LOPT(fg, lidx);

    int num;
    const char *kvps[] = {"dst", "src"};
    char *val;

    eth = fgen_mtod(f, struct ether_header *);
    ether_unformat_addr("FFFF:FFFF:FFFF", (struct ether_addr *)eth->ether_dhost);
    ether_unformat_addr("0000:0000:0000", (struct ether_addr *)eth->ether_shost);

    opt->length = sizeof(struct ether_header);
    fgen_data_len(f) += opt->length;

    if (fg->flags & FGEN_VERBOSE)
        FGEN_INFO("[magenta]params[]:'[orange]%s[]'\n", opt->param_str);

    num = _encode_opts(opt->param_str, fg->params, fgen_countof(fg->params));
    if (num < 0)
        FGEN_ERR_RET("Parameters '%s' invalid\n", opt->param_str);

    for (int i = 0; i < num; i++) {
        switch (parser_kvp(fg->params[i], kvps, fgen_countof(kvps), &val)) {
        case 0:
            ether_unformat_addr(val, (struct ether_addr *)eth->ether_dhost);
            break;
        case 1:
            ether_unformat_addr(val, (struct ether_addr *)eth->ether_shost);
            break;
        default:
            FGEN_ERR_RET("Ether: Invalid key '%s'\n", val);
        }
    }

    switch (next_layer(fg, f, ++lidx)) {
    case FGEN_DOT1Q_TYPE:
        eth->ether_type = htons(FGEN_ETHER_TYPE_VLAN);
        break;
    case FGEN_DOT1AD_TYPE:
        eth->ether_type = htons(FGEN_ETHER_TYPE_QINQ);
        break;
    case FGEN_IPV4_TYPE:
        eth->ether_type = htons(FGEN_ETHER_TYPE_IPV4);
        break;
    case FGEN_ERROR_TYPE:
        FGEN_ERR_RET("Next layer return error\n");
    default:
        eth->ether_type = htons(0x9000);
        break;
    }

    if (fg->flags & FGEN_VERBOSE)
        FGEN_INFO("[magenta]Return '[orange]%s[]'\n", parser_type(opt->typ));

    return FGEN_ETHER_TYPE;
}

static int
_encode_vlan(fgen_t *fg, frame_t *f, int lidx, bool is_dot1ad)
{
    fopt_t *opt = FGEN_LOPT(fg, lidx);
    struct fgen_vlan_hdr *vlan;
    uint16_t vid = 1, prio = 7, cfi = 0;
    const char *kvps[] = {"vlan", "prio", "cfi"};
    char *val;
    int num;

    if (fg->flags & FGEN_VERBOSE)
        FGEN_INFO(
            "[magenta]params[]:'[orange]%s[]' [magenta]is a [orange]%s [magenta]type packet[]\n",
            opt->param_str, (is_dot1ad) ? "Dot1AD" : "Dot1Q");

    num = _encode_opts(opt->param_str, fg->params, fgen_countof(fg->params));
    if (num < 0)
        FGEN_ERR_RET("Parameters '%s' invalid\n", opt->param_str);

    for (int i = 0; i < num; i++) {
        switch (parser_kvp(fg->params[i], kvps, fgen_countof(kvps), &val)) {
        case 0:
            vid = STRTOL(val) & 0xFFF;
            break;
        case 1:
            prio = ((STRTOL(val) & 0x7) << 13);
            break;
        case 2:
            cfi = ((STRTOL(val) & 0x1) << 12);
            break;
        default:
            FGEN_ERR_RET("Dot1Q: Invalid key '%s'\n", val);
        }
    }

    /* Grab the current offset for the vlan pointer */
    vlan = fgen_mtod_offset(f, struct fgen_vlan_hdr *, fgen_data_len(f));

    /* Update the data len and the pkt_len value */
    opt->length = sizeof(struct fgen_vlan_hdr);
    fgen_data_len(f) += opt->length;

    vlan->vlan_tci  = htons(vid | prio | cfi);
    vlan->eth_proto = 0;

    switch (next_layer(fg, f, ++lidx)) {
    case FGEN_DOT1Q_TYPE:
        if (is_dot1ad)
            vlan->eth_proto = htons(FGEN_ETHER_TYPE_VLAN);
        else
            FGEN_ERR_RET("Invalid next layer for Dot1AD\n");
        break;
    case FGEN_IPV4_TYPE:
        vlan->eth_proto = htons(FGEN_ETHER_TYPE_IPV4);
        break;
    case FGEN_DOT1AD_TYPE:
        vlan->eth_proto = htons(FGEN_ETHER_TYPE_QINQ);
        break;
    case FGEN_ERROR_TYPE:
        FGEN_ERR_RET("Next layer return error\n");
    default:
        break;
    }

    if (fg->flags & FGEN_VERBOSE)
        FGEN_INFO("[magenta]Return '[orange]%s[]'\n", parser_type(opt->typ));

    return (is_dot1ad) ? FGEN_DOT1AD_TYPE : FGEN_DOT1Q_TYPE;
}

static int
_encode_dot1q(fgen_t *fg, frame_t *f, int lidx)
{
    return _encode_vlan(fg, f, lidx, false);
}

static int
_encode_dot1ad(fgen_t *fg, frame_t *f, int lidx)
{
    return _encode_vlan(fg, f, lidx, true);
}

static int
_encode_ipv4(fgen_t *fg, frame_t *f, int lidx)
{
    fopt_t *opt = FGEN_LOPT(fg, lidx);
    struct fgen_ipv4_hdr *hdr;
    struct fgen_udp_hdr *udp;
    struct fgen_tcp_hdr *tcp;
    const char *kvps[] = {"dst", "src"};
    char *val;
    uint16_t offset, total_length;
    int num;

    offset = fgen_data_len(f);
    hdr    = fgen_mtod_offset(f, struct fgen_ipv4_hdr *, offset);
    memset(hdr, 0, sizeof(*hdr));

    hdr->version_ihl  = (IPVERSION << 4) | (sizeof(struct fgen_ipv4_hdr) / 4);
    hdr->packet_id    = htons(1);
    hdr->time_to_live = 64;
    inet_pton(AF_INET, "192.10.0.2", &hdr->dst_addr);
    inet_pton(AF_INET, "192.10.0.1", &hdr->src_addr);

    if (fg->flags & FGEN_VERBOSE)
        FGEN_INFO("[magenta]params[]:'[orange]%s[]'\n", opt->param_str);

    num = _encode_opts(opt->param_str, fg->params, fgen_countof(fg->params));
    if (num < 0)
        FGEN_ERR_RET("Parameters '%s' invalid\n", opt->param_str);

    for (int i = 0; i < num; i++) {
        switch (parser_kvp(fg->params[i], kvps, fgen_countof(kvps), &val)) {
        case 0:
            inet_pton(AF_INET, val, &hdr->dst_addr);
            break;
        case 1:
            inet_pton(AF_INET, val, &hdr->src_addr);
            break;
        default:
            FGEN_ERR_RET("IPv4: Invalid key '%s'\n", val);
        }
    }

    fgen_data_len(f) += sizeof(struct fgen_ipv4_hdr);

    hdr->next_proto_id = 0;
    int nxt            = next_layer(fg, f, ++lidx);

    /* Will calculate the checksum when we return from the reset of the layers */
    total_length      = fgen_data_len(f) - offset;
    opt->length       = total_length;
    hdr->total_length = htons(total_length);

    switch (nxt) {
    case FGEN_UDP_TYPE:
        hdr->next_proto_id = IPPROTO_UDP;

        udp              = (struct fgen_udp_hdr *)((char *)hdr + (hdr->version_ihl & 0xf) * 4);
        udp->dgram_cksum = 0;
        udp->dgram_cksum = fgen_ipv4_udptcp_cksum(hdr, udp);
        break;
    case FGEN_TCP_TYPE:
        hdr->next_proto_id = IPPROTO_TCP;

        tcp        = (struct fgen_tcp_hdr *)((char *)hdr + (hdr->version_ihl & 0xf) * 4);
        tcp->cksum = fgen_ipv4_udptcp_cksum(hdr, tcp);
        break;
    case FGEN_ERROR_TYPE:
        FGEN_ERR_RET("Next layer return error\n");
    default:
        break;
    }
    hdr->hdr_checksum = fgen_ipv4_cksum(hdr);

    if (fg->flags & FGEN_VERBOSE)
        FGEN_INFO("[magenta]Return '[orange]%s[]'\n", parser_type(opt->typ));

    return FGEN_IPV4_TYPE;
}

static int
_encode_ipv6(fgen_t *fg, frame_t *f, int lidx)
{
    fopt_t *opt = FGEN_LOPT(fg, lidx);

    if (fg->flags & FGEN_VERBOSE)
        FGEN_INFO("[magenta]params[]:'[orange]%s[]'\n", opt->param_str);

    opt->length = sizeof(struct fgen_ipv6_hdr);
    fgen_data_len(f) += opt->length;

    switch (next_layer(fg, f, ++lidx)) {
    case FGEN_ERROR_TYPE:
        FGEN_ERR_RET("Next layer return error\n");
    default:
        break;
    }

    if (fg->flags & FGEN_VERBOSE)
        FGEN_INFO("[magenta]Return '[orange]%s[]'\n", parser_type(opt->typ));

    return FGEN_IPV6_TYPE;
}

static int
_encode_udp(fgen_t *fg, frame_t *f, int lidx)
{
    fopt_t *opt = FGEN_LOPT(fg, lidx);
    struct fgen_udp_hdr *hdr;
    uint16_t sport, dport;
    const char *kvps[] = {"dport", "sport"};
    char *val;
    uint16_t offset;
    int num;

    offset = fgen_data_len(f);
    hdr    = fgen_mtod_offset(f, struct fgen_udp_hdr *, offset);
    memset(hdr, 0, sizeof(*hdr));

    sport = 1234;
    dport = 5678;

    if (fg->flags & FGEN_VERBOSE)
        FGEN_INFO("[magenta]params[]:'[orange]%s[]'\n", opt->param_str);

    num = _encode_opts(opt->param_str, fg->params, fgen_countof(fg->params));
    if (num < 0)
        FGEN_ERR_RET("Parameters '%s' invalid\n", opt->param_str);

    for (int i = 0; i < num; i++) {
        switch (parser_kvp(fg->params[i], kvps, fgen_countof(kvps), &val)) {
        case 0:
            dport = STRTOL(val);
            break;
        case 1:
            sport = STRTOL(val);
            break;
        default:
            FGEN_ERR_RET("UDP: Invalid key '%s'\n", val);
        }
    }

    fgen_data_len(f) += sizeof(struct fgen_udp_hdr);

    switch (next_layer(fg, f, ++lidx)) {
    case FGEN_ECHO_TYPE:
        sport = dport = 7;
        break;
    case FGEN_VXLAN_TYPE:
        sport = dport = FGEN_VXLAN_DEFAULT_PORT;
        break;
    case FGEN_ERROR_TYPE:
        FGEN_ERR_RET("Next layer return error\n");
    default:
        break;
    }

    offset = fgen_data_len(f) - offset;
    if (fg->flags & FGEN_VERBOSE)
        FGEN_INFO("[magenta]UDP Length[] [orange]%u[] Bytes\n", offset);

    hdr->dst_port  = htons(dport);
    hdr->src_port  = htons(sport);
    hdr->dgram_len = htons(offset);
    opt->length    = offset;

    if (fg->flags & FGEN_VERBOSE)
        FGEN_INFO("[magenta]Return '[orange]%s[]'\n", parser_type(opt->typ));

    return FGEN_UDP_TYPE;
}

static int
_encode_tcp(fgen_t *fg, frame_t *f, int lidx)
{
    fopt_t *opt = FGEN_LOPT(fg, lidx);
    struct fgen_tcp_hdr *hdr;
    uint16_t sport, dport;
    const char *kvps[] = {"dport", "sport"};
    char *val;
    int num;

    hdr = fgen_mtod_offset(f, struct fgen_tcp_hdr *, fgen_data_len(f));
    memset(hdr, 0, sizeof(*hdr));

    sport = 0x1234;
    dport = 0x1111;

    if (fg->flags & FGEN_VERBOSE)
        FGEN_INFO("[magenta]params[]:'[orange]%s[]'\n", opt->param_str);

    num = _encode_opts(opt->param_str, fg->params, fgen_countof(fg->params));
    if (num < 0)
        FGEN_ERR_RET("Parameters '%s' invalid\n", opt->param_str);

    for (int i = 0; i < num; i++) {
        switch (parser_kvp(fg->params[i], kvps, fgen_countof(kvps), &val)) {
        case 0:
            dport = STRTOL(val);
            break;
        case 1:
            sport = STRTOL(val);
            break;
        default:
            FGEN_ERR_RET("UDP: Invalid key '%s'\n", val);
        }
    }

    fgen_data_len(f) += sizeof(struct fgen_tcp_hdr);

    switch (next_layer(fg, f, ++lidx)) {
    case FGEN_ECHO_TYPE:
        break;
    case FGEN_VXLAN_TYPE:
        break;
    case FGEN_ERROR_TYPE:
        FGEN_ERR_RET("Next layer return error\n");
    default:
        break;
    }

    opt->length    = 5 * 4;
    hdr->data_off  = (5 << 4); /* Min TCP length is 5 */
    hdr->rx_win    = htons(8192);
    hdr->tcp_flags = TCP_SYN_FLAG;
    hdr->dst_port  = htons(dport);
    hdr->src_port  = htons(sport);

    if (fg->flags & FGEN_VERBOSE)
        FGEN_INFO("[magenta]Return '[orange]%s[]'\n", parser_type(opt->typ));

    return FGEN_TCP_TYPE;
}

static int
_encode_vxlan(fgen_t *fg, frame_t *f, int lidx)
{
    fopt_t *opt = FGEN_LOPT(fg, lidx);
    struct fgen_vxlan_hdr *hdr;

    hdr = fgen_mtod_offset(f, struct fgen_vxlan_hdr *, fgen_data_len(f));
    memset(hdr, 0, sizeof(*hdr));

    hdr->vx_vni = htonl(1000 & ((1 << 24) - 1));

    if (fg->flags & FGEN_VERBOSE)
        FGEN_INFO("[magenta]params[]:'[orange]%s[]'\n", opt->param_str);

    fgen_data_len(f) += sizeof(struct fgen_vxlan_hdr);

    uint8_t next_protocol          = 0;
    const uint32_t instance_Ibit   = (1 << 27);
    const uint32_t next_proto_Pbit = (0 << 26);
    uint32_t flags                 = instance_Ibit;

    switch (next_layer(fg, f, ++lidx)) {
    case FGEN_ETHER_TYPE:
        next_protocol = FGEN_VXLAN_GPE_TYPE_ETH;
        flags |= next_proto_Pbit;
        break;
    case FGEN_ERROR_TYPE:
        FGEN_ERR_RET("Next layer return error\n");
    default:
        break;
    }
    hdr->vx_flags = htonl(flags | next_protocol);

    if (fg->flags & FGEN_VERBOSE)
        FGEN_INFO("[magenta]Return '[orange]%s[]'\n", parser_type(opt->typ));

    return FGEN_VXLAN_TYPE;
}

static int
_encode_echo(fgen_t *fg, frame_t *f, int lidx)
{
    fopt_t *opt = FGEN_LOPT(fg, lidx);

    if (fg->flags & FGEN_VERBOSE)
        FGEN_INFO("[magenta]params[]:'[orange]%s[]'\n", opt->param_str);

    fgen_data_len(f) += sizeof(struct fgen_tcp_hdr);

    switch (next_layer(fg, f, ++lidx)) {
    case FGEN_ERROR_TYPE:
        FGEN_ERR_RET("Next layer return error\n");
    default:
        break;
    }

    if (fg->flags & FGEN_VERBOSE)
        FGEN_INFO("[magenta]Return '[orange]%s[]'\n", parser_type(opt->typ));

    return FGEN_ECHO_TYPE;
}

static int
_encode_tsc(fgen_t *fg, frame_t *f, int lidx)
{
    fopt_t *opt = FGEN_LOPT(fg, lidx);
    tsc_t *tsc;

    tsc = fgen_mtod_offset(f, tsc_t *, fgen_data_len(f));
    memset(tsc, 0, sizeof(tsc_t));

    f->tsc_off = fgen_data_len(f);

    tsc->tstmp   = TIMESTAMP_ID;
    tsc->tsc_val = 0;

    if (fg->flags & FGEN_VERBOSE)
        FGEN_INFO("[magenta]params[]:'[orange]%s[]'\n", opt->param_str);

    fgen_data_len(f) += sizeof(tsc_t);

    switch (next_layer(fg, f, ++lidx)) {
    case FGEN_ERROR_TYPE:
        FGEN_ERR_RET("Next layer return error\n");
    default:
        break;
    }

    if (fg->flags & FGEN_VERBOSE)
        FGEN_INFO("[magenta]Return '[orange]%s[]'\n", parser_type(opt->typ));

    return FGEN_TSC_TYPE;
}

static int
_encode_raw(fgen_t *fg, frame_t *f, int lidx)
{
    fopt_t *opt = FGEN_LOPT(fg, lidx);

    if (fg->flags & FGEN_VERBOSE)
        FGEN_INFO("[magenta]params[]:'[orange]%s[]'\n", opt->param_str);

    fgen_data_len(f) += sizeof(struct fgen_tcp_hdr);

    switch (next_layer(fg, f, ++lidx)) {
    case FGEN_ERROR_TYPE:
        FGEN_ERR_RET("Next layer return error\n");
    default:
        break;
    }

    if (fg->flags & FGEN_VERBOSE)
        FGEN_INFO("[magenta]Return '[orange]%s[]'\n", parser_type(opt->typ));

    return FGEN_RAW_TYPE;
}

static int
_encode_payload(fgen_t *fg, frame_t *f, int lidx)
{
    fopt_t *opt = FGEN_LOPT(fg, lidx);
    int plen, num, fsize;
    int only = 0, append = 0, pktlen = 0, fill = FGEN_FILLER_PATTERN;
    const char *kvps[] = {"size", "append", "fill"};
    char *val;

    if (fg->flags & FGEN_VERBOSE)
        FGEN_INFO("[magenta]params[]:'[orange]%s[]'\n", opt->param_str);

    num = _encode_opts(opt->param_str, fg->params, fgen_countof(fg->params));
    if (num < 0)
        FGEN_ERR_RET("Parameters '%s' invalid\n", opt->param_str);

    plen = pktlen = fgen_data_len(f);

    for (int i = 0; i < num; i++) {
        switch (parser_kvp(fg->params[i], kvps, fgen_countof(kvps), &val)) {
        case 0: /* Force the frame size to a given length */
            if (only++)
                FGEN_ERR_RET("Can't have append and size at the same time\n");
            fsize = STRTOL(val); /* The size includes the CRC length */

            if (fsize < ETHER_MIN_LEN)
                fsize = ETHER_MIN_LEN;
            else if (fsize > ETHER_MAX_LEN)
                fsize = ETHER_MAX_LEN;

            fsize -= ETHER_CRC_LEN; /* remove the CRC length */

            /* fsize is the absolute packet length even if current packet is greater */
            if (fsize != pktlen)
                pktlen = fsize;
            break;
        case 1: /* append a given number of bytes to frame */
            if (only++)
                FGEN_ERR_RET("Can't have append and size at the same time\n");
            append = STRTOL(val);
            pktlen += append;
            break;
        case 2: /* Fill the payload with a given byte pattern */
            fill = STRTOL(val);
            break;
        default:
            FGEN_ERR_RET("Payload: Invalid key '%s'\n", val);
        }
    }

    fgen_data_len(f) = pktlen;

    if (pktlen > plen)
        memset(fgen_mtod_offset(f, char *, plen), fill, pktlen - plen);

    switch (next_layer(fg, f, ++lidx)) {
    case FGEN_ERROR_TYPE:
        FGEN_ERR_RET("Next layer return error\n");
    default:
        break;
    }

    /* If the frame size was adjusted then make sure we fill the payload */
    if ((fgen_data_len(f) != pktlen) && (fill != 0))
        memset(fgen_mtod_offset(f, char *, pktlen), fill, fgen_data_len(f) - pktlen);

    if (fg->flags & FGEN_VERBOSE)
        FGEN_INFO("[magenta]Return '[orange]%s[]'\n", parser_type(opt->typ));

    return FGEN_PAYLOAD_TYPE;
}

static int
_encode_done(fgen_t *fg, frame_t *f, int lidx __fgen_unused)
{
    fopt_t *opt = FGEN_LOPT(fg, lidx);

    if (fg->flags & FGEN_VERBOSE)
        FGEN_INFO("[magenta]Finish up packet parsing. len %d[]\n", fgen_data_len(f));

    if (fgen_data_len(f) < ETH_ZLEN) {
        if (fg->flags & FGEN_VERBOSE)
            FGEN_WARN("[magenta]Packet is to short [orange]%d[], [magenta]adjusting to [orange]%d "
                     "[magenta]bytes[]\n",
                     fgen_data_len(f), ETH_ZLEN);
        fgen_data_len(f) = ETH_ZLEN;
    }

    if (fgen_data_len(f) > ETH_FRAME_LEN) {
        if (fg->flags & FGEN_VERBOSE)
            FGEN_WARN("[magenta]Packet is to long [orange]%d[], [magenta]adjusting to [orange]%d "
                     "[magenta]bytes[]\n",
                     fgen_data_len(f), ETH_FRAME_LEN);
        fgen_data_len(f) = ETH_FRAME_LEN;
    }

    if (fg->flags & FGEN_VERBOSE)
        FGEN_INFO("[magenta]Return '[orange]%s[]' [magenta]pktlen [orange]%d[]\n",
                 parser_type(opt->typ), fgen_data_len(f));

    return FGEN_DONE_TYPE;
}

// clang-format off
ftable_t fgen_tbl[] = {
    {.str = "",         .fn = _encode_done,      .typ = FGEN_DONE_TYPE},
    {.str = FGEN_ETHER_STR"(",   .fn = _encode_ether,     .typ = FGEN_ETHER_TYPE},
    {.str = FGEN_DOT1Q_STR"(",   .fn = _encode_dot1q,     .typ = FGEN_DOT1Q_TYPE},
    {.str = FGEN_DOT1AD_STR"(",  .fn = _encode_dot1ad,    .typ = FGEN_DOT1AD_TYPE},

    {.str = FGEN_IPv4_STR"(",    .fn = _encode_ipv4,      .typ = FGEN_IPV4_TYPE},
    {.str = FGEN_IPv6_STR"(",    .fn = _encode_ipv6,      .typ = FGEN_IPV6_TYPE},

    {.str = FGEN_UDP_STR"(",     .fn = _encode_udp,       .typ = FGEN_UDP_TYPE},
    {.str = FGEN_TCP_STR"(",     .fn = _encode_tcp,       .typ = FGEN_TCP_TYPE},

    {.str = FGEN_VxLAN_STR"(",   .fn = _encode_vxlan,     .typ = FGEN_VXLAN_TYPE},
    {.str = FGEN_ECHO_STR"(",    .fn = _encode_echo,      .typ = FGEN_ECHO_TYPE},
    {.str = FGEN_TSC_STR"(",     .fn = _encode_tsc,       .typ = FGEN_TSC_TYPE},
    {.str = FGEN_RAW_STR"(",     .fn = _encode_raw,       .typ = FGEN_RAW_TYPE},

    {.str = FGEN_PAYLOAD_STR"(", .fn = _encode_payload,   .typ = FGEN_PAYLOAD_TYPE},
    {.str = NULL, .fn = NULL}
};
// clang-format on

int
_encode_frame(fgen_t *fg, frame_t *f)
{
    fopt_t *opt = NULL;
    char *text;

    if (!fg || !f || !f->frame_text)
        FGEN_ERR_RET("fgen_t or frame_t or frame_t.frame_text is NULL\n");

    memset(fg->params, 0, sizeof(fg->params));
    memset(fg->opts, 0, sizeof(fg->opts));
    memset(&f->l2, 0, sizeof(f->l2));
    memset(&f->l3, 0, sizeof(f->l3));
    memset(&f->l4, 0, sizeof(f->l4));
    memset(f->data, 0, f->bufsz);

    text = strdup(f->frame_text);
    if (!text)
        FGEN_ERR_RET("Unable to allocate memory for frame text\n");

    /* Leave the last entry for the done layer function */
    fg->num_layers = fgen_strtok(text, "/", fg->layers, FGEN_MAX_LAYERS - 1);
    if (fg->num_layers <= 0)
        FGEN_ERR_RET("Number of layers is %d\n", fg->num_layers);

    /* Process each layer of the frame, identifying each layer type */
    for (int i = 0; i < fg->num_layers; i++) {
        char *layer = fg->layers[i];

        /* First pass over the tokens and setup for parsing of each layer */
        for (int j = 1; fgen_tbl[j].str; j++) {
            int len = strlen(fgen_tbl[j].str);

            if (len > 0 && strncasecmp(layer, fgen_tbl[j].str, len) == 0) {
                opt = &fg->opts[i];

                if (fg->flags & FGEN_VERBOSE)
                    FGEN_INFO("[magenta]Add layer[] [orange]%d[] - '[orange]%s[]'\n", i, layer);

                opt->typ       = fgen_tbl[j].typ;
                opt->tbl       = &fgen_tbl[j];
                opt->param_str = &layer[len - 1];        // backup and point to '('
                break;
            }
        }
    }

    /* Setup the done parsing as the last section */
    opt            = &fg->opts[fg->num_layers++];
    opt->typ       = FGEN_DONE_TYPE;
    opt->tbl       = &fgen_tbl[0];
    opt->param_str = NULL;

    if (fg->flags & FGEN_VERBOSE)
        FGEN_INFO("[magenta]Add layer[] [orange]%d[] - [orange]Done[]\n", fg->num_layers - 1);

    /* Parse the layers */
    if (next_layer(fg, f, 0) < 0)
        goto leave;

    if (fg->flags & FGEN_DUMP_DATA)
        fgen_print_frame(NULL, f);

leave:
    free(text);
    return 0;
}
