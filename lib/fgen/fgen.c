/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023-2024 Intel Corporation
 */

#include <stdint.h>        // for uint32_t, uint16_t, int32_t, uint8_t
#include <stdbool.h>
#include <netinet/in.h>    // for ntohs, htonl, htons

#include <fgen_common.h>
#include <fgen_log.h>
#include <fgen_stdio.h>
#include <fgen_strings.h>

#include "fgen.h"

extern int _encode_frame(fgen_t *fg, frame_t *f);

static int
_add_frame(fgen_t *fg, uint16_t idx, const char *name, const char *fstr)
{
    frame_t *f;
    size_t sz;

    if (idx >= fg->max_frames)
        FGEN_ERR_RET("Number of frames exceeds %u\n", fg->max_frames);

    f           = &fg->frames[idx];
    f->data_len = 0;
    f->tsc_off  = 0;

    sz = strnlen(fstr, FGEN_MAX_STRING_LENGTH);

    if (sz <= 0 || sz >= FGEN_MAX_STRING_LENGTH)
        FGEN_ERR_RET("String is 0 or too long %ld > %d\n", sz, FGEN_MAX_STRING_LENGTH);

    f->name[0] = '\0';
    if (name)
        strlcpy(f->name, name, sizeof(f->name));

    if (strlen(f->name) == 0)
        snprintf(f->name, sizeof(f->name), "Frame-%u", f->fidx);

    f->frame_text = strdup(fstr);
    if (!f->frame_text)
        FGEN_ERR_GOTO(leave, "Unable to allocate memory\n");

    if (_encode_frame(fg, f) < 0)
        FGEN_ERR_GOTO(leave, "Failed to parse frame\n");

    fg->nb_frames++;

    return 0;
leave:
    if (f) {
        f->name[0] = '\0';
        free(f->frame_text);
        f->frame_text = NULL;
    }
    return -1;
}

int
fgen_add_frame_at(fgen_t *fg, int idx, const char *name, const char *fstr)
{
    if (!fg)
        FGEN_ERR_RET("fgen_t pointer is NULL\n");

    if (!fstr)
        FGEN_ERR_RET("fgen string is NULL\n");

    if (_add_frame(fg, idx, name, fstr) < 0)
        FGEN_ERR_RET("Failed to parse frame\n");

    if (fg->flags & (FGEN_VERBOSE || FGEN_DUMP_DATA))
        fgen_printf("\n");

    return 0;
}

int
fgen_add_frame(fgen_t *fg, const char *name, const char *fstr)
{
    if (!fg)
        FGEN_ERR_RET("fgen_t pointer is NULL\n");

    return fgen_add_frame_at(fg, fg->nb_frames, name, fstr);
}

fgen_t *
fgen_create(uint16_t max_frames, uint16_t frame_sz, int flags)
{
    fgen_t *fg;
    uint16_t bufsz;

    if (max_frames == 0)
        return NULL;

    if (frame_sz == 0)
        frame_sz = 1;

    bufsz = FGEN_ALIGN_CEIL(frame_sz, FGEN_CACHE_LINE_SIZE);

    fg = calloc(1, sizeof(fgen_t));
    if (fg) {
        frame_t *f;
        char *b;

        fg->frame_bufsz = bufsz;
        fg->max_frames  = max_frames;
        fg->flags       = flags;

        f = fg->frames = calloc(max_frames, sizeof(frame_t));
        if (!fg->frames)
            FGEN_ERR_GOTO(leave, "Unable to allocate frame_t structures\n");

        fg->mm = mmap_alloc(max_frames, bufsz, MMAP_HUGEPAGE_4KB);
        if (!fg->mm)
            FGEN_ERR_GOTO(leave, "Unable to allocate frame buffers\n");

        b = mmap_addr(fg->mm);

        for (int i = 0; i < max_frames; i++, f++) {
            f->fidx  = i;
            f->data  = b;
            f->bufsz = bufsz;
            b += bufsz;
        }
    }

    return fg;
leave:
    fgen_destroy(fg);
    return NULL;
}

void
fgen_destroy(fgen_t *fg)
{
    if (fg) {
        for (int i = 0; i < fg->nb_frames; i++)
            free(fg->frames[i].frame_text);

        free(fg->frames);
        mmap_free(fg->mm);
        free(fg);
    }
}

static int
get_frame_string(FILE *f, char *str, int len)
{
    char buf[FGEN_MAX_STRING_LENGTH], *p, *c;
    long pos;
    int slen = 0;

    memset(buf, 0, sizeof(buf));

    /* Here we are gathering up all of the frame text into a single buffer,
     * as the fgen frame text can be split over multiple lines.
     */
    for (;;) {
        pos = ftell(f); /* Save the position to the start of the line */

        if (fgets(buf, sizeof(buf), f) == NULL)
            break;

        /* Trim off any comments in the line, which could leave nothing in the buffer */
        if ((c = strstr(buf, "//")) != NULL)
            *c = '\0';

        p = strtrim(buf); /* remove any leading or trailing whitespace */

        /* empty string continue */
        if (*p == '\0')
            continue;

        /* Check to see if we have a ':=' string in the line, which means we have read too much */
        c = strstr(p, ":=");
        if (c) {
            /* seek backup in the file to the beginning of the line and leave */
            fseek(f, pos, SEEK_SET);
            break;
        }

        /* This line must be part of the current fgen text line, append the string */
        slen = strlcat(str, p, len);

        /* Make sure the layer or line has a trailing '/' */
        if (str[slen - 1] != '/')
            slen = strlcat(str, "/", len);
    }

    /* Strip off the tailing '/' we added to the string */
    if (slen)
        str[--slen] = '\0';

    return slen;
}

/**
 * Find the next frame string, return -1 on EOF, 0 not found, 1 Found
 */
static int
find_next_frame(FILE *f, char *name, int len)
{
    char line[FGEN_MAX_STRING_LENGTH], *p, *c;
    long pos;

    name[0] = '\0';
    for (;;) {
        pos = ftell(f);

        line[0] = '\0';
        if (fgets(line, sizeof(line), f) == NULL)
            return -1;

        c = strstr(line, "//");
        if (c)
            *c = '\0';

        p = strtrim(line);
        if (!p || strlen(p) < 2)
            continue;

        c = strstr(p, ":=");
        if (c == NULL)
            continue;

        if (c != p) { /* Must be a name present */
            *c = '\0';
            strlcpy(name, p, len);
        }

        p = c + 2; /* Skip the ':=' string */
        p = strtrim(p);

        /* Seek back to the start of the frame-string just after ':=' */
        fseek(f, pos + (p - line), SEEK_SET);
        return 1;
    }

    return 0;
}

int

fgen_load_file(fgen_t *fg, const char *filename)
{
    FILE *f;
    char buf[FGEN_MAX_STRING_LENGTH], name[FGEN_FRAME_NAME_LENGTH + 1];
    int ret, cnt;

    if (!fg || !filename || strlen(filename) == 0)
        FGEN_ERR_RET("Filename is not specified\n");

    f = fopen(filename, "r");
    if (!f)
        FGEN_ERR_RET("Unable to open file '%s'\n", filename);

    memset(name, 0, sizeof(name));
    memset(buf, 0, sizeof(buf));

    for (cnt = 0;; cnt++) {
        name[0] = '\0';
        ret     = find_next_frame(f, name, sizeof(name));
        if (ret <= 0)
            break;

        buf[0] = '\0';
        if (get_frame_string(f, buf, sizeof(buf)) == 0)
            break;

        if (strlen(name) == 0)
            snprintf(name, sizeof(name), "Frame-%d", cnt);

        if (_add_frame(fg, cnt, name, buf) < 0)
            FGEN_ERR_RET("Adding a frame failed\n");
    }

    return fg->nb_frames;
}

int
fgen_load_strings(fgen_t *fg, const char **fstr, int len)
{
    char *c   = NULL;
    char *s   = NULL;
    char *txt = NULL;
    char name[FGEN_FRAME_NAME_LENGTH];
    int cnt;

    if (!fg)
        FGEN_ERR_RET("fgen_t pointer is NULL\n");
    if (!fstr)
        FGEN_ERR_RET("Frame string pointer array is NULL\n");

    for (cnt = 0; cnt < len; cnt++) {
        if (len == 0 && fstr[cnt] == NULL)
            break;

        s = txt = strdup(fstr[cnt]);
        if (!txt)
            FGEN_ERR_RET("Unable to strdup() text\n");

        name[0] = '\0';
        if ((c = strstr(s, ":=")) == NULL)
            snprintf(name, sizeof(name), "frame-%d", cnt);
        else {
            *c = '\0';
            strlcpy(name, s, sizeof(name));
            s = c + 2;
            while ((*s == ' ' || *s == '\t' || *s == '\n') && *s != '\0')
                s++;
            if (*s == '\0')
                FGEN_ERR_RET("Invalid frame name '%s'\n", fstr[cnt]);
        }

        if (_add_frame(fg, cnt, name, s) < 0)
            FGEN_ERR_RET("Adding a frame failed\n");
    }

    free(txt);
    return cnt;
}

void
fgen_print_string(const char *msg, const char *text)
{
    char *layers[FGEN_MAX_LAYERS];
    char *txt;
    int num;

    txt = strdup(text);
    if (!txt)
        FGEN_RET("Unable to strdup() text string\n");

    fgen_printf("\n");
    fgen_printf("[yellow]>>>> [cyan]%s [yellow]<<<<[]\n", (msg) ? msg : "");

    num = fgen_strtok(txt, "/", layers, fgen_countof(layers));
    if (num) {
        for (int i = 0; i < num; i++)
            fgen_printf("   %s\n", layers[i]);
    }
    free(txt);
}
void
fgen_print_frame(const char *msg, frame_t *f)
{
    if (!f->frame_text || strlen(f->frame_text) == 0)
        FGEN_RET("text pointer is NULL or zero length string\n");

    fgen_print_string((msg) ? msg : f->name, f->frame_text);
}
