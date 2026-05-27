# lib/core — Frame Generator Core Library

## Overview

The `lib/core` library is the engine of the `fgen` (frame generator) system. It parses a human-readable, layer-by-layer packet description string and produces a binary network frame in memory. The same infrastructure supports the reverse direction: decoding raw packet bytes back into the text format.

The library exposes three conceptual surfaces:

| Surface | Files | Purpose |
|---|---|---|
| Public API | `fgen.h`, `fgen.c` | Lifecycle, frame loading, frame lookup, string utilities |
| Encoder | `encode.c` | Text-string → binary frame |
| Decoder | `decode.c`, `decode.h` | Binary frame → text-string |

---

## Frame Description String Format

Frames are described as a `/`-separated chain of named protocol layers, each followed by a parenthesised key=value parameter list:

```
<name> := Layer1(key=val,...)/Layer2(key=val,...)/...
```

Example:

```
Port0 := Ether(dst=00:01:02:03:04:05)/IPv4(dst=1.2.3.4)/UDP(sport=5678,dport=1234)/TSC()/Payload(size=64,fill=0xaa)
```

Strings may span multiple lines in a file. Comments begin with `//`. The frame name is separated from the layer chain by `:=`.

---

## Supported Layers

| String token | `opt_type_t` enum | Parameters |
|---|---|---|
| `Ether(…)` | `FGEN_ETHER_TYPE` | `dst=`, `src=` (MAC addresses) |
| `Dot1q(…)` | `FGEN_DOT1Q_TYPE` | `vlan=`, `prio=`, `cfi=` |
| `Dot1ad(…)` | `FGEN_DOT1AD_TYPE` | `vlan=`, `prio=`, `cfi=` |
| `IPv4(…)` | `FGEN_IPV4_TYPE` | `dst=`, `src=` (dotted-quad) |
| `IPv6(…)` | `FGEN_IPV6_TYPE` | `dst=`, `src=` |
| `UDP(…)` | `FGEN_UDP_TYPE` | `dport=`, `sport=` |
| `TCP(…)` | `FGEN_TCP_TYPE` | `dport=`, `sport=` |
| `Vxlan(…)` | `FGEN_VXLAN_TYPE` | (defaults to VNI 1000) |
| `Echo(…)` | `FGEN_ECHO_TYPE` | (none; forces ports to 7) |
| `TSC(…)` | `FGEN_TSC_TYPE` | (none; embeds a timestamp slot) |
| `Raw(…)` | `FGEN_RAW_TYPE` | (raw bytes) |
| `Payload(…)` | `FGEN_PAYLOAD_TYPE` | `size=` (absolute, includes CRC), `append=` (relative), `fill=` (byte pattern) |

Layers are processed in declaration order. Each encoder function sets its header fields, advances the running data-length counter, then calls the next layer recursively. This ensures that length and checksum fields that depend on inner headers (e.g. IPv4 total-length, UDP/TCP checksums) are computed on the way back up the call stack.

---

## Core Data Structures

### `fgen_t` — generator context

```c
typedef struct fgen_s {
    TAILQ_HEAD(frame_head, frame_s) head; // linked list of all frames
    uint32_t flags;                       // FGEN_VERBOSE | FGEN_DUMP_DATA
    uint32_t nb_frames;
    salloc_t *salloc;                     // slab allocator for frame binary data
    uint16_t nb_layers;                   // number of layers parsed from current fstr
    fopt_t   opts[FGEN_MAX_LAYERS];       // per-layer metadata (scratch during encode)
    char    *layers[FGEN_MAX_LAYERS];     // pointers into the tokenised fstr
    char    *params[FGEN_MAX_PARAMS];     // pointers into each layer's parameter string
} fgen_t;
```

`opts`, `layers`, and `params` are scratch space reused on every call to `_encode_frame`. They are not safe to access after the encode returns.

### `frame_t` — a single encoded frame

```c
typedef struct frame_s {
    TAILQ_ENTRY(frame_s) next;
    char     *name;       // strdup'd frame name
    char     *fstr;       // strdup'd original text string
    struct fgen_s *fg;
    uint32_t  data_off;   // offset into salloc slab where binary data lives
    uint16_t  data_len;   // total byte length of the binary frame
    uint16_t  tsc_off;    // byte offset of the TSC timestamp slot (0 if absent)
    uint16_t  port;
    proto_t   l2, l3, l4; // offset + length of each protocol header
} frame_t;
```

Binary frame data is stored in the shared `salloc` slab owned by `fgen_t`, not inside `frame_t` itself. The macros `fbuf_mtod(f, t)` and `fbuf_mtod_offset(f, t, o)` resolve the data pointer at runtime.

### `fopt_t` — per-layer parse option

```c
typedef struct fopt_s {
    opt_type_t typ;      // which layer
    uint16_t   offset;   // byte offset of this header in the frame
    uint16_t   length;   // byte length of this header
    char      *param_str;// pointer to the raw parameter text, e.g. "(dst=...,src=...)"
    ftable_t  *tbl;      // pointer to the dispatch table entry
} fopt_t;
```

### `ftable_t` — layer dispatch table

```c
typedef struct ftable_s {
    opt_type_t typ;
    const char *str;  // prefix string used for matching, e.g. "Ether("
    fgen_fn_t   fn;   // encoder function pointer
} ftable_t;
```

`fgen_tbl[]` in `encode.c` is the static dispatch table. Entry `[0]` is the sentinel `_encode_done` function that enforces minimum/maximum Ethernet frame size constraints.

---

## Encode Path

```
fgen_add_frame(fg, name, text)
  └─ _add_frame()
       ├─ frame_alloc()          allocate frame_t, strdup name + fstr
       └─ _encode_frame(f)
            ├─ fgen_strtok(fstr, "/", layers, …)   split into layer tokens
            ├─ for each token: match fgen_tbl[] prefix → fill fopt_t
            ├─ append sentinel done-layer to opts[]
            └─ next_layer(f, 0)  → recursive dispatch through encoder fns
                 Ether → Dot1Q/IPv4 → UDP/TCP → TSC → Payload → done
```

The encoder writes directly into the `salloc` slab via `fbuf_mtod_offset`. No intermediate copy is made.

---

## Decode Path

```
fgen_decode(dc, data, len, opt_type)
  └─ _decode_ether / _decode_ipv4 / …
       ├─ _append(dc, …)  build text string into dc->buffer (grows with realloc)
       └─ recursive calls for inner layers

fgen_decode_text(dc)   → returns the assembled string
fgen_decode_destroy(dc)
```

The decoder walks raw bytes at `dc->data_off`, reconstructing the fgen text format into a dynamically-allocated, `\0`-terminated string.

`fgen_decode_string()` is a separate utility: it converts a space-separated hex-dump string (e.g. `"00 11 22 33"`) into raw bytes.

---

## Public API Quick Reference

```c
// Lifecycle
fgen_t *fgen_create(int flags);          // FGEN_VERBOSE | FGEN_DUMP_DATA
void    fgen_destroy(fgen_t *fg);

// Loading frames
int fgen_load_files(fgen_t *fg, char **files, int nb_files);
int fgen_load_strings(fgen_t *fg, char **frames, int nb_frames);
int fgen_add_frame(fgen_t *fg, const char *name, const char *text);

// Traversal
frame_t *fgen_find_frame(fgen_t *fg, const char *name);
frame_t *fgen_next_frame(fgen_t *fg, frame_t *prev);  // prev=NULL → first frame
int      fgen_fcnt(fg);                               // macro: number of frames

// Accessing binary data
void    *fbuf_mtod(frame_t *f, type);
void    *fbuf_mtod_offset(frame_t *f, type, offset);
uint16_t fbuf_data_len(frame_t *f);

// Decode (binary → text)
fgen_decode_t *fgen_decode_create(void);
int            fgen_decode(fgen_decode_t *dc, void *data, uint16_t len, opt_type_t opt);
const char    *fgen_decode_text(fgen_decode_t *dc);
void           fgen_decode_destroy(fgen_decode_t *dc);
int            fgen_decode_string(const char *text, uint8_t *buf, int len);

// Diagnostics
void fgen_print_frame(const char *msg, frame_t *f);
void fgen_print_string(const char *msg, const char *text);
```

---

## Design Notes

- **Recursive descent encoder.** Each `_encode_*` function calls `next_layer()` before writing its own variable-length or checksum fields. This means an inner layer's size is known when the outer layer needs to fill in total-length or checksum fields.

- **Shared slab allocator.** All binary frame data lives in a single `salloc` slab owned by `fgen_t`. Frames reference their data by `(data_off, data_len)` rather than by pointer, so the slab can grow without invalidating frame metadata.

- **Scratch-space encode state.** `fg->opts[]`, `fg->layers[]`, and `fg->params[]` are reset at the start of each `_encode_frame` call. The library is not thread-safe for concurrent encode operations on the same `fgen_t`.

- **TSC timestamp slot.** When a `TSC()` layer is present, `frame_t.tsc_off` records its byte offset. The caller can write a hardware timestamp directly into `fbuf_mtod_offset(f, tsc_t *, f->tsc_off)` without re-parsing the frame.

- **File format.** `fgen_load_files` supports multi-line frame strings. Lines containing `//` are comment-stripped. A line containing `:=` starts a new frame definition; everything after ``:=`` until the next `:=` line (or EOF) is joined with `/` separators to form the layer string.
