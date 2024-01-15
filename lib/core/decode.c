/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023-2024 Intel Corporation
 */

#include <stdint.h>        // for uint32_t, uint16_t, int32_t, uint8_t
#include <stdbool.h>
#include <netinet/in.h>        // for ntohs, htonl, htons
#include <net/ethernet.h>
#include <arpa/inet.h>
#include <ctype.h>

#include <fgen_common.h>
#include <fgen_log.h>
#include <net/fgen_ether.h>
#include <net/fgen_ip.h>
#include <net/fgen_udp.h>
#include <net/fgen_tcp.h>
#include <net/fgen_vxlan.h>

#include "fgen.h"
#include "decode.h"

static int _decode_vlan(decode_t *dc, bool is_dot1ad);

static __attribute__((__format__(__printf__, 2, 0))) int
_append(decode_t *dc, const char *format, ...)
{
    va_list ap;
    char str[FGEN_MAX_STRING_LENGTH] = {0};
    int ret, nbytes;

    va_start(ap, format);
    ret = vsnprintf(str, sizeof(str) - 1, format, ap);
    va_end(ap);

    /* First time just allocate some memory to use for buffer */
    if (dc->buffer == NULL) {
        dc->buffer = calloc(1, 4 * FGEN_EXTRA_SPACE);
        if (dc->buffer == NULL)
            FGEN_ERR_RET("unable to allocate buffer memory\n");

        dc->buf_len = (4 * FGEN_EXTRA_SPACE);
        dc->used    = 0;
    }

    nbytes = (ret + dc->used) + FGEN_EXTRA_SPACE;

    /* Increase size of buffer if required */
    if (nbytes >= dc->buf_len) {

        /* Make sure the max length is capped to a max size */
        if (nbytes >= FGEN_MAX_FSTR_LEN)
            FGEN_ERR_RET("total length %d > %d max\n", nbytes, FGEN_MAX_FSTR_LEN);

        /* expand the buffer space */
        char *p = realloc(dc->buffer, nbytes);

        if (p == NULL)
            FGEN_ERR_RET("unable to re-allocate buffer length %d\n", nbytes);

        /* Clear out new buffer space */
        memset(&p[dc->buf_len], 0, nbytes - dc->buf_len);
        dc->buffer  = p;
        dc->buf_len = nbytes;
    }

    /* Add the new string data to the buffer */
    dc->used = strlcat(dc->buffer, str, dc->buf_len);

    return 0;
}

static int
_decode_raw(decode_t *dc)
{
    if (decode_len(dc) > decode_offset(dc)) {
        uint8_t *p = decode_mtod_offset(dc, uint8_t *, decode_offset(dc));
        int len    = decode_len(dc) - decode_offset(dc);

        _append(dc, "Raw('");
        for (int i = 0; i < len; i++, p++) {
            if (isprint(*p))
                _append(dc, "%c", *p);
            else
                _append(dc, "\\x%02x", *p);
        }
        _append(dc, "')/");
        decode_offset(dc) += len;
    }

    return 0;
}

static int
_decode_payload(decode_t *dc)
{
    _decode_raw(dc);
    _append(dc, FGEN_PAYLOAD_STR "(len=%d", decode_len(dc) + ETHER_CRC_LEN);
    _append(dc, ")");

    return 0;
}

static int
_decode_tsc(decode_t *dc)
{
    tsc_t *tsc;

    tsc = decode_mtod_offset(dc, tsc_t *, decode_offset(dc));

    if (tsc->tstmp == TIMESTAMP_ID) {
        _append(dc, FGEN_TSC_STR "(");
        _append(dc, "0x%016lx", tsc->tsc_val);
        _append(dc, ")/");
        decode_offset(dc) += sizeof(tsc_t);
    }

    return _decode_payload(dc);
}

static int
_decode_udp(decode_t *dc)
{
    struct fgen_udp_hdr *udp;

    udp = decode_mtod_offset(dc, struct fgen_udp_hdr *, decode_offset(dc));
    decode_offset(dc) += sizeof(struct fgen_udp_hdr);

    _append(dc, FGEN_UDP_STR "(");
    _append(dc, "dport=%u,sport=%u,len=%u,cksum=0x%x", ntohs(udp->dst_port), ntohs(udp->src_port),
            ntohs(udp->dgram_len), udp->dgram_cksum);
    _append(dc, ")/");

    return _decode_tsc(dc);
}

static int
_decode_tcp(decode_t *dc)
{
    struct fgen_tcp_hdr *tcp;

    tcp = decode_mtod_offset(dc, struct fgen_tcp_hdr *, decode_offset(dc));
    decode_offset(dc) += sizeof(struct fgen_tcp_hdr);

    (void)tcp;

    _append(dc, FGEN_TCP_STR "(");
    _append(dc, "sport=%d", ntohs(tcp->src_port));
    _append(dc, ",dport=%d", ntohs(tcp->dst_port));
    _append(dc, ",seq=%u", ntohl(tcp->sent_seq));
    _append(dc, ",ack=%u", ntohl(tcp->recv_ack));
    _append(dc, ",data_off=%u", tcp->data_off);
    _append(dc, ",flags=%#x", tcp->tcp_flags);
    _append(dc, ",win=%#x", ntohs(tcp->rx_win));
    _append(dc, ",cksum=%#x", ntohs(tcp->cksum));
    _append(dc, ",urp=%%x", ntohs(tcp->tcp_urp));
    _append(dc, ")/");

    return _decode_tsc(dc);
}

static int
_decode_ipv4(decode_t *dc)
{
    struct fgen_ipv4_hdr *ip;
    char buf[64];

    ip = decode_mtod_offset(dc, struct fgen_ipv4_hdr *, decode_offset(dc));

    decode_offset(dc) += sizeof(struct fgen_ipv4_hdr);

    _append(dc, FGEN_IPv4_STR "(");

    _append(dc, "version_ihl=%#x", ip->version_ihl);
    _append(dc, ",tos=%#x", ip->type_of_service);
    _append(dc, ",len=%d", ntohs(ip->total_length));
    _append(dc, ",id=%#x", ntohs(ip->total_length));
    _append(dc, ",fragoff=%d", ntohs(ip->fragment_offset));
    _append(dc, ",ttl=%d", ntohs(ip->time_to_live));
    _append(dc, ",cksum=%d", ntohs(ip->hdr_checksum));

    inet_ntop(AF_INET, &ip->dst_addr, buf, sizeof(buf));
    _append(dc, "dst=%s", buf);

    inet_ntop(AF_INET, &ip->src_addr, buf, sizeof(buf));
    _append(dc, ",src=%s", buf);

    switch (ip->next_proto_id) {
    case IPPROTO_UDP:
        _append(dc, ",proto=udp)/");
        return _decode_udp(dc);
    case IPPROTO_TCP:
        _append(dc, ",proto=tcp)/");
        return _decode_tcp(dc);
    default:
        _append(dc, ",proto=%d)/", ip->next_proto_id);
        break;
    }

    return _decode_tsc(dc);
}

static int
_decode_ipv6(decode_t *dc)
{
    struct fgen_ipv6_hdr *ip;
    char buf[64];

    ip = decode_mtod_offset(dc, struct fgen_ipv6_hdr *, decode_offset(dc));

    decode_offset(dc) += sizeof(struct fgen_ipv6_hdr);

    _append(dc, FGEN_IPv6_STR "(");

    _append(dc, "vtc=%#x", ntohl(ip->vtc_flow));
    _append(dc, ",len=%#x", ntohs(ip->payload_len));
    _append(dc, ",hops=%d", ip->hop_limits);

    inet_ntop(AF_INET6, &ip->dst_addr, buf, sizeof(buf));
    _append(dc, "dst=%s", buf);

    inet_ntop(AF_INET6, &ip->src_addr, buf, sizeof(buf));
    _append(dc, ",src=%s", buf);

    switch (ip->proto) {
    case IPPROTO_UDP:
        _append(dc, ",proto=udp)/");
        return _decode_udp(dc);
    case IPPROTO_TCP:
        _append(dc, ",proto=tcp)/");
        return _decode_tcp(dc);
    default:
        _append(dc, ",proto=%d)/", ip->proto);
        break;
    }

    return _decode_tsc(dc);
}

static int
_decode_dot1ad(decode_t *dc)
{
    return _decode_vlan(dc, true);
}

static int
_decode_dot1q(decode_t *dc)
{
    return _decode_vlan(dc, false);
}

static int
_decode_vlan(decode_t *dc, bool is_dot1ad)
{
    struct fgen_vlan_hdr *vlan;
    uint16_t vid, prio, cfi, tci, proto;

    vlan = decode_mtod_offset(dc, struct fgen_vlan_hdr *, decode_offset(dc));
    decode_offset(dc) += sizeof(struct fgen_vlan_hdr);

    tci  = ntohs(vlan->vlan_tci);
    vid  = tci & 0xFFF;
    prio = (tci >> 13) & 7;
    cfi  = (tci >> 12) & 1;

    _append(dc, "Dot1%s(", is_dot1ad ? "AD" : "Q");
    _append(dc, "vid=%u,prio=%u,cfi=%u", vid, prio, cfi);
    _append(dc, ")/");

    proto = ntohs(vlan->eth_proto);
    if (is_dot1ad && proto == FGEN_ETHER_TYPE_VLAN)
        return _decode_dot1q(dc);
    else if (proto == FGEN_ETHER_TYPE_QINQ)
        return _decode_dot1ad(dc);
    else if (proto == FGEN_ETHER_TYPE_IPV4)
        return _decode_ipv4(dc);
    else if (proto == FGEN_ETHER_TYPE_IPV6)
        return _decode_ipv6(dc);

    return -1;
}

static int
_decode_ether(decode_t *dc)
{
    struct ether_header *eth;
    struct ether_addr *addr;

    eth = decode_mtod(dc, struct ether_header *);

    _append(dc, "Ether(");
    addr = (struct ether_addr *)&eth->ether_dhost;
    _append(dc, "dst=%02X:%02X:%02X:%02X:%02X:%02X", addr->ether_addr_octet[0],
            addr->ether_addr_octet[1], addr->ether_addr_octet[2], addr->ether_addr_octet[3],
            addr->ether_addr_octet[4], addr->ether_addr_octet[5]);
    addr = (struct ether_addr *)&eth->ether_shost;
    _append(dc, ",src=%02X:%02X:%02X:%02X:%02X:%02X", addr->ether_addr_octet[0],
            addr->ether_addr_octet[1], addr->ether_addr_octet[2], addr->ether_addr_octet[3],
            addr->ether_addr_octet[4], addr->ether_addr_octet[5]);
    _append(dc, ")/");

    decode_offset(dc) += sizeof(struct ether_header);

    switch (ntohs(eth->ether_type)) {
    case FGEN_ETHER_TYPE_VLAN:
        return _decode_dot1ad(dc);
    case FGEN_ETHER_TYPE_QINQ:
        return _decode_dot1q(dc);
    case FGEN_ETHER_TYPE_IPV4:
        return _decode_ipv4(dc);
    default:
        break;
    }
    return -1;
}

fgen_decode_t *
fgen_decode_create(void)
{
    return (fgen_decode_t *)calloc(1, sizeof(decode_t));
}

int
fgen_decode(fgen_decode_t *_dc, void *data, uint16_t len, opt_type_t opt)
{
    decode_t *dc = _dc;
    int ret;

    if (!dc || !data || len == 0)
        return -1;

    if (dc->buffer)
        free(dc->buffer);

    dc->buffer   = NULL;
    dc->buf_len  = 0;
    dc->used     = 0;
    dc->data_len = len;
    dc->data_off = 0;
    dc->data     = data;

    switch (opt) {
    default:
    case FGEN_ETHER_TYPE:
        ret = _decode_ether(dc);
        break;
    case FGEN_IPV4_TYPE:
        ret = _decode_ipv4(dc);
        break;
    case FGEN_IPV6_TYPE:
        ret = _decode_ipv6(dc);
        break;
    case FGEN_UDP_TYPE:
        ret = _decode_udp(dc);
        break;
    case FGEN_TCP_TYPE:
        ret = _decode_tcp(dc);
        break;
    }

    return (ret >= 0) ? dc->buf_len : -1;
}

void
fgen_decode_destroy(fgen_decode_t *_dc)
{
    decode_t *dc = _dc;

    if (dc) {
        free(dc->buffer);
        free(dc);
    }
}

const char *
fgen_decode_text(fgen_decode_t *_dc)
{
    decode_t *dc = _dc;

    return (dc && dc->buffer && dc->buf_len > 0) ? dc->buffer : NULL;
}

int
fgen_decode_string(const char *text, uint8_t *buffer, int len)
{
    int cnt = 0;

    if (!text || !buffer || !len)
        return -1;

    while (*text) {
        if (cnt >= len)
            return -1;
        if (isspace(*text)) {
            text++;
            continue;
        }
        if (isxdigit(text[0]) && isxdigit(text[1])) {
            char hex[4] = {0};
            long v      = 0;

            hex[0] = text[0];
            hex[1] = text[1];
            v      = strtol(hex, NULL, 16);
            if (errno == ERANGE)
                return -1;
            buffer[cnt++] = (uint8_t)v;
        }
    }

    return cnt;
}
