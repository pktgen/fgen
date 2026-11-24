/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023-2026 Intel Corporation
 */

/**
 * @file
 * @brief pktperf main application: DPDK initialisation, per-lcore Rx/Tx loops,
 *        thread launch, and the program entry point.
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
#include <pthread.h>

#include <pktperf.h>

#include <fgen.h>
#include <fgen_stdio.h>
#include <fgen_version.h>

txpkts_info_t *info;

/**
 * @brief rte_mempool_obj_iter() callback that writes a test packet into each mbuf.
 *
 * Called once per mbuf in the TX pool; constructs the packet payload and sets
 * the mbuf metadata (data_len, pkt_len, port, etc.).
 *
 * @param mp
 *   The mempool that owns this object.
 * @param opaque
 *   Pointer to the l2p_lport_t for the lcore initialising the pool.
 * @param obj
 *   Pointer to the rte_mbuf to initialise.
 * @param obj_idx
 *   Index of this object within the pool (unused).
 */
static __inline__ void
mbuf_iterate_cb(struct rte_mempool *mp, void *opaque, void *obj, unsigned obj_idx __rte_unused)
{
    l2p_lport_t *lport = (l2p_lport_t *)opaque;
    struct rte_mbuf *m = (struct rte_mbuf *)obj;
    uint16_t plen      = info->pkt_size - RTE_ETHER_CRC_LEN;

    packet_constructor(lport, rte_pktmbuf_mtod(m, uint8_t *), info->ip_proto);

    m->pool     = mp;
    m->next     = NULL;
    m->data_len = plen;
    m->pkt_len  = plen;
    m->port     = 0;
    m->ol_flags = 0;
}

/**
 * @brief Drain the Rx queue for @p lport and update receive statistics.
 *
 * @param lport
 *   Lcore/port context identifying the port and Rx queue to poll.
 * @param mbufs
 *   Caller-provided mbuf pointer array used as the receive burst buffer.
 * @param n_mbufs
 *   Maximum number of packets to receive in one burst.
 * @param curr_tsc
 *   TSC timestamp captured before calling this function (used for timing).
 */
static __inline__ void
do_rx_process(l2p_lport_t *lport, struct rte_mbuf **mbufs, uint32_t n_mbufs, uint64_t curr_tsc)
{
    l2p_port_t *port = lport->port;
    qstats_t *c;
    uint16_t nb_pkts, rx_qid;

    rx_qid = lport->rx_qid;
    c      = &port->pq[rx_qid].curr;

    /* drain the RX queue */
    nb_pkts = rte_eth_rx_burst(port->pid, rx_qid, mbufs, n_mbufs);
    if (nb_pkts) {
        for (uint16_t i = 0; i < nb_pkts; i++)
            c->q_ibytes[rx_qid] += rte_pktmbuf_pkt_len(mbufs[i]);
        c->q_ipackets[rx_qid] += nb_pkts;

        rte_pktmbuf_free_bulk(mbufs, nb_pkts);
        c->q_rx_time[rx_qid] = rte_rdtsc() - curr_tsc;
    }
}

/**
 * @brief Transmit a burst of packets from the TX mempool and update statistics.
 *
 * Acquires @p n_mbufs mbufs directly from the pool (bypassing pktmbuf to avoid
 * altering pre-built packet data), sends them, and retries until all are sent.
 *
 * @param lport
 *   Lcore/port context identifying the port and Tx queue to use.
 * @param mbufs
 *   Caller-provided mbuf pointer array used as the transmit burst buffer.
 * @param n_mbufs
 *   Number of packets to transmit in this burst.
 * @param curr_tsc
 *   TSC timestamp captured before calling this function (used for timing).
 */
static __inline__ void
do_tx_process(l2p_lport_t *lport, struct rte_mbuf **mbufs, uint16_t n_mbufs, uint64_t curr_tsc)
{
    l2p_port_t *port = lport->port;
    struct rte_mempool *mp;
    qstats_t *c;
    uint16_t pid, tx_qid;

    pid    = port->pid;
    tx_qid = lport->tx_qid;
    c      = &port->pq[tx_qid].curr;
    mp     = lport->port->tx_mp;

    /* Use mempool routines instead of pktmbuf to make sure the mbufs is not altered */
    if (rte_mempool_get_bulk(mp, (void **)mbufs, n_mbufs) == 0) {
        uint16_t plen = info->pkt_size - RTE_ETHER_CRC_LEN;
        uint16_t sent, send = n_mbufs;

        do {
            sent = rte_eth_tx_burst(pid, tx_qid, mbufs, send);
            send -= sent;
            mbufs += sent;
        } while (send > 0);

        c->q_opackets[tx_qid] += n_mbufs;
        c->q_obytes[tx_qid] += (n_mbufs * plen); /* does not include FCS */

        c->q_tx_time[tx_qid] = rte_rdtsc() - curr_tsc;
    } else
        c->q_no_txmbufs[tx_qid]++;
}

/**
 * @brief Rx-only processing loop executed by lcores assigned LCORE_MODE_RX.
 *
 * Continuously drains the Rx queue until force_quit is set.
 */
static void
rx_loop(void)
{
    l2p_lport_t *lport;
    uint16_t rx_burst = info->burst_count * 2;
    struct rte_mbuf *mbufs[rx_burst];

    lport = info->lports[rte_lcore_id()];

    DBG_PRINT("Starting loop for lcore:port:queue %3u:%2u:%2u\n", rte_lcore_id(), lport->port->pid,
              lport->rx_qid);

    while (!info->force_quit)
        do_rx_process(lport, mbufs, rx_burst, rte_rdtsc());

    DBG_PRINT("Exiting loop for lcore:port:queue %3u:%2u:%2u\n", rte_lcore_id(), lport->port->pid,
              lport->rx_qid);
}

/**
 * @brief Tx-only processing loop executed by lcores assigned LCORE_MODE_TX.
 *
 * Initialises the TX mempool on first entry, then paces TX bursts to the
 * configured rate until force_quit is set.
 */
static void
tx_loop(void)
{
    l2p_lport_t *lport;
    l2p_port_t *port;
    uint64_t curr_tsc, burst_tsc;
    uint16_t tx_burst = info->burst_count;
    struct rte_mbuf *mbufs[tx_burst];

    lport = info->lports[rte_lcore_id()];
    port  = lport->port;

    DBG_PRINT("Starting loop for lcore:port:queue %3u:%2u:%2u\n", rte_lcore_id(), port->pid,
              lport->tx_qid);

    pthread_spin_lock(&port->tx_lock);
    if (port->tx_inited == 0) {
        port->tx_inited = 1;
        /* iterate over all buffers in the pktmbuf pool and setup the packet data */
        rte_mempool_obj_iter(port->tx_mp, mbuf_iterate_cb, (void *)lport);
    }
    pthread_spin_unlock(&port->tx_lock);

    burst_tsc = rte_rdtsc() + port->tx_cycles;

    while (!info->force_quit) {
        curr_tsc = rte_rdtsc();

        if (unlikely(curr_tsc >= burst_tsc)) {
            burst_tsc = curr_tsc + port->tx_cycles;

            if (likely(port->tx_cycles))
                do_tx_process(lport, mbufs, tx_burst, curr_tsc);
        }
    }
    DBG_PRINT("Exiting loop for lcore:port:queue %3u:%2u:%2u\n", rte_lcore_id(), port->pid,
              lport->tx_qid);
}

/**
 * @brief Combined Rx/Tx processing loop for lcores assigned LCORE_MODE_BOTH.
 *
 * Drains the Rx queue on every iteration and paces TX bursts at the configured
 * rate until force_quit is set.
 */
static void
rxtx_loop(void)
{
    l2p_lport_t *lport;
    l2p_port_t *port;
    uint64_t curr_tsc, burst_tsc;
    uint16_t rx_burst = info->burst_count * 2;
    uint16_t tx_burst = info->burst_count;
    struct rte_mbuf *mbufs[rx_burst];

    lport = info->lports[rte_lcore_id()];
    port  = lport->port;

    DBG_PRINT("Starting loop for lcore:port:queue %3u:%2u:%2u.%2u\n", rte_lcore_id(), port->pid,
              lport->rx_qid, lport->tx_qid);

    pthread_spin_lock(&port->tx_lock);
    if (port->tx_inited == 0) {
        port->tx_inited = 1;
        /* iterate over all buffers in the pktmbuf pool and setup the packet data */
        rte_mempool_obj_iter(port->tx_mp, mbuf_iterate_cb, (void *)lport);
    }
    pthread_spin_unlock(&port->tx_lock);

    burst_tsc = rte_rdtsc() + port->tx_cycles;

    while (!info->force_quit) {
        curr_tsc = rte_rdtsc();

        do_rx_process(lport, mbufs, rx_burst, curr_tsc);

        if (unlikely(curr_tsc >= burst_tsc)) {
            burst_tsc = curr_tsc + port->tx_cycles;

            if (likely(port->tx_cycles))
                do_tx_process(lport, mbufs, tx_burst, curr_tsc);
        }
    }
    DBG_PRINT("Exiting loop for lcore:port:queue %3u:%2u:%2u.%u\n", rte_lcore_id(), port->pid,
              lport->rx_qid, lport->tx_qid);
}

/**
 * @brief DPDK lcore entry point; dispatches to the correct Rx/Tx loop.
 *
 * Launched on every worker lcore via rte_eal_mp_remote_launch().
 *
 * @param dummy
 *   Unused; required by the rte_eal_remote_launch() callback signature.
 * @return
 *   0 on normal exit, EXIT_FAILURE if the lcore has no valid port assignment.
 */
static int
txpkts_launch_one_lcore(__rte_unused void *dummy)
{
    l2p_lport_t *lport = info->lports[rte_lcore_id()];

    if (lport == NULL || lport->port == NULL || lport->port->pid >= RTE_MAX_ETHPORTS)
        ERR_RET("lport or port is NULL\n");

    switch (lport->mode) {
    case LCORE_MODE_RX:
        rx_loop();
        break;
    case LCORE_MODE_TX:
        tx_loop();
        break;
    case LCORE_MODE_BOTH:
        rxtx_loop();
        break;
    case LCORE_MODE_UNKNOWN:
    default:
        ERR_RET("Invalid mode %u\n", lport->mode);
        break;
    }
    return 0;
}

/**
 * @brief Signal handler for SIGINT and SIGTERM.
 *
 * Sets info->force_quit to true so that all processing loops exit cleanly.
 *
 * @param signum
 *   Signal number received (SIGINT or SIGTERM).
 */
static void
signal_handler(int signum)
{
    if (signum == SIGINT || signum == SIGTERM) {
        DBG_PRINT("\n\nSignal %d received, preparing to exit...\n", signum);
        info->force_quit = true;
    }
}

/**
 * @brief Initialise DPDK EAL, count available ports, and parse application arguments.
 *
 * Also installs signal handlers for SIGINT/SIGTERM and seeds the random
 * number generator.
 *
 * @param argc
 *   Full argument count as received by main().
 * @param argv
 *   Full argument vector as received by main().
 * @return
 *   0 on success, EXIT_FAILURE on any error.
 */
static int
initialize_dpdk(int argc, char **argv)
{
    int ret = 0;

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    srandom(RANDOM_SEED);
    setlocale(LC_ALL, "");

    if ((ret = rte_eal_init(argc, argv)) < 0)
        ERR_RET("Invalid DPDK arguments\n");

    argc -= ret; /* Skip DPDK configuration options */
    argv += ret;

    if ((info->num_ports = rte_eth_dev_count_avail()) == 0)
        ERR_RET("No Ethernet ports found - bye\n");

    if (parse_configuration(argc, argv) < 0)
        ERR_RET("Invalid configuration\n");

    return 0;
}

/**
 * @brief Launch all worker lcore threads and run the statistics display loop.
 *
 * Scrolls the console, launches one thread per worker lcore, periodically
 * calls print_stats(), waits for all workers to finish, stops and closes all
 * ports, and calls rte_eal_cleanup().
 *
 * @return
 *   Return value of rte_eal_cleanup() (0 on success).
 */
static int
launch_lcore_threads(void)
{
    int lid;

    /* Scroll the screen to keep console output for debugging */
    for (int i = 0; i < 10; i++)
        PRINT("\n\n\n\n\n\n\n\n\n\n\n");

    /* launch per-lcore init on every worker lcore */
    if (rte_eal_mp_remote_launch(txpkts_launch_one_lcore, NULL, SKIP_MAIN) != 0)
        ERR_RET("Failed to launch lcore threads\n");

    /* Display the statistics  */
    do {
        print_stats();
        rte_delay_us_sleep(info->timeout_secs * Million);
    } while (!info->force_quit);

    RTE_LCORE_FOREACH_WORKER(lid)
    {
        DBG_PRINT("Waiting for lcore %d to exit\n", lid);
        if (rte_eal_wait_lcore(lid) < 0)
            ERR_RET("Error waiting for lcore %d to exit\n", lid);
    }

    for (uint16_t portid = 0; portid < info->num_ports; portid++) {
        DBG_PRINT("Closing port %d... ", portid);
        if (rte_eth_dev_stop(portid) == 0)
            rte_eth_dev_close(portid);
        DBG_PRINT("\n");
    }

    return rte_eal_cleanup();
}

/**
 * @brief Allocate and zero-initialise the global txpkts_info_t structure.
 *
 * Also initialises the per-port spinlocks and marks all port IDs as invalid.
 *
 * @return
 *   Pointer to the newly allocated structure, or NULL on failure.
 */
static txpkts_info_t *
info_alloc(void)
{
    txpkts_info_t *txinfo;

    txinfo = (txpkts_info_t *)calloc(1, sizeof(txpkts_info_t));
    if (txinfo) {
        for (int i = 0; i < RTE_MAX_ETHPORTS; i++) {
            int ret;

            txinfo->ports[i].pid = RTE_MAX_ETHPORTS + 1; /* set to invalid port id */

            ret = pthread_spin_init(&txinfo->ports[i].tx_lock, PTHREAD_PROCESS_PRIVATE);
            if (ret != 0) {
                free(txinfo);
                ERR_RET_NULL("Unable to initialize tx_lock for port %d: %s\n", i, strerror(ret));
            }
        }
    }

    return txinfo;
}

/**
 * @brief pktperf application entry point.
 *
 * Allocates application state, initialises DPDK, and runs the lcore thread
 * loop.
 *
 * @param argc
 *   Argument count.
 * @param argv
 *   Argument vector.
 * @return
 *   EXIT_SUCCESS on clean exit, EXIT_FAILURE on any error.
 */
int
main(int argc, char **argv)
{
    fgen_printf("Version: %s\n", fgen_version());
    info = info_alloc();
    if (info) {
        if (initialize_dpdk(argc, argv) == 0) {
            if (launch_lcore_threads() == 0) {        // Waits for all threads to exit
                free(info);
                return EXIT_SUCCESS;
            }
        }
        free(info);
    }

    return EXIT_FAILURE;
}
