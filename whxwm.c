#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <ctype.h>
#include <signal.h>
#include <errno.h>

// ==================== СТРУКТУРЫ ====================

typedef struct Client {
    Window window;
    Window frame;
    int x, y;
    int width, height;
    int float_x, float_y;
    int float_width, float_height;
    int is_float;
    int is_discord;
    struct Client *prev;
    struct Client *next;
} Client;

typedef struct Keybind {
    unsigned int mod;
    KeySym keysym;
    char *command;
    struct Keybind *next;
} Keybind;

typedef struct {
    Display *dpy;
    Window root;
    Client *head;
    Client *active;
    int screen_width;
    int screen_height;
    int scroll_offset;
    int running;
    int border_width;
    int title_height;
    int gap;
    int window_width;
    int window_height;
    Keybind *keybinds;
    Atom wm_delete_window;
    Atom wm_protocols;
    int error_occurred;
    int is_dragging;
    int drag_start_x, drag_start_y;
    int drag_orig_x, drag_orig_y;
    Client *drag_client;
} WM;

// ==================== ПРОТОТИПЫ ФУНКЦИЙ ====================

void arrange_windows(WM *wm);
void toggle_float(WM *wm, Client *c);
void move_window_up(WM *wm);
void move_window_down(WM *wm);
void scroll_down(WM *wm);
void scroll_up(WM *wm);

// ==================== ОБРАБОТЧИК ОШИБОК ====================

static int error_handler(Display *dpy, XErrorEvent *ev) {
    char buf[256];
    XGetErrorText(dpy, ev->error_code, buf, sizeof(buf));
    
    if (ev->error_code == BadWindow || 
        ev->error_code == BadMatch ||
        ev->error_code == BadDrawable ||
        ev->error_code == BadAccess ||
        ev->error_code == BadValue) {
        return 0;
    }
    
    fprintf(stderr, "X Error: %s (request code: %d, minor: %d)\n", 
            buf, ev->request_code, ev->minor_code);
    return 0;
}

// ==================== ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ ====================

int count_clients(Client *head) {
    int count = 0;
    Client *c = head;
    while (c) { count++; c = c->next; }
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

void spawn_command(WM *wm, const char *cmd) {
    pid_t pid = fork();
    if (pid == 0) {
        setsid();
        close(ConnectionNumber(wm->dpy));
        execlp("/bin/sh", "sh", "-c", cmd, NULL);
        exit(0);
    }
}

// ==================== ЛОГИКА УПРАВЛЕНИЯ ОКНАМИ ====================

void toggle_float(WM *wm, Client *c) {
    if (!c) return;
    
    if (c->is_float) {
        c->is_float = 0;
        c->float_x = c->x;
        c->float_y = c->y;
        c->float_width = c->width;
        c->float_height = c->height;
    } else {
        c->is_float = 1;
        if (c->float_width == 0) {
            c->float_width = c->width;
            c->float_height = c->height;
            c->float_x = (wm->screen_width - c->float_width) / 2;
            c->float_y = (wm->screen_height - c->float_height) / 2;
        }
    }
    arrange_windows(wm);
}

void move_window_up(WM *wm) {
    if (!wm->active || !wm->active->is_float) return;
    
    Client *c = wm->active;
    c->float_y -= 20;
    arrange_windows(wm);
}

void move_window_down(WM *wm) {
    if (!wm->active || !wm->active->is_float) return;
    
    Client *c = wm->active;
    c->float_y += 20;
    arrange_windows(wm);
}

// ==================== ЛОГИКА ХОЛСТА И СКРОЛЛА ====================

void arrange_windows(WM *wm) {
    int count = count_clients(wm->head);
    if (count == 0) return;
    
    wm->gap = 10;
    wm->window_height = (wm->screen_height * 80) / 100;
    wm->window_width = wm->screen_width - (wm->gap * 2);
    
    Client *c = wm->head;
    int i = 0;
    
    while (c != NULL) {
        int x, y, w, h;
        
        if (c->is_float) {
            x = c->float_x;
            y = c->float_y;
            w = c->float_width;
            h = c->float_height;
            
            if (x + w > wm->screen_width) x = wm->screen_width - w;
            if (y + h > wm->screen_height) y = wm->screen_height - h;
            if (x < 0) x = 0;
            if (y < 0) y = 0;
            
            XMapWindow(wm->dpy, c->frame);
        } else {
            y = i * (wm->window_height + wm->gap) - wm->scroll_offset;
            x = wm->gap;
            w = wm->window_width;
            h = wm->window_height;
            
            if (y + h < -50 || y > wm->screen_height + 50) {
                XUnmapWindow(wm->dpy, c->frame);
                c = c->next;
                i++;
                continue;
            } else {
                XMapWindow(wm->dpy, c->frame);
            }
        }
        
        XMoveResizeWindow(wm->dpy, c->frame, x, y, w, h);
        
        int border = wm->border_width;
        int title_h = wm->title_height;
        XMoveResizeWindow(wm->dpy, c->window,
                          border, title_h + border,
                          w - border * 2,
                          h - title_h - border * 2);
        
        c->x = x;
        c->y = y;
        c->width = w;
        c->height = h;
        
        c = c->next;
        i++;
    }
    
    for (c = wm->head; c; c = c->next) {
        if (c == wm->active) {
            XSetWindowBorder(wm->dpy, c->frame, 0xD4A05A);
            XRaiseWindow(wm->dpy, c->frame);
        } else {
            XSetWindowBorder(wm->dpy, c->frame, 0x2B1E16);
        }
    }
    
    XSync(wm->dpy, False);
}

// ==================== ПРОКРУТКА ====================

void scroll_down(WM *wm) {
    if (count_clients(wm->head) == 0) return;
    wm->scroll_offset += (wm->window_height + wm->gap);
    arrange_windows(wm);
}

void scroll_up(WM *wm) {
    if (count_clients(wm->head) == 0) return;
    wm->scroll_offset -= (wm->window_height + wm->gap);
    arrange_windows(wm);
}

// ==================== УПРАВЛЕНИЕ ОКНАМИ ====================

int is_discord_window(WM *wm, Window window) {
    char *window_name = NULL;
    if (XFetchName(wm->dpy, window, &window_name) && window_name) {
        if (strcasestr(window_name, "discord") ||
            strcasestr(window_name, "Discord")) {
            XFree(window_name);
            return 1;
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
                return 1;
            }
        }
        XFree(class_hint->res_class);
        XFree(class_hint->res_name);
        XFree(class_hint);
    }
    
    return 0;
}

int should_manage_window(WM *wm, Window window) {
    XWindowAttributes attrs;
    if (!XGetWindowAttributes(wm->dpy, window, &attrs)) {
        return 0;
    }
    
    if (attrs.override_redirect) {
        return 0;
    }
    
    if (attrs.width < 100 || attrs.height < 100) {
        return 0;
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
        if (!result) return 0;
    }
    
    return 1;
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
    
    Window frame = XCreateSimpleWindow(wm->dpy, wm->root, 0, 0, 
                                       attrs.width ? attrs.width : 100, 
                                       attrs.height ? attrs.height : 100,
                                       wm->border_width, 0x2B1E16, 0x1A1410);
    
    Client *client = calloc(1, sizeof(Client));
    if (!client) {
        XDestroyWindow(wm->dpy, frame);
        XMapWindow(wm->dpy, window);
        return;
    }
    
    client->window = window;
    client->frame = frame;
    client->is_discord = is_discord;
    
    if (is_discord) {
        client->is_float = 1;
        client->float_width = wm->screen_width * 0.8;
        client->float_height = wm->screen_height * 0.8;
        client->float_x = (wm->screen_width - client->float_width) / 2;
        client->float_y = (wm->screen_height - client->float_height) / 2;
    }
    
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
                 EnterWindowMask | LeaveWindowMask | PointerMotionMask);
    XSelectInput(wm->dpy, window, StructureNotifyMask | PropertyChangeMask);
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

// ==================== ПАРСИНГ КОНФИГА ====================

void parse_config(WM *wm) {
    const char *path = "/home/doogike/C-basics/whxwm/whxwm.conf";
    FILE *f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "Config not found at %s. Using defaults.\n", path);
        return;
    }
    
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '#' || *p == '\n') continue;
        
        char *eq = strchr(p, '=');
        if (!eq) continue;
        
        *eq = '\0';
        char *key_part = p;
        char *cmd_part = eq + 1;
        
        char *end = key_part + strlen(key_part) - 1;
        while (end > key_part && (*end == ' ' || *end == '\t')) *end-- = '\0';
        while (*cmd_part == ' ' || *cmd_part == '\t') cmd_part++;
        
        char mod_str[32] = {0};
        char key_str[32] = {0};
        if (sscanf(key_part, "%[^+]+%s", mod_str, key_str) != 2) {
            if (sscanf(key_part, "%s", key_str) == 1) {
                strcpy(mod_str, "None");
            } else {
                continue;
            }
        }
        
        Keybind *kb = calloc(1, sizeof(Keybind));
        if (!kb) continue;
        
        kb->command = strdup(cmd_part);
        kb->mod = 0;
        
        if (strstr(mod_str, "Super")) kb->mod |= Mod4Mask;
        if (strstr(mod_str, "Alt")) kb->mod |= Mod1Mask;
        if (strstr(mod_str, "Ctrl")) kb->mod |= ControlMask;
        if (strstr(mod_str, "Shift")) kb->mod |= ShiftMask;
        
        if (strcmp(key_str, "Return") == 0) kb->keysym = XK_Return;
        else if (strcmp(key_str, "Tab") == 0) kb->keysym = XK_Tab;
        else if (strcmp(key_str, "Space") == 0) kb->keysym = XK_space;
        else if (strcmp(key_str, "Up") == 0) kb->keysym = XK_Up;
        else if (strcmp(key_str, "Down") == 0) kb->keysym = XK_Down;
        else if (strlen(key_str) == 1) kb->keysym = XStringToKeysym(key_str);
        else kb->keysym = XStringToKeysym(key_str);
        
        if (kb->keysym != NoSymbol) {
            printf("Loaded keybind: Mod=%d, KeySym='%s', Command='%s'\n", kb->mod, key_str, kb->command);
            
            XGrabKey(wm->dpy, XKeysymToKeycode(wm->dpy, kb->keysym), kb->mod,
                     wm->root, True, GrabModeAsync, GrabModeAsync);
            XGrabKey(wm->dpy, XKeysymToKeycode(wm->dpy, kb->keysym), kb->mod,
                     None, True, GrabModeAsync, GrabModeAsync);
            
            kb->next = wm->keybinds;
            wm->keybinds = kb;
        } else {
            printf("ERROR: Unknown keysym in config line: %s\n", line);
            free(kb->command);
            free(kb);
        }
    }
    fclose(f);
}

void free_keybinds(Keybind *kb) {
    while (kb) {
        Keybind *next = kb->next;
        free(kb->command);
        free(kb);
        kb = next;
    }
}

// ==================== ОБРАБОТЧИКИ СОБЫТИЙ ====================

void handle_key_press(WM *wm, XKeyEvent *ev) {
    KeySym keysym = XLookupKeysym(ev, 0);
    unsigned int state = ev->state & (Mod4Mask | Mod1Mask | ControlMask | ShiftMask);
    
    if (state == Mod4Mask && keysym == XK_q) { 
        if (wm->active) close_window(wm, wm->active->window); 
        return; 
    }
    if (state == Mod4Mask && keysym == XK_j) { scroll_down(wm); return; }
    if (state == Mod4Mask && keysym == XK_k) { scroll_up(wm); return; }
    
    if (state == Mod4Mask && keysym == XK_space) {
        if (wm->active) {
            toggle_float(wm, wm->active);
        }
        return;
    }
    
    if (state == Mod4Mask && keysym == XK_Up) {
        move_window_up(wm);
        return;
    }
    if (state == Mod4Mask && keysym == XK_Down) {
        move_window_down(wm);
        return;
    }
    
    for (Keybind *kb = wm->keybinds; kb; kb = kb->next) {
        if (state == kb->mod && keysym == kb->keysym) {
            spawn_command(wm, kb->command);
            return;
        }
    }
}

void handle_button_press(WM *wm, XButtonEvent *ev) {
    if (ev->window == wm->root) {
        switch (ev->button) {
            case Button4: scroll_up(wm); break;
            case Button5: scroll_down(wm); break;
        }
        return;
    }
    
    Client *c = find_client_by_frame(wm, ev->window);
    if (!c) return;
    
    switch (ev->button) {
        case Button4: scroll_up(wm); break;
        case Button5: scroll_down(wm); break;
        case Button1: 
            wm->active = c;
            if (c->is_float) {
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
    (void)ev; // Подавляем предупреждение о неиспользуемом параметре
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

// ==================== ИНИЦИАЛИЗАЦИЯ ====================

void init_wm(WM *wm) {
    wm->dpy = XOpenDisplay(NULL);
    if (!wm->dpy) { 
        fprintf(stderr, "Cannot open display. Make sure DISPLAY environment variable is set.\n");
        exit(1); 
    }
    
    XSetErrorHandler(error_handler);
    
    wm->root = DefaultRootWindow(wm->dpy);
    printf("Root window: 0x%lx\n", wm->root);
    
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
    
    XWindowAttributes attrs;
    XGetWindowAttributes(wm->dpy, wm->root, &attrs);
    wm->screen_width = attrs.width;
    wm->screen_height = attrs.height;
    printf("Screen: %dx%d\n", wm->screen_width, wm->screen_height);
    
    wm->head = NULL;
    wm->active = NULL;
    wm->scroll_offset = 0;
    wm->running = 1;
    wm->border_width = 2;
    wm->title_height = 25;
    wm->gap = 10;
    wm->window_width = 0;
    wm->window_height = 0;
    wm->keybinds = NULL;
    wm->error_occurred = 0;
    wm->is_dragging = 0;
    wm->drag_client = NULL;
    
    XSelectInput(wm->dpy, wm->root,
                 SubstructureRedirectMask | SubstructureNotifyMask |
                 StructureNotifyMask | PropertyChangeMask |
                 KeyPressMask | KeyReleaseMask | ButtonPressMask |
                 ButtonReleaseMask | UnmapNotify | ColormapChangeMask);
    
    XSync(wm->dpy, False);
    
    XSetWindowBackground(wm->dpy, wm->root, 0x2B1E16);
    XClearWindow(wm->dpy, wm->root);
    
    XUngrabKey(wm->dpy, AnyKey, AnyModifier, wm->root);
    
    XGrabKey(wm->dpy, XKeysymToKeycode(wm->dpy, XK_q), Mod4Mask,
             wm->root, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(wm->dpy, XKeysymToKeycode(wm->dpy, XK_j), Mod4Mask,
             wm->root, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(wm->dpy, XKeysymToKeycode(wm->dpy, XK_k), Mod4Mask,
             wm->root, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(wm->dpy, XKeysymToKeycode(wm->dpy, XK_space), Mod4Mask,
             wm->root, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(wm->dpy, XKeysymToKeycode(wm->dpy, XK_Up), Mod4Mask,
             wm->root, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(wm->dpy, XKeysymToKeycode(wm->dpy, XK_Down), Mod4Mask,
             wm->root, True, GrabModeAsync, GrabModeAsync);
    
    parse_config(wm);
    
    XSync(wm->dpy, False);
    
    printf("=== whxwm initialized successfully ===\n");
}

// ==================== ГЛАВНЫЙ ЦИКЛ ====================

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
            case CreateNotify:
                break;
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
    free_keybinds(wm->keybinds);
    XCloseDisplay(wm->dpy);
}

int main() {
    WM wm = {0};
    
    printf("Starting whxwm...\n");
    
    init_wm(&wm);
    run_wm(&wm);
    cleanup_wm(&wm);
    
    return 0;
}
