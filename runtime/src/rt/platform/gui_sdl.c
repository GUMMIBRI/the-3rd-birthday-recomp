/* SPDX-License-Identifier: GPL-2.0-or-later
 * Native SDL/Vulkan frontend adapted from the upstream gui.c SDL path.
 * No Win32 fallback: initialization must succeed before gameplay can start.
 */
#include "../recomp.h"
#include "game_addrs.h"
#include "../gpu_sdl3vk/sdl3vk.h"
#include "../gpu_sdl3vk/ge_gpu.h"
#include "clock.h"
#include <SDL3/SDL.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#define PSP_W 480
#define PSP_H 272
static int s_on, s_sdl3;
static uint32_t *s_px;
static uint32_t s_buttons;
static uint8_t s_lx = 128, s_ly = 128;
static float s_rx, s_ry;
static int s_pad_present;
static int64_t s_frequency, s_last;
static void gui_exit(int code) {
    s_on = s_sdl3 = 0;
    gegpu_shutdown();
    sdl3vk_shutdown();
    free(s_px); s_px = NULL;
    _Exit(code);
}
static void gui_fatal(const char *stage) {
    fprintf(stderr, "native frontend: %s failed: %s\n", stage, SDL_GetError());
    fflush(stderr);
    gui_exit(1);
}
void gui_init(const char *title) {
    if (s_on) return;
    if (!sdl3vk_init(title ? title : "The 3rd Birthday")) gui_fatal("SDL/Vulkan initialization");
    s_px = calloc(PSP_W * PSP_H, sizeof(*s_px));
    if (!s_px) gui_fatal("framebuffer allocation");
    const char *gpu = getenv("SR_GPU_GE");
    if (gpu && !strcmp(gpu, "0")) {
        fprintf(stderr, "native frontend: SR_GPU_GE=0 is unsupported; native GPU rendering is required\n");
        gui_exit(1);
    }
    if (!gegpu_init()) gui_fatal("GPU GE initialization");
    sr_clock_setup();
    s_frequency = sr_clock_frequency(); s_last = sr_clock_ticks();
    s_sdl3 = s_on = 1;
}
int gui_on(void) { return s_on; }
uint32_t gui_buttons(void) { return s_buttons; }
void gui_analog(uint8_t *lx, uint8_t *ly) { if (lx) *lx=s_lx; if (ly) *ly=s_ly; }
void gui_rstick(float *rx, float *ry) { if (rx) *rx=s_rx; if (ry) *ry=s_ry; }
void gui_mouse_look(float *yaw, float *pitch) {
    float y=0, p=0;
    if (s_on) sdl3vk_mouse_look(&y,&p);
    if (yaw) *yaw=y;
    if (pitch) *pitch=p;
}
void gui_toast(const char *msg) {
    if (!msg) return;
    fprintf(stderr, "toast: %s\n", msg);
    if (s_on) sdl3vk_toast(msg,3000);
}
int gui_cfg_get(const char *key,int def) { return s_on ? sdl3vk_cfg_get(key,def) : def; }
int gui_cam_reset(void) { return s_on ? sdl3vk_cam_reset() : 0; }
int gui_sens_steps(void) { return s_on ? sdl3vk_sens_steps() : 0; }
int gui_pad_present(void) { return s_pad_present; }
static int framebuffer_valid(uint32_t address, int format, uint32_t stride) {
    const uint64_t start=SR_PHYS(address);
    if (!g_mem || format < 0 || format > 3 || stride < PSP_W || stride > 2048) return 0;
    const uint64_t pixel_bytes=format==3 ? 4u : 2u;
    const uint64_t end=start+((uint64_t)(PSP_H-1)*stride+PSP_W)*pixel_bytes;
    if (start & (pixel_bytes-1)) return 0;
    return (start >= 0x04000000u && end <= 0x04200000u) ||
           (start >= 0x08000000u && end <= 0x0a000000u);
}
/* PSP framebuffer unpacking for CPU-written movie/menu frames, not CPU emulation.
 * Preserve upstream color expansion and the BGRA-word presenter contract. */
static void convert_fb(uint32_t address, int format, uint32_t stride) {
    if (!framebuffer_valid(address,format,stride)) {
        fprintf(stderr,"native frontend: invalid framebuffer %08x format %d stride %u\n",address,format,stride);
        gui_exit(1);
    }
    for (int y=0; y<PSP_H; ++y) for (int x=0; x<PSP_W; ++x) {
        const uint32_t at=(uint32_t)y*stride+(uint32_t)x;
        uint32_t r,g,b;
        if (format==3) {
            uint32_t p=sr_r32(address+at*4); r=p&255; g=(p>>8)&255; b=(p>>16)&255;
        } else {
            uint16_t p=sr_r16(address+at*2);
            if (format==1) { r=(p&31)<<3; g=((p>>5)&31)<<3; b=((p>>10)&31)<<3; }
            else if (format==2) { r=(p&15)<<4; g=((p>>4)&15)<<4; b=((p>>8)&15)<<4; }
            else { r=(p&31)<<3; g=((p>>5)&63)<<2; b=((p>>11)&31)<<3; }
        }
        s_px[y*PSP_W+x]=(r<<16)|(g<<8)|b;
    }
}
static int present_frame(uint32_t address,int format,uint32_t stride) {
    int shown=gegpu_present(address,format,stride);
    if (shown<0) { convert_fb(address,format,stride); shown=sdl3vk_present_rgba(s_px); }
    return shown;
}
/* ---- frame interpolation presentation (see gui_present) ---------------------------------- */
static int      s_fi_phase = 0;         /* 1: in-between frame due, 2: the held real frame due */
static void    *s_fi_img = NULL;
static uint32_t s_fi_fb = 0, s_fi_stride = 0;
static int      s_fi_fmt = 0;

static void fi_show_held(void) {
    int shown = 1;
    if (s_fi_phase == 1) shown = gegpu_present_interp(s_fi_img);
    if (shown != 0) {
        shown = gegpu_present(s_fi_fb, s_fi_fmt, s_fi_stride);
        if (shown < 0) { convert_fb(s_fi_fb, s_fi_fmt, s_fi_stride); shown = sdl3vk_present_rgba(s_px); }
    }
    s_fi_phase = 0;
    if (shown == 0) { gui_exit(0); }
}

/* Called by the scheduler on every vblank. */
void gui_vblank(void) {
    { extern void sr_mods_vblank(void); sr_mods_vblank(); }   /* data mods: memory patches */
    { extern void sr_cheats_vblank(void); sr_cheats_vblank(); }   /* Esc menu > Cheats */
    if (!s_sdl3 || !s_fi_phase) return;
    { extern int sched_fast_forward(void); if (sched_fast_forward()) { s_fi_phase = 0; return; } }
    /* SR_FI_SHOT=n: save the real frame before the n-th in-between frame, that frame and the
     * real frame after it as screenshots (checking the interpolation) */
    {
        static long at = -2, n = 0;
        static int on_move = 0;
        if (at == -2) {
            const char *e = getenv("SR_FI_SHOT");
            at = e ? atol(e) : -1;
            on_move = e && !strcmp(e, "move");     /* SR_FI_SHOT=move: while Aya walks */
        }
        if (on_move && at == 0 && sr_twinstick_setting(SR_TS_MOVING) != 0.0f && s_fi_phase == 2) {
            static int seen = 0;
            if (++seen == 20) at = n + 2;           /* well into the walk, not its first step */
        }
        /* SR_FI_SHOT=turn: the first in-between frame while the camera is turning */
        if (at == 0 && sr_twinstick_setting(SR_TS_TURNING) != 0.0f && s_fi_phase == 2) at = n + 2;
        if (at > 0 && ((s_fi_phase == 2 && (n == at - 1 || n == at)) || (s_fi_phase == 1 && n + 1 == at)))
            sdl3vk_request_screenshot();
        if (s_fi_phase == 1) n++;
    }
    int shown;
    if (s_fi_phase == 1) {
        shown = gegpu_present_interp(s_fi_img);
        s_fi_phase = 2;
    } else {
        shown = gegpu_present(s_fi_fb, s_fi_fmt, s_fi_stride);
        if (shown < 0) { convert_fb(s_fi_fb, s_fi_fmt, s_fi_stride); shown = sdl3vk_present_rgba(s_px); }
        s_fi_phase = 0;
    }
    if (shown == 0) { gui_exit(0); }
}

/* ---- in-game settings menu (Esc, or Back+Start on a controller) --------------------------
 * The game is paused while it is open: this loop keeps re-presenting the last frame with the
 * menu drawn over it and only returns when the menu closes. */
static int s_input_block = 0;

/* Controls page: every action with two keys and a controller button. Left/Right picks the
 * column, Enter / A waits for the new key, mouse button or controller button, Delete / X(pad)
 * clears, Esc goes back. */
static void controls_run(uint32_t fbaddr, int fmt, uint32_t stride) {
    enum { VIS = 10 };
    const int nb = sdl3vk_bind_count(), no = sdl3vk_input_opt_count();
    const int n = nb + no, rows = n + 2;               /* bindings, options, "Reset to defaults", "Back" */
    int cur = 0, col = 0, top = 0;
    for (;;) {
        if (cur < top) top = cur;
        if (cur >= top + VIS) top = cur - VIS + 1;
        char buf[16][96]; const char *lines[16]; int k = 0, selline = 2;
        const int cap = sdl3vk_bind_capturing();
        snprintf(buf[k++], 96, "Controls");
        snprintf(buf[k++], 96, "Action\tKey / mouse\tKey / mouse\tController");
        for (int r = top; r < rows && r < top + VIS; r++) {
            if (r == cur) selline = k;
            if (r == n) { snprintf(buf[k++], 96, "Reset to defaults"); continue; }
            if (r == n + 1) { snprintf(buf[k++], 96, "Back"); continue; }
            if (r >= nb) { sdl3vk_input_opt_text(r - nb, buf[k++], 96); continue; }
            char c[3][40];
            for (int j = 0; j < 3; j++) {
                char t[32]; sdl3vk_bind_text(r, j, t, sizeof(t));
                if (r == cur && j == col) snprintf(c[j], 40, cap ? "[ press... ]" : "[ %s ]", t[0] ? t : "-");
                else snprintf(c[j], 40, "%s", t[0] ? t : "-");
            }
            snprintf(buf[k++], 96, "%s\t%s\t%s\t%s", sdl3vk_bind_label(r), c[0], c[1], c[2]);
        }
        snprintf(buf[k++], 96, " ");
        snprintf(buf[k++], 96, cap ? (col < 2 ? "Press the new key or mouse button   (Esc: cancel)"
                                              : "Press the new controller button   (Guide: cancel)")
                                   : "Enter / A: change     Delete / X: clear     Esc / B: back");
        for (int i = 0; i < k; i++) lines[i] = buf[i];
        sdl3vk_menu_show(lines, k, selline);
        int shown = present_frame(fbaddr, fmt, stride);
        if (shown == 0) { gui_exit(0); }
        const int ev = sdl3vk_menu_events();
        if (!cap) {
            if (ev & (SDL3VK_MENU_TOGGLE | SDL3VK_MENU_BACK)) break;
            if (ev & SDL3VK_MENU_UP)    cur = cur > 0 ? cur - 1 : rows - 1;
            if (ev & SDL3VK_MENU_DOWN)  cur = cur < rows - 1 ? cur + 1 : 0;
            const int opt = cur >= nb && cur < n;
            if (opt && (ev & (SDL3VK_MENU_LEFT | SDL3VK_MENU_RIGHT | SDL3VK_MENU_OK)))
                sdl3vk_input_opt_change(cur - nb, (ev & SDL3VK_MENU_LEFT) ? -1 : 1);
            else {
                if (ev & SDL3VK_MENU_LEFT)  col = col > 0 ? col - 1 : 2;
                if (ev & SDL3VK_MENU_RIGHT) col = col < 2 ? col + 1 : 0;
                if (ev & SDL3VK_MENU_OK) {
                    if (cur == n) sdl3vk_bind_reset();
                    else if (cur == n + 1) break;
                    else sdl3vk_bind_capture(cur, col);
                }
            }
            if ((ev & SDL3VK_MENU_CLEAR) && cur < nb) sdl3vk_bind_clear(cur, col);
        }
        SDL_Delay(15);
    }
    (void)sdl3vk_menu_events();
}

/* Cheats page (cheats.c): each cheat on/off, saved in cheats.cfg. */
static void cheats_run(uint32_t fbaddr, int fmt, uint32_t stride) {
    extern int sr_cheat_count(void); extern void sr_cheat_text(int, char *, int);
    extern void sr_cheat_toggle(int); extern void sr_cheats_all_off(void);
    const int n = sr_cheat_count(), rows = n + 2;      /* cheats, "All off", "Back" */
    int cur = 0;
    for (;;) {
        char buf[16][96]; const char *lines[16]; int k = 0, selline = 1;
        snprintf(buf[k++], 96, "Cheats");
        for (int r = 0; r < rows; r++) {
            if (r == cur) selline = k;
            if (r < n) sr_cheat_text(r, buf[k++], 96);
            else snprintf(buf[k++], 96, r == n ? "All off" : "Back");
        }
        snprintf(buf[k++], 96, " ");
        snprintf(buf[k++], 96, "They stay on until you switch them off (cheats.cfg).");
        snprintf(buf[k++], 96, "Enter / A: on / off     Esc / B: back");
        for (int i = 0; i < k; i++) lines[i] = buf[i];
        sdl3vk_menu_show(lines, k, selline);
        int shown = present_frame(fbaddr, fmt, stride);
        if (shown == 0) { gui_exit(0); }
        const int ev = sdl3vk_menu_events();
        if (ev & (SDL3VK_MENU_TOGGLE | SDL3VK_MENU_BACK)) break;
        if (ev & SDL3VK_MENU_UP)   cur = cur > 0 ? cur - 1 : rows - 1;
        if (ev & SDL3VK_MENU_DOWN) cur = cur < rows - 1 ? cur + 1 : 0;
        if (ev & (SDL3VK_MENU_OK | SDL3VK_MENU_LEFT | SDL3VK_MENU_RIGHT)) {
            if (cur < n) sr_cheat_toggle(cur);
            else if (cur == n) { if (ev & SDL3VK_MENU_OK) sr_cheats_all_off(); }
            else if (ev & SDL3VK_MENU_OK) break;
        }
        SDL_Delay(15);
    }
    (void)sdl3vk_menu_events();
}

static void menu_run(uint32_t fbaddr, int fmt, uint32_t stride) {
    enum { M_RESUME = 1, M_CONTROLS, M_CHEATS, M_CAMSTYLE, M_FREEAIM, M_SPEED, M_INVX, M_INVY, M_RECENTER, M_RES, M_WIDE, M_FPS, M_SHOWFPS, M_GLOW, M_INTRO, M_FULL, M_SHOT, M_QUIT, M_COUNT };
    int sel = M_RESUME;
    const int active_scale = sdl3vk_render_scale();
    static int boot_cfg = -1;                      /* render_scale setting the game started with */
    if (boot_cfg < 0) boot_cfg = sdl3vk_render_scale_cfg();
    for (;;) {
        char buf[M_COUNT + 1][96];
        const char *lines[M_COUNT + 1];
        const int cfg_scale = sdl3vk_render_scale_cfg();
        snprintf(buf[0],96,"Settings (game paused) - PS5 development");
        snprintf(buf[M_RESUME], 96, "Resume");
        snprintf(buf[M_CONTROLS], 96, "Controls...");
        snprintf(buf[M_CHEATS], 96, "Cheats...");
        snprintf(buf[M_CAMSTYLE], 96, "Camera style:  %s", sr_twinstick_setting(SR_TS_STYLE) != 0.0f
                 ? "Modern (over the shoulder, free look)" : "Classic (orbit, returns behind Aya)");
        snprintf(buf[M_FREEAIM], 96, "Free aim (mouse / right stick while aiming):  %s", sr_twinstick_setting(SR_TS_FREE_AIM) != 0.0f ? "On" : "Off");
        snprintf(buf[M_SPEED], 96, "Camera speed:  %.2f", sr_twinstick_setting(SR_TS_SENSITIVITY));
        snprintf(buf[M_INVX], 96, "Invert camera left/right:  %s", sr_twinstick_setting(SR_TS_INVERT_X) != 0.0f ? "On" : "Off");
        snprintf(buf[M_INVY], 96, "Invert camera up/down:  %s", sr_twinstick_setting(SR_TS_INVERT_Y) != 0.0f ? "On" : "Off");
        snprintf(buf[M_RECENTER], 96, "Camera returns behind Aya:  %s", sr_twinstick_setting(SR_TS_RECENTER) != 0.0f ? "On" : "Off");
        const char *restart = cfg_scale != boot_cfg ? "  (restart to apply)" : "";
        if (cfg_scale <= 0)
            snprintf(buf[M_RES], 96, "Resolution:  Auto  (now %dx)%s", active_scale, restart);
        else
            snprintf(buf[M_RES], 96, "Resolution:  %dx  (%dx%d)%s", cfg_scale, 480 * cfg_scale, 272 * cfg_scale, restart);
        snprintf(buf[M_WIDE], 96, "Aspect ratio:  %s", sdl3vk_cfg_get("widescreen", 0) ? "Fill the window (wider view)" : "Original");
        snprintf(buf[M_FPS], 96, "Frame rate:  %s", gegpu_interp_enabled() ? "60 (smooth)" : "30 (original)");
        snprintf(buf[M_INTRO], 96, "Skip intro at start:  %s", sdl3vk_cfg_get("skip_intro", 0) ? "On" : "Off");
        snprintf(buf[M_GLOW], 96, "Glow effect (bloom):  %s", sdl3vk_glow() ? "On (original)" : "Off");
        snprintf(buf[M_SHOWFPS], 96, "Show FPS counter:  %s", sdl3vk_show_fps() ? "On" : "Off");
        snprintf(buf[M_FULL], 96, "Fullscreen:  %s", sdl3vk_fullscreen() ? "On" : "Off");
        snprintf(buf[M_SHOT], 96, "Save screenshot  (F12)");
        snprintf(buf[M_QUIT], 96, "Quit game");
        for (int i = 0; i < M_COUNT; i++) lines[i] = buf[i];
        snprintf(buf[M_COUNT], 96, "Esc / B: back     Enter / A: select     Left / Right: change");
        lines[M_COUNT] = buf[M_COUNT];
        sdl3vk_menu_show(lines, M_COUNT + 1, sel);

        int shown = present_frame(fbaddr, fmt, stride);
        if (shown == 0) { gui_exit(0); }

        const int ev = sdl3vk_menu_events();
        if (ev && getenv("SR_MENULOG")) fprintf(stderr, "menu: events 0x%x sel %d\n", ev, sel);
        if (ev & (SDL3VK_MENU_TOGGLE | SDL3VK_MENU_BACK)) break;
        if (ev & SDL3VK_MENU_UP)   sel = sel > M_RESUME ? sel - 1 : M_COUNT - 1;
        if (ev & SDL3VK_MENU_DOWN) sel = sel < M_COUNT - 1 ? sel + 1 : M_RESUME;
        const int dir = (ev & SDL3VK_MENU_RIGHT) ? 1 : (ev & SDL3VK_MENU_LEFT) ? -1 : (ev & SDL3VK_MENU_OK) ? 1 : 0;
        if (dir) {
            switch (sel) {
            case M_RESUME: if (ev & SDL3VK_MENU_OK) goto done; break;
            case M_CONTROLS: if (ev & SDL3VK_MENU_OK) controls_run(fbaddr, fmt, stride); break;
            case M_CHEATS: if (ev & SDL3VK_MENU_OK) cheats_run(fbaddr, fmt, stride); break;
            case M_CAMSTYLE: sr_twinstick_set(SR_TS_STYLE, sr_twinstick_setting(SR_TS_STYLE) == 0.0f); break;
            case M_FREEAIM: sr_twinstick_set(SR_TS_FREE_AIM, sr_twinstick_setting(SR_TS_FREE_AIM) == 0.0f); break;
            case M_SPEED:
                sr_twinstick_set(SR_TS_SENSITIVITY, sr_twinstick_setting(SR_TS_SENSITIVITY) * (dir > 0 ? 1.15f : 1.0f / 1.15f));
                break;
            case M_INVX: sr_twinstick_set(SR_TS_INVERT_X, sr_twinstick_setting(SR_TS_INVERT_X) == 0.0f); break;
            case M_INVY: sr_twinstick_set(SR_TS_INVERT_Y, sr_twinstick_setting(SR_TS_INVERT_Y) == 0.0f); break;
            case M_RECENTER: sr_twinstick_set(SR_TS_RECENTER, sr_twinstick_setting(SR_TS_RECENTER) == 0.0f); break;
            case M_RES: {
                int v = cfg_scale + dir;
                if (v < 0) v = 6;
                if (v > 6) v = 0;
                sdl3vk_set_render_scale_cfg(v);
                break;
            }
            case M_GLOW: sdl3vk_set_glow(!sdl3vk_glow()); break;
            case M_SHOWFPS: sdl3vk_set_show_fps(!sdl3vk_show_fps()); break;
            case M_WIDE: sdl3vk_set_widescreen(!sdl3vk_cfg_get("widescreen", 0)); break;
            case M_INTRO: sdl3vk_cfg_set("skip_intro", !sdl3vk_cfg_get("skip_intro", 0)); break;
            case M_FPS: {
                const int on = !gegpu_interp_enabled();
                gegpu_interp_set(on);
                sdl3vk_cfg_set("frame_interpolation", on);
                break;
            }
            case M_FULL: sdl3vk_set_fullscreen(!sdl3vk_fullscreen()); break;
            case M_SHOT: if (ev & SDL3VK_MENU_OK) sdl3vk_request_screenshot(); break;
            case M_QUIT: if (ev & SDL3VK_MENU_OK) { gui_exit(0); } break;
            default: break;
            }
        }
        SDL_Delay(15);
    }
done:
    sdl3vk_menu_show(NULL, 0, 0);
    s_input_block = 1;
}

static uint32_t s_last_fb, s_last_stride = 512; static int s_last_fmt = 3, s_last_ok = 0;

/* Modal list dialog over the last game frame (the PSP system save/load screens). Items are
 * single lines; details[i] (may hold '
') is shown under the list for the selected item.
 * Returns 1 = chosen (*sel), 0 = backed out, -1 = no window (headless: caller decides). */
int gui_dialog(const char *title, const char *const *items, int n, const char *const *details,
               int *sel, const char *help) {
    if (!s_on || !s_sdl3 || !s_last_ok || !items || !sel || n <= 0) return -1;
    enum { VIS = 7 };
    int cur = *sel < 0 || *sel >= n ? 0 : *sel, top = 0;
    (void)sdl3vk_menu_events();                    /* drop presses made before the dialog */
    int result = 0;
    for (;;) {
        if (cur < top) top = cur;
        if (cur >= top + VIS) top = cur - VIS + 1;
        char buf[16][96]; const char *lines[16]; int k = 0, selline = 1;
        snprintf(buf[k], 96, "%s", title ? title : "Select"); k++;
        for (int i = top; i < n && i < top + VIS; i++) {
            if (i == cur) selline = k;
            snprintf(buf[k], 96, "%s%s", items[i], (i == top + VIS - 1 && i < n - 1) ? "   ..." : ""); k++;
        }
        snprintf(buf[k], 96, " "); k++;
        if (details && details[cur]) {
            const char *d = details[cur];
            while (*d && k < 15) {
                const char *e = strchr(d, '\n'); int len = e ? (int)(e - d) : (int)strlen(d);
                if (len > 90) len = 90;
                snprintf(buf[k], 96, "   %.*s", len, d); k++;
                if (!e) break;
                d = e + 1;
            }
        }
        if (k < 16) { snprintf(buf[k], 96, "%s", help ? help : "X / Enter: select     Z / Backspace: back"); k++; }
        for (int i = 0; i < k; i++) lines[i] = buf[i];
        sdl3vk_menu_show(lines, k, selline);
        int shown = present_frame(s_last_fb, s_last_fmt, s_last_stride);
        if (shown == 0) { gui_exit(0); }
        int ev = sdl3vk_menu_events();
        if (ev & SDL3VK_MENU_UP)   cur = cur > 0 ? cur - 1 : n - 1;
        if (ev & SDL3VK_MENU_DOWN) cur = cur < n - 1 ? cur + 1 : 0;
        if (ev & SDL3VK_MENU_OK)   { result = 1; break; }
        if (ev & (SDL3VK_MENU_BACK | SDL3VK_MENU_TOGGLE)) { result = 0; break; }
        SDL_Delay(15);
    }
    sdl3vk_menu_show(NULL, 0, 0);
    s_input_block = 1;                             /* the confirming press must not reach the game */
    *sel = cur;
    return result;
}


extern int sched_vbl_paced(void);
void gui_present(uint32_t fbaddr,int fmt,uint32_t stride) {
    if (!s_on) return;
    if (!stride) stride=512;
    if (!framebuffer_valid(fbaddr,fmt,stride)) {
        fprintf(stderr,"native frontend: refusing invalid framebuffer %08x/%d/%u\n",fbaddr,fmt,stride);
        gui_exit(1);
    }
    s_last_fb=fbaddr; s_last_fmt=fmt; s_last_stride=stride; s_last_ok=1;
    { extern volatile int g_drawlog_on; if (g_drawlog_on) --g_drawlog_on; }
    { extern float g_ge_ws; if (s_sdl3) g_ge_ws = sdl3vk_widescreen_factor(); }   /* next frame's squeeze */
    if (s_sdl3) sdl3vk_fps_game_frame();
    { extern uint32_t g_ge_skip_glow; g_ge_skip_glow = (s_sdl3 && !sdl3vk_glow()) ? ADDR_GLOW_BUFFER : 0; }
    if (s_sdl3) {
        /* GPU-resident framebuffer: blit straight from the GE's target image. Falls back
         * to the guest-VRAM convert for CPU-written frames (movies).
         * With frame interpolation, a game frame finished every second vblank (the game's
         * 30 fps) is not shown now: the next vblank shows a frame halfway between it and the
         * previous one, the vblank after that shows the frame itself (gui_vblank). */
        int shown = 1;
        void *mid = NULL;
        extern int sched_fast_forward(void);
        if (sched_fast_forward()) {           /* intro skip: show every 4th frame, no waiting */
            static unsigned ffn = 0;
            s_fi_phase = 0;
            if ((ffn++ & 3) != 0) return;
            shown = gegpu_present(fbaddr, fmt, stride);
            if (shown < 0) { convert_fb(fbaddr, fmt, stride); shown = sdl3vk_present_rgba(s_px); }
            if (shown == 0) { gui_exit(0); }
            s_buttons = sdl3vk_buttons();
            sdl3vk_analog(&s_lx, &s_ly);
            sdl3vk_rstick(&s_rx, &s_ry);
            return;
        }
        { extern int g_ge_interp_on; g_ge_interp_on = gegpu_interp_enabled(); }
        if (gegpu_interp_enabled()) {
            extern uint64_t sched_vblank_count(void);
            static uint64_t last_flip = 0;
            const uint64_t vb = sched_vblank_count();
            if (s_fi_phase) fi_show_held();              /* still holding the previous frame */
            /* A host that can't keep up with 60 fps here (vblanks keep arriving late) would slow
             * the whole game down, sound included: show the game's own 30 fps for a while, then
             * try 60 again (waiting longer each time it fails, up to a minute). */
            static int drop = 0; static double retry_at = 0.0, hold = 4.0, calm_since = 0.0;
            {
                extern float sched_late_fraction(void);
                const double now_s=(double)sr_clock_ticks()/(double)s_frequency;
                static int adapt = -1;                     /* SR_INTERP_ADAPT=0: never drop (tests) */
                if (adapt < 0) { const char *e = getenv("SR_INTERP_ADAPT"); adapt = !(e && e[0] == '0'); }
                const float late = adapt ? sched_late_fraction() : 0.0f;
                if (!drop && late > 0.3f) {
                    drop = 1; retry_at = now_s + hold;
                    fprintf(stderr, "interp: falling behind (%.0f%% of vblanks late), 30 fps for %.0f s\n", late * 100.0f, hold);
                    if (hold < 60.0) hold *= 2.0;
                } else if (drop && now_s >= retry_at && late < 0.05f) {
                    drop = 0;
                    fprintf(stderr, "interp: trying 60 fps again\n");
                }
                if (drop || late >= 0.02f) calm_since = 0.0;   /* 60 fps holding a minute: forget old failures */
                else if (calm_since == 0.0) calm_since = now_s;
                else if (now_s - calm_since > 60.0) hold = 4.0;
            }
            mid = gegpu_interp_frame(fbaddr, fmt, !drop && vb - last_flip == 2 ? 0.5f : -1.0f);
            { extern void ge_interp_frame(void); ge_interp_frame(); }   /* skinned-draw tables */
            last_flip = vb;
        }
        if (mid) {
            s_fi_img = mid; s_fi_fb = fbaddr; s_fi_fmt = fmt; s_fi_stride = stride; s_fi_phase = 1;
        } else {
            shown = gegpu_present(fbaddr, fmt, stride);
            if (shown < 0) {
                convert_fb(fbaddr, fmt, stride);
                shown = sdl3vk_present_rgba(s_px);
            }
        }
        if (shown == 0) { gui_exit(0); }
        /* SR_PACELOG=1: every 5 s, how evenly frames were presented (stutter hunting) */
        {
            static int on = -1;
            if (on < 0) on = getenv("SR_PACELOG") ? 1 : 0;
            if (on) {
                static int64_t prev, t0;
                static unsigned n, slow, vslow; static double maxgap;
                const int64_t now=sr_clock_ticks();
                if (prev) {
                    double g = (double)(now - prev) * 1000.0 / (double)s_frequency;
                    n++; if (g > 40.0) slow++; if (g > 70.0) vslow++; if (g > maxgap) maxgap = g;
                } else t0 = now;
                prev = now;
                if ((double)(now - t0) / (double)s_frequency >= 5.0) {
                    extern uint64_t sched_vblank_count(void); extern float sched_late_fraction(void);
                    static uint64_t vb0 = 0; const uint64_t vbn = sched_vblank_count();
                    fprintf(stderr, "PACE 5s: presents %u, gaps >40ms %u, >70ms %u, max %.0f ms, vblanks %llu (300 = full speed), late %.0f%%\n",
                            n, slow, vslow, maxgap, (unsigned long long)(vbn - vb0), sched_late_fraction() * 100.0f);
                    vb0 = vbn;
                    n = slow = vslow = 0; maxgap = 0; t0 = now;
                }
            }
        }
        if (sdl3vk_menu_events() & SDL3VK_MENU_TOGGLE) { s_fi_phase = 0; menu_run(fbaddr, fmt, stride); }
        s_buttons = sdl3vk_buttons();
        sdl3vk_analog(&s_lx, &s_ly);
        sdl3vk_rstick(&s_rx, &s_ry);
        s_pad_present = sdl3vk_pad_present();
        if (s_input_block) {          /* after the menu: ignore input until everything is released */
            if (s_buttons == 0 && s_lx > 88 && s_lx < 168 && s_ly > 88 && s_ly < 168) s_input_block = 0;
            else { s_buttons = 0; s_lx = s_ly = 128; s_rx = s_ry = 0.0f; }
        }
    }
    if (sched_vbl_paced()) { s_last=sr_clock_ticks(); return; }
    const int64_t now=sr_clock_ticks();
    const double elapsed=(double)(now-s_last)/(double)s_frequency;
    if (elapsed<1.0/60.0) {
        const uint32_t ms=(uint32_t)((1.0/60.0-elapsed)*1000.0);
        if (ms && ms<100) SDL_Delay(ms);
    }
    s_last=sr_clock_ticks();
}
