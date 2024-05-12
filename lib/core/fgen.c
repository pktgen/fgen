/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023-2024 Intel Corporation
 */

#include <stdint.h>        // for uint32_t, uint16_t, int32_t, uint8_t
#include <stdbool.h>
#include <netinet/in.h>        // for ntohs, htonl, htons
#include <net/ethernet.h>
#include <sys/queue.h>

#include <fgen_common.h>
#include <fgen_log.h>
#include <fgen_stdio.h>
#include <fgen_strings.h>

#include "fgen.h"

extern int _encode_frame(frame_t *f);

static frame_t *
frame_alloc(fgen_t *fg, const char *name, const char *fstr)
{
    frame_t *f;

    f = calloc(1, sizeof(*f));
    if (!f)
        return NULL;

    f->name = strdup(name);
    if (!f->name) {
        free(f);
        return NULL;
    }

    f->fstr = strdup(fstr);
    if (!f->fstr) {
        free(f->name);
        free(f);
        return NULL;
    }
    f->fg = fg;        // save the fgen_t pointer
    fg->nb_frames++;

    TAILQ_INSERT_TAIL(&fg->head, f, next);

    return f;
}

static void
frame_free(fgen_t *fg, frame_t *f)
{
    if (f) {
        fg->nb_frames--;
        TAILQ_REMOVE(&fg->head, f, next);
        free(f->fstr);
        free(f->name);
        free(f);
    }
}

static int
_add_frame(fgen_t *fg, const char *name, const char *fstr)
{
    frame_t *f;

    if (!fg)
        FGEN_ERR_RET("fgen_t pointer is NULL\n");

    if (!name || name[0] == '\0')
        FGEN_ERR_RET("frame name is NULL\n");

    if (!fstr || fstr[0] == '\0')
        FGEN_ERR_RET("frame string is NULL\n");

    f = fgen_find_frame(fg, name);
    if (f)
        FGEN_ERR_RET("frame %s already exists", name);

    f = frame_alloc(fg, name, fstr);
    if (f == NULL)
        FGEN_ERR_RET("failed to allocate frame\n");

    if (_encode_frame(f) < 0)
        FGEN_ERR_GOTO(leave, "Failed to parse frame\n");

    return 0;
leave:
    frame_free(fg, f);
    return -1;
}

int
fgen_add_frame(fgen_t *fg, const char *name, const char *fstr)
{
    if (!fg)
        FGEN_ERR_RET("fgen_t pointer is NULL\n");

    if (!fstr)
        FGEN_ERR_RET("fgen string is NULL\n");

    if (_add_frame(fg, name, fstr) < 0)
        FGEN_ERR_RET("Failed to parse frame\n");

    if (fg->flags & (FGEN_VERBOSE || FGEN_DUMP_DATA))
        fgen_printf("\n");

    return 0;
}

fgen_t *
fgen_create(int flags)
{
    fgen_t *fg;

    fg = calloc(1, sizeof(fgen_t));
    if (fg) {
        fg->flags  = flags;
        fg->salloc = salloc_create(0);
        if (!fg->salloc) {
            free(fg);
            FGEN_NULL_RET("Failed to allocate memory for build buffer\n");
        }
        TAILQ_INIT(&fg->head);
    }
    return fg;
}

void
fgen_destroy(fgen_t *fg)
{
    if (fg) {
        frame_t *f, *tmp;

        TAILQ_FOREACH_SAFE (f, &fg->head, next, tmp) {
            frame_free(fg, f);
        }

        salloc_destroy(fg->salloc);
        free(fg);
    }
}

frame_t *
fgen_find_frame(fgen_t *fg, const char *name)
{
    frame_t *f;
    size_t len;

    if (!fg)
        return NULL;

    len = strlen(name);
    TAILQ_FOREACH (f, &fg->head, next) {
        if (!strncmp(f->name, name, len))
            return f;
    }

    return NULL;
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

        if (_add_frame(fg, name, buf) < 0)
            FGEN_ERR_RET("Adding a frame failed\n");
    }

    return fg->nb_frames;
}

/*
 * Example of a fgen string:
 *  Port0 := Ether( dst=00:01:02:03:04:05 )/IPv4(dst=1.2.3.4)/
 *           UDP(sport=5678, dport=1234)/TSC()/Payload(size=32, fill=0xaa)/Port(0)
 *
 * The frame name is "Port0" and the rest describes the frame content.
 */
int
fgen_load_strings(fgen_t *fg, const char *const *fstr, int len)
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

        // Parse the frame name from the string
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

        if (_add_frame(fg, name, s) < 0)
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
    if (!f->fstr || strlen(f->fstr) == 0)
        FGEN_RET("text pointer is NULL or zero length string\n");

    fgen_print_string((msg) ? msg : f->name, f->fstr);
}

frame_t *
fgen_next_frame(fgen_t *fg, frame_t *prev)
{
    frame_t *f = NULL;

    if (!fg)
        FGEN_NULL_RET("fgen_t pointer is NULL\n");

    if (prev)
        f = TAILQ_NEXT(prev, next);
    else
        f = TAILQ_FIRST(&fg->head);

    return f;
}
