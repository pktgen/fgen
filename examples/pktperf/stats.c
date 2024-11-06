/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Intel Corporation
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/queue.h>
#include <setjmp.h>
#include <stdarg.h>
#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <stdbool.h>
#include <alloca.h>
#include <locale.h>

#include <pktperf.h>

#define sprint(name, cntr, nl)                                         \
    do {                                                               \
        qstats_t *r;                                                   \
        uint64_t total = 0;                                            \
        printf("  %-8s", name);                                        \
        for (uint16_t q = 0; q < port->num_rx_qids; q++) {             \
            r = &port->pq[q].rate;                                     \
            total += r->cntr[q];                                       \
            printf("|%'12" PRIu64, (r->cntr[q] / info->timeout_secs)); \
        }                                                              \
        printf("|%'14" PRIu64 "|", (total / info->timeout_secs));      \
        if (nl)                                                        \
            printf("\n");                                              \
        fflush(stdout);                                                \
    } while (0)

/* Print out statistics on packets dropped */
void
print_stats(void)
{
    struct rte_eth_link link;
    struct rte_eth_stats rate;
    char link_status_text[RTE_ETH_LINK_MAX_STR_LEN];
    char twirl[]   = "|/-\\";
    static int cnt = 0;

    const char clr[]      = {27, '[', '2', 'J', '\0'};
    const char top_left[] = {27, '[', '1', ';', '1', 'H', '\0'};

    /* Clear screen and move to top left */
    printf("%s%s", clr, top_left);

    printf("Port    : Rate Statistics per queue (%c)\n", twirl[cnt++ % 4]);

    for (uint16_t pid = 0; pid < info->num_ports; pid++) {
        l2p_port_t *port = &info->ports[pid];

        if (rte_atomic16_read(&port->inited) == 0) {
            printf("Port %u is not initialized\n", pid);
            continue;
        }

        memset(&port->link, 0, sizeof(port->link));
        rte_eth_link_get_nowait(port->pid, &port->link);
        packet_rate(port);

        rte_eth_stats_get(port->pid, &port->stats);

        rate.imissed   = port->stats.imissed - port->pstats.imissed;
        rate.ierrors   = port->stats.ierrors - port->pstats.ierrors;
        rate.oerrors   = port->stats.oerrors - port->pstats.oerrors;
        rate.rx_nombuf = port->stats.rx_nombuf - port->pstats.rx_nombuf;
        memcpy(&port->pstats, &port->stats, sizeof(struct rte_eth_stats));

        for (uint16_t q = 0; q < port->num_rx_qids; q++) {
            qstats_t *c, *p, *r;

            c = &port->pq[q].curr;
            p = &port->pq[q].prev;
            r = &port->pq[q].rate;

            r->q_opackets[q] = c->q_opackets[q] - p->q_opackets[q];
            r->q_obytes[q]   = c->q_obytes[q] - p->q_obytes[q];

            r->q_ipackets[q] = c->q_ipackets[q] - p->q_ipackets[q];
            r->q_ibytes[q]   = c->q_ibytes[q] - p->q_ibytes[q];

            r->q_no_txmbufs[q] = c->q_no_txmbufs[q] - p->q_no_txmbufs[q];
            r->q_tx_drops[q]   = c->q_tx_drops[q] - p->q_tx_drops[q];
            r->q_tx_time[q]    = c->q_tx_time[q];
            r->q_rx_time[q]    = c->q_rx_time[q];

            memcpy(p, c, sizeof(qstats_t));
        }

        memset(&link, 0, sizeof(link));
        rte_eth_link_get_nowait(pid, &link);
        rte_eth_link_to_str(link_status_text, sizeof(link_status_text), &link);

        printf("%2u >> %s, ", pid, link_status_text);

        packet_rate(port);
        printf("MaxPPS: %'" PRIu64 ", TxCPB: %'" PRIu64 "\n", port->pps, port->tx_cycles);

        printf("  Queue ID");
        for (uint16_t q = 0; q < port->num_rx_qids; q++)
            printf("|%8u    ", q);
        printf("|  %8s    |\n", "Total");
        printf("  --------+------------+------------+------------+--------------+\n");

        sprint("RxQs", q_ipackets, 0);
        if (rate.ierrors)
            printf(" Err : %'12" PRIu64, rate.ierrors);
        if (rate.imissed)
            printf(" Miss: %'12" PRIu64, rate.imissed);
        printf("\n");
        sprint("TxQs", q_opackets, 0);
        if (rate.oerrors)
            printf(" Err : %'12" PRIu64, rate.oerrors);
        printf("\n");
        sprint("TxDrop", q_tx_drops, 1);
        sprint("NoTxMBUF", q_no_txmbufs, 1);
        sprint("RxTime", q_rx_time, 1);
        sprint("TxTime", q_tx_time, 1);
        printf("\n");
    }
    printf("Pktperf: Burst: %'u, MBUFs/port: %'" PRIu32 ", PktSize:%'" PRIu32
           ", Rx/Tx %'d/%'d, TxRate %u%%, PID: %d\n",
           info->burst_count, info->mbuf_count, info->pkt_size, info->nb_rxd, info->nb_txd,
           info->tx_rate, getpid());
    printf("         Port mapping: ");
    for (int i = 0; i < info->num_mappings; i++)
        printf("%s ", info->mappings[i]);
    printf("\n");

    fflush(stdout);
}
