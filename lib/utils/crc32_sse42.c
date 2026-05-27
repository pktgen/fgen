/**
 * @file
 * @brief SSE 4.2 hardware-accelerated CRC-32C implementation.
 *
 * Derived from crc32c.c version 1.1 by Mark Adler.  Uses the Intel
 * CRC32 instruction for three-way parallel throughput and GF(2) matrix
 * exponentiation to combine the partial results.  All original copyright
 * and licence terms are preserved exactly.
 */

/*
 * Derived from crc32c.c version 1.1 by Mark Adler.
 *
 * Copyright (c) 2013 Mark Adler
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * In no event will the author be held liable for any damages arising from the
 * use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 * Mark Adler
 * madler@alumni.caltech.edu
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/**
 * This file is compiled in userspace in order to run ATF unit tests.
 */
#ifdef USERSPACE_TESTING
#include <stdint.h>
#include <stdlib.h>
#else
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#endif

/**
 * @brief Apply the CRC32 instruction to one byte using the CRC-32C polynomial.
 *
 * @param x
 *   Running 32-bit CRC accumulator.
 * @param y
 *   Next input byte to fold in.
 * @return
 *   Updated CRC accumulator.
 */
static __inline uint32_t
_mm_crc32_u8(uint32_t x, uint8_t y)
{
    /*
     * clang (at least 3.9.[0-1]) pessimizes "rm" (y) and "m" (y)
     * significantly and "r" (y) a lot by copying y to a different
     * local variable (on the stack or in a register), so only use
     * the latter.  This costs a register and an instruction but
     * not a uop.
     */
    __asm("crc32b %1,%0" : "+r"(x) : "r"(y));

    return (x);
}

#ifdef __amd64__
/**
 * @brief Apply the CRC32 instruction to one 64-bit word (amd64 only).
 *
 * @param x
 *   Running 64-bit CRC accumulator.
 * @param y
 *   Next 64-bit word to fold in.
 * @return
 *   Updated CRC accumulator.
 */
static __inline uint64_t
_mm_crc32_u64(uint64_t x, uint64_t y)
{
    __asm("crc32q %1,%0" : "+r"(x) : "r"(y));
    return (x);
}
#else
/**
 * @brief Apply the CRC32 instruction to one 32-bit word (32-bit x86 only).
 *
 * @param x
 *   Running 32-bit CRC accumulator.
 * @param y
 *   Next 32-bit word to fold in.
 * @return
 *   Updated CRC accumulator.
 */
static __inline uint32_t
_mm_crc32_u32(uint32_t x, uint32_t y)
{
    __asm("crc32l %1,%0" : "+r"(x) : "r"(y));
    return (x);
}
#endif

/* CRC-32C (iSCSI) polynomial in reversed bit order. */
#define POLY 0x82f63b78

/**
 * Block sizes for three-way parallel crc computation.  LONG and SHORT must
 * both be powers of two.
 */
#define LONG  128
#define SHORT 64

/**
 * Tables for updating a crc for LONG, 2 * LONG, SHORT and 2 * SHORT bytes
 * of value 0 later in the input stream, in the same way that the hardware
 * would, but in software without calculating intermediate steps.
 */
static uint32_t crc32c_long[4][256];
static uint32_t crc32c_2long[4][256];
static uint32_t crc32c_short[4][256];
static uint32_t crc32c_2short[4][256];

/**
 * @brief Multiply a GF(2) matrix by a vector.
 *
 * Each element of the matrix and vector is one bit of an unsigned integer.
 * @p mat must contain at least as many rows as the position of the most
 * significant set bit in @p vec.
 *
 * @param mat
 *   Pointer to the matrix rows (32 uint32_t values).
 * @param vec
 *   Input vector as a bit-field.
 * @return
 *   Result of mat * vec over GF(2).
 */
static inline uint32_t
gf2_matrix_times(uint32_t *mat, uint32_t vec)
{
    uint32_t sum;

    sum = 0;
    while (vec) {
        if (vec & 1)
            sum ^= *mat;
        vec >>= 1;
        mat++;
    }
    return (sum);
}

/**
 * @brief Compute the square of a 32x32 GF(2) matrix.
 *
 * @param square
 *   Output: receives mat * mat over GF(2); must point to 32 uint32_t values.
 * @param mat
 *   Input matrix; must point to 32 uint32_t values.
 */
static inline void
gf2_matrix_square(uint32_t *square, uint32_t *mat)
{
    int n;

    for (n = 0; n < 32; n++)
        square[n] = gf2_matrix_times(mat, mat[n]);
}

/**
 * @brief Build the GF(2) operator that advances a CRC by @p len zero bytes.
 *
 * @p len should be a power of two.  If it is not, the result is the same as
 * for the largest power of two less than @p len.  The result for len == 0 is
 * the same as for len == 1.
 *
 * @param even
 *   Output: 32-element array that receives the computed operator matrix.
 * @param len
 *   Number of zero bytes the operator should advance over (power of two).
 */
static void
crc32c_zeros_op(uint32_t *even, size_t len)
{
    uint32_t odd[32]; /* Odd-power-of-two zeros operator */
    uint32_t row;
    int n;

    /* Put operator for one zero bit in odd */
    odd[0] = POLY; /* CRC-32C polynomial */
    row    = 1;
    for (n = 1; n < 32; n++) {
        odd[n] = row;
        row <<= 1;
    }

    /* Put operator for two zero bits in even */
    gf2_matrix_square(even, odd);

    /* Put operator for four zero bits in odd */
    gf2_matrix_square(odd, even);

    /*
     * First square will put the operator for one zero byte (eight zero
     * bits), in even -- next square puts operator for two zero bytes in
     * odd, and so on, until len has been rotated down to zero
     */
    do {
        gf2_matrix_square(even, odd);
        len >>= 1;
        if (len == 0)
            return;
        gf2_matrix_square(odd, even);
        len >>= 1;
    } while (len);

    /* Answer ended up in odd -- copy to even */
    for (n = 0; n < 32; n++)
        even[n] = odd[n];
}

/**
 * @brief Populate four 256-entry lookup tables for byte-wise zero-advance of @p len.
 *
 * @param zeros
 *   Output: 4 x 256 array of uint32_t receiving the lookup tables.
 * @param len
 *   Number of zero bytes to represent (passed to crc32c_zeros_op()).
 */
static void
crc32c_zeros(uint32_t zeros[][256], size_t len)
{
    uint32_t op[32];
    uint32_t n;

    crc32c_zeros_op(op, len);
    for (n = 0; n < 256; n++) {
        zeros[0][n] = gf2_matrix_times(op, n);
        zeros[1][n] = gf2_matrix_times(op, n << 8);
        zeros[2][n] = gf2_matrix_times(op, n << 16);
        zeros[3][n] = gf2_matrix_times(op, n << 24);
    }
}

/**
 * @brief Advance a CRC over a block of zero bytes using pre-computed tables.
 *
 * @param zeros
 *   4 x 256 lookup tables produced by crc32c_zeros().
 * @param crc
 *   Current CRC value to advance.
 * @return
 *   CRC value after advancing over the block of zeros the tables represent.
 */
static inline uint32_t
crc32c_shift(uint32_t zeros[][256], uint32_t crc)
{
    return (zeros[0][crc & 0xff] ^ zeros[1][(crc >> 8) & 0xff] ^ zeros[2][(crc >> 16) & 0xff] ^
            zeros[3][crc >> 24]);
}

/**
 * @brief Initialise the CRC-32C shift tables.
 *
 * Populates the four lookup-table sets used by the three-way parallel CRC
 * computation.  Called automatically at startup when USERSPACE_TESTING is
 * defined (via a constructor attribute) or explicitly from kernel SYSINIT.
 */
static void
#ifdef USERSPACE_TESTING
    __attribute__((__constructor__))
#endif
    crc32c_init_hw(void)
{
    crc32c_zeros(crc32c_long, LONG);
    crc32c_zeros(crc32c_2long, 2 * LONG);
    crc32c_zeros(crc32c_short, SHORT);
    crc32c_zeros(crc32c_2short, 2 * SHORT);
}
#ifdef _KERNEL
SYSINIT(crc32c_sse42, SI_SUB_LOCK, SI_ORDER_ANY, crc32c_init_hw, NULL);
#endif

/**
 * @brief Compute CRC-32C using Intel SSE 4.2 hardware instructions.
 *
 * Uses three-way parallel CRC32 execution and GF(2) shift tables to hide
 * the 3-cycle latency of the CRC32 instruction on Nehalem/Westmere/Sandy
 * Bridge/Ivy Bridge micro-architectures.
 *
 * @param crc
 *   Initial CRC value (use 0 to start a new computation).
 * @param buf
 *   Pointer to the input data buffer.
 * @param len
 *   Number of bytes to process.
 * @return
 *   CRC-32C checksum of the input buffer.
 */
#ifdef USERSPACE_TESTING
uint32_t sse42_crc32c(uint32_t, const unsigned char *, unsigned);
#endif
uint32_t
sse42_crc32c(uint32_t crc, const unsigned char *buf, unsigned len)
{
#ifdef __amd64__
    const size_t align = 8;
#else
    const size_t align = 4;
#endif
    const unsigned char *next, *end;
#ifdef __amd64__
    uint64_t crc0, crc1, crc2;
#else
    uint32_t crc0, crc1, crc2;
#endif

    next = buf;
    crc0 = crc;

    /* Compute the crc to bring the data pointer to an aligned boundary. */
    while (len && ((uintptr_t)next & (align - 1)) != 0) {
        crc0 = _mm_crc32_u8(crc0, *next);
        next++;
        len--;
    }

#if LONG > SHORT
    /**
     * Compute the crc on sets of LONG*3 bytes, executing three independent
     * crc instructions, each on LONG bytes -- this is optimized for the
     * Nehalem, Westmere, Sandy Bridge, and Ivy Bridge architectures, which
     * have a throughput of one crc per cycle, but a latency of three
     * cycles.
     */
    crc = 0;
    while (len >= LONG * 3) {
        crc1 = 0;
        crc2 = 0;
        end  = next + LONG;
        do {
#ifdef __amd64__
            crc0 = _mm_crc32_u64(crc0, *(const uint64_t *)next);
            crc1 = _mm_crc32_u64(crc1, *(const uint64_t *)(next + LONG));
            crc2 = _mm_crc32_u64(crc2, *(const uint64_t *)(next + (LONG * 2)));
#else
            crc0 = _mm_crc32_u32(crc0, *(const uint32_t *)next);
            crc1 = _mm_crc32_u32(crc1, *(const uint32_t *)(next + LONG));
            crc2 = _mm_crc32_u32(crc2, *(const uint32_t *)(next + (LONG * 2)));
#endif
            next += align;
        } while (next < end);
        /**
         * Update the crc.  Try to do it in parallel with the inner
         * loop. 'crc' is used to accumulate crc0 and crc1
         * produced by the inner loop so that the next iteration
         * of the loop doesn't depend on anything except crc2.
         *
         * The full expression for the update is:
         *     crc = S*S*S*crc + S*S*crc0 + S*crc1
         * where the terms are polynomials modulo the CRC polynomial.
         * We regroup this subtly as:
         *     crc = S*S * (S*crc + crc0) + S*crc1.
         * This has an extra dependency which reduces possible
         * parallelism for the expression, but it turns out to be
         * best to intentionally delay evaluation of this expression
         * so that it competes less with the inner loop.
         *
         * We also intentionally reduce parallelism by feedng back
         * crc2 to the inner loop as crc0 instead of accumulating
         * it in crc. This synchronizes the loop with crc update.
         * CPU and/or compiler schedulers produced bad order without
         * this.
         *
         * Shifts take about 12 cycles each, so 3 here with 2
         * parallelizable take about 24 cycles and the crc update
         * takes slightly longer. 8 dependent crc32 instructions
         * can run in 24 cycles, so the 3-way blocking is worse
         * than useless for sizes less than 8 * <word size> = 64
         * on amd64. In practice, SHORT = 32 confirms these
         * timing calculations by giving a small improvement
         * starting at size 96. Then the inner loop takes about
         * 12 cycles and the crc update about 24, but these are
         * partly in parallel so the total time is less than the
         * 36 cycles that 12 dependent crc32 instructions would
         * take.
         *
         * To have a chance of completely hiding the overhead for
         * the crc update, the inner loop must take considerably
         * longer than 24 cycles. LONG = 64 makes the inner loop
         * take about 24 cycles, so is not quite large enough.
         * LONG = 128 works OK. Unhideable overheads are about
         * 12 cycles per inner loop. All assuming timing like
         * Haswell.
         */
        crc  = crc32c_shift(crc32c_long, crc) ^ crc0;
        crc1 = crc32c_shift(crc32c_long, crc1);
        crc  = crc32c_shift(crc32c_2long, crc) ^ crc1;
        crc0 = crc2;
        next += LONG * 2;
        len -= LONG * 3;
    }
    crc0 ^= crc;
#endif /* LONG > SHORT */

    /**
     * Do the same thing, but now on SHORT*3 blocks for the remaining data
     * less than a LONG*3 block
     */
    crc = 0;
    while (len >= SHORT * 3) {
        crc1 = 0;
        crc2 = 0;
        end  = next + SHORT;
        do {
#ifdef __amd64__
            crc0 = _mm_crc32_u64(crc0, *(const uint64_t *)next);
            crc1 = _mm_crc32_u64(crc1, *(const uint64_t *)(next + SHORT));
            crc2 = _mm_crc32_u64(crc2, *(const uint64_t *)(next + (SHORT * 2)));
#else
            crc0 = _mm_crc32_u32(crc0, *(const uint32_t *)next);
            crc1 = _mm_crc32_u32(crc1, *(const uint32_t *)(next + SHORT));
            crc2 = _mm_crc32_u32(crc2, *(const uint32_t *)(next + (SHORT * 2)));
#endif
            next += align;
        } while (next < end);
        crc  = crc32c_shift(crc32c_short, crc) ^ crc0;
        crc1 = crc32c_shift(crc32c_short, crc1);
        crc  = crc32c_shift(crc32c_2short, crc) ^ crc1;
        crc0 = crc2;
        next += SHORT * 2;
        len -= SHORT * 3;
    }
    crc0 ^= crc;

    /* Compute the crc on the remaining bytes at native word size. */
    end = next + (len - (len & (align - 1)));
    while (next < end) {
#ifdef __amd64__
        crc0 = _mm_crc32_u64(crc0, *(const uint64_t *)next);
#else
        crc0 = _mm_crc32_u32(crc0, *(const uint32_t *)next);
#endif
        next += align;
    }
    len &= (align - 1);

    /* Compute the crc for any trailing bytes. */
    while (len) {
        crc0 = _mm_crc32_u8(crc0, *next);
        next++;
        len--;
    }

    return ((uint32_t)crc0);
}
