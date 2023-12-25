/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023-2024 Intel Corporation
 */

/**
 * @file
 *
 * Generic, commonly-used macro and inline function definitions.
 */

#ifndef _FGEN_COMMON_H_
#define _FGEN_COMMON_H_

#include <stdint.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <unistd.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <linux/version.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef typeof
#define typeof __typeof__
#endif

#ifndef asm
#define asm __asm__
#endif

#define FGEN_VER_PREFIX       "FGEN"
#define FGEN_NAME_LEN         24
#define FGEN_CACHE_LINE_SIZE  64

/**
 * Helper routine to set a default value if x == y then x = z.
 *
 * @param x
 *   Value to set for default
 * @param y
 *   The default value to be tested for, if equal then set x = z
 * @param z
 *   The default value to be used
 * @return
 *   Sets the default value z, if x == y
 */
#define FGEN_DEFAULT_SET(x, y, z) \
    do {                         \
        if (x == y)              \
            x = z;               \
    } while ((0))

/**
 * Helper routine to set a max value if x > z then x = z.
 *
 * @param x
 *   Value to set
 * @param z
 *   The Max value to be used
 * @return
 *   Sets the default value z, if x > z
 */
#define FGEN_MAX_SET(x, z) \
    do {                  \
        if (x > z)        \
            x = z;        \
    } while ((0))

/** C extension macro for environments lacking C11 features. */
#if !defined(__STDC_VERSION__) || __STDC_VERSION__ < 201112L
#define FGEN_STD_C11 __extension__
#else
#define FGEN_STD_C11
#endif

typedef uint64_t unaligned_uint64_t;
typedef uint32_t unaligned_uint32_t;
typedef uint16_t unaligned_uint16_t;

#define FGEN_API __attribute__((visibility("default")))

#ifdef FGEN_BUILD_SHARED_LIBS
#define FUNCTION_VERSION(internal, api, ver) __asm__(".symver " #internal ", " #api "@" #ver)
#define DEFAULT_VERSION(internal, api, ver)  __asm__(".symver " #internal ", " #api "@@" #ver)
#else
#define FUNCTION_VERSION(internal, api, ver)
#define DEFAULT_VERSION(internal, api, ver)
#endif

/**
 * Compiler barrier.
 *
 * Guarantees that operation reordering does not occur at compile time
 * for operations directly before and after the barrier.
 */
#define fgen_compiler_barrier()           \
    do {                                 \
        asm volatile("" : : : "memory"); \
    } while (0)

/**
 * Force alignment
 */
#define __fgen_aligned(a) __attribute__((aligned(a)))

/**
 * Force a structure to be packed
 */
#define __fgen_packed __attribute__((__packed__))

/******* Macro to mark functions and fields scheduled for removal *****/
#define __fgen_deprecated __attribute__((__deprecated__))

/**
 * Mark a function or variable to a weak reference.
 */
#define __fgen_weak __attribute__((__weak__))

/*********** Macros to eliminate unused variable warnings ********/

/**
 * short definition to mark a function parameter unused
 */
#define __fgen_unused __attribute__((__unused__))

/**
 * definition to mark a variable or function parameter as used so
 * as to avoid a compiler warning
 */
#define FGEN_SET_USED(x) (void)(x)

#define fgen_roundup(_x, _y) ((((_x) + ((_y)-1)) / (_y)) * (_y))
#define fgen_ctz(_v)         __builtin_ctz(_v)
#define fgen_prefixbits(_v)  ((__typeof__(_v))(sizeof(_v) * 8) - fgen_ctz(_v))
#define fgen_numbytes(_v)    ((fgen_prefixbits(_v) + 7) / 8)

#define FGEN_PRIORITY_INIT   101
#define FGEN_PRIORITY_START  102
#define FGEN_PRIORITY_THREAD 103
#define FGEN_PRIORITY_STATE  110
#define FGEN_PRIORITY_CLASS  120
#define FGEN_PRIORITY_STACK  130
#define FGEN_PRIORITY_LAST   65535

#define FGEN_PRIO(prio) FGEN_PRIORITY_##prio

/**
 * Run function before main() with high priority.
 *
 * @param func
 *   Constructor function.
 * @param prio
 *   Priority number must be above 100.
 *   Lowest number is the first to run.
 */
#ifndef FGEN_INIT_PRIO /* Allow to override from FGEN */
#define FGEN_INIT_PRIO(func, prio) \
    static void __attribute__((constructor(FGEN_PRIO(prio)), used)) func(void)
#endif

/**
 * Run function before main() with low priority.
 *
 * The constructor will be run after prioritized constructors.
 *
 * @param func
 *   Constructor function.
 */
#define FGEN_INIT(func) FGEN_INIT_PRIO(func, LAST)

/**
 * Run after main() with low priority.
 *
 * @param func
 *   Destructor function name.
 * @param prio
 *   Priority number must be above 100.
 *   Lowest number is the last to run.
 */
#ifndef FGEN_FINI_PRIO /* Allow to override from FGEN */
#define FGEN_FINI_PRIO(func, prio) \
    static void __attribute__((destructor(FGEN_PRIO(prio)), used)) func(void)
#endif

/**
 * Run after main() with high priority.
 *
 * The destructor will be run *before* prioritized destructors.
 *
 * @param func
 *   Destructor function name.
 */
#define FGEN_FINI(func) FGEN_FINI_PRIO(func, LAST)

/**
 * Force a function to be inlined
 */
#define __fgen_always_inline inline __attribute__((__always_inline__))

/**
 * Force a function to be no inlined
 */
#define __fgen_noinline __attribute__((__noinline__))

/**
 * Force type to a new type
 */
#define FGEN_PTR_CAST(ptr, x) (x)((uintptr_t)(ptr))

/*********** Macros for pointer arithmetic ********/

/**
 * add a byte-value offset to a pointer
 */
#define FGEN_PTR_ADD(ptr, x) ((void *)((uintptr_t)(ptr) + (x)))

/**
 * subtract a byte-value offset from a pointer
 */
#define FGEN_PTR_SUB(ptr, x) ((void *)((uintptr_t)ptr - (x)))

/**
 * get the difference between two pointer values, i.e. how far apart
 * in bytes are the locations they point two. It is assumed that
 * ptr1 is greater than ptr2.
 */
#define FGEN_PTR_DIFF(ptr1, ptr2) ((uintptr_t)(ptr1) - (uintptr_t)(ptr2))

/**
 * Workaround to cast a const field of a structure to non-const type.
 */
#define FGEN_CAST_FIELD(var, field, type) \
    (*(type *)((uintptr_t)(var) + offsetof(typeof(*(var)), field)))

/*********** Macros/static functions for doing alignment ********/

/**
 * Macro to align a pointer to a given power-of-two. The resultant
 * pointer will be a pointer of the same type as the first parameter, and
 * point to an address no higher than the first parameter. Second parameter
 * must be a power-of-two value.
 */
#define FGEN_PTR_ALIGN_FLOOR(ptr, align) ((typeof(ptr))FGEN_ALIGN_FLOOR((uintptr_t)ptr, align))

/**
 * Macro to align a value to a given power-of-two. The resultant value
 * will be of the same type as the first parameter, and will be no
 * bigger than the first parameter. Second parameter must be a
 * power-of-two value.
 */
#define FGEN_ALIGN_FLOOR(val, align) (typeof(val))((val) & (~((typeof(val))((align)-1))))

/**
 * Macro to align a pointer to a given power-of-two. The resultant
 * pointer will be a pointer of the same type as the first parameter, and
 * point to an address no lower than the first parameter. Second parameter
 * must be a power-of-two value.
 */
#define FGEN_PTR_ALIGN_CEIL(ptr, align) \
    FGEN_PTR_ALIGN_FLOOR((typeof(ptr))FGEN_PTR_ADD(ptr, (align)-1), align)

/**
 * Macro to align a value to a given power-of-two. The resultant value
 * will be of the same type as the first parameter, and will be no lower
 * than the first parameter. Second parameter must be a power-of-two
 * value.
 */
#define FGEN_ALIGN_CEIL(val, align) FGEN_ALIGN_FLOOR(((val) + ((typeof(val))(align)-1)), align)

/**
 * Macro to align a pointer to a given power-of-two. The resultant
 * pointer will be a pointer of the same type as the first parameter, and
 * point to an address no lower than the first parameter. Second parameter
 * must be a power-of-two value.
 * This function is the same as FGEN_PTR_ALIGN_CEIL
 */
#define FGEN_PTR_ALIGN(ptr, align) FGEN_PTR_ALIGN_CEIL(ptr, align)

/**
 * Macro to align a value to a given power-of-two. The resultant
 * value will be of the same type as the first parameter, and
 * will be no lower than the first parameter. Second parameter
 * must be a power-of-two value.
 * This function is the same as FGEN_ALIGN_CEIL
 */
#define FGEN_ALIGN(val, align) FGEN_ALIGN_CEIL(val, align)

/**
 * Macro to align a value to the multiple of given value. The resultant
 * value will be of the same type as the first parameter and will be no lower
 * than the first parameter.
 */
#define FGEN_ALIGN_MUL_CEIL(v, mul) \
    (((v + (typeof(v))(mul)-1) / ((typeof(v))(mul))) * (typeof(v))(mul))

/**
 * Macro to align a value to the multiple of given value. The resultant
 * value will be of the same type as the first parameter and will be no higher
 * than the first parameter.
 */
#define FGEN_ALIGN_MUL_FLOOR(v, mul) ((v / ((typeof(v))(mul))) * (typeof(v))(mul))

/**
 * Macro to align value to the nearest multiple of the given value.
 * The resultant value might be greater than or less than the first parameter
 * whichever difference is the lowest.
 */
#define FGEN_ALIGN_MUL_NEAR(v, mul)                     \
    ({                                                 \
        typeof(v) ceil  = FGEN_ALIGN_MUL_CEIL(v, mul);  \
        typeof(v) floor = FGEN_ALIGN_MUL_FLOOR(v, mul); \
        (ceil - v) > (v - floor) ? floor : ceil;       \
    })

/**
 * Checks if a pointer is aligned to a given power-of-two value
 *
 * @param ptr
 *   The pointer whose alignment is to be checked
 * @param align
 *   The power-of-two value to which the ptr should be aligned
 *
 * @return
 *   True(1) where the pointer is correctly aligned, false(0) otherwise
 */
static inline int
fgen_is_aligned(void *ptr, unsigned align)
{
    return FGEN_PTR_ALIGN(ptr, (uintptr_t)align) == ptr;
}

/*********** Macros for compile type checks ********/

/**
 * Triggers an error at compilation time if the condition is true.
 */
#define FGEN_BUILD_BUG_ON(condition) ((void)sizeof(char[1 - 2 * !!(condition)]))

/*********** Cache line related macros ********/

/** Cache line mask. */
#define FGEN_CACHE_LINE_MASK (FGEN_CACHE_LINE_SIZE - 1)

/** Return the first cache-aligned value greater or equal to size. */
#define FGEN_CACHE_LINE_ROUNDUP(size) \
    (FGEN_CACHE_LINE_SIZE * ((size + FGEN_CACHE_LINE_SIZE - 1) / FGEN_CACHE_LINE_SIZE))

/** Cache line size in terms of log2 */
#if FGEN_CACHE_LINE_SIZE == 64
#define FGEN_CACHE_LINE_SIZE_LOG2 6
#else
#error "Unsupported cache line size"
#endif

/** Minimum Cache line size. */
#define FGEN_CACHE_LINE_MIN_SIZE 64

/** Force alignment to cache line. */
#define __fgen_cache_aligned __fgen_aligned(FGEN_CACHE_LINE_SIZE)

/** Force minimum cache line alignment. */
#define __fgen_cache_min_aligned __fgen_aligned(FGEN_CACHE_LINE_MIN_SIZE)

/** Generic marker for any place in a structure. */
__extension__ typedef void *FGEN_MARKER[0];
/** Marker for 1B alignment in a structure. */
__extension__ typedef uint8_t FGEN_MARKER8[0];
/** Marker for 2B alignment in a structure. */
__extension__ typedef uint16_t FGEN_MARKER16[0];
/** Marker for 4B alignment in a structure. */
__extension__ typedef uint32_t FGEN_MARKER32[0];
/** Marker for 8B alignment in a structure. */
__extension__ typedef uint64_t FGEN_MARKER64[0];

/**
 * Combines 32b inputs most significant set bits into the least
 * significant bits to construct a value with the same MSBs as x
 * but all 1's under it.
 *
 * @param x
 *    The integer whose MSBs need to be combined with its LSBs
 * @return
 *    The combined value.
 */
static inline uint32_t
fgen_combine32ms1b(uint32_t x)
{
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;

    return x;
}

/**
 * Combines 64b inputs most significant set bits into the least
 * significant bits to construct a value with the same MSBs as x
 * but all 1's under it.
 *
 * @param v
 *    The integer whose MSBs need to be combined with its LSBs
 * @return
 *    The combined value.
 */
static inline uint64_t
fgen_combine64ms1b(uint64_t v)
{
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v |= v >> 32;

    return v;
}

/*********** Macros to work with powers of 2 ********/

/**
 * Macro to return 1 if n is a power of 2, 0 otherwise
 */
#define FGEN_IS_POWER_OF_2(n) ((n) && !(((n)-1) & (n)))

/**
 * Returns true if n is a power of 2
 * @param n
 *     Number to check
 * @return 1 if true, 0 otherwise
 */
static inline int
fgen_is_power_of_2(uint32_t n)
{
    return n && !(n & (n - 1));
}

/**
 * Aligns input parameter to the next power of 2
 *
 * @param x
 *   The integer value to align
 *
 * @return
 *   Input parameter aligned to the next power of 2
 */
static inline uint32_t
fgen_align32pow2(uint32_t x)
{
    x--;
    x = fgen_combine32ms1b(x);

    return x + 1;
}

/**
 * Aligns input parameter to the previous power of 2
 *
 * @param x
 *   The integer value to align
 *
 * @return
 *   Input parameter aligned to the previous power of 2
 */
static inline uint32_t
fgen_align32prevpow2(uint32_t x)
{
    x = fgen_combine32ms1b(x);

    return x - (x >> 1);
}

/**
 * Aligns 64b input parameter to the next power of 2
 *
 * @param v
 *   The 64b value to align
 *
 * @return
 *   Input parameter aligned to the next power of 2
 */
static inline uint64_t
fgen_align64pow2(uint64_t v)
{
    v--;
    v = fgen_combine64ms1b(v);

    return v + 1;
}

/**
 * Aligns 64b input parameter to the previous power of 2
 *
 * @param v
 *   The 64b value to align
 *
 * @return
 *   Input parameter aligned to the previous power of 2
 */
static inline uint64_t
fgen_align64prevpow2(uint64_t v)
{
    v = fgen_combine64ms1b(v);

    return v - (v >> 1);
}

/*********** Macros for calculating min and max **********/

/**
 * Macro to return the minimum of two numbers
 */
#define FGEN_MIN(a, b)       \
    __extension__({         \
        typeof(a) _a = (a); \
        typeof(b) _b = (b); \
        _a < _b ? _a : _b;  \
    })

/**
 * Macro to return the maximum of two numbers
 */
#define FGEN_MAX(a, b)       \
    __extension__({         \
        typeof(a) _a = (a); \
        typeof(b) _b = (b); \
        _a > _b ? _a : _b;  \
    })

/*********** Other general functions / macros ********/

/**
 * Searches the input parameter for the least significant set bit
 * (starting from zero).
 * If a least significant 1 bit is found, its bit index is returned.
 * If the content of the input parameter is zero, then the content of the return
 * value is undefined.
 * @param v
 *     input parameter, should not be zero.
 * @return
 *     least significant set bit in the input parameter.
 */
static inline uint32_t
fgen_bsf32(uint32_t v)
{
    return (uint32_t)__builtin_ctz(v);
}

/**
 * Searches the input parameter for the least significant set bit
 * (starting from zero). Safe version (checks for input parameter being zero).
 *
 * @warning ``pos`` must be a valid pointer. It is not checked!
 *
 * @param v
 *     The input parameter.
 * @param pos
 *     If ``v`` was not 0, this value will contain position of least significant
 *     bit within the input parameter.
 * @return
 *     Returns 0 if ``v`` was 0, otherwise returns 1.
 */
static inline int
fgen_bsf32_safe(uint64_t v, uint32_t *pos)
{
    if (v == 0)
        return 0;

    *pos = fgen_bsf32(v);
    return 1;
}

/**
 * Return the rounded-up log2 of a integer.
 *
 * @param v
 *     The input parameter.
 * @return
 *     The rounded-up log2 of the input, or 0 if the input is 0.
 */
static inline uint32_t
fgen_log2_u32(uint32_t v)
{
    if (v == 0)
        return 0;
    v = fgen_align32pow2(v);
    return fgen_bsf32(v);
}

/**
 * Return the last (most-significant) bit set.
 *
 * @note The last (most significant) bit is at position 32.
 * @note fgen_fls_u32(0) = 0, fgen_fls_u32(1) = 1, fgen_fls_u32(0x80000000) = 32
 *
 * @param x
 *     The input parameter.
 * @return
 *     The last (most-significant) bit set, or 0 if the input is 0.
 */
static inline int
fgen_fls_u32(uint32_t x)
{
    return (x == 0) ? 0 : 32 - __builtin_clz(x);
}

/**
 * Searches the input parameter for the least significant set bit
 * (starting from zero).
 * If a least significant 1 bit is found, its bit index is returned.
 * If the content of the input parameter is zero, then the content of the return
 * value is undefined.
 * @param v
 *     input parameter, should not be zero.
 * @return
 *     least significant set bit in the input parameter.
 */
static inline int
fgen_bsf64(uint64_t v)
{
    return (uint32_t)__builtin_ctzll(v);
}

/**
 * Searches the input parameter for the least significant set bit
 * (starting from zero). Safe version (checks for input parameter being zero).
 *
 * @warning ``pos`` must be a valid pointer. It is not checked!
 *
 * @param v
 *     The input parameter.
 * @param pos
 *     If ``v`` was not 0, this value will contain position of least significant
 *     bit within the input parameter.
 * @return
 *     Returns 0 if ``v`` was 0, otherwise returns 1.
 */
static inline int
fgen_bsf64_safe(uint64_t v, uint32_t *pos)
{
    if (v == 0)
        return 0;

    *pos = fgen_bsf64(v);
    return 1;
}

/**
 * Return the last (most-significant) bit set.
 *
 * @note The last (most significant) bit is at position 64.
 * @note fgen_fls_u64(0) = 0, fgen_fls_u64(1) = 1,
 *       fgen_fls_u64(0x8000000000000000) = 64
 *
 * @param x
 *     The input parameter.
 * @return
 *     The last (most-significant) bit set, or 0 if the input is 0.
 */
static inline int
fgen_fls_u64(uint64_t x)
{
    return (x == 0) ? 0 : 64 - __builtin_clzll(x);
}

/**
 * Return the rounded-up log2 of a 64-bit integer.
 *
 * @param v
 *     The input parameter.
 * @return
 *     The rounded-up log2 of the input, or 0 if the input is 0.
 */
static inline uint32_t
fgen_log2_u64(uint64_t v)
{
    if (v == 0)
        return 0;
    v = fgen_align64pow2(v);
    /* we checked for v being 0 already, so no undefined behavior */
    return fgen_bsf64(v);
}

#ifndef offsetof
/** Return the offset of a field in a structure. */
#define offsetof(TYPE, MEMBER) __builtin_offsetof(TYPE, MEMBER)
#endif

/**
 * Return pointer to the wrapping struct instance.
 *
 * Example:
 *
 *  struct wrapper {
 *      ...
 *      struct child c;
 *      ...
 *  };
 *
 *  struct child *x = obtain(...);
 *  struct wrapper *w = container_of(x, struct wrapper, c);
 */
#ifndef container_of
#define container_of(ptr, type, member)                            \
    __extension__({                                                \
        const typeof(((type *)0)->member) *_ptr   = (ptr);         \
        __attribute__((unused)) type *_target_ptr = (type *)(ptr); \
        (type *)(((uintptr_t)_ptr) - offsetof(type, member));      \
    })
#endif

/**
 * Get the size of a field in a structure.
 *
 * @param type
 *   The type of the structure.
 * @param field
 *   The field in the structure.
 * @return
 *   The size of the field in the structure, in bytes.
 */
#define FGEN_SIZEOF_FIELD(type, field) (sizeof(((type *)0)->field))

#define _FGEN_STR(x) #x
/** Take a macro value and get a string version of it */
#define FGEN_STR(x) _FGEN_STR(x)

/**
 * ISO C helpers to modify format strings using variadic macros.
 * This is a replacement for the ", ## __VA_ARGS__" GNU extension.
 * An empty %s argument is appended to avoid a dangling comma.
 */
#define FGEN_FMT(fmt, ...)      fmt "%.0s", __VA_ARGS__ ""
#define FGEN_FMT_HEAD(fmt, ...) fmt
#define FGEN_FMT_TAIL(fmt, ...) __VA_ARGS__

/** Mask value of type "tp" for the first "ln" bit set. */
#define FGEN_LEN2MASK(ln, tp) ((tp)((uint64_t)-1 >> (sizeof(uint64_t) * CHAR_BIT - (ln))))

/** Number of elements in the array. */
#define FGEN_DIM(a)     (int)(sizeof(a) / sizeof((a)[0]))
#define fgen_countof(a) FGEN_DIM(a)

/**
 * Converts a numeric string to the equivalent uint64_t value.
 * As well as straight number conversion, also recognizes the suffixes
 * k, m and g for kilobytes, megabytes and gigabytes respectively.
 *
 * If a negative number is passed in  i.e. a string with the first non-black
 * character being "-", zero is returned. Zero is also returned in the case of
 * an error with the strtoull call in the function.
 *
 * @param str
 *     String containing number to convert.
 * @return
 *     Number.
 */
static inline uint64_t
fgen_str_to_size(const char *str)
{
    char *endptr;
    unsigned long long size;

    while (isspace((int)*str))
        str++;
    if (*str == '-')
        return 0;

    errno = 0;
    size  = strtoull(str, &endptr, 0);
    if (errno)
        return 0;

    if (*endptr == ' ')
        endptr++; /* allow 1 space gap */

    switch (*endptr) {
    case 'G':
    case 'g':
        size *= 1024; /* fall-through */
    case 'M':
    case 'm':
        size *= 1024; /* fall-through */
    case 'K':
    case 'k':
        size *= 1024; /* fall-through */
    default:
        break;
    }
    return size;
}

#ifdef __cplusplus
}
#endif

#endif
