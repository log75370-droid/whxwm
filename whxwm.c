#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <ctype.h>
#include <signal.h>
#include <errno.h>
#include <stdbool.h>
#include <time.h>
#include <sys/wait.h>

#include "whxwm.h"

static int error_handler(Display *dpy, XErrorEvent *ev) {
    char buf[256];
    XGetErrorText(dpy, ev->error_code, buf, sizeof(buf));
    if (ev->error_code == BadWindow || ev->error_code == BadMatch ||
        ev->error_code == BadDrawable || ev->error_code == BadAccess ||
        ev->error_code == BadValue) {
        return 0;
    }
    fprintf(stderr, "X Error: %s (request code: %d, minor: %d)\n", 
            buf, ev->request_code, ev->minor_code);
    return 0;
}

int count_clients_on_desktop(Client *head, int desktop) {
    int count = 0;
    Client *c = head;
    while (c) {
        if (!(c->flags & CLIENT_HIDDEN) && 
            (c->flags & CLIENT_STICKY || c->desktop == desktop || c->desktop == -1)) {
            count++;
        }
        c = c->next;
    }
    return count;
}

Client* find_client_by_window(WM *wm, Window window) {
    for (Client *c = wm->head; c; c = c->next)
        if (c->window == window) return c;
    return NULL;
}

Client* find_client_by_frame(WM *wm, Window frame) {
    for (Client *c = wm->head; c; c = c->next)
        if (c->frame == frame) return c;
    return NULL;
}

Client* find_active_client_on_desktop(WM *wm, int desktop) {
    for (Client *c = wm->head; c; c = c->next) {
        if (!(c->flags & CLIENT_HIDDEN) && 
            (c->flags & CLIENT_STICKY || c->desktop == desktop || c->desktop == -1)) {
            return c;
        }
    }
    return NULL;
}

void spawn_action(const char **cmd, void *data) {
    if (!cmd || !cmd[0]) return;
    
    WM *wm = (WM*)data;
    pid_t pid = fork();
    if (pid == 0) {
        setsid();
        close(ConnectionNumber(wm->dpy));
        execvp(cmd[0], (char * const *)cmd);
        perror("execvp");
        exit(1);
    }
}

void close_window(WM *wm, Window window) {
    Atom wm_delete = XInternAtom(wm->dpy, "WM_DELETE_WINDOW", False);
    Atom wm_protocols = XInternAtom(wm->dpy, "WM_PROTOCOLS", False);
    XEvent ev;
    ev.type = ClientMessage;
    ev.xclient.window = window;
    ev.xclient.message_type = wm_protocols;
    ev.xclient.format = 32;
    ev.xclient.data.l[0] = wm_delete;
    ev.xclient.data.l[1] = CurrentTime;
    XSendEvent(wm->dpy, window, False, NoEventMask, &ev);
}

void toggle_float(WM *wm, Client *c) {
    if (!c) return;
    if (c->flags & CLIENT_FLOAT) {
        c->flags &= ~CLIENT_FLOAT;
        c->float_x = c->x;
        c->float_y = c->y;
        c->float_width = c->width;
        c->float_height = c->height;
    } else {
        c->flags |= CLIENT_FLOAT;
        if (c->float_width == 0) {
            c->float_width = c->width;
            c->float_height = c->height;
            c->float_x = wm->monitor_x + (wm->monitor_width - c->float_width) / 2;
            c->float_y = wm->monitor_y + (wm->monitor_height - c->float_height) / 2;
        }
    }
    arrange_windows(wm);
}

void toggle_sticky(WM *wm, Client *c) {
    if (!c) return;
    c->flags ^= CLIENT_STICKY;
    arrange_windows(wm);
}

void move_window_up(WM *wm) {
    if (!wm->active || !(wm->active->flags & CLIENT_FLOAT)) return;
    Client *c = wm->active;
    c->float_y -= 20;
    arrange_windows(wm);
}

void move_window_down(WM *wm) {
    if (!wm->active || !(wm->active->flags & CLIENT_FLOAT)) return;
    Client *c = wm->active;
    c->float_y += 20;
    arrange_windows(wm);
}

void init_desktops(WM *wm) {
    wm->desktop_count = DEFAULT_DESKTOPS;
    wm->current_desktop = 0;
    
    const char *names[] = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "10"};
    for (int i = 0; i < MAX_DESKTOPS && i < wm->desktop_count; i++) {
        strncpy(wm->desktop_names[i], names[i], 31);
        wm->desktop_names[i][31] = '\0';
    }
    
    int colors[] = DESKTOP_COLORS;
    for (int i = 0; i < MAX_DESKTOPS && i < wm->desktop_count; i++) {
        wm->desktop_wallpapers[i] = colors[i % 10];
    }
}

void switch_desktop(WM *wm, int desktop) {
    if (desktop < 0 || desktop >= wm->desktop_count || desktop == wm->current_desktop) {
        return;
    }
    
    Client *c;
    for (c = wm->head; c; c = c->next) {
        if (!(c->flags & CLIENT_STICKY) && c->desktop == wm->current_desktop) {
            XUnmapWindow(wm->dpy, c->frame);
        }
    }
    
    wm->current_desktop = desktop;
    
    XSetWindowBackground(wm->dpy, wm->root, wm->desktop_wallpapers[desktop]);
    XClearWindow(wm->dpy, wm->root);
    
    arrange_windows(wm);
    draw_desktop_bar(wm);
    
    wm->active = find_active_client_on_desktop(wm, desktop);
    arrange_windows(wm);
}

void next_desktop(WM *wm) {
    int next = (wm->current_desktop + 1) % wm->desktop_count;
    switch_desktop(wm, next);
}

void prev_desktop(WM *wm) {
    int prev = (wm->current_desktop - 1 + wm->desktop_count) % wm->desktop_count;
    switch_desktop(wm, prev);
}

void send_to_desktop(WM *wm, Client *c, int desktop) {
    if (!c || desktop < 0 || desktop >= wm->desktop_count) return;
    c->desktop = desktop;
    arrange_windows(wm);
    draw_desktop_bar(wm);
}

bool is_discord_window(WM *wm, Window window) {
    char *window_name = NULL;
    if (XFetchName(wm->dpy, window, &window_name) && window_name) {
        if (strcasestr(window_name, "discord") || strcasestr(window_name, "Discord")) {
            XFree(window_name);
            return true;
        }
        XFree(window_name);
    }
    XClassHint *class_hint = XAllocClassHint();
    if (XGetClassHint(wm->dpy, window, class_hint)) {
        if (class_hint->res_class) {
            if (strcasestr(class_hint->res_class, "discord") ||
                strcasestr(class_hint->res_class, "Discord")) {
                XFree(class_hint->res_class);
                XFree(class_hint->res_name);
                XFree(class_hint);
                return true;
            }
        }
        XFree(class_hint->res_class);
        XFree(class_hint->res_name);
        XFree(class_hint);
    }
    return false;
}

bool should_manage_window(WM *wm, Window window) {
    XWindowAttributes attrs;
    if (!XGetWindowAttributes(wm->dpy, window, &attrs)) {
        return false;
    }
    if (attrs.override_redirect) {
        return false;
    }
    if (attrs.width < 100 || attrs.height < 100) {
        return false;
    }
    Atom wm_type = XInternAtom(wm->dpy, "_NET_WM_WINDOW_TYPE", False);
    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    unsigned char *prop = NULL;
    
    if (XGetWindowProperty(wm->dpy, window, wm_type, 0, 1024, False, 
                           XA_ATOM, &actual_type, &actual_format,
                           &nitems, &bytes_after, &prop) == Success && prop) {
        Atom *types = (Atom*)prop;
        int result = 1;
        for (unsigned long i = 0; i < nitems; i++) {
            char *name = XGetAtomName(wm->dpy, types[i]);
            if (name) {
                if (strstr(name, "_NET_WM_WINDOW_TYPE_DOCK") ||
                    strstr(name, "_NET_WM_WINDOW_TYPE_DESKTOP") ||
                    strstr(name, "_NET_WM_WINDOW_TYPE_MENU") ||
                    strstr(name, "_NET_WM_WINDOW_TYPE_TOOLTIP") ||
                    strstr(name, "_NET_WM_WINDOW_TYPE_NOTIFICATION") ||
                    strstr(name, "_NET_WM_WINDOW_TYPE_SPLASH") ||
                    strstr(name, "_NET_WM_WINDOW_TYPE_UTILITY") ||
                    strstr(name, "_NET_WM_WINDOW_TYPE_DIALOG") ||
                    strstr(name, "_NET_WM_WINDOW_TYPE_POPUP_MENU")) {
                    result = 0;
                }
                XFree(name);
            }
        }
        XFree(prop);
        if (!result) return false;
    }
    return true;
}

void apply_rules(WM *wm, Client *c) {
    char *class_hint = NULL;
    char *window_name = NULL;
    
    XClassHint *class = XAllocClassHint();
    if (XGetClassHint(wm->dpy, c->window, class)) {
        if (class->res_class) class_hint = class->res_class;
    }
    XFetchName(wm->dpy, c->window, &window_name);
    
    for (unsigned int i = 0; i < NUM_RULES; i++) {
        int match = 0;
        if (rules[i].window_class && class_hint) {
            if (strcasestr(class_hint, rules[i].window_class)) match = 1;
        }
        if (rules[i].window_title && window_name) {
            if (strcasestr(window_name, rules[i].window_title)) match = 1;
        }
        if (match) {
            if (rules[i].desktop >= 0) c->desktop = rules[i].desktop;
            if (rules[i].floating) c->flags |= CLIENT_FLOAT;
            if (rules[i].sticky) c->flags |= CLIENT_STICKY;
            if (rules[i].width > 0) c->float_width = rules[i].width;
            if (rules[i].height > 0) c->float_height = rules[i].height;
            break;
        }
    }
    
    if (class) XFree(class);
    if (window_name) XFree(window_name);
}

void draw_desktop_bar(WM *wm) {
    int bar_height = BAR_HEIGHT;
    int bar_y = wm->screen_height - bar_height;
    
    XSetForeground(wm->dpy, wm->gc, COLOR_BAR_BG);
    XFillRectangle(wm->dpy, wm->root, wm->gc, 0, bar_y, wm->screen_width, bar_height);
    
    int indicator_width = 60;
    int indicator_height = 20;
    int total_width = wm->desktop_count * (indicator_width + 10);
    int start_x = (wm->screen_width - total_width) / 2;
    int indicator_y = bar_y + (bar_height - indicator_height) / 2;
    
    for (int i = 0; i < wm->desktop_count; i++) {
        int x = start_x + i * (indicator_width + 10);
        
        if (i == wm->current_desktop) {
            XSetForeground(wm->dpy, wm->gc, COLOR_BAR_ACTIVE);
        } else {
            XSetForeground(wm->dpy, wm->gc, COLOR_BAR_INACTIVE);
        }
        XFillRectangle(wm->dpy, wm->root, wm->gc, x, indicator_y, indicator_width, indicator_height);
        
        int count = count_clients_on_desktop(wm->head, i);
        char label[32];
        snprintf(label, sizeof(label), "%s (%d)", wm->desktop_names[i], count);
        
        XSetForeground(wm->dpy, wm->gc, COLOR_BAR_TEXT);
        XDrawString(wm->dpy, wm->root, wm->gc, x + 5, indicator_y + 15, label, strlen(label));
    }
    
    XSync(wm->dpy, False);
}

void arrange_windows(WM *wm) {
    int count = count_clients_on_desktop(wm->head, wm->current_desktop);
    if (count == 0) {
        draw_desktop_bar(wm);
        return;
    }
    
    wm->gap = GAP;
    wm->window_height = (wm->screen_height * WINDOW_HEIGHT_RATIO) / 100;
    wm->window_width = wm->screen_width - (wm->gap * 2);
    
    int total_height = count * (wm->window_height + wm->gap) - wm->gap;
    int max_scroll = total_height - wm->screen_height + BAR_HEIGHT;
    if (max_scroll < 0) max_scroll = 0;
    if (wm->scroll_offset < 0) wm->scroll_offset = 0;
    if (wm->scroll_offset > max_scroll) wm->scroll_offset = max_scroll;
    
    Client *c = wm->head;
    int i = 0;
    
    while (c != NULL) {
        bool visible = (c->flags & CLIENT_STICKY) || 
                       c->desktop == wm->current_desktop || 
                       c->desktop == -1;
        
        if (!(c->flags & CLIENT_HIDDEN) && visible) {
            int x, y, w, h;
            
            if (c->flags & CLIENT_FLOAT) {
                x = c->float_x;
                y = c->float_y;
                w = c->float_width;
                h = c->float_height;
                
                if (x + w > wm->screen_width) x = wm->screen_width - w;
                if (y + h > wm->screen_height - BAR_HEIGHT) y = wm->screen_height - h - BAR_HEIGHT;
                if (x < 0) x = 0;
                if (y < 0) y = 0;
                
                XMapWindow(wm->dpy, c->frame);
            } else {
                y = i * (wm->window_height + wm->gap) - wm->scroll_offset;
                x = wm->gap;
                w = wm->window_width;
                h = wm->window_height;
                
                if (y + h < -100 || y > wm->screen_height + 100) {
                    XUnmapWindow(wm->dpy, c->frame);
                } else {
                    XMapWindow(wm->dpy, c->frame);
                    XMoveResizeWindow(wm->dpy, c->frame, x, y, w, h);
                    
                    int inner_x = wm->border_width;
                    int inner_y = wm->title_height + wm->border_width;
                    int inner_w = w - (wm->border_width * 2);
                    int inner_h = h - wm->title_height - (wm->border_width * 2);
                    
                    XMoveResizeWindow(wm->dpy, c->window, inner_x, inner_y, inner_w, inner_h);
                }
            }
            
            c->x = x;
            c->y = y;
            c->width = w;
            c->height = h;
        } else {
            XUnmapWindow(wm->dpy, c->frame);
        }
        
        c = c->next;
        i++;
    }
    
    for (c = wm->head; c; c = c->next) {
        bool visible = (c->flags & CLIENT_STICKY) || 
                       c->desktop == wm->current_desktop || 
                       c->desktop == -1;
        
        if (visible && !(c->flags & CLIENT_HIDDEN) && !(c->flags & CLIENT_FLOAT)) {
            if (c == wm->active) {
                XSetWindowBorder(wm->dpy, c->frame, COLOR_BORDER_ACTIVE);
                XRaiseWindow(wm->dpy, c->frame);
            } else {
                XSetWindowBorder(wm->dpy, c->frame, COLOR_BORDER_INACTIVE);
            }
        }
    }
    
    draw_desktop_bar(wm);
    XSync(wm->dpy, False);
}

void scroll_down_smooth(WM *wm) {
    int count = count_clients_on_desktop(wm->head, wm->current_desktop);
    if (count == 0) return;
    
    int target_offset = wm->scroll_offset + (wm->window_height + wm->gap);
    int total_height = count * (wm->window_height + wm->gap) - wm->gap;
    int max_scroll = total_height - wm->screen_height + BAR_HEIGHT;
    if (target_offset > max_scroll) target_offset = max_scroll;
    
    int steps = ANIMATION_STEPS;
    int start_offset = wm->scroll_offset;
    
    for (int i = 0; i <= steps; i++) {
        float t = (float)i / steps;
        wm->scroll_offset = start_offset + (target_offset - start_offset) * t;
        arrange_windows(wm);
        usleep(ANIMATION_DELAY);
    }
    
    wm->scroll_offset = target_offset;
    arrange_windows(wm);
}

void scroll_up_smooth(WM *wm) {
    int count = count_clients_on_desktop(wm->head, wm->current_desktop);
    if (count == 0) return;
    
    int target_offset = wm->scroll_offset - (wm->window_height + wm->gap);
    if (target_offset < 0) target_offset = 0;
    
    int steps = ANIMATION_STEPS;
    int start_offset = wm->scroll_offset;
    
    for (int i = 0; i <= steps; i++) {
        float t = (float)i / steps;
        wm->scroll_offset = start_offset + (target_offset - start_offset) * t;
        arrange_windows(wm);
        usleep(ANIMATION_DELAY);
    }
    
    wm->scroll_offset = target_offset;
    arrange_windows(wm);
}

void scroll_page_down(WM *wm) {
    int count = count_clients_on_desktop(wm->head, wm->current_desktop);
    if (count == 0) return;
    
    int page_height = wm->screen_height - BAR_HEIGHT - 30;
    wm->scroll_offset += page_height;
    
    int total_height = count * (wm->window_height + wm->gap) - wm->gap;
    int max_scroll = total_height - wm->screen_height + BAR_HEIGHT;
    if (wm->scroll_offset > max_scroll) wm->scroll_offset = max_scroll;
    
    arrange_windows(wm);
}

void scroll_page_up(WM *wm) {
    int count = count_clients_on_desktop(wm->head, wm->current_desktop);
    if (count == 0) return;
    
    int page_height = wm->screen_height - BAR_HEIGHT - 30;
    wm->scroll_offset -= page_height;
    if (wm->scroll_offset < 0) wm->scroll_offset = 0;
    
    arrange_windows(wm);
}

void scroll_to_window(WM *wm, Client *target) {
    if (!target || target->flags & CLIENT_FLOAT) return;
    
    int count = count_clients_on_desktop(wm->head, wm->current_desktop);
    if (count == 0) return;
    
    int index = 0;
    Client *c = wm->head;
    while (c) {
        bool visible = (c->flags & CLIENT_STICKY) || 
                       c->desktop == wm->current_desktop || 
                       c->desktop == -1;
        if (visible && !(c->flags & CLIENT_HIDDEN)) {
            if (c == target) break;
            index++;
        }
        c = c->next;
    }
    
    if (!c) return;
    
    int target_y = index * (wm->window_height + wm->gap);
    int total_height = count * (wm->window_height + wm->gap) - wm->gap;
    int max_scroll = total_height - wm->screen_height + BAR_HEIGHT;
    int center_offset = target_y - (wm->screen_height - wm->window_height - BAR_HEIGHT - 30) / 2;
    if (center_offset < 0) center_offset = 0;
    if (center_offset > max_scroll) center_offset = max_scroll;
    
    int steps = 20;
    int start_offset = wm->scroll_offset;
    
    for (int i = 0; i <= steps; i++) {
        float t = (float)i / steps;
        wm->scroll_offset = start_offset + (center_offset - start_offset) * t;
        arrange_windows(wm);
        usleep(5000);
    }
    
    wm->scroll_offset = center_offset;
    arrange_windows(wm);
}

void manage_new_window(WM *wm, Window window) {
    if (find_client_by_window(wm, window)) return;
    
    int is_discord = is_discord_window(wm, window);
    
    if (!should_manage_window(wm, window) && !is_discord) {
        XMapWindow(wm->dpy, window);
        return;
    }
    
    XWindowAttributes attrs;
    XGetWindowAttributes(wm->dpy, window, &attrs);
    
    Window frame = XCreateSimpleWindow(wm->dpy, wm->root, 
                                       wm->monitor_x, wm->monitor_y,
                                       attrs.width ? attrs.width : 100, 
                                       attrs.height ? attrs.height : 100,
                                       wm->border_width, COLOR_BORDER_INACTIVE, COLOR_TITLE_BG);
    
    Client *client = calloc(1, sizeof(Client));
    if (!client) {
        XDestroyWindow(wm->dpy, frame);
        XMapWindow(wm->dpy, window);
        return;
    }
    
    client->window = window;
    client->frame = frame;
    client->desktop = wm->current_desktop;
    client->flags = 0;
    
    if (is_discord) {
        client->flags |= CLIENT_FLOAT | CLIENT_DISCORD;
        client->float_width = wm->monitor_width * 0.8;
        client->float_height = wm->monitor_height * 0.8;
        client->float_x = wm->monitor_x + (wm->monitor_width - client->float_width) / 2;
        client->float_y = wm->monitor_y + (wm->monitor_height - client->float_height) / 2;
    }
    
    apply_rules(wm, client);
    
    if (!wm->head) {
        wm->head = client;
        wm->active = client;
    } else {
        Client *last = wm->head;
        while (last->next) last = last->next;
        last->next = client;
        client->prev = last;
    }
    
    XReparentWindow(wm->dpy, window, frame, wm->border_width, wm->title_height + wm->border_width);
    XSelectInput(wm->dpy, frame, ExposureMask | ButtonPressMask | ButtonReleaseMask | 
                 EnterWindowMask | LeaveWindowMask | PointerMotionMask | KeyPressMask);
    XSelectInput(wm->dpy, window, StructureNotifyMask | PropertyChangeMask | KeyPressMask);
    XMapWindow(wm->dpy, frame);
    XMapWindow(wm->dpy, window);
    
    arrange_windows(wm);
}

void unmanage_window(WM *wm, Window window) {
    Client *c = find_client_by_window(wm, window);
    if (!c) return;
    
    if (c->prev) c->prev->next = c->next;
    else wm->head = c->next;
    if (c->next) c->next->prev = c->prev;
    
    if (wm->active == c) {
        if (c->next) wm->active = c->next;
        else if (c->prev) wm->active = c->prev;
        else wm->active = NULL;
    }
    
    XReparentWindow(wm->dpy, c->window, wm->root, 0, 0);
    XDestroyWindow(wm->dpy, c->frame);
    free(c);
    arrange_windows(wm);
}

void handle_button_press(WM *wm, XButtonEvent *ev) {
    if (ev->window == wm->root) {
        switch (ev->button) {
            case Button4: scroll_up_smooth(wm); break;
            case Button5: scroll_down_smooth(wm); break;
        }
        return;
    }
    
    Client *c = find_client_by_frame(wm, ev->window);
    if (!c) return;
    
    switch (ev->button) {
        case Button4: scroll_up_smooth(wm); break;
        case Button5: scroll_down_smooth(wm); break;
        case Button1: 
            wm->active = c;
            if (!(c->flags & CLIENT_FLOAT)) {
                scroll_to_window(wm, c);
            }
            if (c->flags & CLIENT_FLOAT) {
                wm->is_dragging = 1;
                wm->drag_client = c;
                wm->drag_start_x = ev->x_root;
                wm->drag_start_y = ev->y_root;
                wm->drag_orig_x = c->float_x;
                wm->drag_orig_y = c->float_y;
                XGrabPointer(wm->dpy, c->frame, False,
                            ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
                            GrabModeAsync, GrabModeAsync, None, None, CurrentTime);
            }
            arrange_windows(wm);
            break;
        case Button3: 
            close_window(wm, c->window);
            break;
    }
}

void handle_button_release(WM *wm, XButtonEvent *ev) {
    (void)ev;
    if (wm->is_dragging) {
        wm->is_dragging = 0;
        wm->drag_client = NULL;
        XUngrabPointer(wm->dpy, CurrentTime);
    }
}

void handle_motion_notify(WM *wm, XMotionEvent *ev) {
    if (!wm->is_dragging || !wm->drag_client) return;
    
    Client *c = wm->drag_client;
    int dx = ev->x_root - wm->drag_start_x;
    int dy = ev->y_root - wm->drag_start_y;
    
    c->float_x = wm->drag_orig_x + dx;
    c->float_y = wm->drag_orig_y + dy;
    
    if (c->float_x < 0) c->float_x = 0;
    if (c->float_y < 0) c->float_y = 0;
    if (c->float_x + c->float_width > wm->screen_width) 
        c->float_x = wm->screen_width - c->float_width;
    if (c->float_y + c->float_height > wm->screen_height - BAR_HEIGHT) 
        c->float_y = wm->screen_height - c->float_height - BAR_HEIGHT;
    
    XMoveWindow(wm->dpy, c->frame, c->float_x, c->float_y);
}

void handle_configure_request(WM *wm, XConfigureRequestEvent *ev) {
    Client *c = find_client_by_window(wm, ev->window);
    if (c) {
        arrange_windows(wm);
    } else {
        XWindowChanges changes = {
            .x = ev->x, .y = ev->y,
            .width = ev->width, .height = ev->height,
            .border_width = ev->border_width,
            .sibling = ev->above, .stack_mode = ev->detail
        };
        XConfigureWindow(wm->dpy, ev->window, ev->value_mask, &changes);
    }
}

void handle_map_request(WM *wm, XMapRequestEvent *ev) {
    if (find_client_by_window(wm, ev->window)) {
        XMapWindow(wm->dpy, ev->window);
        return;
    }
    manage_new_window(wm, ev->window);
}

void handle_unmap_notify(WM *wm, XUnmapEvent *ev) {
    Client *c = find_client_by_window(wm, ev->window);
    if (c) {
        unmanage_window(wm, ev->window);
    }
}

void handle_destroy_notify(WM *wm, XDestroyWindowEvent *ev) {
    Client *c = find_client_by_window(wm, ev->window);
    if (c) {
        unmanage_window(wm, ev->window);
    }
}

void close_window_action(void *data) {
    WM *wm = (WM*)data;
    if (wm->active) close_window(wm, wm->active->window);
}

void scroll_down_action(void *data) {
    scroll_down_smooth((WM*)data);
}

void scroll_up_action(void *data) {
    scroll_up_smooth((WM*)data);
}

void scroll_page_down_action(void *data) {
    scroll_page_down((WM*)data);
}

void scroll_page_up_action(void *data) {
    scroll_page_up((WM*)data);
}

void next_desktop_action(void *data) {
    next_desktop((WM*)data);
}

void prev_desktop_action(void *data) {
    prev_desktop((WM*)data);
}

void toggle_float_action(void *data) {
    WM *wm = (WM*)data;
    if (wm->active) toggle_float(wm, wm->active);
}

void toggle_sticky_action(void *data) {
    WM *wm = (WM*)data;
    if (wm->active) toggle_sticky(wm, wm->active);
}

void focus_window_action(void *data) {
    WM *wm = (WM*)data;
    if (wm->active) scroll_to_window(wm, wm->active);
}

void move_up_action(void *data) {
    move_window_up((WM*)data);
}

void move_down_action(void *data) {
    move_window_down((WM*)data);
}

void reload_config_action(void *data) {
    (void)data;
}

void show_help_action(void *data) {
    (void)data;
}

#define SWITCH_DESKTOP_FUNC(n) \
void switch_desktop_action_##n(void *data) { \
    WM *wm = (WM*)data; \
    int idx = n == 0 ? 9 : n - 1; \
    if (idx < wm->desktop_count) switch_desktop(wm, idx); \
}

SWITCH_DESKTOP_FUNC(1)
SWITCH_DESKTOP_FUNC(2)
SWITCH_DESKTOP_FUNC(3)
SWITCH_DESKTOP_FUNC(4)
SWITCH_DESKTOP_FUNC(5)
SWITCH_DESKTOP_FUNC(6)
SWITCH_DESKTOP_FUNC(7)
SWITCH_DESKTOP_FUNC(8)
SWITCH_DESKTOP_FUNC(9)
SWITCH_DESKTOP_FUNC(10)

#define SEND_TO_DESKTOP_FUNC(n) \
void send_to_desktop_action_##n(void *data) { \
    WM *wm = (WM*)data; \
    int idx = n == 0 ? 9 : n - 1; \
    if (wm->active && idx < wm->desktop_count) { \
        send_to_desktop(wm, wm->active, idx); \
    } \
}

SEND_TO_DESKTOP_FUNC(1)
SEND_TO_DESKTOP_FUNC(2)
SEND_TO_DESKTOP_FUNC(3)
SEND_TO_DESKTOP_FUNC(4)
SEND_TO_DESKTOP_FUNC(5)
SEND_TO_DESKTOP_FUNC(6)
SEND_TO_DESKTOP_FUNC(7)
SEND_TO_DESKTOP_FUNC(8)
SEND_TO_DESKTOP_FUNC(9)
SEND_TO_DESKTOP_FUNC(10)

void init_hotkeys(WM *wm) {
    XUngrabKey(wm->dpy, AnyKey, AnyModifier, wm->root);
    
    for (unsigned int i = 0; i < NUM_KEYS; i++) {
        KeyCode keycode = XKeysymToKeycode(wm->dpy, keys[i].keysym);
        if (keycode == 0) continue;
        
        XGrabKey(wm->dpy, keycode, keys[i].mod, wm->root,
                 True, GrabModeAsync, GrabModeAsync);
        XGrabKey(wm->dpy, keycode, keys[i].mod | Mod2Mask, wm->root,
                 True, GrabModeAsync, GrabModeAsync);
        XGrabKey(wm->dpy, keycode, keys[i].mod | LockMask, wm->root,
                 True, GrabModeAsync, GrabModeAsync);
        XGrabKey(wm->dpy, keycode, keys[i].mod | Mod2Mask | LockMask, wm->root,
                 True, GrabModeAsync, GrabModeAsync);
    }
    
    Client *c = wm->head;
    while (c) {
        if (c->frame) {
            for (unsigned int i = 0; i < NUM_KEYS; i++) {
                KeyCode keycode = XKeysymToKeycode(wm->dpy, keys[i].keysym);
                if (keycode == 0) continue;
                
                XGrabKey(wm->dpy, keycode, keys[i].mod, c->frame,
                         True, GrabModeAsync, GrabModeAsync);
                XGrabKey(wm->dpy, keycode, keys[i].mod | Mod2Mask, c->frame,
                         True, GrabModeAsync, GrabModeAsync);
                XGrabKey(wm->dpy, keycode, keys[i].mod | LockMask, c->frame,
                         True, GrabModeAsync, GrabModeAsync);
                XGrabKey(wm->dpy, keycode, keys[i].mod | Mod2Mask | LockMask, c->frame,
                         True, GrabModeAsync, GrabModeAsync);
            }
        }
        c = c->next;
    }
    
    XSync(wm->dpy, False);
}

void grab_keys_on_window(WM *wm, Window window) {
    if (!window) return;
    
    for (unsigned int i = 0; i < NUM_KEYS; i++) {
        KeyCode keycode = XKeysymToKeycode(wm->dpy, keys[i].keysym);
        if (keycode == 0) continue;
        
        XGrabKey(wm->dpy, keycode, keys[i].mod, window,
                 True, GrabModeAsync, GrabModeAsync);
        XGrabKey(wm->dpy, keycode, keys[i].mod | Mod2Mask, window,
                 True, GrabModeAsync, GrabModeAsync);
        XGrabKey(wm->dpy, keycode, keys[i].mod | LockMask, window,
                 True, GrabModeAsync, GrabModeAsync);
        XGrabKey(wm->dpy, keycode, keys[i].mod | Mod2Mask | LockMask, window,
                 True, GrabModeAsync, GrabModeAsync);
    }
}

void handle_key_press(WM *wm, XKeyEvent *ev) {
    KeySym keysym = XLookupKeysym(ev, 0);
    unsigned int state = ev->state & ~(Mod2Mask | LockMask);
    
    for (unsigned int i = 0; i < NUM_KEYS; i++) {
        if (keys[i].keysym == keysym && keys[i].mod == state) {
            if (keys[i].is_cmd) {
                spawn_action(keys[i].func.cmd, wm);
            } else if (keys[i].func.func) {
                keys[i].func.func(wm);
            }
            return;
        }
    }
}

void init_wm(WM *wm) {
    wm->dpy = XOpenDisplay(NULL);
    if (!wm->dpy) {
        fprintf(stderr, "Cannot open display\n");
        exit(1);
    }
    
    XSetErrorHandler(error_handler);
    wm->root = DefaultRootWindow(wm->dpy);
    
    XSelectInput(wm->dpy, wm->root, SubstructureRedirectMask);
    XSync(wm->dpy, False);
    
    Window unused_root, unused_parent;
    Window *children;
    unsigned int nchildren;
    if (!XQueryTree(wm->dpy, wm->root, &unused_root, &unused_parent, &children, &nchildren)) {
        fprintf(stderr, "Failed to query root window tree\n");
        exit(1);
    }
    if (children) XFree(children);
    
    get_primary_monitor_geometry(wm->dpy, &wm->monitor_x, &wm->monitor_y, 
                                  &wm->monitor_width, &wm->monitor_height);
    
    wm->screen_width = wm->monitor_width;
    wm->screen_height = wm->monitor_height;
    
    wm->head = NULL;
    wm->active = NULL;
    wm->scroll_offset = 0;
    wm->running = 1;
    wm->border_width = BORDER_WIDTH;
    wm->title_height = TITLE_HEIGHT;
    wm->gap = GAP;
    wm->window_width = 0;
    wm->window_height = 0;
    wm->error_occurred = 0;
    wm->is_dragging = 0;
    wm->drag_client = NULL;
    
    wm->gc = XCreateGC(wm->dpy, wm->root, 0, NULL);
    
    init_desktops(wm);
    
    XSelectInput(wm->dpy, wm->root,
                 SubstructureRedirectMask | SubstructureNotifyMask |
                 StructureNotifyMask | PropertyChangeMask |
                 KeyPressMask | KeyReleaseMask | ButtonPressMask |
                 ButtonReleaseMask | UnmapNotify | ColormapChangeMask);
    
    XSync(wm->dpy, False);
    
    XSetWindowBackground(wm->dpy, wm->root, wm->desktop_wallpapers[wm->current_desktop]);
    XClearWindow(wm->dpy, wm->root);
    
    init_hotkeys(wm);
    XSync(wm->dpy, False);
}

void run_wm(WM *wm) {
    XEvent ev;
    while (wm->running) {
        XNextEvent(wm->dpy, &ev);
        switch (ev.type) {
            case KeyPress: 
                handle_key_press(wm, &ev.xkey); 
                break;
            case ButtonPress: 
                handle_button_press(wm, &ev.xbutton); 
                break;
            case ButtonRelease:
                handle_button_release(wm, &ev.xbutton);
                break;
            case MotionNotify:
                handle_motion_notify(wm, &ev.xmotion);
                break;
            case MapRequest: 
                handle_map_request(wm, &ev.xmaprequest); 
                break;
            case ConfigureRequest: 
                handle_configure_request(wm, &ev.xconfigurerequest); 
                break;
            case UnmapNotify: 
                handle_unmap_notify(wm, &ev.xunmap); 
                break;
            case DestroyNotify: 
                handle_destroy_notify(wm, &ev.xdestroywindow); 
                break;
            case CreateNotify: {
                XCreateWindowEvent *cev = (XCreateWindowEvent*)&ev;
                if (cev->window != wm->root) {
                    grab_keys_on_window(wm, cev->window);
                }
                break;
            }
            default: 
                break;
        }
        XSync(wm->dpy, False);
    }
}

void cleanup_wm(WM *wm) {
    for (Client *c = wm->head; c; ) {
        Client *next = c->next;
        XReparentWindow(wm->dpy, c->window, wm->root, 0, 0);
        XDestroyWindow(wm->dpy, c->frame);
        free(c);
        c = next;
    }
    if (wm->gc) XFreeGC(wm->dpy, wm->gc);
    XCloseDisplay(wm->dpy);
}

int main() {
    WM wm = {0};
    init_wm(&wm);
    draw_desktop_bar(&wm);
    run_wm(&wm);
    cleanup_wm(&wm);
    return 0;
}
