#ifndef WHXWM_H
#define WHXWM_H

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xrandr.h>
#include <stdbool.h>

#include "config.def.h"

typedef struct Client {
    Window window;
    Window frame;
    int x, y;
    int width, height;
    int float_x, float_y;
    int float_width, float_height;
    int desktop;
    unsigned int flags;
    struct Client *prev;
    struct Client *next;
} Client;

typedef struct {
    Display *dpy;
    Window root;
    Client *head;
    Client *active;
    int screen_width;
    int screen_height;
    int monitor_x;
    int monitor_y;
    int monitor_width;
    int monitor_height;
    int scroll_offset;
    int running;
    int border_width;
    int title_height;
    int gap;
    int window_width;
    int window_height;
    int error_occurred;
    int is_dragging;
    int drag_start_x, drag_start_y;
    int drag_orig_x, drag_orig_y;
    Client *drag_client;
    int current_desktop;
    int desktop_count;
    char desktop_names[MAX_DESKTOPS][32];
    int desktop_wallpapers[MAX_DESKTOPS];
    GC gc;
} WM;

#define CLIENT_FLOAT (1 << 0)
#define CLIENT_DISCORD (1 << 1)
#define CLIENT_HIDDEN (1 << 2)
#define CLIENT_STICKY (1 << 3)
#define CLIENT_MINIMIZED (1 << 4)

void init_wm(WM *wm);
void init_desktops(WM *wm);
void init_hotkeys(WM *wm);
Client* find_client_by_window(WM *wm, Window window);
Client* find_client_by_frame(WM *wm, Window frame);
Client* find_active_client_on_desktop(WM *wm, int desktop);
void manage_new_window(WM *wm, Window window);
void unmanage_window(WM *wm, Window window);
void apply_rules(WM *wm, Client *c);
bool should_manage_window(WM *wm, Window window);
void arrange_windows(WM *wm);
void draw_desktop_bar(WM *wm);
void scroll_to_window(WM *wm, Client *target);
void scroll_down_smooth(WM *wm);
void scroll_up_smooth(WM *wm);
void scroll_page_down(WM *wm);
void scroll_page_up(WM *wm);
void switch_desktop(WM *wm, int desktop);
void next_desktop(WM *wm);
void prev_desktop(WM *wm);
void send_to_desktop(WM *wm, Client *c, int desktop);
int count_clients_on_desktop(Client *head, int desktop);
void toggle_float(WM *wm, Client *c);
void toggle_sticky(WM *wm, Client *c);
void move_window_up(WM *wm);
void move_window_down(WM *wm);
void close_window(WM *wm, Window window);
void spawn_action(const char **cmd, void *data);
void handle_key_press(WM *wm, XKeyEvent *ev);
void handle_button_press(WM *wm, XButtonEvent *ev);
void handle_button_release(WM *wm, XButtonEvent *ev);
void handle_motion_notify(WM *wm, XMotionEvent *ev);
void handle_map_request(WM *wm, XMapRequestEvent *ev);
void handle_configure_request(WM *wm, XConfigureRequestEvent *ev);
void handle_unmap_notify(WM *wm, XUnmapEvent *ev);
void handle_destroy_notify(WM *wm, XDestroyWindowEvent *ev);
void run_wm(WM *wm);
void cleanup_wm(WM *wm);

#endif
