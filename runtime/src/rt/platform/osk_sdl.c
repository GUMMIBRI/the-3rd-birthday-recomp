/* SPDX-License-Identifier: GPL-2.0-or-later
 * Controller-operated native OSK over the real SDL frontend modal list UI.
 * hle.c converts each guest UTF-16 unit into one wchar_t, even on 32-bit-wchar
 * hosts. Preserve those units; never reinterpret that buffer as UTF-32 text.
 */
#include <wchar.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

extern int gui_dialog(const char *title, const char *const *items, int n,
                      const char *const *details, int *selected, const char *help);
extern void gui_toast(const char *message);

enum { OSK_MAX_UNITS = 4096, PAGE_CHARACTERS = 6 };
static const char *const character_groups[] = {
    "abcdefghijklmnopqrstuvwxyz",
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ",
    "0123456789",
    "!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~"
};
static const char *const group_names[] = { "Lowercase", "Uppercase", "Digits", "Punctuation" };

/* ASCII display plus explicit UTF-16 escapes. The editing buffer itself is never
 * escaped, normalized or converted, so existing Unicode survives unchanged.
 */
static void show_units(const wchar_t *text, size_t units, char *out, size_t capacity) {
    size_t used = 0;
    if (!out || !capacity) return;
    out[0] = 0;
    for (size_t i = 0; text && i < units && text[i]; ++i) {
        char encoded[16];
        uint32_t unit = (uint32_t)text[i];
        int n;
        if (unit >= 32 && unit <= 126) { encoded[0] = (char)unit; encoded[1] = 0; n = 1; }
        else n = snprintf(encoded, sizeof(encoded), "\\u%04x", (unsigned)unit);
        if (n < 0 || (size_t)n >= sizeof(encoded) || (size_t)n >= capacity - used) break;
        memcpy(out + used, encoded, (size_t)n);
        used += (size_t)n;
        out[used] = 0;
    }
}
static void describe(const wchar_t *working, size_t length, int capacity,
                     const char *prompt, const char *status, char *out, size_t out_size) {
    char preview[80];
    size_t first = length > 12 ? length - 12 : 0;
    /* Do not display half a surrogate pair at the start of the tail preview. */
    if (first && working[first] >= 0xdc00 && working[first] <= 0xdfff &&
            working[first - 1] >= 0xd800 && working[first - 1] <= 0xdbff) --first;
    show_units(working + first, length - first, preview, sizeof(preview));
    snprintf(out, out_size, "%s\nText: %s%s\n%zu / %d UTF-16 units\n%s",
             prompt, first ? "..." : "", preview, length, capacity - 1,
             status && *status ? status : "Existing Unicode is kept; new input is ASCII.");
}
static void erase_last(wchar_t *working, size_t *length) {
    if (!*length) return;
    size_t n = *length;
    if (n >= 2 && working[n - 1] >= 0xdc00 && working[n - 1] <= 0xdfff &&
            working[n - 2] >= 0xd800 && working[n - 2] <= 0xdbff) n -= 2;
    else --n;
    working[n] = 0;
    *length = n;
}
static int add_character(wchar_t *working, size_t *length, int capacity, char character) {
    if (*length >= (size_t)capacity - 1) return 0;
    working[(*length)++] = (wchar_t)(unsigned char)character;
    working[*length] = 0;
    return 1;
}
/* Returns 1 only after explicit human acceptance; 0 cancels/fails and leaves out
 * untouched. No synthetic input, timeout, default acceptance or headless fallback.
 */
int sr_osk_input(const wchar_t *desc, const wchar_t *initial, wchar_t *out, int cap) {
    wchar_t *working;
    size_t length = 0;
    int selected = 0;
    char prompt[96], detail[384];
    const char *status = "";
    static const char *const actions[] = {
        "Add lowercase letters", "Add uppercase letters", "Add digits", "Add punctuation",
        "Add space", "Backspace", "Accept text", "Cancel"
    };
    if (!out || cap < 2 || cap > OSK_MAX_UNITS) {
        fprintf(stderr, "OSK: invalid output capacity %d\n", cap);
        gui_toast("Text entry unavailable: invalid length limit");
        return 0;
    }
    working = (wchar_t *)calloc((size_t)cap, sizeof(*working));
    if (!working) { gui_toast("Text entry unavailable: allocation failed"); return 0; }
    while (initial && initial[length] && length < (size_t)cap - 1) {
        if ((uint32_t)initial[length] > UINT16_MAX) {
            gui_toast("Text entry unavailable: invalid UTF-16 input"); free(working); return 0;
        }
        working[length] = initial[length];
        ++length;
    }
    if (initial && initial[length]) {
        gui_toast("Text entry unavailable: existing text exceeds limit"); free(working); return 0;
    }
    show_units(desc, 128, prompt, sizeof(prompt));
    if (!prompt[0]) snprintf(prompt, sizeof(prompt), "Enter a name");
    for (;;) {
        const char *details[8];
        describe(working, length, cap, prompt, status, detail, sizeof(detail));
        for (size_t i = 0; i < sizeof(details) / sizeof(details[0]); ++i) details[i] = detail;
        int result = gui_dialog("Text entry", actions, 8, details, &selected,
                                "Select: action     Back: cancel text entry");
        if (result != 1 || selected == 7) {
            if (result < 0) fprintf(stderr, "OSK: native dialog unavailable; no text accepted\n");
            free(working); return 0;
        }
        status = "";
        if (selected == 6) {
            memcpy(out, working, (length + 1) * sizeof(*out));
            free(working); return 1;
        }
        if (selected == 5) { erase_last(working, &length); continue; }
        if (selected == 4) {
            if (!add_character(working, &length, cap, ' ')) status = "Text limit reached; backspace to make room.";
            continue;
        }
        if (selected < 0 || selected >= 4) { free(working); return 0; }
        const char *characters = character_groups[selected];
        const int group = selected;
        const size_t count = strlen(characters), pages = (count + PAGE_CHARACTERS - 1) / PAGE_CHARACTERS;
        size_t page = 0;
        int key = 0;
        for (;;) {
            char title[96], labels[PAGE_CHARACTERS][24];
            const char *items[PAGE_CHARACTERS + 3], *key_details[PAGE_CHARACTERS + 3];
            size_t start = page * PAGE_CHARACTERS, visible = count - start;
            if (visible > PAGE_CHARACTERS) visible = PAGE_CHARACTERS;
            snprintf(title, sizeof(title), "%s (%zu / %zu)", group_names[group], page + 1, pages);
            for (size_t i = 0; i < visible; ++i) {
                snprintf(labels[i], sizeof(labels[i]), "Add [%c]", characters[start + i]);
                items[i] = labels[i];
            }
            items[visible] = "Previous page";
            items[visible + 1] = "Next page";
            items[visible + 2] = "Back to keyboard";
            describe(working, length, cap, prompt, status, detail, sizeof(detail));
            for (size_t i = 0; i < visible + 3; ++i) key_details[i] = detail;
            result = gui_dialog(title, items, (int)visible + 3, key_details, &key,
                                "Select: add character     Back: keyboard actions");
            if (result < 0) { fprintf(stderr, "OSK: character dialog unavailable\n"); free(working); return 0; }
            if (!result || key == (int)visible + 2) break;
            status = "";
            if (key >= 0 && (size_t)key < visible) {
                if (!add_character(working, &length, cap, characters[start + (size_t)key]))
                    status = "Text limit reached; return to keyboard and backspace.";
            } else if (key == (int)visible) { page = page ? page - 1 : pages - 1; key = 0; }
            else if (key == (int)visible + 1) { page = (page + 1) % pages; key = 0; }
            else { free(working); return 0; }
        }
    }
}
