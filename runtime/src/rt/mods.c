/* Data mods: memory patches from text files in patches/ next to the executable.
 *
 * Each line:   <hex address>  <u8|u16|u32|f32>  <value>  [once|always]
 *   08C3C700  u32  0x00000001  always     # re-written every vblank (the default)
 *   08C3C704  f32  1.5         once       # written once, at the first vblank
 * '#' starts a comment. Patches change game DATA in memory; addresses inside the game's code
 * do nothing, because the code is recompiled (code mods are C, see mods/README.md and
 * sr_mods_hook below, which scripts/build.sh generates when mods/ has code mods).
 *
 * Texture packs (textures/<hash>.png) are in gpu_sdl3vk/ge_gpu.c. */

#include "recomp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <limits.h>
#include <float.h>
#include "platform/host_dir.h"
#ifdef _WIN32
#include <windows.h>
#endif

typedef struct { uint32_t addr, value; uint8_t size, always; } Patch;
static Patch *s_patch = NULL;
static int s_npatch = 0, s_loaded = 0, s_once_done = 0;

/* Parse the complete line, never accepting a valid prefix of a malformed field.
 * The permitted range is the original game-data interval [08000000,0a000000).
 */
static int parse_patch(char *line, Patch *patch, const char **why) {
    char *tokens[5], *cursor = line, *end;
    size_t count = 0;
    char *comment = strchr(line, '#');
    if (comment) *comment = 0;
    while (*cursor) {
        while (isspace((unsigned char)*cursor)) ++cursor;
        if (!*cursor) break;
        if (count == sizeof(tokens) / sizeof(tokens[0])) { *why = "too many fields"; return -1; }
        tokens[count++] = cursor;
        while (*cursor && !isspace((unsigned char)*cursor)) ++cursor;
        if (*cursor) *cursor++ = 0;
    }
    if (!count) return 0;
    if (count < 3 || count > 4) { *why = "expected address, type, value and optional timing"; return -1; }
    memset(patch, 0, sizeof(*patch));
    if (!strcmp(tokens[1], "u8")) patch->size = 1;
    else if (!strcmp(tokens[1], "u16")) patch->size = 2;
    else if (!strcmp(tokens[1], "u32") || !strcmp(tokens[1], "f32")) patch->size = 4;
    else { *why = "unknown patch type"; return -1; }
    patch->always = 1;
    if (count == 4) {
        if (!strcmp(tokens[3], "once")) patch->always = 0;
        else if (strcmp(tokens[3], "always")) { *why = "timing must be once or always"; return -1; }
    }
    if (tokens[0][0] == '-' || tokens[0][0] == '+') { *why = "invalid address"; return -1; }
    errno = 0;
    unsigned long long address = strtoull(tokens[0], &end, 16);
    if (errno || end == tokens[0] || *end || address > UINT32_MAX) { *why = "invalid address"; return -1; }
    patch->addr = (uint32_t)address;
    if (patch->addr < 0x08000000u || patch->addr > 0x0a000000u - patch->size) {
        *why = "complete patch span is outside game RAM"; return -1;
    }
    errno = 0;
    if (!strcmp(tokens[1], "f32")) {
        float value = strtof(tokens[2], &end);
        if (errno || end == tokens[2] || *end || (value != value || value > FLT_MAX || value < -FLT_MAX)) { *why = "invalid or out-of-range f32 value"; return -1; }
        memcpy(&patch->value, &value, sizeof(value));
    } else {
        unsigned long long value;
        if (tokens[2][0] == '-' || tokens[2][0] == '+') { *why = "invalid unsigned value"; return -1; }
        value = strtoull(tokens[2], &end, 0);
        const uint64_t max_value = patch->size == 1 ? UINT8_MAX : patch->size == 2 ? UINT16_MAX : UINT32_MAX;
        if (errno || end == tokens[2] || *end || value > max_value) { *why = "invalid or out-of-range unsigned value"; return -1; }
        patch->value = (uint32_t)value;
    }
    return 1;
}
static void load_file(const char *path) {
    FILE *file = fopen(path, "r");
    if (!file) { fprintf(stderr, "mods: cannot read %s (errno %d)\n", path, errno); return; }
    char line[256];
    size_t lineno = 0;
    int accepted = 0;
    while (fgets(line, sizeof(line), file)) {
        Patch patch;
        const char *why = NULL;
        ++lineno;
        if (!strchr(line, '\n') && !feof(file)) {
            int c = fgetc(file);
            if (c != EOF && c != '\n') {
                do { c = fgetc(file); } while (c != EOF && c != '\n');
                fprintf(stderr, "mods: %s:%zu: line too long\n", path, lineno);
                continue;
            }
        }
        int parsed = parse_patch(line, &patch, &why);
        if (!parsed) continue;
        if (parsed < 0) { fprintf(stderr, "mods: %s:%zu: %s\n", path, lineno, why); continue; }
        if (s_npatch == INT_MAX || (size_t)s_npatch + 1 > SIZE_MAX / sizeof(Patch)) {
            fprintf(stderr, "mods: patch capacity exceeded in %s\n", path); break;
        }
        Patch *grown = realloc(s_patch, ((size_t)s_npatch + 1) * sizeof(Patch));
        if (!grown) { fprintf(stderr, "mods: patch allocation failed in %s\n", path); break; }
        s_patch = grown; s_patch[s_npatch++] = patch; ++accepted;
    }
    if (ferror(file)) fprintf(stderr, "mods: read failed for %s (errno %d)\n", path, errno);
    fclose(file);
    fprintf(stderr, "mods: %s: %d patches\n", path, accepted);
}
static void load_all(void) {
    s_loaded = 1;
    const char *md = getenv("SR_MODDIR");
#ifdef _WIN32
    char pat[MAX_PATH];
    snprintf(pat, sizeof(pat), "%s/patches/*.txt", md && *md ? md : ".");
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        char path[MAX_PATH * 2];
        snprintf(path, sizeof(path), "%s/patches/%s", md && *md ? md : ".", fd.cFileName);
        load_file(path);
    } while (FindNextFileA(h, &fd));
    FindClose(h);
#else
    char directory[SR_DIR_PATH_CAPACITY];
    SrHostDir *handle;
    SrDirEntry entry;
    SrDirError error;
    if (sr_dir_join(md && *md ? md : ".", "patches", directory, sizeof(directory), &error) != 0) {
        fprintf(stderr, "mods: patch directory invalid (status %d)\n", error.status); return;
    }
    if (sr_dir_open(directory, &handle, &error) != 0) {
        if (error.status != SR_DIR_MISSING)
            fprintf(stderr, "mods: patch directory unavailable (status %d, native %u)\n", error.status, error.native_code);
        return;
    }
    int next;
    while ((next = sr_dir_next(handle, &entry, &error)) > 0) {
        size_t n = strlen(entry.name);
        if (entry.kind != SR_DIR_REGULAR || n < 4 || entry.name[n - 4] != '.' ||
            tolower((unsigned char)entry.name[n - 3]) != 't' ||
            tolower((unsigned char)entry.name[n - 2]) != 'x' ||
            tolower((unsigned char)entry.name[n - 1]) != 't') continue;
        load_file(entry.path);
    }
    if (next < 0) fprintf(stderr, "mods: patch enumeration failed (status %d, native %u)\n", error.status, error.native_code);
    if (sr_dir_close(handle, &error) != 0)
        fprintf(stderr, "mods: patch directory close failed (status %d, native %u)\n", error.status, error.native_code);
#endif
}
static void apply(const Patch *p) {
    switch (p->size) {
        case 1: MEM_W8(p->addr, (uint8_t)p->value); break;
        case 2: MEM_W16(p->addr, (uint16_t)p->value); break;
        default: MEM_W32(p->addr, p->value); break;
    }
}

/* every vblank (gui.c gui_vblank) */
void sr_mods_vblank(void) {
    if (!s_loaded) load_all();
    if (!s_npatch) return;
    for (int i = 0; i < s_npatch; i++)
        if (s_patch[i].always || !s_once_done) apply(&s_patch[i]);
    s_once_done = 1;
}

/* Code mods are compiled into game.dll (with the game code, see tools/prepare.py); driver.c
 * sets this to the DLL's sr_game_mods_hook. */
#if defined(SR_STATIC_CODE_MODS)
/* Compile only generated mods_gen.c with -Dsr_mods_hook=sr_generated_mods_hook.
 * Strong binding makes an omitted enabled dispatcher a link error. */
extern int sr_generated_mods_hook(CpuState *, uint32_t);
int (*g_game_mods_hook)(CpuState *, uint32_t) = sr_generated_mods_hook;
#else
int (*g_game_mods_hook)(CpuState *, uint32_t) = NULL;
#endif
int sr_mods_hook(CpuState *s, uint32_t addr) { return g_game_mods_hook ? g_game_mods_hook(s, addr) : 0; }
