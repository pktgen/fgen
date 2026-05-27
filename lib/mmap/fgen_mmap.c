/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2019-2026 Intel Corporation
 */

/**
 * @file
 * @brief FGEN hugepage mmap allocator implementation.
 *
 * Allocates anonymous memory-mapped regions backed by 4 KB, 2 MB, or
 * 1 GB huge pages, with automatic fall-back to smaller page sizes and
 * SIGBUS recovery for hugetlb quota violations.
 */

// IWYU pragma: no_include <bits/mman-map-flags-generic.h>

#include <signal.h>            // for sigaction, SIGBUS, sigaddset, sigemptyset
#include <stdbool.h>           // for bool, false, true
#include <string.h>            // for strerror, memset
#include <sys/mman.h>          // for munmap, MAP_FAILED, mmap, MAP_ANONYMOUS
#include <setjmp.h>            // for siglongjmp, sigjmp_buf, sigsetjmp
#include <fgen_common.h>       // for fgen_log2_u64, fgen_countof, FGEN_ALIGN_CEIL
#include <errno.h>             // for errno
#include <strings.h>           // for strcasecmp
#include <unistd.h>            // for getpagesize
#include <stdint.h>            // for uint64_t, uint32_t
#include <stdlib.h>            // for free, calloc

#include "mmap_private.h"        // for mmap_data
#include "fgen_mmap.h"            // for mmap_sizes_t, MMAP_HUGEPAGE_4KB, MMAP_HUGE...
#include "fgen_log.h"

#ifdef __clang__
/* clang doesn't have -Wclobbered */
#else
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wclobbered"
#endif

static mmap_stats_t mmap_stats;                        /**< Global allocation statistics */
static mmap_type_t mmap_default_type = MMAP_HUGEPAGE_4KB; /**< Default page size for new allocations */

static sigjmp_buf huge_jmpenv;               /**< Jump buffer for SIGBUS recovery */
static struct sigaction old_sigbus_action;   /**< Saved previous SIGBUS handler */
static bool restore_old_sigbus;              /**< True when old handler must be restored */

/**
 * @brief SIGBUS signal handler used during hugepage fault testing.
 *
 * When a mapping to a 2 MB or 1 GB page succeeds but access is denied
 * (e.g. hugetlb cgroup quota), the kernel sends SIGBUS.  This handler
 * jumps back to the recovery point in mmap_alloc() so a smaller page
 * size can be tried.
 *
 * @param signum
 *   Signal number (always SIGBUS; unused).
 */
static void
sigbus_handler(int signum __fgen_unused)
{
    siglongjmp(huge_jmpenv, -1);
}

/**
 * @brief Install the SIGBUS handler and record the old one for later restoration.
 */
static void
start_sigbus_handler(void)
{
    struct sigaction act;
    sigset_t mask;

    sigemptyset(&mask);
    sigaddset(&mask, SIGBUS);

    memset(&act, 0, sizeof(act));
    act.sa_handler = sigbus_handler;
    act.sa_mask    = mask;
    act.sa_flags   = 0;

    if (sigaction(SIGBUS, &act, &old_sigbus_action))
        FGEN_WARN("Failed to set SIGBUS action: %s\n", strerror(errno));
    else
        restore_old_sigbus = true;
}

/**
 * @brief Restore the previous SIGBUS handler if one was saved.
 */
static void
stop_sigbus_handler(void)
{
    if (restore_old_sigbus) {
        restore_old_sigbus = false;
        if (sigaction(SIGBUS, &old_sigbus_action, NULL))
            FGEN_WARN("Failed to restore old SIGBUS action: %s\n", strerror(errno));
    }
}

/** @brief Mapping table between human-readable page-size names and mmap_type_t values. */
static struct {
    const char *name; /**< Human-readable size string, e.g. "2MB" */
    mmap_type_t typ;  /**< Corresponding mmap_type_t constant */
} mmap_types[] = {
    // clang-format off
    { "4KB", MMAP_HUGEPAGE_4KB },
    { "2MB", MMAP_HUGEPAGE_2MB },
    { "1GB", MMAP_HUGEPAGE_1GB }
    // clang-format on
};

/**
 * @brief Compute the MAP_HUGETLB | MAP_HUGE_* flags for a given page size.
 *
 * @param page_sz
 *   Page size in bytes (e.g. 2*1024*1024 for 2 MB).
 * @return
 *   Combined mmap() flag bits for the requested huge-page size, or 0 for
 *   the default system page size.
 */
static int
pagesz_flags(uint64_t page_sz)
{
    /* as per mmap() manpage, all page sizes are log2 of page size
     * shifted by MAP_HUGE_SHIFT
     */
    int log2 = fgen_log2_u64(page_sz);

    /* Do not set use huge pages for default page size */
    if (page_sz == (uint64_t)getpagesize())
        return 0;
    return (log2 << MAP_HUGE_SHIFT) | MAP_HUGETLB;
}

/**
 * @brief Resolve a hugepage type string to its mmap_type_t value.
 *
 * @param htype
 *   Case-insensitive page-size name: "4KB", "2MB", "1GB", or "default".
 * @return
 *   The matching mmap_type_t, or the current default type if not found.
 */
mmap_type_t
mmap_type_by_name(const char *htype)
{
    if (htype && htype[0] != '\0') {
        if (!strcasecmp(htype, "default"))
            return mmap_default_type;

        for (int i = 0; i < fgen_countof(mmap_types); i++) {
            if (!strcasecmp(htype, mmap_types[i].name))
                return mmap_types[i].typ;
        }
    }

    return mmap_default_type;
}

/**
 * @brief Return the name string for a given mmap_type_t value.
 *
 * @param typ
 *   A valid mmap_type_t value.
 * @return
 *   Pointer to a constant string ("4KB", "2MB", or "1GB"), or the default
 *   type name if @p typ is out of range.
 */
const char *
mmap_name_by_type(mmap_type_t typ)
{
    if (typ < fgen_countof(mmap_types))
        return mmap_types[typ].name;

    return mmap_types[mmap_default_type].name;
}

/**
 * @brief Set the default hugepage type for future allocations.
 *
 * @param htype
 *   The mmap_type_t value to use as the new default.
 */
void
mmap_set_default(mmap_type_t htype)
{
    mmap_default_type = htype;
}

/**
 * @brief Set the default hugepage type by name string.
 *
 * @param name
 *   Case-insensitive page-size name, e.g. "2MB".  Unrecognised names
 *   leave the current default unchanged.
 */
void
mmap_set_default_by_name(const char *name)
{
    mmap_set_default(mmap_type_by_name(name));
}

/**
 * @brief Internal helper: call mmap() for a single page-size type.
 *
 * Fills in mm->typ, mm->align, and mm->sz, then calls mmap().
 *
 * @param mm
 *   Pointer to the mmap_data descriptor to populate.
 * @param typ
 *   The page-size type to attempt.
 * @return
 *   The mmap() return value (MAP_FAILED on error).
 */
static void *
__alloc_mem(struct mmap_data *mm, mmap_type_t typ)
{
    int flags = MAP_SHARED | MAP_ANONYMOUS | MAP_POPULATE;
    uint64_t len;

    mm->typ   = typ;
    mm->align = mmap_stats.sizes[typ].page_sz;

    len    = (uint64_t)mm->bufcnt * (uint64_t)mm->bufsz;
    mm->sz = FGEN_ALIGN_CEIL(len, mm->align);

    flags |= pagesz_flags(mmap_stats.sizes[typ].page_sz);

    /* map the segment, and populate page tables, the kernel fills
     * this segment with zeros if it's a new page.
     */
    return mmap(NULL, mm->sz, PROT_READ | PROT_WRITE, flags, -1, 0);
}

/**
 * @brief Allocate a memory-mapped region.
 *
 * @param bufcnt
 *   Number of buffers to accommodate in the region.
 * @param bufsz
 *   Size in bytes of each buffer.
 * @param typ
 *   Preferred hugepage backing type; the allocator falls back to smaller
 *   page sizes automatically.
 * @return
 *   Opaque mmap_t handle on success, or NULL on failure.
 */
mmap_t *
mmap_alloc(uint32_t bufcnt, uint32_t bufsz, mmap_type_t typ)
{
    struct mmap_data *mm;
    void *va;

    if (mmap_stats.inited == 0) {
        mmap_stats.inited = 1;

        mmap_stats.sizes[MMAP_HUGEPAGE_4KB].page_sz = getpagesize();
        mmap_stats.sizes[MMAP_HUGEPAGE_2MB].page_sz = (2 * 1024 * 1024);
        mmap_stats.sizes[MMAP_HUGEPAGE_1GB].page_sz = (1024 * 1024 * 1024);
    }

    if (typ < MMAP_HUGEPAGE_4KB || typ >= MMAP_HUGEPAGE_CNT)
        typ = MMAP_HUGEPAGE_4KB;

    mm = calloc(1, sizeof(struct mmap_data));
    if (!mm)
        FGEN_NULL_RET("Failed to allocate mmap_data structure\n");

    if (!bufcnt || !bufsz)
        FGEN_ERR_GOTO(leave, "bufcnt %u * bufsz %u is zero\n", bufcnt, bufsz);

    mm->bufcnt = bufcnt;
    mm->bufsz  = bufsz;

retry:
    /* Try the requested size and if not available, degrade to the next available size */
    switch (typ) {
    case MMAP_HUGEPAGE_1GB:
        va = __alloc_mem(mm, MMAP_HUGEPAGE_1GB);
        if (va != MAP_FAILED)
            break;
        FGEN_WARN("Failed to allocate %s hugepages, trying %s pages\n",
                 mmap_types[MMAP_HUGEPAGE_1GB].name, mmap_types[MMAP_HUGEPAGE_2MB].name);
        /* fall through */

    case MMAP_HUGEPAGE_2MB:
        va = __alloc_mem(mm, MMAP_HUGEPAGE_2MB);
        if (va != MAP_FAILED)
            break;
        FGEN_WARN("Failed to allocate %s hugepages, trying %s pages\n",
                 mmap_types[MMAP_HUGEPAGE_2MB].name, mmap_types[MMAP_HUGEPAGE_4KB].name);
        /* fall through */

    default:
    case MMAP_HUGEPAGE_4KB:
        va = __alloc_mem(mm, MMAP_HUGEPAGE_4KB);
        if (va == MAP_FAILED)
            FGEN_ERR_GOTO(leave, "Failed to allocate %s pages for %'ld bytes:\n    Error: %s\n",
                         mmap_types[MMAP_HUGEPAGE_4KB].name,
                         ((uint64_t)mm->bufcnt * (uint64_t)mm->bufsz), strerror(errno));
        break;
    }

    mm->addr = va;

    /* In linux, hugetlb limitations, like cgroup, are
     * enforced at fault time instead of mmap(), even
     * with the option of MAP_POPULATE. Kernel will send
     * a SIGBUS signal. To avoid to be killed, save stack
     * environment here, if SIGBUS happens, we can jump
     * back here.
     */
    if (sigsetjmp(huge_jmpenv, 1)) {
        stop_sigbus_handler();

        /* Unmap previous failing region before trying a new one */
        if (munmap(mm->addr, mm->sz))
            FGEN_ERR_GOTO(leave, "munmap(%p, %ld) failed: %s\n", mm->addr, mm->sz, strerror(errno));

        mm->addr = NULL;

        switch (mm->typ) {
        case MMAP_HUGEPAGE_1GB:
            FGEN_WARN("Failed to allocate %s hugepages, trying %s pages\n",
                     mmap_types[MMAP_HUGEPAGE_1GB].name, mmap_types[MMAP_HUGEPAGE_2MB].name);
            typ = MMAP_HUGEPAGE_2MB;
            break;
        case MMAP_HUGEPAGE_2MB:
            FGEN_WARN("Failed to allocate %s hugepages, trying %s pages\n",
                     mmap_types[MMAP_HUGEPAGE_2MB].name, mmap_types[MMAP_HUGEPAGE_4KB].name);
            typ = MMAP_HUGEPAGE_4KB;
            break;
        case MMAP_HUGEPAGE_4KB:
        default:
            FGEN_ERR_GOTO(leave, "Failed to allocate %s pages for %'ld bytes\n",
                         mmap_types[MMAP_HUGEPAGE_4KB].name,
                         (uint64_t)mm->bufcnt * (uint64_t)mm->bufsz);
        }

        goto retry;
    }

    /* we need to trigger a write to the page to enforce page fault and
     * ensure that page is accessible to us, but we can't overwrite value
     * that is already there, so read the old value, and write it back.
     * kernel populates the page with zeroes initially.
     */
    start_sigbus_handler();
    *(volatile int *)va = *(volatile int *)va;
    stop_sigbus_handler();

    mmap_stats.sizes[typ].allocated += mm->sz;
    mmap_stats.sizes[typ].num_allocated++;

    return (mmap_t *)mm;

leave:
    free(mm);
    return NULL;
}

/**
 * @brief Free a memory-mapped region created by mmap_alloc().
 *
 * @param _mm
 *   Opaque mmap_t handle returned by mmap_alloc(); NULL is silently ignored.
 * @return
 *   0 on success, -1 if munmap() fails (the handle is NOT freed in that case).
 */
int
mmap_free(mmap_t *_mm)
{
    struct mmap_data *mm = _mm;

    if (!mm)
        return 0;

    if (mm->typ < MMAP_HUGEPAGE_4KB || mm->typ >= MMAP_HUGEPAGE_CNT)
        FGEN_ERR_RET("mmap type is invalid %d\n", mm->typ);

    if (mm->addr && mm->sz) {
        if (munmap(mm->addr, mm->sz))
            /* Do not free mm if munmap fails so application can handle it */
            FGEN_ERR_RET("munmap(%p, %ld) failed: %s\n", mm->addr, mm->sz, strerror(errno));
        else {
            mmap_sizes_t *ss;

            ss = &mmap_stats.sizes[mm->typ];
            ss->freed += mm->sz;
            ss->num_freed++;
        }
    }

    free(mm);
    return 0;
}

/**
 * @brief Return a pointer at a byte offset into a mapped region.
 *
 * @param _mm
 *   Opaque mmap_t handle.
 * @param offset
 *   Byte offset from the start of the region; must be <= region size.
 * @return
 *   Pointer to the requested location, or NULL on error.
 */
void *
mmap_addr_at_offset(mmap_t *_mm, size_t offset)
{
    struct mmap_data *mm = _mm;

    if (mm && offset <= mmap_size(mm, NULL, NULL))
        return FGEN_PTR_ADD(mm->addr, offset);
    else
        return NULL;
}

/**
 * @brief Return the base address of a mapped region.
 *
 * @param _mm
 *   Opaque mmap_t handle.
 * @return
 *   Pointer to the start of the region, or NULL on error.
 */
void *
mmap_addr(mmap_t *_mm)
{
    return mmap_addr_at_offset(_mm, 0);
}

/**
 * @brief Query the real size and buffer parameters of a mapped region.
 *
 * @param _mm
 *   Opaque mmap_t handle.
 * @param bufcnt
 *   If non-NULL, receives the buffer count originally passed to mmap_alloc().
 * @param bufsz
 *   If non-NULL, receives the buffer size originally passed to mmap_alloc().
 * @return
 *   Actual mapped size in bytes (aligned to page boundary), or 0 on error.
 */
size_t
mmap_size(mmap_t *_mm, uint32_t *bufcnt, uint32_t *bufsz)
{
    struct mmap_data *mm = _mm;

    if (mm) {
        if (bufcnt)
            *bufcnt = mm->bufcnt;
        if (bufsz)
            *bufsz = mm->bufsz;
        return mm->sz;
    }

    return 0;
}

#ifdef __clang__
/* clang doesn't have -Wclobbered */
#else
#pragma GCC diagnostic pop
#endif
