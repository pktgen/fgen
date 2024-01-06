/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2019-2023 Intel Corporation
 */

#include <stdio.h>             // for fflush, vprintf, stdout
#include <stdlib.h>            // for free, abort, calloc, srand
#include <stdarg.h>            // for va_end, va_list, va_start
#include <string.h>            // for strdup
#include <stdatomic.h>         // for atomic_fetch_add, atomic_load, atomic_uint

#include <fgen_common.h>
#include <fgen_log.h>
#include <fgen_stdio.h>
#include "tst_info.h"

struct tst_stat {
    atomic_uint fail;
    atomic_uint pass;
    atomic_uint skip;
};

/* Global test statistics updated by tst_end(). */
static struct tst_stat tst_stats;

int
tst_exit_code(void)
{
    if (atomic_load(&tst_stats.fail))
        return EXIT_FAILURE;
    else if (atomic_load(&tst_stats.skip))
        return EXIT_SKIPPED;
    return EXIT_SUCCESS;
}

uint32_t
tst_summary(void)
{
    uint32_t fail = atomic_load(&tst_stats.fail);

    fgen_printf("-------------\n");
    fgen_printf("Test Summary:\n");
    fgen_printf("-------------\n");
    fgen_printf("[red]Fail: %u[]\n", fail);
    fgen_printf("[green]Pass: %u[]\n", atomic_load(&tst_stats.pass));
    fgen_printf("[yellow]Skip: %u[]\n", atomic_load(&tst_stats.skip));

    return fail;
}

tst_info_t *
tst_start(const char *msg)
{
    tst_info_t *tst;

    tst = calloc(1, sizeof(tst_info_t));
    if (!tst) {
        fgen_printf("[red]Error[]: [magenta]Failed to allocate tst_info_t structure[]\n");
        abort();
    }

    tst->name = strdup(msg);

    srand(0x56063011);

    fgen_printf("[cyan]>>>> [yellow]%s [green]tests[]\n", tst->name);

    return tst;
}

void
tst_end(tst_info_t *tst, int result)
{
    if (!tst)
        fgen_panic("tst cannot be NULL\n");

    fgen_printf("[cyan]<<<< [yellow]%s [green]Tests[]: [magenta]done.[]\n\n", tst->name);

    if (result == TST_PASSED)
        atomic_fetch_add(&tst_stats.pass, 1);
    else if (result == TST_SKIPPED)
        atomic_fetch_add(&tst_stats.skip, 1);
    else
        atomic_fetch_add(&tst_stats.fail, 1);
    free(tst->name);
    free(tst);
}

void
tst_skip(const char *fmt, ...)
{
    va_list va_list;

    va_start(va_list, fmt);
    fgen_printf("[yellow]  ** [green]SKIP[] - [green]TEST[]: [cyan]");
    fgen_vprintf(fmt, va_list);
    fgen_printf("[]\n");
    va_end(va_list);

    fflush(stdout);
}

void
tst_ok(const char *fmt, ...)
{
    va_list va_list;

    va_start(va_list, fmt);
    fgen_printf("[yellow]  ** [green]PASS[] - [green]TEST[]: [cyan]");
    fgen_vprintf(fmt, va_list);
    fgen_printf("[]\n");
    va_end(va_list);

    fflush(stdout);
}

void
tst_error(const char *fmt, ...)
{
    va_list va_list;

    va_start(va_list, fmt);
    fgen_printf("[yellow]  >> [red]FAIL[] - [green]TEST[]: [cyan]");
    fgen_vprintf(fmt, va_list);
    fgen_printf("[]\n");
    va_end(va_list);

    fflush(stdout);
}

void
tst_info(const char *fmt, ...)
{
    va_list va_list;

    va_start(va_list, fmt);
    fgen_printf("\n[yellow]  == [blue]INFO[] - [green]TEST[]: [cyan]");
    fgen_vprintf(fmt, va_list);
    fgen_printf("[]\n");
    va_end(va_list);

    fflush(stdout);
}
