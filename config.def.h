#ifndef CONFIG_DEF_H
#define CONFIG_DEF_H

#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <stdbool.h>

#define GAP 10
#define BORDER_WIDTH 2
#define TITLE_HEIGHT 25
#define BAR_HEIGHT 30
#define WINDOW_HEIGHT_RATIO 80

#define SCROLL_STEP 50
#define ANIMATION_STEPS 15
#define ANIMATION_DELAY 8000

#define DEFAULT_DESKTOPS 4
#define MAX_DESKTOPS 10

#define PRIMARY_MONITOR "eDP-1"

#define COLOR_BACKGROUND 0x1A1410
#define COLOR_FOREGROUND 0xFFFFFF
#define COLOR_BORDER_ACTIVE 0xD4A05A
#define COLOR_BORDER_INACTIVE 0x2B1E16
#define COLOR_TITLE_BG 0x2B1E16
#define COLOR_TITLE_TEXT 0xFFFFFF
#define COLOR_BAR_BG 0x1A1410
#define COLOR_BAR_ACTIVE 0xD4A05A
#define COLOR_BAR_INACTIVE 0x555555
#define COLOR_BAR_TEXT 0xFFFFFF

#define DESKTOP_COLORS { \
    0x1A1410, 0x0D1B2A, 0x1A0D1B, 0x1B1A0D, 0x0D1B1A, \
    0x1A0D0D, 0x0D0D1B, 0x1B0D1A, 0x0D1B0D, 0x1B1B0D \
}

static const char *st[] = {"st", NULL};
static const char *dmenu[] = {"dmenu_run", "-p", "Run:", "-fn", "monospace-12", 
                              "-nb", "#1A1410", "-nf", "#FFFFFF", 
                              "-sb", "#D4A05A", "-sf", "#000000", NULL};
static const char *firefox[] = {"firefox", NULL};
static const char *thunar[] = {"thunar", NULL};
static const char *spotify[] = {"spotify", NULL};

#define MODKEY Mod4Mask
#define MODKEY_ALT Mod1Mask
#define MODKEY_CTRL ControlMask
#define MODKEY_SHIFT ShiftMask

typedef union {
    void (*func)(void *);
    const char **cmd;
} KeyFunc;

typedef struct {
    unsigned int mod;
    KeySym keysym;
    KeyFunc func;
    const char *description;
    bool is_cmd;
} Key;

void close_window_action(void *data);
void scroll_down_action(void *data);
void scroll_up_action(void *data);
void scroll_page_down_action(void *data);
void scroll_page_up_action(void *data);
void next_desktop_action(void *data);
void prev_desktop_action(void *data);
void toggle_float_action(void *data);
void toggle_sticky_action(void *data);
void focus_window_action(void *data);
void move_up_action(void *data);
void move_down_action(void *data);
void reload_config_action(void *data);
void show_help_action(void *data);

void switch_desktop_action_1(void *data);
void switch_desktop_action_2(void *data);
void switch_desktop_action_3(void *data);
void switch_desktop_action_4(void *data);
void switch_desktop_action_5(void *data);
void switch_desktop_action_6(void *data);
void switch_desktop_action_7(void *data);
void switch_desktop_action_8(void *data);
void switch_desktop_action_9(void *data);
void switch_desktop_action_10(void *data);

void send_to_desktop_action_1(void *data);
void send_to_desktop_action_2(void *data);
void send_to_desktop_action_3(void *data);
void send_to_desktop_action_4(void *data);
void send_to_desktop_action_5(void *data);
void send_to_desktop_action_6(void *data);
void send_to_desktop_action_7(void *data);
void send_to_desktop_action_8(void *data);
void send_to_desktop_action_9(void *data);
void send_to_desktop_action_10(void *data);

static const Key keys[] = {
    { MODKEY, XK_q, { .func = close_window_action }, "Close window", false },
    { MODKEY, XK_f, { .func = focus_window_action }, "Focus window", false },
    { MODKEY, XK_space, { .func = toggle_float_action }, "Toggle float", false },
    { MODKEY | MODKEY_CTRL, XK_s, { .func = toggle_sticky_action }, "Toggle sticky", false },
    { MODKEY, XK_j, { .func = scroll_down_action }, "Scroll down", false },
    { MODKEY, XK_k, { .func = scroll_up_action }, "Scroll up", false },
    { MODKEY | MODKEY_CTRL, XK_j, { .func = scroll_page_down_action }, "Page down", false },
    { MODKEY | MODKEY_CTRL, XK_k, { .func = scroll_page_up_action }, "Page up", false },
    { MODKEY | MODKEY_CTRL, XK_Right, { .func = next_desktop_action }, "Next desktop", false },
    { MODKEY | MODKEY_CTRL, XK_Left, { .func = prev_desktop_action }, "Prev desktop", false },
    { MODKEY, XK_Up, { .func = move_up_action }, "Move up", false },
    { MODKEY, XK_Down, { .func = move_down_action }, "Move down", false },
    { MODKEY, XK_1, { .func = switch_desktop_action_1 }, "Desktop 1", false },
    { MODKEY, XK_2, { .func = switch_desktop_action_2 }, "Desktop 2", false },
    { MODKEY, XK_3, { .func = switch_desktop_action_3 }, "Desktop 3", false },
    { MODKEY, XK_4, { .func = switch_desktop_action_4 }, "Desktop 4", false },
    { MODKEY, XK_5, { .func = switch_desktop_action_5 }, "Desktop 5", false },
    { MODKEY, XK_6, { .func = switch_desktop_action_6 }, "Desktop 6", false },
    { MODKEY, XK_7, { .func = switch_desktop_action_7 }, "Desktop 7", false },
    { MODKEY, XK_8, { .func = switch_desktop_action_8 }, "Desktop 8", false },
    { MODKEY, XK_9, { .func = switch_desktop_action_9 }, "Desktop 9", false },
    { MODKEY, XK_0, { .func = switch_desktop_action_10 }, "Desktop 10", false },
    { MODKEY | MODKEY_SHIFT, XK_1, { .func = send_to_desktop_action_1 }, "Send to desktop 1", false },
    { MODKEY | MODKEY_SHIFT, XK_2, { .func = send_to_desktop_action_2 }, "Send to desktop 2", false },
    { MODKEY | MODKEY_SHIFT, XK_3, { .func = send_to_desktop_action_3 }, "Send to desktop 3", false },
    { MODKEY | MODKEY_SHIFT, XK_4, { .func = send_to_desktop_action_4 }, "Send to desktop 4", false },
    { MODKEY | MODKEY_SHIFT, XK_5, { .func = send_to_desktop_action_5 }, "Send to desktop 5", false },
    { MODKEY | MODKEY_SHIFT, XK_6, { .func = send_to_desktop_action_6 }, "Send to desktop 6", false },
    { MODKEY | MODKEY_SHIFT, XK_7, { .func = send_to_desktop_action_7 }, "Send to desktop 7", false },
    { MODKEY | MODKEY_SHIFT, XK_8, { .func = send_to_desktop_action_8 }, "Send to desktop 8", false },
    { MODKEY | MODKEY_SHIFT, XK_9, { .func = send_to_desktop_action_9 }, "Send to desktop 9", false },
    { MODKEY | MODKEY_SHIFT, XK_0, { .func = send_to_desktop_action_10 }, "Send to desktop 10", false },
    { MODKEY, XK_t, { .cmd = st }, "Launch terminal", true },
    { MODKEY, XK_d, { .cmd = dmenu }, "Launch dmenu", true },
    { MODKEY, XK_b, { .cmd = firefox }, "Launch browser", true },
    { MODKEY, XK_e, { .cmd = thunar }, "Launch file manager", true },
    { MODKEY, XK_m, { .cmd = spotify }, "Launch spotify", true },
    { MODKEY | MODKEY_CTRL, XK_r, { .func = reload_config_action }, "Reload config", false },
    { MODKEY, XK_h, { .func = show_help_action }, "Show help", false },
};

#define NUM_KEYS (sizeof(keys) / sizeof(keys[0]))

typedef struct {
    const char *window_class;
    const char *window_title;
    int desktop;
    bool floating;
    int width;
    int height;
    bool sticky;
} Rule;

static const Rule rules[] = {
    { "discord", NULL, 0, true, 800, 600, false },
    { "Firefox", NULL, 1, false, 0, 0, false },
    { "Alacritty", NULL, -1, false, 0, 0, false },
    { NULL, ".*spotify.*", 2, false, 0, 0, true },
    { "Gimp", NULL, 3, true, 600, 400, false },
    { "Steam", NULL, 4, true, 1024, 768, false },
};

#define NUM_RULES (sizeof(rules) / sizeof(rules[0]))

static inline void get_primary_monitor_geometry(Display *dpy, int *x, int *y, int *width, int *height) {
    *x = 0;
    *y = 0;
    *width = DisplayWidth(dpy, DefaultScreen(dpy));
    *height = DisplayHeight(dpy, DefaultScreen(dpy));
    
    int event_base, error_base;
    if (!XRRQueryExtension(dpy, &event_base, &error_base)) {
        return;
    }
    
    XRRScreenResources *res = XRRGetScreenResources(dpy, DefaultRootWindow(dpy));
    if (!res) return;
    
    for (int i = 0; i < res->noutput; i++) {
        XRROutputInfo *output_info = XRRGetOutputInfo(dpy, res, res->outputs[i]);
        if (!output_info) continue;
        
        if (output_info->connection == RR_Connected && output_info->crtc) {
            XRRCrtcInfo *crtc_info = XRRGetCrtcInfo(dpy, res, output_info->crtc);
            if (crtc_info) {
                if (strcmp(output_info->name, PRIMARY_MONITOR) == 0) {
                    *x = crtc_info->x;
                    *y = crtc_info->y;
                    *width = crtc_info->width;
                    *height = crtc_info->height;
                    XRRFreeCrtcInfo(crtc_info);
                    XRRFreeOutputInfo(output_info);
                    break;
                }
                XRRFreeCrtcInfo(crtc_info);
            }
        }
        XRRFreeOutputInfo(output_info);
    }
    
    XRRFreeScreenResources(res);
}

#endif
