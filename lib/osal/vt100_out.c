/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2019-2026 Intel Corporation.
 */

/**
 * @file
 * @brief Implementation of VT100 color-tag parsing for FGEN formatted output.
 *
 * Provides vt_color_parse() and its internal helpers that scan a character
 * buffer for [fg:bg:attr] color tags and replace them with the appropriate
 * ANSI RGB SGR escape sequences.
 */

#include <stdio.h>              // for NULL
#include <string.h>             // for strlen, memset, strcpy, strncmp
#include <fgen_common.h>         // for FGEN_PTR_CAST, fgen_countof
#include <fgen_strings.h>        // for fgen_strtok, strtrimset
#include <bsd/string.h>         // for strlcpy
#include <strings.h>            // for index, strncasecmp
#include <stdlib.h>             // for calloc, free

#include "fgen_tty.h"          // for tty_is_color_on
#include "vt100_out.h"        // for vt_rgb_code_t, vt_attrs, vt_rgb_name_t, vt_...

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstringop-overflow"
#pragma GCC diagnostic ignored "-Warray-bounds"
#endif

static struct vt_attrs _attrs[] = VT_ATTRS;

static vt_rgb_code_t _rgb_codes[] = RGB_COLOR_CODES;
static vt_rgb_name_t _rgb_names[] = RGB_COLOR_NAMES;

/**
 * Look up a color name string in the RGB name table.
 *
 * @param color
 *   Lowercase color name to search for (e.g., "red", "darkblue").
 * @return
 *   Pointer to the matching vt_rgb_code_t entry, or the default (index 0)
 *   entry if the name is not found.
 */
static vt_rgb_code_t *
match_rgb_color(char *color)
{
    vt_rgb_name_t *n;

    for (n = &_rgb_names[0]; n->name != NULL; n++) {
        if (!strncasecmp(n->name, color, strlen(color)))
            return &_rgb_codes[n->color_id];
    }
    return &_rgb_codes[0]; /* return default codes */
}

/**
 * Look up an attribute name string in the VT attribute table.
 *
 * @param attr
 *   Lowercase attribute name to search for (e.g., "bold", "italic").
 * @return
 *   Pointer to the matching vt_attrs entry, or the default (index 0) entry
 *   if the name is not found.
 */
static struct vt_attrs *
match_attr(char *attr)
{
    struct vt_attrs *a;

    for (a = &_attrs[0]; a->name != NULL; a++) {
        if (!strncasecmp(a->name, attr, strlen(attr)))
            return a;
    }
    return &_attrs[0]; /* return default codes */
}

/**
 * Translate a single [fg:bg:attr] color tag into ANSI escape bytes.
 *
 * Parses the color tuple between @p p and @p e, resolves the foreground
 * color, background color, and text attribute, then writes the corresponding
 * ANSI SGR escape sequences into the buffer pointed to by @p *cb.
 *
 * @param p
 *   Pointer to the opening '[' of the color tag in the source buffer.
 * @param e
 *   Pointer to the character just after the closing ']' of the color tag.
 * @param cb
 *   In/out pointer to the current write position in the destination buffer.
 *   Advanced by the number of bytes written.
 * @param len
 *   Remaining space in the destination buffer.
 * @return
 *   Number of bytes written on success, -1 if color output is disabled or
 *   the tag is empty.
 */
static int
color_begin(char *p, char *e, char **cb, int len)
{
    vt_rgb_code_t *fg, *bg;
    struct vt_attrs *attr;
    char data[64], *d = data, *c;
    char *toks[4] = {0};
    vt_rgb_t r, g, b;
    int n;

    if (!tty_is_color_on())
        return -1;

    memset(data, 0, sizeof(data));

    c = *cb;
    n = (e - p) + 1;

    if (n > (int)sizeof(data))
        n = (int)sizeof(data);

    /* Grab a copy of the color string tuple and trim out [] */
    strlcpy(d, p, n);
    if (strncmp("[]", d, n) == 0)
        strcpy(d, VT_DEFAULT_NAME ":" VT_DEFAULT_NAME ":off");
    else
        d = strtrimset(d, "[]");

    /* color tuple [fg_color:bg_color:attr] */
    n = fgen_strtok(d, ":", toks, fgen_countof(toks));

    /* Setup some defaults for missing fields */
    if (toks[0] == NULL)
        toks[0] = FGEN_PTR_CAST(VT_DEFAULT_NAME, char *);
    if (toks[1] == NULL)
        toks[1] = FGEN_PTR_CAST(VT_DEFAULT_NAME, char *);
    if (toks[2] == NULL)
        toks[2] = FGEN_PTR_CAST(VT_DEFAULT_NAME, char *);

    /* Get RGB color format and attr pointers */
    fg   = match_rgb_color(toks[0]);
    bg   = match_rgb_color(toks[1]);
    attr = match_attr(toks[2]);

    /* Set the attribute first */
    if (attr && attr->attr != VT_NO_ATTR) {
        n = vt_attr_str(c, len, attr->attr);
        len -= n;
        c += n;
    }

    if (bg && bg->rgb_color != COLOR_DEFAULT) {
        r = (bg->rgb_color >> 16) & 0xFF;
        g = (bg->rgb_color >> 8) & 0xFF;
        b = bg->rgb_color & 0xFF;
        n = vt_rgb_str(c, len, VT_RGB_BG, r, g, b);
        len -= n;
        c += n;
    }

    if (fg && fg->rgb_color != COLOR_DEFAULT) {
        r = (fg->rgb_color >> 16) & 0xFF;
        g = (fg->rgb_color >> 8) & 0xFF;
        b = fg->rgb_color & 0xFF;
        n = vt_rgb_str(c, len, VT_RGB_FG, r, g, b);
        len -= n;
        c += n;
    }

    n = c - *cb;

    *cb = c;

    return n;
}

int
vt_color_parse(char *buff, int len)
{
    char *cbuf, *c, *b, *e;
    int b_index = 0;

    c = index(buff, '[');
    if (!c)
        return strlen(buff);

    cbuf = calloc(len + 4, 1);
    if (!cbuf)
        return -1;

    /* Move buff into cbuf and zero buff */
    strlcpy(cbuf, buff, len);

    memset(buff, 0, len);

    /* Parse cbuf for the color tuple and fill in buff */
    for (c = cbuf, b = buff; *c != '\0' && b_index < len;) {
        if (*c != '[') {
            *b++ = *c++; /* Fill buffer with text up to '[' */
            b_index++;
            len--;
            continue;
        }

        e = index(c, ']');
        if (!e) {
            *b++ = *c++; /* ']' not found assume only a '[' in text */
            b_index++;
            len--;
            continue;
        }

        /* Convert the color into a set of vt100 codes */
        if (color_begin(c, ++e, &b, len) == 0) {
            *b++ = *c++;
            b_index++;
        } else
            c = e; /* move pointer to after closing bracket */
    }
    free(cbuf);

    return b - buff; /* True size of the string or data in buffer */
}

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
