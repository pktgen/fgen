/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023-2024 Intel Corporation
 */

#ifndef __FGEN_H
#define __FGEN_H

#include <stdint.h>
#include <sys/queue.h>

#ifdef __cplusplus
extern "C" {
#endif

#include <salloc.h>

typedef void fgen_decode_t;

#include <fgen_mmap.h>

enum {
    FGEN_MAX_STRING_LENGTH = 4096, /**< Maximum string length for text string */
    FGEN_MAX_LAYERS        = 32,   /**< Maximum number of layers in the text string */
    FGEN_MAX_PARAMS        = 16,   /**< Maximum number of parameters in the text string */
    FGEN_MAX_KVP_TOKENS    = 4,    /**< Maximum number of tokens in a key/value pair + 1 */
    FGEN_FILLER_PATTERN    = '%',  /**< Filler pattern byte value */
    FGEN_FRAME_NAME_LENGTH = 32,   /**< Frame name length */
    FGEN_MAX_FSTR_LEN      = 4096, /**< Maximum number of bytes in the fgen string */
    FGEN_ETHER_MTU         = 1518, /**< Ethernet frame MTU */
    FGEN_ETHER_JUMBO_MTU   = 9216, /**< Ethernet Jumbo frame MTU */
    FGEN_EXTRA_SPACE       = 64,   /**< Extra space in the fgen string */
    FGEN_BUF_COUNT         = 10,   /**< Buffer count */
    FGEN_BUF_SIZE          = 1024, /**< Buffer size */
    FGEN_ALLOC_SIZE        = 8192, /**< Allocation size */
    FGEN_MAX_FRAME_SIZE    = (FGEN_BUF_COUNT * FGEN_BUF_SIZE), /**< Maximum buffer size */
};

typedef enum {
    FGEN_ETHER_TYPE,     /**< Ethernet type layer */
    FGEN_DOT1Q_TYPE,     /**< Dot1Q type layer */
    FGEN_DOT1AD_TYPE,    /**< Dot1AD type layer */
    FGEN_IPV4_TYPE,      /**< IPv4 type layer */
    FGEN_IPV6_TYPE,      /**< IPv6 type layer */
    FGEN_UDP_TYPE,       /**< UDP type layer */
    FGEN_TCP_TYPE,       /**< TCP type layer */
    FGEN_VXLAN_TYPE,     /**< VxLan type layer */
    FGEN_ECHO_TYPE,      /**< ECHO type layer */
    FGEN_TSC_TYPE,       /**< Timestamp type layer */
    FGEN_RAW_TYPE,       /**< Raw type layer */
    FGEN_PAYLOAD_TYPE,   /**< Payload type layer */
    FGEN_TYPE_COUNT,     /**< Number of layers total */
    FGEN_ERROR_TYPE = -1 /**< Error type */
} opt_type_t;

/**< The set of strings matching the opt_type_t structure */
#define FGEN_ETHER_STR   "Ether"
#define FGEN_DOT1Q_STR   "Dot1q"
#define FGEN_DOT1AD_STR  "Dot1ad"
#define FGEN_IPv4_STR    "IPv4"
#define FGEN_IPv6_STR    "IPv6"
#define FGEN_UDP_STR     "UDP"
#define FGEN_TCP_STR     "TCP"
#define FGEN_VxLAN_STR   "Vxlan"
#define FGEN_ECHO_STR    "Echo"
#define FGEN_TSC_STR     "TSC"
#define FGEN_RAW_STR     "Raw"
#define FGEN_PAYLOAD_STR "Payload"

// clang-format off
#define FGEN_TYPE_STRINGS \
    {                     \
        FGEN_ETHER_STR,   \
        FGEN_DOT1Q_STR,   \
        FGEN_DOT1AD_STR,  \
        FGEN_IPv4_STR,    \
        FGEN_IPv6_STR,    \
        FGEN_UDP_STR,     \
        FGEN_TCP_STR,     \
        FGEN_VxLAN_STR,   \
        FGEN_ECHO_STR,    \
        FGEN_TSC_STR,     \
        FGEN_RAW_STR,     \
        FGEN_PAYLOAD_STR, \
        NULL              \
    }
// clang-format on

#define FGEN_DONE_TYPE FGEN_TYPE_COUNT /**< A parsing done flag */

/* Forward declarations */
struct fgen_s;
struct fopt_s;
struct frame_s;

typedef int (*fgen_fn_t)(struct frame_s *f, int lidx);

typedef struct ftable_s {
    opt_type_t typ;  /**< Type of layer */
    const char *str; /**< Name of the layer and string for comparing */
    fgen_fn_t fn;    /**< Routine to call for a given layer */
} ftable_t;

typedef struct fopt_s {
    opt_type_t typ;  /**< Type of layer */
    uint16_t offset; /**< Offset into the buffer for this layer */
    uint16_t length; /**< Length of the layer */
    char *param_str; /**< Parameter string for the layer */
    ftable_t *tbl;   /**< The table containing the layer parsing routine */
} fopt_t;

typedef struct proto_s {
    uint16_t offset; /**< Offset to the protocol header in buffer */
    uint16_t length; /**< Length of the protocol header in buffer */
} proto_t;

struct fgen_s;

typedef struct frame_s {
    TAILQ_ENTRY(frame_s) next; /**< Frame list entry */
    char *name;                /**< Name of the frame, this string allocated by strdup() */
    char *fstr;                /**< Frame text string, this string allocated by strdup() */
    struct fgen_s *fg;         /**< Pointer to the fgen_t structure */
    uint32_t data_off;         /**< Frame offset into buffer space */
    uint16_t data_len;         /**< Total length of frame */
    uint16_t tsc_off;          /**< Offset to the Timestamp */
    uint16_t port;             /**< Port number */
    proto_t l2;                /**< Information about L2 header */
    proto_t l3;                /**< Information about L3 header */
    proto_t l4;                /**< Information about L4 header */
} frame_t;

typedef struct fgen_s {
    TAILQ_HEAD(frame_head, frame_s) head; /**< Frame list head */
    uint32_t flags;                       /**< Flags for debugging and parsing the text */
    uint32_t nb_frames;                   /**< Number of frames */
    salloc_t *salloc;                     /**< Allocator for frame_t structures */
    uint16_t nb_layers;                   /**< Number of layers in the text string */
    fopt_t opts[FGEN_MAX_LAYERS];         /**< The option information one for each layer */
    char *layers[FGEN_MAX_LAYERS];        /**< information about each layer */
    char *params[FGEN_MAX_PARAMS];        /**< Parameters for each layer */
} fgen_t;

enum {
    FGEN_VERBOSE   = (1 << 0), /**< Debug flag to enable verbose output */
    FGEN_DUMP_DATA = (1 << 1), /**< Debug flag to hexdump the data */
};

/**
 * Return the packet data length.
 *
 * @param f
 *   The frame_t structure pointer.
 */
#define fbuf_data_len(f) (f)->data_len

/**
 * Return the packet data pointer at the given offset.
 *
 * @param f
 *   The frame_t structure pointer.
 * @param t
 *   The pointer type cast.
 * @param o
 *   The offset into the packet.
 */
#define fbuf_mtod_offset(f, t, o) (t)((char *)salloc_ptr((f)->fg->salloc, (f)->data_off) + (o))

/**
 * Return the first byte address of the packet data.
 *
 * @param f
 *   The frame_t structure pointer.
 * @param t
 *   The pointer type cast.
 */
#define fbuf_mtod(f, t) fbuf_mtod_offset(f, t, 0)

/**
 * Current number of frames in the frame list
 *
 * @param fg
 *   The fgen_t structure pointer.
 * @return
 *   The number of frames in the frame list.
 */
#define fgen_fcnt(fg) (fg)->nb_frames

/**
 * Create a frame generator object with number of frames and size of each frame.
 *
 * @param flags
 *   Flags used for debugging the frame generator object.
 *   FGEN_VERBOSE, FGEN_DUMP_DATA, ...
 * @return
 *   NULL on error or Pointer to fgen_t structure on success.
 */
FGEN_API fgen_t *fgen_create(int flags);

/**
 * Destroy the frame generator object.
 *
 * @param fg
 *   The fgen_t pointer returned from fgen_create()
 */
FGEN_API void fgen_destroy(fgen_t *fg);

/**
 * Load a fgen text file and grab the fgen frame strings/names.
 *
 * @param fg
 *   The fgen_t pointer returned from fgen_create()
 * @param filename
 *   The name of the file to load, must be a fgen text file
 * @param nb_files
 *   The number of files to load.
 * @return
 *   -1 on error or number of frames loaded
 */
FGEN_API int fgen_load_files(fgen_t *fg, char **files, int nb_files);

/**
 * Load a fgen string array and create a fgen_file_t structure pointer.
 *
 * @param fg
 *   The fgen_t pointer returned from fgen_create()
 * @param frames
 *   The array of fgen text strings
 * @param nb_frames
 *   The number of entries in the array. The len can be zero, but the array
 *   must be NULL terminated.
 * @return
 *   -1 on error or number of frames loaded
 */
FGEN_API int fgen_load_strings(fgen_t *fg, char **frames, int nb_frames);

/**
 * Parse the fgen string to generate the frame and add to the list.
 *
 * @param fg
 *   The fgen_t pointer returned from fgen_create()
 * @param name
 *   The name of the frame text to create.
 * @param text
 *   The text string to parse to generate the frame data.
 * @return
 *   0 on success or -1 on error.
 */
FGEN_API int fgen_add_frame(fgen_t *fg, const char *name, const char *text);

/**
 * Print out a fgen frame text string
 *
 * @param msg
 *   A header message, can be NULL.
 * @param fpk5
 *   The frame_t pointer to print.
 */
FGEN_API void fgen_print_frame(const char *msg, frame_t *fpkt);

/**
 * Create a decode raw packet into a frame string setup routine.
 *
 * @return
 *   NULL on error or pointer to fgen_decode_t structure.
 */
FGEN_API fgen_decode_t *fgen_decode_create(void);

/**
 * Decode the raw packet data into a string
 *
 * @param dc
 *   The fgen_decode_t structure pointer.
 * @param data
 *   The frame data pointer.
 * @param len
 *   The length of the data to decode.
 * @param opt
 *   The starting protocol to decode the packet, i.e. FGEN_ETHER_TYPE or FGEN_IPV4_TYPE
 * @return
 *   -1 on error or length of decoded string.
 */
FGEN_API int fgen_decode(fgen_decode_t *dc, void *data, uint16_t len, opt_type_t opt);

/**
 * Free the unparse information.
 *
 * @param dc
 *   The fgen_decode_t pointer to free.
 */
FGEN_API void fgen_decode_destroy(fgen_decode_t *dc);

/**
 * Return the buffer of the decoded packet.
 *
 * @param dc
 *   The fgen_decode_t pointer to free.
 * @return
 *   The buffer of the decoded packet or NULL on error
 */
FGEN_API const char *fgen_decode_text(fgen_decode_t *dc);

/**
 * Decode a raw hex dump like string into a binary format.
 *
 * @param text
 *   The raw hex dump text string, must be NULL terminated.
 * @param buffer
 *   The buffer to place the decoded data.
 * @param len
 *   The length of the decode buffer.
 * @return
 *   -1 on error or number of decoded bytes in the buffer.
 */
FGEN_API int fgen_decode_string(const char *text, uint8_t *buffer, int len);

/**
 * Print out the FGEN text string in layers.
 *
 * @param msg
 *   A message to print before printout.
 * @param text
 *   The FGEN string to print.
 */
FGEN_API void fgen_print_string(const char *msg, const char *text);

/**
 * Find a frame in the frame list by name.
 *
 * @param fg
 *   The fgen_t pointer returned from fgen_create().
 * @param name
 *   The name of the frame to find.
 * @return
 *  The frame_t pointer or NULL if not found.
 */
FGEN_API frame_t *fgen_find_frame(fgen_t *fg, const char *name);

/**
 * Find next frame in the frame
 *
 * @param fg
 *   The fgen_t pointer returned from fgen_create().
 * @param prev
 *   Previous frame pointer
 * @return
 *  The frame_t pointer or NULL if not found.
 */
FGEN_API frame_t *fgen_next_frame(fgen_t *fg, frame_t *prev);

#ifdef __cplusplus
}
#endif

#endif /* __FGEN_H */
