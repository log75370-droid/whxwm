// DONT TOUCH THIS IF YOU DONT KNOW WHAT YOU DOING
#ifndef CONFIG_DEF_H
#define CONFIG_DEF_H

#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/extensions/Xrandr.h>
#include <stdbool.h>
#include <string.h>

// ===================== WINDOW SETTINGS =====================
#define GAP 10                      // Gap between windows
#define BORDER_WIDTH 2              // Window border thickness
#define TITLE_HEIGHT 25             // Window title bar height
#define BAR_HEIGHT 30               // Bottom bar height
#define WINDOW_HEIGHT_RATIO 80      // Window height percentage of screen

// ===================== SCROLL SETTINGS =====================
#define SCROLL_STEP 50              // Scroll step in pixels
#define ANIMATION_STEPS 15          // Number of animation steps
#define ANIMATION_DELAY 8000        // Delay between animation steps (microseconds)

// ===================== DESKTOP SETTINGS =====================
#define DEFAULT_DESKTOPS 4          // Default number of desktops
#define MAX_DESKTOPS 10             // Maximum number of desktops

// ===================== MONITOR SETTINGS =====================
// Primary monitor name (find with: xrandr | grep " connected")
// Examples: "eDP-1", "HDMI-1", "DP-1", etc.
#define PRIMARY_MONITOR "eDP-1"     // CHANGE THIS TO YOUR MONITOR NAME!

// ===================== COLORS =====================
// Use HEX format: 0xRRGGBB
#define COLOR_BACKGROUND 0x1A1410           // Background color
#define COLOR_FOREGROUND 0xFFFFFF           // Foreground/text color
#define COLOR_BORDER_ACTIVE 0xD4A05A        // Active window border color
#define COLOR_BORDER_INACTIVE 0x2B1E16      // Inactive window border color
#define COLOR_TITLE_BG 0x2B1E16             // Title bar background color
#define COLOR_TITLE_TEXT 0xFFFFFF           // Title bar text color

#define COLOR_BAR_BG 0x1A1410               // Bar background color
#define COLOR_BAR_ACTIVE 0xD4A05A           // Active bar button color
#define COLOR_BAR_INACTIVE 0x555555         // Inactive bar button color
#define COLOR_BAR_TEXT 0xFFFFFF             // Bar text color

// Background colors for each desktop
#define DESKTOP_COLORS { \
    0x1A1410, 0x0D1B2A, 0x1A0D1B, 0x1B1A0D, 0x0D1B1A, \
    0x1A0D0D, 0x0D0D1B, 0x1B0D1A, 0x0D1B0D, 0x1B1B0D \
}

// ===================== APPLICATIONS =====================
#define TERMINAL_CMD "alacritty"            // Terminal command
#define DMENU_CMD "dmenu_run -p 'Run:' -fn 'monospace-12' -nb '#1A1410' -nf '#FFFFFF' -sb '#D4A05A' -sf '#000000'"
#define BROWSER_CMD "firefox"               // Browser command
#define FILE_MANAGER_CMD "thunar"           // File manager command

// ===================== KEYBOARD SHORTCUTS =====================
#define MODKEY Mod4Mask                     // Modifier key (Super/Win)
#define MODKEY_ALT Mod1Mask                 // Alt key
#define MODKEY_CTRL ControlMask             // Ctrl key
#define MODKEY_SHIFT ShiftMask              // Shift key

// Key binding structure
typedef struct {
    unsigned int mod;                       // Modifier mask
    KeySym keysym;                          // Key symbol
    void (*func)(void *);                   // Function to call
    const char *description;                // Description
} Key;

// Function to get primary monitor geometry using XRandR
static inline void get_primary_monitor_geometry(Display *dpy, int *x, int *y, int *width, int *height) {
    // Default to full screen
    *x = 0;
    *y = 0;
    *width = DisplayWidth(dpy, DefaultScreen(dpy));
    *height = DisplayHeight(dpy, DefaultScreen(dpy));
    
    // Check if XRandR is available
    int event_base, error_base;
    if (!XRRQueryExtension(dpy, &event_base, &error_base)) {
        return; // XRandR not available, use default
    }
    
    XRRScreenResources *res = XRRGetScreenResources(dpy, DefaultRootWindow(dpy));
    if (!res) return;
    
    // Find primary monitor by name
    for (int i = 0; i < res->noutput; i++) {
        XRROutputInfo *output_info = XRRGetOutputInfo(dpy, res, res->outputs[i]);
        if (!output_info) continue;
        
        if (output_info->connection == RR_Connected && output_info->crtc) {
            XRRCrtcInfo *crtc_info = XRRGetCrtcInfo(dpy, res, output_info->crtc);
            if (crtc_info) {
                // Check if this is the primary monitor by name
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

// DONT TOUCH THIS AGAIN x2
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
void spawn_terminal_action(void *data);
void spawn_dmenu_action(void *data);
void spawn_browser_action(void *data);
void spawn_file_manager_action(void *data);

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

// YOUR KEYS
static const Key keys[] = {
    { MODKEY, XK_q, close_window_action, "Close window" },
    { MODKEY, XK_f, focus_window_action, "Focus window" },
    { MODKEY, XK_space, toggle_float_action, "Toggle float" },
    { MODKEY | MODKEY_CTRL, XK_s, toggle_sticky_action, "Toggle sticky" },
    { MODKEY, XK_j, scroll_down_action, "Scroll down" },
    { MODKEY, XK_k, scroll_up_action, "Scroll up" },
    { MODKEY | MODKEY_CTRL, XK_j, scroll_page_down_action, "Page down" },
    { MODKEY | MODKEY_CTRL, XK_k, scroll_page_up_action, "Page up" },
    { MODKEY | MODKEY_CTRL, XK_Right, next_desktop_action, "Next desktop" },
    { MODKEY | MODKEY_CTRL, XK_Left, prev_desktop_action, "Prev desktop" },
    { MODKEY, XK_Up, move_up_action, "Move up" },
    { MODKEY, XK_Down, move_down_action, "Move down" },
    { MODKEY, XK_1, switch_desktop_action_1, "Desktop 1" },
    { MODKEY, XK_2, switch_desktop_action_2, "Desktop 2" },
    { MODKEY, XK_3, switch_desktop_action_3, "Desktop 3" },
    { MODKEY, XK_4, switch_desktop_action_4, "Desktop 4" },
    { MODKEY, XK_5, switch_desktop_action_5, "Desktop 5" },
    { MODKEY, XK_6, switch_desktop_action_6, "Desktop 6" },
    { MODKEY, XK_7, switch_desktop_action_7, "Desktop 7" },
    { MODKEY, XK_8, switch_desktop_action_8, "Desktop 8" },
    { MODKEY, XK_9, switch_desktop_action_9, "Desktop 9" },
    { MODKEY, XK_0, switch_desktop_action_10, "Desktop 10" },
    { MODKEY | MODKEY_SHIFT, XK_1, send_to_desktop_action_1, "Send to desktop 1" },
    { MODKEY | MODKEY_SHIFT, XK_2, send_to_desktop_action_2, "Send to desktop 2" },
    { MODKEY | MODKEY_SHIFT, XK_3, send_to_desktop_action_3, "Send to desktop 3" },
    { MODKEY | MODKEY_SHIFT, XK_4, send_to_desktop_action_4, "Send to desktop 4" },
    { MODKEY | MODKEY_SHIFT, XK_5, send_to_desktop_action_5, "Send to desktop 5" },
    { MODKEY | MODKEY_SHIFT, XK_6, send_to_desktop_action_6, "Send to desktop 6" },
    { MODKEY | MODKEY_SHIFT, XK_7, send_to_desktop_action_7, "Send to desktop 7" },
    { MODKEY | MODKEY_SHIFT, XK_8, send_to_desktop_action_8, "Send to desktop 8" },
    { MODKEY | MODKEY_SHIFT, XK_9, send_to_desktop_action_9, "Send to desktop 9" },
    { MODKEY | MODKEY_SHIFT, XK_0, send_to_desktop_action_10, "Send to desktop 10" },
    { MODKEY, XK_t, spawn_terminal_action, "Launch terminal" },
    { MODKEY, XK_d, spawn_dmenu_action, "Launch dmenu" },
    { MODKEY, XK_b, spawn_browser_action, "Launch browser" },
    { MODKEY, XK_e, spawn_file_manager_action, "Launch file manager" },
    { MODKEY | MODKEY_CTRL, XK_r, reload_config_action, "Reload config" },
    { MODKEY, XK_h, show_help_action, "Show help" },
};

#define NUM_KEYS (sizeof(keys) / sizeof(keys[0]))

// Window rules structure
typedef struct {
    const char *window_class;       // Window class to match
    const char *window_title;       // Window title to match
    int desktop;                    // Desktop to place window on (-1 for current)
    bool floating;                  // Whether window should float
    int width;                      // Width for floating windows
    int height;                     // Height for floating windows
    bool sticky;                    // Whether window should be sticky
} Rule;

// Window rules
static const Rule rules[] = {
    { "discord", NULL, 0, true, 800, 600, false },
    { "Firefox", NULL, 1, false, 0, 0, false },
    { "Alacritty", NULL, -1, false, 0, 0, false },
    { NULL, ".*spotify.*", 2, false, 0, 0, true },
    { "Gimp", NULL, 3, true, 600, 400, false },
};

#define NUM_RULES (sizeof(rules) / sizeof(rules[0]))

#endif
