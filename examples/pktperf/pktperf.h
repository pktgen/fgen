/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023-2026 Intel Corporation
 */

/**
 * @file
 * @brief pktperf example application: shared types, constants, macros, and
 *        function declarations used across all pktperf source files.
 *
 * Defines the main application state structure (txpkts_info_t), per-port and
 * per-lcore descriptors, queue statistics, and the primary entry points for
 * parsing, port setup, packet construction, and statistics display.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <rte_common.h>
#include <rte_log.h>
#include <rte_malloc.h>
#include <rte_memory.h>
#include <rte_memcpy.h>
#include <rte_eal.h>
#include <rte_launch.h>
#include <rte_cycles.h>
#include <rte_prefetch.h>
#include <rte_lcore.h>
#include <rte_per_lcore.h>
#include <rte_branch_prediction.h>
#include <rte_interrupts.h>
#include <rte_random.h>
#include <rte_debug.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_string_fns.h>

#include <fgen_common.h>
#include <fgen.h>
#include <utils.h>

/** @brief Print @p format to stdout and flush immediately. */
#define PRINT(format, args...)  \
    do {                        \
        printf(format, ##args); \
        fflush(stdout);         \
    } while (0)

/** @brief Print an INFO-tagged message with function name and line number, then flush. */
#define INFO_PRINT(format, args...)                                \
    do {                                                           \
        char buf[64];                                              \
        snprintf(buf, sizeof(buf), "%s(%'d)", __func__, __LINE__); \
        printf("INFO>%-24s:" format, buf, ##args);                 \
        fflush(stdout);                                            \
    } while (0)

/** @brief Print an ERROR-tagged message with function name and line number, then flush. */
#define ERR_PRINT(format, args...)                                 \
    do {                                                           \
        char buf[64];                                              \
        snprintf(buf, sizeof(buf), "%s(%'d)", __func__, __LINE__); \
        printf("ERROR>%-24s:" format, buf, ##args);                \
        fflush(stdout);                                            \
    } while (0)

/** @brief Print an ERROR-tagged message then return EXIT_FAILURE from the enclosing function. */
#define ERR_RET(format, args...)                                   \
    do {                                                           \
        char buf[64];                                              \
        snprintf(buf, sizeof(buf), "%s(%'d)", __func__, __LINE__); \
        printf("ERROR>%-24s:" format, buf, ##args);                \
        fflush(stdout);                                            \
        return EXIT_FAILURE;                                       \
    } while (0)

/** @brief Print an ERROR-tagged message then return NULL from the enclosing function. */
#define ERR_RET_NULL(format, args...)                              \
    do {                                                           \
        char buf[64];                                              \
        snprintf(buf, sizeof(buf), "%s(%'d)", __func__, __LINE__); \
        printf("ERROR>%-24s:" format, buf, ##args);                \
        fflush(stdout);                                            \
        return NULL;                                               \
    } while (0)

/** @brief Print a DEBUG-tagged message (only when info->verbose is set), then flush. */
#define DBG_PRINT(format, args...)                                     \
    do {                                                               \
        if (info->verbose) {                                           \
            char buf[64];                                              \
            snprintf(buf, sizeof(buf), "%s(%'d)", __func__, __LINE__); \
            printf("DEBUG> %-24s:" format, buf, ##args);               \
            fflush(stdout);                                            \
        }                                                              \
    } while (0)

#define JUMBO_ETHER_MTU     9216 /**< Total size of an Ethernet jumbo frame in bytes. */
#define JUMBO_DATAROOM_SIZE 9000 /**< Data room size in an Ethernet jumbo frame in bytes. */
/** @brief Headroom size in an Ethernet jumbo frame (MTU minus data room). */
#define JUMBO_HEADROOM_SIZE \
    (JUMBO_ETHER_MTU - JUMBO_DATAROOM_SIZE)
/** @brief Total mbuf size needed for a jumbo frame including DPDK headroom. */
#define JUMBO_MBUF_SIZE (JUMBO_ETHER_MTU + RTE_PKTMBUF_HEADROOM)

/**
 * @brief Application-wide numeric constants for defaults, limits, and sizing.
 */
enum {
    DEFAULT_PKT_SIZE         = 64,           /* Default packet size */
    DEFAULT_TX_RATE          = 100,          /* Default TX rate */
    DEFAULT_RX_DESC          = 1024,         /* RX descriptors by default */
    DEFAULT_TX_DESC          = 1024,         /* TX descriptors by default */
    DEFAULT_BURST_COUNT      = 32,           /* default RX/TX burst count */
    DEFAULT_TX_DRAIN_US      = 100,          /* default TX drain every ~100us */
    DEFAULT_TIMEOUT_PERIOD   = 1,            /* 1 seconds default */
    DEFAULT_PROMISCUOUS_MODE = 1,            /* default to enabled */
    DEFAULT_JUMBO_FRAME_MODE = 0,            /* default to disabled for jumbo frames */
    DEFAULT_MBUF_COUNT       = (8 * 1024),   /* default to 16K mbufs */
    MAX_MBUF_COUNT           = (128 * 1024), /* max to 128K mbufs */
    MIN_RX_DESC              = 512,          /* Minimum number of RX descriptors */
    MIN_TX_DESC              = 512,          /* Minimum number of TX descriptors */
    MAX_RX_DESC              = 4096,         /* Maximum number of RX descriptors */
    MAX_TX_DESC              = 4096,         /* Maximum number of TX descriptors */
    MAX_QUEUES_PER_PORT      = 16,           /* Max number of queues per port */
    MAX_MAPPINGS             = 32,           /* Max number of mappings */
    MAX_TX_RATE              = 100,          /* Max TX rate percentage */
    MAX_PKT_SIZE             = 1518,         /* Maximum packet size */
    MAX_ALLOCA_SIZE          = 1024,         /* Maximum size of an allocation */
    MAX_BURST_COUNT          = 512,          /* max burst count */
    MAX_CHECK_TIME           = 40,           /* (40 * CHECK_INTERVAL) is 10s */

    RANDOM_SEED           = 0x19560630,                     /* Random seed */
    MEMPOOL_CACHE_SIZE    = RTE_MEMPOOL_CACHE_MAX_SIZE / 2, /* Size of mempool cache */
    PKT_BUFF_SIZE         = 2048,                           /* Size of packet buffers */
    MAX_TIMEOUT_PERIOD    = (60 * 60),                      /* 1 hour max */
    CHECK_INTERVAL        = 250,                            /* Link status check interval 250ms */
    INTER_FRAME_GAP       = 12,                             /* inter-frame gap in bytes */
    START_FRAME_DELIMITER = 1,                              /* Start Frame Delimiter in bytes*/
    PKT_PREAMBLE_SIZE     = 7,                              /* Packet preamble in bytes */
    PKT_OVERHEAD_SIZE =
        (INTER_FRAME_GAP + START_FRAME_DELIMITER + PKT_PREAMBLE_SIZE + RTE_ETHER_CRC_LEN),
};
#define Million (uint64_t)(1000000UL)    /**< Constant for one million (10^6). */
#define Billion (uint64_t)(1000000000UL) /**< Constant for one billion (10^9). */

/**
 * @brief Operating mode assigned to each lcore.
 */
enum {
    LCORE_MODE_UNKNOWN = 0, /**< Mode not yet assigned. */
    LCORE_MODE_RX      = 1, /**< Lcore handles receive only. */
    LCORE_MODE_TX      = 2, /**< Lcore handles transmit only. */
    LCORE_MODE_BOTH    = 3, /**< Lcore handles both receive and transmit. */
};

/**
 * @brief Per-queue statistics counters, cache-line aligned to avoid false sharing.
 */
typedef struct qstats_s {
    uint64_t q_ipackets[MAX_QUEUES_PER_PORT];   /**< Received packet count per queue. */
    uint64_t q_ibytes[MAX_QUEUES_PER_PORT];     /**< Received byte count per queue. */

    uint64_t q_opackets[MAX_QUEUES_PER_PORT];   /**< Transmitted packet count per queue. */
    uint64_t q_obytes[MAX_QUEUES_PER_PORT];     /**< Transmitted byte count per queue. */

    uint64_t q_rx_time[MAX_QUEUES_PER_PORT];    /**< TSC cycles spent receiving a burst. */
    uint64_t q_tx_drops[MAX_QUEUES_PER_PORT];   /**< Packets dropped due to a full TX ring. */
    uint64_t q_tx_time[MAX_QUEUES_PER_PORT];    /**< TSC cycles spent transmitting a burst. */
    uint64_t q_no_txmbufs[MAX_QUEUES_PER_PORT]; /**< Count of TX attempts with no free mbufs. */
} qstats_t __rte_cache_aligned;

/**
 * @brief Port/queue statistics triple: current, previous, and rate (delta).
 */
typedef struct pq_s {
    qstats_t curr; /**< Cumulative counters for the current interval. */
    qstats_t prev; /**< Snapshot of counters from the previous interval. */
    qstats_t rate; /**< Delta (rate) counters: curr minus prev. */
} pq_t;

/**
 * @brief State and statistics for a single Ethernet port.
 */
typedef struct l2p_port_s {
    rte_atomic16_t inited;          /**< Atomic flag; non-zero once the port is initialised. */
    pthread_spinlock_t tx_lock;     /**< Spinlock protecting TX initialisation of this port. */
    volatile uint16_t tx_inited;    /**< Set to 1 after mbuf pool iteration has run for this port. */
    uint16_t pid;                   /**< DPDK port ID. */
    uint16_t num_rx_qids;           /**< Number of Rx queues configured for this port. */
    uint16_t num_tx_qids;           /**< Number of Tx queues configured for this port. */
    uint16_t mtu_size;              /**< Configured MTU in bytes. */
    uint16_t cksum_requires_phdr;   /**< Non-zero if the NIC driver needs a pseudo-header for checksum offload. */
    uint64_t tx_cycles;             /**< TSC cycles between successive TX bursts at the configured rate. */
    uint64_t wire_size;             /**< Wire size of a single packet in bits (including overhead). */
    uint64_t pps;                   /**< Maximum achievable packets per second at current rate. */
    struct rte_mempool *rx_mp;      /**< Mempool used for Rx packet buffers. */
    struct rte_mempool *tx_mp;      /**< Mempool used for Tx packet buffers. */
    struct rte_eth_link link;       /**< Current link status (speed, duplex, etc.). */
    struct rte_ether_addr mac_addr; /**< Hardware MAC address of this port. */
    struct rte_eth_stats stats;     /**< Most-recently read hardware statistics. */
    struct rte_eth_stats pstats;    /**< Hardware statistics from the previous read. */
    pq_t pq[MAX_QUEUES_PER_PORT];   /**< Per-queue statistics for all queues on this port. */
} l2p_port_t;

/**
 * @brief Per-lcore port/queue assignment.
 *
 * Each worker lcore is associated with exactly one l2p_lport_t that records
 * which port and which Rx/Tx queue IDs that lcore is responsible for.
 */
typedef struct l2p_lport_s {
    uint16_t mode;    /**< Lcore operating mode: LCORE_MODE_RX, TX, or BOTH. */
    uint16_t lid;     /**< Lcore ID. */
    uint16_t rx_qid;  /**< Rx queue ID this lcore services. */
    uint16_t tx_qid;  /**< Tx queue ID this lcore services. */
    l2p_port_t *port; /**< Pointer to the port this lcore is attached to. */
} l2p_lport_t;

/**
 * @brief Top-level application state shared between all modules.
 *
 * Allocated once at startup and accessed via the global @c info pointer.
 */
typedef struct {
    volatile bool force_quit; /**< Set to true by the signal handler to request shutdown. */
    bool verbose;             /**< Enable verbose/debug output when true. */
    uint16_t num_lcores;      /**< Total number of worker lcores in use. */
    uint16_t num_ports;       /**< Total number of available Ethernet ports. */
    uint16_t num_mappings;    /**< Number of lcore-to-port/queue mappings configured. */

    /** @brief Per-lcore port assignment pointers, cache-line aligned. */
    l2p_lport_t *lports[RTE_MAX_LCORE] __rte_cache_aligned;
    /** @brief Per-port state array, cache-line aligned. */
    l2p_port_t ports[RTE_MAX_ETHPORTS] __rte_cache_aligned;
    char *mappings[MAX_MAPPINGS]; /**< Raw mapping strings from the command line. */

    /* Configuration values from command line options */
    uint32_t mbuf_count;     /**< Total number of mbufs to allocate (all pools). */
    uint32_t mbuf_size;      /**< Per-mbuf data buffer size in bytes. */
    uint16_t tx_rate;        /**< Target TX rate as a percentage of line rate (0–100). */
    uint16_t promiscuous_on; /**< Non-zero to enable promiscuous mode on all ports. */
    uint16_t jumbo_frame_on; /**< Non-zero to enable jumbo frame support. */
    uint16_t burst_count;    /**< Rx/Tx burst size in packets. */
    uint16_t pkt_size;       /**< Packet size in bytes including FCS. */
    uint16_t nb_rxd;         /**< Number of Rx descriptors per queue. */
    uint16_t nb_txd;         /**< Number of Tx descriptors per queue. */
    uint16_t timeout_secs;   /**< Statistics display interval in seconds. */
    uint16_t ip_proto;       /**< IP protocol for generated packets (IPPROTO_UDP or IPPROTO_TCP). */
    fgen_t *fgen;            /**< FGEN frame generator instance. */
    const char *fgen_file;   /**< Path to the FGEN frame definition file, or NULL. */
} txpkts_info_t;

/** @brief Global application state pointer; allocated in main() and freed on exit. */
extern txpkts_info_t *info;

/**
 * @brief Parse all command-line options and set up ports and mappings.
 *
 * @param argc
 *   Remaining argument count after DPDK EAL arguments have been consumed.
 * @param argv
 *   Remaining argument vector.
 * @return
 *   0 on success, EXIT_FAILURE on error.
 */
int parse_configuration(int argc, char **argv);

/**
 * @brief Calculate and update the TX cycle count and PPS for @p port.
 *
 * Uses the current link speed and the configured TX rate to compute
 * the number of TSC cycles between successive TX bursts.
 *
 * @param port
 *   Pointer to the port whose rate fields are to be updated.
 */
void packet_rate(l2p_port_t *port);

/**
 * @brief Clear the screen and print per-port, per-queue rate statistics.
 */
void print_stats(void);

/**
 * @brief Initialise and start a single Ethernet port.
 *
 * Configures queues, sets the MTU, enables promiscuous mode if requested,
 * and starts the device.
 *
 * @param port
 *   Pointer to the port descriptor to initialise.
 * @return
 *   0 on success, EXIT_FAILURE on error.
 */
int port_setup(l2p_port_t *port);

/**
 * @brief Build an IPv4/UDP or IPv4/TCP test packet directly into @p pkt.
 *
 * Fills Ethernet, IPv4, and transport headers with pseudo-random addresses
 * and computes checksums.  If an FGEN file is configured the function is a
 * no-op (frames are taken from the generator instead).
 *
 * @param lport
 *   Lcore/port context providing the source MAC address and TX queue ID.
 * @param pkt
 *   Pointer to the destination buffer (must be at least info->pkt_size bytes).
 * @param proto
 *   IP protocol: IPPROTO_UDP or IPPROTO_TCP.
 */
void packet_constructor(l2p_lport_t *lport, uint8_t *pkt, uint16_t proto);

/**
 * @brief Print usage information and exit.
 *
 * @param err
 *   Exit code passed to exit().
 */
void usage(int err);

/**
 * @brief Return the NUMA socket ID of the calling lcore.
 *
 * @return
 *   NUMA socket ID, or 0 if rte_socket_id() returns -1 (socket unknown).
 */
static __inline__ int
pg_socket_id(void)
{
    int sid = rte_socket_id();

    return (sid == -1) ? 0 : sid;
}

/**
 * @brief Return the NUMA socket ID of an Ethernet device.
 *
 * @param pid
 *   DPDK port ID of the Ethernet device.
 * @return
 *   NUMA socket ID, or 0 if the socket ID cannot be determined.
 */
static __inline__ int
pg_eth_dev_socket_id(int pid)
{
    int sid = rte_eth_dev_socket_id(pid);

    return (sid == -1) ? 0 : sid;
}

#ifdef __cplusplus
}
#endif
