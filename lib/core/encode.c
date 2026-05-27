/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023-2026 Intel Corporation
 */

/**
 * @file encode.c
 * @brief Frame encoder: converts a fgen layer-description string into binary packet data.
 *
 * The public entry point is _encode_frame(), called internally by fgen_add_frame().
 * It tokenizes the frame string, builds an @c fopt_t entry for each layer, then
 * dispatches through @c fgen_tbl[] in a recursive-descent pattern — each layer
 * encoder writes its header fields into the shared salloc slab, advances the
 * running data-length counter, and calls next_layer() for the inner layer before
 * computing any fields that depend on inner length or checksum values.
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

extern int _encode_frame(frame_t *f);

/**
 * @brief Bounds-checked accessor for a layer option slot.
 *
 * Returns a pointer to @c (_p)->opts[(_i)] after verifying that @c (_i) is
 * within @c FGEN_MAX_LAYERS; triggers FGEN_ERR_RET and returns -1 on overflow.
 *
 * @param _p  Pointer to a @c fgen_t.
 * @param _i  Layer index.
 */
#define FGEN_LOPT(_p, _i)                                   \
    ({                                                      \
        if ((_i) >= FGEN_MAX_LAYERS)                        \
            FGEN_ERR_RET("Invalid layer index %d\n", (_i)); \
        &(_p)->opts[(_i)];                                  \
    })

/**
 * @brief Parse a decimal/hex/octal number string, returning on errno.
 *
 * Wraps strtol() with base 0 (auto-detect) and returns immediately with -1
 * via FGEN_ERR_RET if strtol sets errno (e.g. ERANGE).
 *
 * @param _v  NUL-terminated number string.
 */
#define STRTOL(_v)                                               \
    ({                                                           \
        long _x;                                                 \
        errno = 0;                                               \
        _x    = strtol((_v), NULL, 0);                           \
        if (errno)                                               \
            FGEN_ERR_RET("Unable to parse number '%s'\n", (_v)); \
        _x;                                                      \
    })

/**
 * @brief Map an opt_type_t value to its display name string.
 *
 * @param typ  Layer type identifier.
 * @return     Constant string such as "Ether", "IPv4", etc., or "Unknown type".
 */
static inline const char *
parser_type(opt_type_t typ)
{
    static const char *ptypes[] = FGEN_TYPE_STRINGS;

    if (typ >= 0 && typ <= FGEN_TYPE_COUNT)
        return ptypes[typ];
    return "Unknown type";
}

/**
 * @brief Dispatch to the encoder function for layer @p idx.
 *
 * Looks up @c fg->opts[idx].tbl->fn and calls it.  This is the core of the
 * recursive-descent encode loop: each layer encoder calls next_layer() for the
 * layer that follows it before writing any fields that depend on inner size.
 *
 * @param f    Frame being built.
 * @param idx  Index into @c fg->opts[].
 * @return     The opt_type_t returned by the called encoder, or -1 on error.
 */
static inline int
next_layer(frame_t *f, int idx)
{
    fgen_t *fg = f->fg;
    ftable_t *t;

    if (idx >= fg->nb_layers)
        FGEN_ERR_RET("Next layer %d >= %d\n", idx, fg->nb_layers);

    t = fg->opts[idx].tbl;

    return (t && t->fn) ? t->fn(f, idx) : -1;
}

/**
 * @brief Tokenize a layer parameter string into individual key=value tokens.
 *
 * Strips surrounding whitespace and parentheses from @p str, then splits on
 * ',' and ' ' delimiters.  Modifies @p str in-place.
 *
 * @param str      Layer parameter string, e.g. "(dst=aa:bb:cc:dd:ee:ff, src=…)".
 * @param toks     Output array of token pointers.
 * @param nb_toks  Capacity of @p toks.
 * @return         Number of tokens found, 0 if empty, or -1 on error.
 */
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

/**
 * @brief Split a single "key=value" token into exactly two parts.
 *
 * Strips surrounding whitespace and parentheses from @p str, then splits on
 * '=' and ' '.  Returns -1 if the result is not exactly two tokens.
 *
 * @param str     Single key=value token string.
 * @param vars    Output array; vars[0]=key, vars[1]=value on success.
 * @param nb_vars Capacity of @p vars (must be >= 2).
 * @return        2 on success, 0 if empty, -1 if not a valid key=value pair.
 */
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

/**
 * @brief Match a "key=value" parameter against a known-key table.
 *
 * Splits @p param into key and value, then searches @p keys[] for a
 * case-insensitive match.  On a match, stores the value string in @p *val
 * and returns the matched index.  On no match, stores the key string in
 * @p *val and returns -1 so the caller can report the unknown key.
 *
 * @param param  Single "key=value" token (modified in-place by _encode_vars).
 * @param keys   Array of known key strings to match against.
 * @param len    Number of entries in @p keys.
 * @param val    Output: pointer to the value string (match) or key string (no match).
 * @return       Index of matched key in @p keys, or -1 if not found.
 */
static inline int
parser_kvp(char *param, const char **keys, int len, char **val)
{
    char *kvp[FGEN_MAX_KVP_TOKENS] = {0};

    if (_encode_vars(param, kvp, fgen_countof(kvp)) > 0) {
        for (int i = 0; i < len; i++) {
            if (strncasecmp(kvp[0], keys[i], strlen(keys[i])) == 0) {
                *val = kvp[1];
                return i;
            }
        }
    }
    *val = (kvp[0]) ? kvp[0] : (char *)(uintptr_t) "Unknown_key";
    return -1;
}

/**
 * @brief Encode an Ethernet II header at the current frame offset.
 *
 * Defaults: dst=FF:FF:FF:FF:FF:FF, src=00:00:00:00:00:00.
 * Accepted parameters: @c dst, @c src (colon- or colon-separated MAC address).
 * Sets @c ether_type based on the type returned by the next layer.
 *
 * @param f    Frame being built.
 * @param lidx Index of this layer in @c fg->opts[].
 * @return     FGEN_ETHER_TYPE on success, -1 on error.
 */
static int
_encode_ether(frame_t *f, int lidx)
{
    fgen_t *fg = f->fg;
    struct ether_header *eth;
    fopt_t *opt = FGEN_LOPT(fg, lidx);

    int num;
    const char *keys[] = {"dst", "src"};
    char *val;

    eth = fbuf_mtod(f, struct ether_header *);
    ether_unformat_addr("FFFF:FFFF:FFFF", (struct ether_addr *)eth->ether_dhost);
    ether_unformat_addr("0000:0000:0000", (struct ether_addr *)eth->ether_shost);

    opt->length = sizeof(struct ether_header);
    fbuf_data_len(f) += opt->length;

    if (fg->flags & FGEN_VERBOSE)
        FGEN_INFO("[magenta]params[]:'[orange]%s[]'\n", opt->param_str);

    num = _encode_opts(opt->param_str, fg->params, fgen_countof(fg->params));
    if (num < 0)
        FGEN_ERR_RET("Parameters '%s' invalid\n", opt->param_str);

    for (int i = 0; i < num; i++) {
        switch (parser_kvp(fg->params[i], keys, fgen_countof(keys), &val)) {
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

    switch (next_layer(f, ++lidx)) {
    case FGEN_DOT1Q_TYPE:
        eth->ether_type = htons(FGEN_ETHER_TYPE_VLAN);
        break;
    case FGEN_DOT1AD_TYPE:
        eth->ether_type = htons(FGEN_ETHER_TYPE_QINQ);
        break;
    case FGEN_IPV4_TYPE:
        eth->ether_type = htons(FGEN_ETHER_TYPE_IPV4);
        break;
    case FGEN_IPV6_TYPE:
        eth->ether_type = htons(FGEN_ETHER_TYPE_IPV6);
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

/**
 * @brief Encode a VLAN tag (Dot1Q or Dot1AD) at the current frame offset.
 *
 * Shared implementation for both Dot1Q and Dot1AD.  Defaults: vlan=1, prio=7, cfi=0.
 * Accepted parameters: @c vlan (12-bit VID), @c prio (3-bit priority), @c cfi (1-bit).
 * Sets @c eth_proto based on the next-layer type.
 *
 * @param f          Frame being built.
 * @param lidx       Index of this layer in @c fg->opts[].
 * @param is_dot1ad  True for Dot1AD (QinQ outer tag), false for Dot1Q.
 * @return           FGEN_DOT1AD_TYPE or FGEN_DOT1Q_TYPE on success, -1 on error.
 */
static int
_encode_vlan(frame_t *f, int lidx, bool is_dot1ad)
{
    fgen_t *fg = f->fg;
    fopt_t *opt = FGEN_LOPT(fg, lidx);
    struct fgen_vlan_hdr *vlan;
    uint16_t vid = 1, prio = 7, cfi = 0;
    const char *keys[] = {"vlan", "prio", "cfi"};
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
        switch (parser_kvp(fg->params[i], keys, fgen_countof(keys), &val)) {
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
    vlan = fbuf_mtod_offset(f, struct fgen_vlan_hdr *, fbuf_data_len(f));

    /* Update the data len and the pkt_len value */
    opt->length = sizeof(struct fgen_vlan_hdr);
    fbuf_data_len(f) += opt->length;

    vlan->vlan_tci  = htons(vid | prio | cfi);
    vlan->eth_proto = 0;

    switch (next_layer(f, ++lidx)) {
    case FGEN_DOT1Q_TYPE:
        if (is_dot1ad)
            vlan->eth_proto = htons(FGEN_ETHER_TYPE_VLAN);
        else
            FGEN_ERR_RET("Invalid next layer for Dot1AD\n");
        break;
    case FGEN_IPV4_TYPE:
        vlan->eth_proto = htons(FGEN_ETHER_TYPE_IPV4);
        break;
    case FGEN_IPV6_TYPE:
        vlan->eth_proto = htons(FGEN_ETHER_TYPE_IPV6);
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

/** @brief Encode a Dot1Q VLAN tag. Delegates to _encode_vlan(). */
static int
_encode_dot1q(frame_t *f, int lidx)
{
    return _encode_vlan(f, lidx, false);
}

/** @brief Encode a Dot1AD (QinQ outer) VLAN tag. Delegates to _encode_vlan(). */
static int
_encode_dot1ad(frame_t *f, int lidx)
{
    return _encode_vlan(f, lidx, true);
}

/**
 * @brief Encode an IPv4 header at the current frame offset.
 *
 * Defaults: version=4, IHL=5, TTL=64, id=1, dst=192.10.0.2, src=192.10.0.1.
 * Accepted parameters: @c dst, @c src (dotted-quad).
 * Calls next_layer() before filling @c total_length so the full inner size is
 * known.  Computes the IPv4 header checksum and, for UDP/TCP, the L4
 * pseudo-header checksum on the way back up.
 *
 * @param f    Frame being built.
 * @param lidx Index of this layer in @c fg->opts[].
 * @return     FGEN_IPV4_TYPE on success, -1 on error.
 */
static int
_encode_ipv4(frame_t *f, int lidx)
{
    fgen_t *fg = f->fg;
    fopt_t *opt = FGEN_LOPT(fg, lidx);
    struct fgen_ipv4_hdr *hdr;
    struct fgen_udp_hdr *udp;
    struct fgen_tcp_hdr *tcp;
    const char *keys[] = {"dst", "src"};
    char *val;
    uint16_t offset, total_length;
    int num;

    offset = fbuf_data_len(f);
    hdr    = fbuf_mtod_offset(f, struct fgen_ipv4_hdr *, offset);
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
        switch (parser_kvp(fg->params[i], keys, fgen_countof(keys), &val)) {
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

    fbuf_data_len(f) += sizeof(struct fgen_ipv4_hdr);

    hdr->next_proto_id = 0;
    int nxt            = next_layer(f, ++lidx);

    /* Will calculate the checksum when we return from the reset of the layers */
    total_length      = fbuf_data_len(f) - offset;
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

/**
 * @brief Encode an IPv6 header at the current frame offset.
 *
 * Defaults: version=6, hop_limits=64, dst=::2, src=::1.
 * Accepted parameters: @c dst, @c src (IPv6 address string).
 * Calls next_layer() before filling @c payload_len; sets @c proto to
 * IPPROTO_UDP or IPPROTO_TCP based on the next-layer type.
 *
 * @param f    Frame being built.
 * @param lidx Index of this layer in @c fg->opts[].
 * @return     FGEN_IPV6_TYPE on success, -1 on error.
 */
static int
_encode_ipv6(frame_t *f, int lidx)
{
    fgen_t *fg = f->fg;
    fopt_t *opt = FGEN_LOPT(fg, lidx);
    struct fgen_ipv6_hdr *hdr;
    const char *keys[] = {"dst", "src"};
    char *val;
    uint16_t offset;
    int num, nxt;

    offset = fbuf_data_len(f);
    hdr    = fbuf_mtod_offset(f, struct fgen_ipv6_hdr *, offset);
    memset(hdr, 0, sizeof(*hdr));

    hdr->vtc_flow   = htonl(6u << 28); /* version = 6 */
    hdr->hop_limits = 64;
    inet_pton(AF_INET6, "::2", hdr->dst_addr);
    inet_pton(AF_INET6, "::1", hdr->src_addr);

    if (fg->flags & FGEN_VERBOSE)
        FGEN_INFO("[magenta]params[]:'[orange]%s[]'\n", opt->param_str);

    num = _encode_opts(opt->param_str, fg->params, fgen_countof(fg->params));
    if (num < 0)
        FGEN_ERR_RET("Parameters '%s' invalid\n", opt->param_str);

    for (int i = 0; i < num; i++) {
        switch (parser_kvp(fg->params[i], keys, fgen_countof(keys), &val)) {
        case 0:
            inet_pton(AF_INET6, val, hdr->dst_addr);
            break;
        case 1:
            inet_pton(AF_INET6, val, hdr->src_addr);
            break;
        default:
            FGEN_ERR_RET("IPv6: Invalid key '%s'\n", val);
        }
    }

    fbuf_data_len(f) += sizeof(struct fgen_ipv6_hdr);
    nxt = next_layer(f, ++lidx);

    hdr->payload_len = htons(fbuf_data_len(f) - offset - sizeof(struct fgen_ipv6_hdr));
    opt->length      = fbuf_data_len(f) - offset;

    switch (nxt) {
    case FGEN_UDP_TYPE:
        hdr->proto = IPPROTO_UDP;
        break;
    case FGEN_TCP_TYPE:
        hdr->proto = IPPROTO_TCP;
        break;
    case FGEN_ERROR_TYPE:
        FGEN_ERR_RET("Next layer return error\n");
    default:
        break;
    }

    if (fg->flags & FGEN_VERBOSE)
        FGEN_INFO("[magenta]Return '[orange]%s[]'\n", parser_type(opt->typ));

    return FGEN_IPV6_TYPE;
}

/**
 * @brief Encode a UDP header at the current frame offset.
 *
 * Defaults: sport=1234, dport=5678.  Accepted parameters: @c dport, @c sport.
 * If the next layer is Echo, overrides both ports to 7 (UDP echo service).
 * If the next layer is VxLAN, overrides both ports to FGEN_VXLAN_DEFAULT_PORT.
 * Fills @c dgram_len after the inner layers have been encoded.
 *
 * @param f    Frame being built.
 * @param lidx Index of this layer in @c fg->opts[].
 * @return     FGEN_UDP_TYPE on success, -1 on error.
 */
static int
_encode_udp(frame_t *f, int lidx)
{
    fgen_t *fg = f->fg;
    fopt_t *opt = FGEN_LOPT(fg, lidx);
    struct fgen_udp_hdr *hdr;
    uint16_t sport, dport;
    const char *keys[] = {"dport", "sport"};
    char *val;
    uint16_t offset;
    int num;

    offset = fbuf_data_len(f);
    hdr    = fbuf_mtod_offset(f, struct fgen_udp_hdr *, offset);
    memset(hdr, 0, sizeof(*hdr));

    sport = 1234;
    dport = 5678;

    if (fg->flags & FGEN_VERBOSE)
        FGEN_INFO("[magenta]params[]:'[orange]%s[]'\n", opt->param_str);

    num = _encode_opts(opt->param_str, fg->params, fgen_countof(fg->params));
    if (num < 0)
        FGEN_ERR_RET("Parameters '%s' invalid\n", opt->param_str);

    for (int i = 0; i < num; i++) {
        switch (parser_kvp(fg->params[i], keys, fgen_countof(keys), &val)) {
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

    fbuf_data_len(f) += sizeof(struct fgen_udp_hdr);

    switch (next_layer(f, ++lidx)) {
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

    offset = fbuf_data_len(f) - offset;
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

/**
 * @brief Encode a TCP header at the current frame offset.
 *
 * Defaults: sport=0x1234, dport=0x1111, data_off=5 (20-byte header),
 * rx_win=8192, flags=SYN.  Accepted parameters: @c dport, @c sport.
 * The TCP checksum is computed by the parent IPv4/IPv6 encoder after this
 * function returns.
 *
 * @param f    Frame being built.
 * @param lidx Index of this layer in @c fg->opts[].
 * @return     FGEN_TCP_TYPE on success, -1 on error.
 */
static int
_encode_tcp(frame_t *f, int lidx)
{
    fgen_t *fg = f->fg;
    fopt_t *opt = FGEN_LOPT(fg, lidx);
    struct fgen_tcp_hdr *hdr;
    uint16_t sport, dport;
    const char *keys[] = {"dport", "sport"};
    char *val;
    int num;

    hdr = fbuf_mtod_offset(f, struct fgen_tcp_hdr *, fbuf_data_len(f));
    memset(hdr, 0, sizeof(*hdr));

    sport = 0x1234;
    dport = 0x1111;

    if (fg->flags & FGEN_VERBOSE)
        FGEN_INFO("[magenta]params[]:'[orange]%s[]'\n", opt->param_str);

    num = _encode_opts(opt->param_str, fg->params, fgen_countof(fg->params));
    if (num < 0)
        FGEN_ERR_RET("Parameters '%s' invalid\n", opt->param_str);

    for (int i = 0; i < num; i++) {
        switch (parser_kvp(fg->params[i], keys, fgen_countof(keys), &val)) {
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

    fbuf_data_len(f) += sizeof(struct fgen_tcp_hdr);

    switch (next_layer(f, ++lidx)) {
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

/**
 * @brief Encode a VxLAN GPE header at the current frame offset.
 *
 * Default VNI is 1000.  Sets the I-bit (instance valid) unconditionally.
 * If the next layer is Ethernet, sets the P-bit and next-protocol field to
 * FGEN_VXLAN_GPE_TYPE_ETH.  The encapsulating UDP ports are set to
 * FGEN_VXLAN_DEFAULT_PORT by the parent _encode_udp() on the way back up.
 *
 * @param f    Frame being built.
 * @param lidx Index of this layer in @c fg->opts[].
 * @return     FGEN_VXLAN_TYPE on success, -1 on error.
 */
static int
_encode_vxlan(frame_t *f, int lidx)
{
    fgen_t *fg = f->fg;
    fopt_t *opt = FGEN_LOPT(fg, lidx);
    struct fgen_vxlan_hdr *hdr;

    hdr = fbuf_mtod_offset(f, struct fgen_vxlan_hdr *, fbuf_data_len(f));
    memset(hdr, 0, sizeof(*hdr));

    hdr->vx_vni = htonl(1000 & ((1 << 24) - 1));

    if (fg->flags & FGEN_VERBOSE)
        FGEN_INFO("[magenta]params[]:'[orange]%s[]'\n", opt->param_str);

    fbuf_data_len(f) += sizeof(struct fgen_vxlan_hdr);

    uint8_t next_protocol          = 0;
    const uint32_t instance_Ibit   = (1 << 27);
    const uint32_t next_proto_Pbit = (0 << 26);
    uint32_t flags                 = instance_Ibit;

    switch (next_layer(f, ++lidx)) {
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

/**
 * @brief Reserve space for an Echo application-layer payload.
 *
 * Advances the data length to reserve room for the echo data.  The parent
 * UDP encoder sets both src/dst ports to 7 (echo service) when this layer
 * is present.
 *
 * @param f    Frame being built.
 * @param lidx Index of this layer in @c fg->opts[].
 * @return     FGEN_ECHO_TYPE on success, -1 on error.
 */
static int
_encode_echo(frame_t *f, int lidx)
{
    fgen_t *fg = f->fg;
    fopt_t *opt = FGEN_LOPT(fg, lidx);

    if (fg->flags & FGEN_VERBOSE)
        FGEN_INFO("[magenta]params[]:'[orange]%s[]'\n", opt->param_str);

    fbuf_data_len(f) += sizeof(struct fgen_tcp_hdr);

    switch (next_layer(f, ++lidx)) {
    case FGEN_ERROR_TYPE:
        FGEN_ERR_RET("Next layer return error\n");
    default:
        break;
    }

    if (fg->flags & FGEN_VERBOSE)
        FGEN_INFO("[magenta]Return '[orange]%s[]'\n", parser_type(opt->typ));

    return FGEN_ECHO_TYPE;
}

/**
 * @brief Encode a hardware timestamp slot (TSC layer).
 *
 * Writes a @c tsc_t record with the @c TIMESTAMP_ID magic marker and a zeroed
 * @c tsc_val field.  Records the byte offset in @c frame_t.tsc_off so callers
 * can inject a hardware TSC value at transmit time without re-parsing.
 *
 * @param f    Frame being built.
 * @param lidx Index of this layer in @c fg->opts[].
 * @return     FGEN_TSC_TYPE on success, -1 on error.
 */
static int
_encode_tsc(frame_t *f, int lidx)
{
    fgen_t *fg = f->fg;
    fopt_t *opt = FGEN_LOPT(fg, lidx);
    tsc_t *tsc;

    tsc = fbuf_mtod_offset(f, tsc_t *, fbuf_data_len(f));
    memset(tsc, 0, sizeof(tsc_t));

    f->tsc_off = fbuf_data_len(f);

    tsc->tstmp   = TIMESTAMP_ID;
    tsc->tsc_val = 0;

    if (fg->flags & FGEN_VERBOSE)
        FGEN_INFO("[magenta]params[]:'[orange]%s[]'\n", opt->param_str);

    fbuf_data_len(f) += sizeof(tsc_t);

    switch (next_layer(f, ++lidx)) {
    case FGEN_ERROR_TYPE:
        FGEN_ERR_RET("Next layer return error\n");
    default:
        break;
    }

    if (fg->flags & FGEN_VERBOSE)
        FGEN_INFO("[magenta]Return '[orange]%s[]'\n", parser_type(opt->typ));

    return FGEN_TSC_TYPE;
}

/**
 * @brief Reserve space for a raw data region.
 *
 * Advances the data length to make room for raw bytes.  No header fields
 * are written; the caller is responsible for filling the reserved space.
 *
 * @param f    Frame being built.
 * @param lidx Index of this layer in @c fg->opts[].
 * @return     FGEN_RAW_TYPE on success, -1 on error.
 */
static int
_encode_raw(frame_t *f, int lidx)
{
    fgen_t *fg = f->fg;
    fopt_t *opt = FGEN_LOPT(fg, lidx);

    if (fg->flags & FGEN_VERBOSE)
        FGEN_INFO("[magenta]params[]:'[orange]%s[]'\n", opt->param_str);

    fbuf_data_len(f) += sizeof(struct fgen_tcp_hdr);

    switch (next_layer(f, ++lidx)) {
    case FGEN_ERROR_TYPE:
        FGEN_ERR_RET("Next layer return error\n");
    default:
        break;
    }

    if (fg->flags & FGEN_VERBOSE)
        FGEN_INFO("[magenta]Return '[orange]%s[]'\n", parser_type(opt->typ));

    return FGEN_RAW_TYPE;
}

/**
 * @brief Set the final frame length and fill unused payload bytes.
 *
 * Accepted parameters:
 *  - @c size=N   — set the absolute frame length to N bytes (includes 4-byte CRC;
 *                  clamped to [ETHER_MIN_LEN, ETHER_MAX_LEN]).
 *  - @c append=N — extend the current frame by N bytes.
 *  - @c fill=0xNN — byte pattern used to fill any added space (default: '%').
 *
 * @c size and @c append are mutually exclusive.
 *
 * @param f    Frame being built.
 * @param lidx Index of this layer in @c fg->opts[].
 * @return     FGEN_PAYLOAD_TYPE on success, -1 on error.
 */
static int
_encode_payload(frame_t *f, int lidx)
{
    fgen_t *fg = f->fg;
    fopt_t *opt = FGEN_LOPT(fg, lidx);
    int plen, num, fsize;
    int only = 0, append = 0, pktlen = 0, fill = FGEN_FILLER_PATTERN;
    const char *keys[] = {"size", "append", "fill"};
    char *val;

    if (fg->flags & FGEN_VERBOSE)
        FGEN_INFO("[magenta]params[]:'[orange]%s[]'\n", opt->param_str);

    num = _encode_opts(opt->param_str, fg->params, fgen_countof(fg->params));
    if (num < 0)
        FGEN_ERR_RET("Parameters '%s' invalid\n", opt->param_str);

    plen = pktlen = fbuf_data_len(f);

    for (int i = 0; i < num; i++) {
        switch (parser_kvp(fg->params[i], keys, fgen_countof(keys), &val)) {
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

    fbuf_data_len(f) = pktlen;

    if (pktlen > plen)
        memset(fbuf_mtod_offset(f, char *, plen), fill, pktlen - plen);

    switch (next_layer(f, ++lidx)) {
    case FGEN_ERROR_TYPE:
        FGEN_ERR_RET("Next layer (%d) return error\n", lidx);
    default:
        break;
    }

    /* If the frame size was adjusted then make sure we fill the payload */
    if ((fbuf_data_len(f) != pktlen) && (fill != 0))
        memset(fbuf_mtod_offset(f, char *, pktlen), fill, fbuf_data_len(f) - pktlen);

    if (fg->flags & FGEN_VERBOSE)
        FGEN_INFO("[magenta]Return '[orange]%s[]'\n", parser_type(opt->typ));

    return FGEN_PAYLOAD_TYPE;
}

/**
 * @brief Sentinel encoder called at the end of every frame's layer chain.
 *
 * Enforces minimum (ETH_ZLEN) and maximum (ETH_FRAME_LEN) frame lengths,
 * adjusting @c data_len with a warning if either bound is violated.
 *
 * @param f    Frame being finalized.
 * @param lidx Unused — always the last slot in @c fg->opts[].
 * @return     FGEN_DONE_TYPE.
 */
static int
_encode_done(frame_t *f, int lidx __fgen_unused)
{
    fgen_t *fg = f->fg;
    fopt_t *opt = FGEN_LOPT(fg, lidx);

    if (fg->flags & FGEN_VERBOSE)
        FGEN_INFO("[magenta]Finish up packet parsing. len %d[]\n", fbuf_data_len(f));

    if (fbuf_data_len(f) < ETH_ZLEN) {
        if (fg->flags & FGEN_VERBOSE)
            FGEN_WARN("[magenta]Packet is to short [orange]%d[], [magenta]adjusting to [orange]%d "
                      "[magenta]bytes[]\n",
                      fbuf_data_len(f), ETH_ZLEN);
        fbuf_data_len(f) = ETH_ZLEN;
    }

    if (fbuf_data_len(f) > ETH_FRAME_LEN) {
        if (fg->flags & FGEN_VERBOSE)
            FGEN_WARN("[magenta]Packet is to long [orange]%d[], [magenta]adjusting to [orange]%d "
                      "[magenta]bytes[]\n",
                      fbuf_data_len(f), ETH_FRAME_LEN);
        fbuf_data_len(f) = ETH_FRAME_LEN;
    }

    if (fg->flags & FGEN_VERBOSE)
        FGEN_INFO("[magenta]Return '[orange]%s[]' [magenta]pktlen [orange]%d[]\n",
                  parser_type(opt->typ), fbuf_data_len(f));

    return FGEN_DONE_TYPE;
}

/**
 * @brief Static dispatch table mapping layer token prefixes to encoder functions.
 *
 * Entry [0] is the sentinel _encode_done function (empty string prefix).
 * _encode_frame() walks entries [1..] to match each layer token from the
 * frame description string, populating @c fgen_t.opts[] before the recursive
 * encode pass begins.
 */
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

/**
 * @brief Parse a frame description string and write binary packet data into the salloc slab.
 *
 * Two-pass operation:
 *  1. Tokenize @c f->fstr on '/' into @c fg->layers[], then match each token
 *     against @c fgen_tbl[] to fill @c fg->opts[] with type, dispatch pointer,
 *     and parameter substring.
 *  2. Call next_layer(f, 0) to start the recursive-descent encode.  Each layer
 *     encoder writes its header into the slab, advances @c f->data_len, and
 *     calls next_layer() for the following layer before computing length or
 *     checksum fields that depend on inner content.
 *
 * @param f  Allocated frame_t with @c fstr set; @c fg must point to a valid fgen_t.
 * @return   0 on success, -1 if any layer fails to encode.
 */
int
_encode_frame(frame_t *f)
{
    fgen_t *fg = f->fg;
    fopt_t *opt = NULL;
    char *fstr;

    if (!fg || !f || !f->fstr)
        FGEN_ERR_RET("fgen_t is NULL\n");
    if (!f)
        FGEN_ERR_RET("frame_t is NULL\n");
    if (!f->fstr)
        FGEN_ERR_RET("frame_t.fstr is NULL\n");

    memset(fg->params, 0, sizeof(fg->params));
    memset(fg->opts, 0, sizeof(fg->opts));
    memset(&f->l2, 0, sizeof(f->l2));
    memset(&f->l3, 0, sizeof(f->l3));
    memset(&f->l4, 0, sizeof(f->l4));

    fstr = strdup(f->fstr);
    if (!fstr)
        FGEN_ERR_RET("Unable to allocate memory for frame string\n");

    /* Leave the last entry for the done layer function */
    fg->nb_layers = fgen_strtok(fstr, "/", fg->layers, FGEN_MAX_LAYERS - 1);
    if (fg->nb_layers <= 0)
        FGEN_ERR_RET("Number of layers is %d\n", fg->nb_layers);

    /* Process each layer of the frame, identifying each layer type */
    for (int i = 0; i < fg->nb_layers; i++) {
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
    opt            = &fg->opts[fg->nb_layers++];
    opt->typ       = FGEN_DONE_TYPE;
    opt->tbl       = &fgen_tbl[0];
    opt->param_str = NULL;

    if (fg->flags & FGEN_VERBOSE)
        FGEN_INFO("[magenta]Add layer[] [orange]%d[] - [orange]Done[]\n", fg->nb_layers - 1);

    int ret = 0;

    /* Parse the layers */
    if (next_layer(f, 0) < 0)
        ret = -1;
    else if (fg->flags & FGEN_DUMP_DATA)
        fgen_print_frame(NULL, f);

    free(fstr);
    return ret;
}
