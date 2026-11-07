/* PPSSPP texture packs, used as they are: put the pack's folder contents (textures.ini and its
 * images) into textures/ or textures/ULUS10567/ and the textures are replaced like in PPSSPP.
 *
 * PPSSPP names a replacement by the texture's cache key and a hash of its bytes in PSP memory
 * (GPU/Common/TextureReplacer.cpp, TextureCacheCommon.cpp), not by its decoded pixels like this
 * port's own packs (ge_gpu.c tp_replace). This file computes the same key and hash:
 *   cache key = (address & 0x3FFFFFFF) << 32 | texture size bits, XOR (palette hash ^ the CLUT
 *               format command word) for palette textures;
 *   hash      = QUICK (PPSSPP's SSE2 quick hash), XXH32 or XXH64 of the texture's bytes, per
 *               textures.ini [options] hash, with reduceHash and [hashranges] / [reducehashranges];
 * and looks the file up like PPSSPP: [hashes] entries and hash-named files in the pack's root,
 * with the same wildcard fallbacks (no data hash, no address, no palette hash, ...).
 * Not supported: .dds / .ktx2 images (PNG only), mipmap levels (level 0 only), [filtering],
 * the 512-row maxSeenV rule and palettes read from a render target. */

#include "../recomp.h"
#include "../ge_shared.h"
#include "../platform/host_dir.h"
#include <SDL3/SDL_stdinc.h>

#define XXH_INLINE_ALL
#include "../../../third_party/xxhash/xxhash.h"
#include <emmintrin.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { uint64_t ck; uint32_t h; int prio; char *file; } Ent;   /* file NULL: "don't replace" */
typedef struct { uint64_t key; int w, h; } Range;
typedef struct { uint64_t key; float v; } Reduce;

static Ent *s_ent; static int s_n, s_cap;
static Range s_range[256]; static int s_nrange;
static Reduce s_reduce[256]; static int s_nreduce;
static int s_hash_kind;                  /* 0 quick, 1 xxh32, 2 xxh64 */
static int s_ignore_addr, s_reduce_on;
static float s_reduce_global = 0.5f;
static char s_dir[SR_DIR_PATH_CAPACITY]; /* the folder textures.ini is in */

static void pack_error(const char *action, const char *path, const SrDirError *error) {
    fprintf(stderr, "texture pack: %s %s (status %d, native %u)\n", action, path,
            (int)error->status, (unsigned)error->native_code);
}

/* Inspect one leaf without following links. Missing is distinct from denied or
 * unreadable; an optional absent image may fall back, a broken pack is logged. */
static int pack_leaf(const char *parent, const char *leaf, SrDirKind *kind) {
    SrHostDir *dir = NULL;
    SrDirError error;
    SrDirEntry entry;
    int status = sr_dir_open(parent, &dir, &error);
    if (status != SR_DIR_OK) {
        if (status != SR_DIR_MISSING) pack_error("cannot inspect", parent, &error);
        return status;
    }
    int found = 0;
    while ((status = sr_dir_next(dir, &entry, &error)) == 1) {
#ifdef _WIN32
        const int equal = SDL_strcasecmp(entry.name, leaf) == 0;
#else
        const int equal = strcmp(entry.name, leaf) == 0;
#endif
        if (equal) { *kind = entry.kind; found = 1; break; }
    }
    if (status < 0) pack_error("cannot enumerate", parent, &error);
    SrDirError close_error;
    if (sr_dir_close(dir, &close_error) != SR_DIR_OK) {
        pack_error("cannot close", parent, &close_error);
        return close_error.status;
    }
    return status < 0 ? status : found ? SR_DIR_OK : SR_DIR_MISSING;
}

/* Nested PNG names are legitimate. Resolve bounded individual components, not
 * a truncated printf or a substring '..' check. Each component must have the
 * expected non-link type. This is init-time validation, not a race-free sandbox. */
static int pack_path(const char *relative, char *out, int inspect) {
    char parent[SR_DIR_PATH_CAPACITY];
    const char *p = relative;
    if (!p || !*p || strlen(s_dir) >= sizeof(parent)) return 0;
    memcpy(parent, s_dir, strlen(s_dir) + 1);
    for (;;) {
        const char *end = p;
        char leaf[SR_DIR_NAME_CAPACITY], next[SR_DIR_PATH_CAPACITY];
        while (*end && *end != '/' && *end != '\\') ++end;
        const size_t length = (size_t)(end - p);
        if (!length || length >= sizeof(leaf)) return 0;
        memcpy(leaf, p, length); leaf[length] = 0;
        SrDirError error;
        if (sr_dir_join(parent, leaf, next, sizeof(next), &error) != SR_DIR_OK) return 0;
        if (inspect) {
            SrDirKind kind;
            if (pack_leaf(parent, leaf, &kind) != SR_DIR_OK ||
                    kind != (*end ? SR_DIR_DIRECTORY : SR_DIR_REGULAR)) return 0;
        }
        memcpy(parent, next, strlen(next) + 1);
        if (!*end) break;
        p = end + 1;
    }
    memcpy(out, parent, strlen(parent) + 1);
    return 1;
}

static const uint8_t k_bpp[16] = { 16, 16, 16, 32, 4, 8, 16, 32, 4, 8, 8 };
static const uint32_t k_align[16] = {
    0x7FF & ~7u, 0x7FF & ~7u, 0x7FF & ~7u, 0x7FF & ~3u, 0x7FF & ~31u, 0x7FF & ~15u, 0x7FF & ~7u, 0x7FF & ~3u,
    0x7FF, 0x7FF, 0x7FF };

/* PPSSPP's QuickTexHashSSE2 (GPU/Common/TextureDecoder.cpp), the Windows "quick" hash */
static uint32_t quick_hash(const void *checkp, uint32_t size) {
    uint32_t check = 0;
    if (((intptr_t)checkp & 0xf) == 0 && (size & 0x3f) == 0) {
        __m128i cursor = _mm_set1_epi32(0);
        __m128i cursor2 = _mm_set_epi16(0x0001, 0x0083, 0x4309, 0x4d9b, (short)0xb651, 0x4b73, (short)0x9bd9, (short)0xc00b);
        const __m128i update = _mm_set1_epi16(0x2455);
        const __m128i *p = (const __m128i *)checkp;
        for (uint32_t i = 0; i < size / 16; i += 4) {
            __m128i chunk = _mm_mullo_epi16(_mm_load_si128(&p[i]), cursor2);
            cursor = _mm_add_epi16(cursor, chunk);
            cursor = _mm_xor_si128(cursor, _mm_load_si128(&p[i + 1]));
            cursor = _mm_add_epi32(cursor, _mm_load_si128(&p[i + 2]));
            chunk = _mm_mullo_epi16(_mm_load_si128(&p[i + 3]), cursor2);
            cursor = _mm_xor_si128(cursor, chunk);
            cursor2 = _mm_add_epi16(cursor2, update);
        }
        cursor = _mm_add_epi32(cursor, cursor2);
        cursor = _mm_add_epi32(cursor, _mm_srli_si128(cursor, 8));
        cursor = _mm_add_epi32(cursor, _mm_srli_si128(cursor, 4));
        check = (uint32_t)_mm_cvtsi128_si32(cursor);
    } else {
        const uint32_t *p = (const uint32_t *)checkp;
        for (uint32_t i = 0; i < size / 8; ++i) { check += *p++; check ^= *p++; }
    }
    return check;
}
static uint32_t data_hash(const void *p, uint32_t n) {
    switch (s_hash_kind) {
    case 1:  return XXH32(p, n, 0xBACD7814);
    case 2:  return (uint32_t)XXH64(p, n, 0xBACD7814);
    default: return quick_hash(p, n);
    }
}

static void add(uint64_t ck, uint32_t h, int prio, const char *file, int inspect) {
    char *f = NULL;
    if (file && *file) {
        size_t n = strcspn(file, "|");                /* mip levels "a.png|b.png": level 0 */
        if (!n || n >= SR_DIR_PATH_CAPACITY) return;
        f = malloc(n + 1);
        if (!f) { fprintf(stderr, "texture pack: filename allocation failed\n"); return; }
        memcpy(f, file, n); f[n] = 0;
        for (char *c = f; *c; c++) if (*c == '\\') *c = '/';
        char path[SR_DIR_PATH_CAPACITY];
        if (!pack_path(f, path, inspect)) {
            fprintf(stderr, "texture pack: skipped unavailable or unsafe image %s\n", f);
            free(f); return;
        }
    }
    if (s_n == s_cap) {
        if (s_cap > INT_MAX / 2) { free(f); return; }
        int cap = s_cap ? s_cap * 2 : 1024;
        Ent *grown = realloc(s_ent, (size_t)cap * sizeof(Ent));
        if (!grown) { fprintf(stderr, "texture pack: entry allocation failed\n"); free(f); return; }
        s_ent = grown; s_cap = cap;
    }
    s_ent[s_n++] = (Ent){ ck, h, prio, f };
}
static int ent_cmp(const void *a, const void *b) {
    const Ent *x = a, *y = b;
    if (x->ck != y->ck) return x->ck < y->ck ? -1 : 1;
    if (x->h != y->h) return x->h < y->h ? -1 : 1;
    return y->prio - x->prio;                          /* textures.ini before file names */
}
static const Ent *find(uint64_t ck, uint32_t h) {
    int lo = 0, hi = s_n - 1;
    while (lo <= hi) {
        const int mid = (lo + hi) / 2; const Ent *e = &s_ent[mid];
        if (e->ck == ck && e->h == h) {
            int i = mid;
            while (i > 0 && s_ent[i - 1].ck == ck && s_ent[i - 1].h == h) i--;   /* textures.ini entry first */
            return &s_ent[i];
        }
        if (e->ck < ck || (e->ck == ck && e->h < h)) lo = mid + 1; else hi = mid - 1;
    }
    return NULL;
}

/* The same entries by (low 32 bits of the cache key, data hash): the texture without its address.
 * Heap textures sit 0x600 higher in this port than in PPSSPP (its kernel allocates differently),
 * so their full keys never match; size, palette and the bytes themselves still identify them. */
static int *s_noaddr; static int s_nnoaddr;
static int noaddr_cmp(const void *a, const void *b) {
    const Ent *x = &s_ent[*(const int *)a], *y = &s_ent[*(const int *)b];
    const uint32_t xl = (uint32_t)x->ck, yl = (uint32_t)y->ck;
    if (xl != yl) return xl < yl ? -1 : 1;
    if (x->h != y->h) return x->h < y->h ? -1 : 1;
    return y->prio - x->prio;
}
static const Ent *find_noaddr(uint32_t lo, uint32_t h) {
    int a = 0, b = s_nnoaddr - 1;
    while (a <= b) {
        const int m = (a + b) / 2; const Ent *e = &s_ent[s_noaddr[m]];
        if ((uint32_t)e->ck == lo && e->h == h) return e;
        if ((uint32_t)e->ck < lo || ((uint32_t)e->ck == lo && e->h < h)) a = m + 1; else b = m - 1;
    }
    return NULL;
}

static void trim(char *t) {
    char *a = t; while (*a == ' ' || *a == '\t') a++;
    memmove(t, a, strlen(a) + 1);
    size_t n = strlen(t); while (n && (t[n - 1] == ' ' || t[n - 1] == '\t' || t[n - 1] == '\r' || t[n - 1] == '\n')) t[--n] = 0;
}
static int truthy(const char *v) { return !SDL_strcasecmp(v, "true") || !strcmp(v, "1") || !SDL_strcasecmp(v, "yes"); }

static int load_ini(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "texture pack: cannot read manifest %s (errno %d)\n", path, errno);
        return 0;
    }
    char line[1024], sec[64] = "";
    int malformed = 0;
    while (fgets(line, sizeof(line), f)) {
        /* Reject a split manifest record rather than using a truncated filename. */
        if (!strchr(line, '\n') && !feof(f)) {
            int next = fgetc(f);
            if (next != EOF) {
                fprintf(stderr, "texture pack: overlong manifest line in %s\n", path);
                malformed = 1; break;
            }
        }
        trim(line);
        if (!line[0] || line[0] == ';' || line[0] == '#') continue;
        if (line[0] == '[') { char *e = strchr(line, ']'); if (e) *e = 0; snprintf(sec, sizeof(sec), "%s", line + 1); continue; }
        char *eq = strchr(line, '='); if (!eq) continue;
        *eq = 0; char *k = line, *v = eq + 1; trim(k); trim(v);
        if (!SDL_strcasecmp(sec, "options")) {
            if (!SDL_strcasecmp(k, "hash")) s_hash_kind = !SDL_strcasecmp(v, "xxh32") ? 1 : !SDL_strcasecmp(v, "xxh64") ? 2 : 0;
            else if (!SDL_strcasecmp(k, "ignoreAddress")) s_ignore_addr = truthy(v);
            else if (!SDL_strcasecmp(k, "reduceHash")) s_reduce_on = truthy(v);
        } else if (!SDL_strcasecmp(sec, "hashes")) {
            unsigned long long ck = 0; unsigned h = 0; int level = 0;
            if (sscanf(k, "%16llx%8x_%d", &ck, &h, &level) >= 1 && level == 0) add(ck, h, 1, v, 1);
        } else if (!SDL_strcasecmp(sec, "hashranges") && s_nrange < 256) {
            unsigned a, fw, fh, tw, th;
            if (sscanf(k, "%x,%u,%u", &a, &fw, &fh) == 3 && sscanf(v, "%u,%u", &tw, &th) == 2 && tw && th && tw <= fw && th <= fh)
                s_range[s_nrange++] = (Range){ ((uint64_t)a << 32) | ((uint64_t)fw << 16) | fh, (int)tw, (int)th };
        } else if (!SDL_strcasecmp(sec, "reducehashranges") && s_nreduce < 256) {
            unsigned fw, fh; float rv;
            if (sscanf(k, "%u,%u", &fw, &fh) == 2 && sscanf(v, "%f", &rv) == 1 && rv != 0.0f)
                s_reduce[s_nreduce++] = (Reduce){ ((uint64_t)fw << 16) | fh, rv };
        }
    }
    const int read_error = ferror(f);
    const int close_error = fclose(f);
    if (malformed || read_error || close_error) {
        fprintf(stderr, "texture pack: manifest I/O failed %s\n", path);
        return 0;
    }
    /* PPSSPP turns these off for the quick hash */
    if (s_hash_kind == 0) s_reduce_on = s_ignore_addr = 0;
    return 1;
}
static int scan_root(int *ini, char *ini_path) { /* typed manifest and hash-named PNGs */
    SrHostDir *dir = NULL;
    SrDirError error;
    SrDirEntry entry;
    *ini = 0;
    int status = sr_dir_open(s_dir, &dir, &error);
    if (status != SR_DIR_OK) {
        if (status != SR_DIR_MISSING) pack_error("cannot open", s_dir, &error);
        return status;
    }
    while ((status = sr_dir_next(dir, &entry, &error)) == 1) {
        if (!SDL_strcasecmp(entry.name, "textures.ini") && entry.kind != SR_DIR_REGULAR) {
            error.status = SR_DIR_UNSUPPORTED; error.native_code = 0;
            status = SR_DIR_UNSUPPORTED; break;
        }
        if (entry.kind != SR_DIR_REGULAR) continue;
        if (!SDL_strcasecmp(entry.name, "textures.ini")) {
            *ini = 1; memcpy(ini_path, entry.path, strlen(entry.path) + 1); continue;
        }
        const size_t length = strlen(entry.name);
        if (length < 4 || SDL_strcasecmp(entry.name + length - 4, ".png")) continue;
        const size_t n = length - 4;
        if (n != 24 && !((n == 26 || n == 27) && entry.name[24] == '_')) continue;
        unsigned long long ck = 0; unsigned h = 0; int level = 0;
        /* Enumeration already established regular-file type: do not re-enumerate
         * the entire directory per root image (quadratic for large HD packs). */
        if (sscanf(entry.name, "%16llx%8x_%d", &ck, &h, &level) >= 1 && level == 0) add(ck, h, 0, entry.name, 0);
    }
    if (status < 0) pack_error("cannot enumerate", s_dir, &error);
    SrDirError close_error;
    if (sr_dir_close(dir, &close_error) != SR_DIR_OK) {
        pack_error("cannot close", s_dir, &close_error);
        return close_error.status;
    }
    return status;
}

static void reset_pack(void) {
    for (int i = 0; i < s_n; ++i) free(s_ent[i].file);
    free(s_ent); s_ent = NULL; s_n = s_cap = 0;
    free(s_noaddr); s_noaddr = NULL; s_nnoaddr = 0;
    s_nrange = s_nreduce = s_hash_kind = s_ignore_addr = s_reduce_on = 0;
    s_reduce_global = 0.5f;
}

/* Look for a PPSSPP pack in texdir or texdir/ULUS10567: a textures.ini, or (like PPSSPP without
 * one: quick hash) hash-named images, e.g. PPSSPP's own "Save new textures" output.
 * Returns the number of entries. */
int ppx_init(const char *texdir) {
    char p[SR_DIR_PATH_CAPACITY];
    char game_dir[SR_DIR_PATH_CAPACITY], new_dir[SR_DIR_PATH_CAPACITY];
    SrDirError error;
    reset_pack();
    s_dir[0] = 0;
    if (!texdir || strlen(texdir) >= sizeof(s_dir) ||
            sr_dir_join(texdir, "ULUS10567", game_dir, sizeof(game_dir), &error) != SR_DIR_OK ||
            sr_dir_join(game_dir, "new", new_dir, sizeof(new_dir), &error) != SR_DIR_OK) return 0;
    const char *subs[] = { texdir, game_dir, new_dir };
    for (int i = 0; i < 3; i++) {
        memcpy(s_dir, subs[i], strlen(subs[i]) + 1);
        int ini = 0;
        int status = scan_root(&ini, p);
        if (status == SR_DIR_MISSING) continue;
        if (status < 0 || (ini && !load_ini(p))) { reset_pack(); s_dir[0] = 0; return 0; }
        if (!s_n) { reset_pack(); continue; }
        qsort(s_ent, (size_t)s_n, sizeof(Ent), ent_cmp);
        s_noaddr = malloc((size_t)s_n * sizeof(int)); s_nnoaddr = 0;
        if (!s_noaddr) { reset_pack(); s_dir[0] = 0; return 0; }
        for (int k = 0; k < s_n; k++) if (s_ent[k].h && (s_ent[k].ck >> 32)) s_noaddr[s_nnoaddr++] = k;
        qsort(s_noaddr, (size_t)s_nnoaddr, sizeof(int), noaddr_cmp);
        fprintf(stderr, "texture pack: PPSSPP pack in %s (%d entries, hash %s%s%s)\n", s_dir, s_n,
                s_hash_kind == 1 ? "xxh32" : s_hash_kind == 2 ? "xxh64" : "quick",
                s_ignore_addr ? ", ignoreAddress" : "", s_reduce_on ? ", reduceHash" : "");
        return s_n;
    }
    s_dir[0] = 0;
    return 0;
}

static int log2i(int v) { int n = 0; while ((1 << n) < v && n < 15) n++; return n; }

/* PPSSPP's TextureReplacer::ComputeHash for the current texture */
static uint32_t compute_hash(uint32_t addr, int bufw, int w, int h, int fmt) {
    for (int i = 0; i < s_nrange; i++)
        if (s_range[i].key == (((uint64_t)addr << 32) | ((uint64_t)w << 16) | (uint64_t)h)) { w = s_range[i].w; h = s_range[i].h; break; }
    float red = 1.0f;
    if (s_reduce_on) {
        red = s_reduce_global;
        for (int i = 0; i < s_nreduce; i++) if (s_reduce[i].key == (((uint64_t)w << 16) | (uint64_t)h)) { red = s_reduce[i].v; break; }
        red = red < 0.0f ? 0.0f : red > 1.0f ? 1.0f : red;
    }
    const int ram = (addr & 0x3F000000u) == 0x08000000u || (addr & 0x3F000000u) == 0x09000000u;
    const int vram = (addr & 0x3F800000u) == 0x04000000u;
    if (!ram && !vram) return 0;
    const uint32_t end = ram ? 0x0A000000u : 0x04200000u, a = addr & 0x0FFFFFFFu;
    const uint8_t *p = SR_HOST(addr);
    if (bufw <= w) {
        const uint32_t total = (uint32_t)(bufw * h + (w - bufw));
        const uint32_t size = (uint32_t)((float)((k_bpp[fmt] * total) / 8) * red);
        if (a + size > end) return 0;
        return data_hash(p, size);
    }
    const uint32_t line = (uint32_t)((float)((k_bpp[fmt] * w) / 8) * red), stride = (uint32_t)(k_bpp[fmt] * bufw) / 8;
    if (h > 0 && a + (uint32_t)(h - 1) * stride + line > end) return 0;
    uint32_t r = 0;
    for (int y = 0; y < h; y++) { r = (r * 11) ^ data_hash(p, line); p += stride; }
    return r;
}

/* The replacement image file for the current texture, or NULL. */
const char *ppx_lookup(const GeState *ge) {
    if (!s_n) return NULL;
    const int fmt = (int)(ge->tex_fmt & 0xF);
    if (fmt > 10 || ge->tex_w <= 0 || ge->tex_h <= 0) return NULL;
    const uint32_t addr = (ge->tex_addr & 0x00FFFFF0u) | (ge->tex_addr & 0x0F000000u);
    const int w = ge->tex_w, h = ge->tex_h;
    int bufw = (int)(ge->tex_bufw & k_align[fmt]);
    if (bufw == 0) bufw = 128 / k_bpp[fmt];
    uint64_t ck = ((uint64_t)(addr & 0x3FFFFFFFu) << 32) | (uint64_t)(log2i(w) | (log2i(h) << 8));
    if (fmt & 4) {   /* palette texture: PPSSPP's clutHash_ ^ the CLUT format command word */
        const uint32_t cf = 0xC5000000u | (ge->clut_fmt & 0x00FFFFFFu);
        const uint32_t base = (((cf >> 16) & 0x1F) << 4) * ((cf & 3) == 3 ? 4u : 2u);
        uint32_t ext = ge->clut_load_bytes + base;
        if (ext > ge->clut_max_bytes) ext = ge->clut_max_bytes;
        if (ext > sizeof(ge->clutram)) ext = sizeof(ge->clutram);
        ck ^= (uint32_t)(XXH32(ge->clutram, ext, 0xC0108888) ^ cf);
    }
    const uint32_t hash = compute_hash(addr, bufw, w, h, fmt);
    if (s_ignore_addr) ck &= 0xFFFFFFFFull;
    /* PPSSPP LookupWildcard order */
    const uint64_t lo = ck & 0xFFFFFFFFull, hi = ck & ~0xFFFFFFFFull;
    const struct { uint64_t ck; uint32_t h; int need_addr; } tries[] = {
        { ck, hash, 0 }, { lo, 0, 0 }, { ck, 0, 1 }, { lo, hash, 0 }, { hi, hash, 1 }, { hi, 0, 1 }, { 0, hash, 0 } };
    for (unsigned i = 0; i <= sizeof(tries) / sizeof(tries[0]); i++) {
        const Ent *e;
        if (i < sizeof(tries) / sizeof(tries[0])) {
            if (tries[i].need_addr && s_ignore_addr) continue;
            e = find(tries[i].ck, tries[i].h);
        } else {
            e = hash ? find_noaddr((uint32_t)lo, hash) : NULL;   /* last: the same texture elsewhere */
        }
        if (!e) continue;
        if (!e->file) return NULL;                     /* marked "don't replace" */
        static char path[SR_DIR_PATH_CAPACITY];
        if (!pack_path(e->file, path, 0)) return NULL;
        static int nlog = 0;
        static int all = -1; if (all < 0) all = getenv("SR_PPXLOG") != NULL;
        if (all || nlog++ < 50) fprintf(stderr, "texture pack: %016llx%08x -> %s\n", (unsigned long long)ck, hash, e->file);
        return path;
    }
    if (getenv("SR_PPXLOG")) fprintf(stderr, "texture pack: no match %016llx%08x\n", (unsigned long long)ck, hash);
    return NULL;
}
