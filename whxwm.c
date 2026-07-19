// ==================== whxwm_desktop.c ====================
// 1.2 version

// Posix macros
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE

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
#include <stdbool.h>

// Если strcasestr не определен, добавляем свою реализацию
#ifndef HAVE_STRCASESTR
// Используем другое имя, чтобы не конфликтовать с объявлением в string.h
static char* my_strcasestr(const char *haystack, const char *needle) {
    if (!haystack || !needle) return NULL;
    
    size_t needle_len = strlen(needle);
    if (needle_len == 0) return (char*)haystack;
    
    for (const char *h = haystack; *h; h++) {
        if (tolower((unsigned char)*h) == tolower((unsigned char)*needle)) {
            size_t i;
            for (i = 0; i < needle_len; i++) {
                if (tolower((unsigned char)h[i]) != tolower((unsigned char)needle[i])) break;
            }
            if (i == needle_len) return (char*)h;
        }
    }
    return NULL;
}
#define strcasestr my_strcasestr
#endif

// ==================== КОНСТАНТЫ ====================

#define MAX_DESKTOPS 10
#define DEFAULT_DESKTOPS 4
#define MIN_WINDOW_WIDTH 100
#define MIN_WINDOW_HEIGHT 100
#define ANIMATION_STEPS 10
#define ANIMATION_DELAY 10000  // микросекунды

// ==================== СТРУКТУРЫ ====================

typedef struct Client {
    Window window;
    Window frame;
    int x, y;
    int width, height;
    int float_x, float_y;
    int float_width, float_height;
    int desktop;                // Номер рабочего стола (-1 = все)
    unsigned int flags;         // Битовая маска флагов
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
    
    // Десктопы
    int current_desktop;
    int desktop_count;
    char desktop_names[MAX_DESKTOPS][32];
    int desktop_wallpapers[MAX_DESKTOPS];  // Цвета фона для десктопов
} WM;

// Флаги клиентов
#define CLIENT_FLOAT     (1 << 0)
#define CLIENT_DISCORD   (1 << 1)
#define CLIENT_HIDDEN    (1 << 2)
#define CLIENT_STICKY    (1 << 3)    // Виден на всех десктопах
#define CLIENT_MINIMIZED (1 << 4)

// ==================== ПРОТОТИПЫ ФУНКЦИЙ ====================

// Основные функции
void arrange_windows(WM *wm);
void toggle_float(WM *wm, Client *c);
void move_window_up(WM *wm);
void move_window_down(WM *wm);
void scroll_down(WM *wm);
void scroll_up(WM *wm);

// Функции десктопов
void init_desktops(WM *wm);
void switch_desktop(WM *wm, int desktop);
void next_desktop(WM *wm);
void prev_desktop(WM *wm);
void send_to_desktop(WM *wm, Client *c, int desktop);
void toggle_sticky(WM *wm, Client *c);
void draw_desktop_indicator(WM *wm);
void draw_desktop_bar(WM *wm);

// Анимация
void animate_window_move(WM *wm, Client *c, int target_x, int target_y);
void animate_window_resize(WM *wm, Client *c, int target_w, int target_h);

// ==================== ОБРАБОТЧИК ОШИБОК ====================

static int error_handler(Display *dpy, XErrorEvent *ev) {
    char buf[256];
    XGetErrorText(dpy, ev->error_code, buf, sizeof(buf));
    
    // Игнорируем некритические ошибки
    if (ev->error_code == BadWindow || 
        ev->error_code == BadMatch ||
        ev->error_code == BadDrawable ||
        ev->error_code == BadAccess ||
        ev->error_code == BadValue) {
        return 0;
    }
    
    fprintf(stderr, "X Error: %s (request code: %d, minor: %d)\n", 
            buf, ev->request_code, ev->minor_code);
    fprintf(stderr, "  Resource ID: 0x%lx\n", ev->resourceid);
    return 0;
}

// ==================== ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ ====================

int count_clients(Client *head) {
    int count = 0;
    Client *c = head;
    while (c) { 
        if (!(c->flags & CLIENT_HIDDEN)) count++;
        c = c->next; 
    }
    return count;
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
    // Сначала ищем активный клиент на текущем десктопе
    for (Client *c = wm->head; c; c = c->next) {
        if (!(c->flags & CLIENT_HIDDEN) && 
            (c->flags & CLIENT_STICKY || c->desktop == desktop || c->desktop == -1)) {
            return c;
        }
    }
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

// ==================== АНИМАЦИЯ ====================

void animate_window_move(WM *wm, Client *c, int target_x, int target_y) {
    if (!c) return;
    
    int start_x = c->x;
    int start_y = c->y;
    
    for (int i = 0; i <= ANIMATION_STEPS; i++) {
        float t = (float)i / ANIMATION_STEPS;
        int x = start_x + (target_x - start_x) * t;
        int y = start_y + (target_y - start_y) * t;
        XMoveWindow(wm->dpy, c->frame, x, y);
        usleep(ANIMATION_DELAY / ANIMATION_STEPS);
    }
}

void animate_window_resize(WM *wm, Client *c, int target_w, int target_h) {
    if (!c) return;
    
    int start_w = c->width;
    int start_h = c->height;
    
    for (int i = 0; i <= ANIMATION_STEPS; i++) {
        float t = (float)i / ANIMATION_STEPS;
        int w = start_w + (target_w - start_w) * t;
        int h = start_h + (target_h - start_h) * t;
        XResizeWindow(wm->dpy, c->frame, w, h);
        usleep(ANIMATION_DELAY / ANIMATION_STEPS);
    }
}

// ==================== ЛОГИКА УПРАВЛЕНИЯ ОКНАМИ ====================

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
            c->float_x = (wm->screen_width - c->float_width) / 2;
            c->float_y = (wm->screen_height - c->float_height) / 2;
        }
    }
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

void toggle_sticky(WM *wm, Client *c) {
    if (!c) return;
    c->flags ^= CLIENT_STICKY;
    arrange_windows(wm);
}

// ==================== ЛОГИКА ДЕСКТОПОВ ====================

void init_desktops(WM *wm) {
    wm->desktop_count = DEFAULT_DESKTOPS;
    wm->current_desktop = 0;
    
    // Названия десктопов
    const char *names[] = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "10"};
    for (int i = 0; i < MAX_DESKTOPS && i < wm->desktop_count; i++) {
        strncpy(wm->desktop_names[i], names[i], 31);
        wm->desktop_names[i][31] = '\0';
    }
    
    // Цвета фона для каждого десктопа
    int colors[] = {
        0x2B1E16,  // Темно-коричневый
        0x1A2B1E,  // Темно-зеленый
        0x1E1A2B,  // Темно-фиолетовый
        0x2B1A1E,  // Темно-красный
        0x1A2B2B,  // Темно-синий
        0x2B2B1A,  // Темно-желтый
        0x2B1A2B,  // Темно-розовый
        0x1A2B1A,  // Зеленый
        0x2B2B2B,  // Серый
        0x1A1A2B   // Темно-синий
    };
    
    for (int i = 0; i < MAX_DESKTOPS && i < wm->desktop_count; i++) {
        wm->desktop_wallpapers[i] = colors[i % 10];
    }
}

void switch_desktop(WM *wm, int desktop) {
    if (desktop < 0 || desktop >= wm->desktop_count || desktop == wm->current_desktop) {
        return;
    }
    
    // Скрываем все окна на старом десктопе
    Client *c;
    for (c = wm->head; c; c = c->next) {
        if (!(c->flags & CLIENT_STICKY) && c->desktop == wm->current_desktop) {
            XUnmapWindow(wm->dpy, c->frame);
        }
    }
    
    wm->current_desktop = desktop;
    
    // Устанавливаем фон рабочего стола
    XSetWindowBackground(wm->dpy, wm->root, wm->desktop_wallpapers[desktop]);
    XClearWindow(wm->dpy, wm->root);
    
    // Показываем окна на новом десктопе
    arrange_windows(wm);
    draw_desktop_bar(wm);
    
    // Находим активное окно на новом десктопе
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

// ==================== ОТРИСОВКА ПАНЕЛИ ДЕСКТОПОВ ====================

void draw_desktop_bar(WM *wm) {
    // Используем простую отрисовку через X
    int bar_height = 30;
    int bar_y = wm->screen_height - bar_height;
    
    // Очищаем панель
    XSetWindowBackground(wm->dpy, wm->root, wm->desktop_wallpapers[wm->current_desktop]);
    XClearWindow(wm->dpy, wm->root);
    
    // Рисуем панель
    GC gc = XCreateGC(wm->dpy, wm->root, 0, NULL);
    XSetForeground(wm->dpy, gc, 0x333333);
    XFillRectangle(wm->dpy, wm->root, gc, 0, bar_y, wm->screen_width, bar_height);
    
    // Рисуем индикаторы десктопов
    int indicator_width = 60;
    int indicator_height = 20;
    int total_width = wm->desktop_count * (indicator_width + 10);
    int start_x = (wm->screen_width - total_width) / 2;
    int indicator_y = bar_y + (bar_height - indicator_height) / 2;
    
    for (int i = 0; i < wm->desktop_count; i++) {
        int x = start_x + i * (indicator_width + 10);
        
        // Фон индикатора
        if (i == wm->current_desktop) {
            XSetForeground(wm->dpy, gc, 0xD4A05A);  // Активный
        } else {
            XSetForeground(wm->dpy, gc, 0x555555);  // Неактивный
        }
        XFillRectangle(wm->dpy, wm->root, gc, x, indicator_y, indicator_width, indicator_height);
        
        // Подсчет окон на десктопе
        int count = count_clients_on_desktop(wm->head, i);
        char label[32];
        snprintf(label, sizeof(label), "%s (%d)", wm->desktop_names[i], count);
        
        // Текст
        XSetForeground(wm->dpy, gc, 0xFFFFFF);
        XDrawString(wm->dpy, wm->root, gc, x + 5, indicator_y + 15, label, strlen(label));
    }
    
    XFreeGC(wm->dpy, gc);
    XSync(wm->dpy, False);
}

// ==================== ЛОГИКА ХОЛСТА И СКРОЛЛА ====================

void arrange_windows(WM *wm) {
    int count = count_clients_on_desktop(wm->head, wm->current_desktop);
    if (count == 0) {
        draw_desktop_bar(wm);
        return;
    }
    
    wm->gap = 10;
    wm->window_height = (wm->screen_height * 80) / 100;
    wm->window_width = wm->screen_width - (wm->gap * 2);
    
    Client *c = wm->head;
    int i = 0;
    
    while (c != NULL) {
        // Проверяем, принадлежит ли окно текущему десктопу
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
                
                // Ограничения
                if (x + w > wm->screen_width) x = wm->screen_width - w;
                if (y + h > wm->screen_height - 30) y = wm->screen_height - h - 30;
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
        } else {
            XUnmapWindow(wm->dpy, c->frame);
        }
        
        c = c->next;
        i++;
    }
    
    // Обновляем границы
    for (c = wm->head; c; c = c->next) {
        bool visible = (c->flags & CLIENT_STICKY) || 
                       c->desktop == wm->current_desktop || 
                       c->desktop == -1;
        
        if (visible && !(c->flags & CLIENT_HIDDEN)) {
            if (c == wm->active) {
                XSetWindowBorder(wm->dpy, c->frame, 0xD4A05A);
                XRaiseWindow(wm->dpy, c->frame);
            } else {
                XSetWindowBorder(wm->dpy, c->frame, 0x2B1E16);
            }
        }
    }
    
    draw_desktop_bar(wm);
    XSync(wm->dpy, False);
}

// ==================== ПРОКРУТКА ====================

void scroll_down(WM *wm) {
    if (count_clients_on_desktop(wm->head, wm->current_desktop) == 0) return;
    wm->scroll_offset += (wm->window_height + wm->gap);
    arrange_windows(wm);
}

void scroll_up(WM *wm) {
    if (count_clients_on_desktop(wm->head, wm->current_desktop) == 0) return;
    wm->scroll_offset -= (wm->window_height + wm->gap);
    if (wm->scroll_offset < 0) wm->scroll_offset = 0;
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
    
    if (attrs.width < MIN_WINDOW_WIDTH || attrs.height < MIN_WINDOW_HEIGHT) {
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
        fprintf(stderr, "Failed to allocate memory for client\n");
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
        client->float_width = wm->screen_width * 0.8;
        client->float_height = wm->screen_height * 0.8;
        client->float_x = (wm->screen_width - client->float_width) / 2;
        client->float_y = (wm->screen_height - client->float_height) / 2;
    }
    
    // Добавляем клиент в список
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
    const char *path = "~/.config/whxwm/whxwm.conf";
    FILE *f = fopen(path, "r");
    if (!f) {
        // Пробуем альтернативные пути
        path = "/etc/whxwm/whxwm.conf";
        f = fopen(path, "r");
        if (!f) {
            fprintf(stderr, "Config not found. Using defaults.\n");
            return;
        }
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
        
        // Проверяем специальные команды для десктопов
        if (strcmp(key_part, "desktops") == 0) {
            int count = atoi(cmd_part);
            if (count > 0 && count <= MAX_DESKTOPS) {
                wm->desktop_count = count;
            }
            continue;
        }
        
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
        if (!kb->command) {
            free(kb);
            continue;
        }
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
        else if (strcmp(key_str, "Left") == 0) kb->keysym = XK_Left;
        else if (strcmp(key_str, "Right") == 0) kb->keysym = XK_Right;
        else if (strlen(key_str) == 1) kb->keysym = XStringToKeysym(key_str);
        else kb->keysym = XStringToKeysym(key_str);
        
        if (kb->keysym != NoSymbol) {
            printf("Loaded keybind: Mod=%d, KeySym='%s', Command='%s'\n", 
                   kb->mod, key_str, kb->command);
            
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
    
    // Глобальные хоткеи
    if (state == Mod4Mask && keysym == XK_q) { 
        if (wm->active) close_window(wm, wm->active->window); 
        return; 
    }
    
    // Навигация по десктопам
    if (state == Mod4Mask && keysym == XK_1) { switch_desktop(wm, 0); return; }
    if (state == Mod4Mask && keysym == XK_2) { switch_desktop(wm, 1); return; }
    if (state == Mod4Mask && keysym == XK_3) { switch_desktop(wm, 2); return; }
    if (state == Mod4Mask && keysym == XK_4) { switch_desktop(wm, 3); return; }
    if (state == Mod4Mask && keysym == XK_5) { switch_desktop(wm, 4); return; }
    if (state == Mod4Mask && keysym == XK_6) { switch_desktop(wm, 5); return; }
    if (state == Mod4Mask && keysym == XK_7) { switch_desktop(wm, 6); return; }
    if (state == Mod4Mask && keysym == XK_8) { switch_desktop(wm, 7); return; }
    if (state == Mod4Mask && keysym == XK_9) { switch_desktop(wm, 8); return; }
    if (state == Mod4Mask && keysym == XK_0) { switch_desktop(wm, 9); return; }
    
    if (state == (Mod4Mask | ControlMask) && keysym == XK_Right) { next_desktop(wm); return; }
    if (state == (Mod4Mask | ControlMask) && keysym == XK_Left) { prev_desktop(wm); return; }
    
    // Отправка окна на другой десктоп
    if (state == (Mod4Mask | ShiftMask) && keysym >= XK_1 && keysym <= XK_9) {
        int desktop = keysym - XK_1;
        if (desktop < wm->desktop_count && wm->active) {
            send_to_desktop(wm, wm->active, desktop);
        }
        return;
    }
    
    if (state == (Mod4Mask | ShiftMask) && keysym == XK_0) {
        if (wm->active && wm->desktop_count > 9) {
            send_to_desktop(wm, wm->active, 9);
        }
        return;
    }
    
    // Закрепить окно на всех десктопах
    if (state == (Mod4Mask | ControlMask) && keysym == XK_s) {
        if (wm->active) toggle_sticky(wm, wm->active);
        return;
    }
    
    // Прокрутка
    if (state == Mod4Mask && keysym == XK_j) { scroll_down(wm); return; }
    if (state == Mod4Mask && keysym == XK_k) { scroll_up(wm); return; }
    
    // Плавающий режим
    if (state == Mod4Mask && keysym == XK_space) {
        if (wm->active) toggle_float(wm, wm->active);
        return;
    }
    
    // Перемещение плавающих окон
    if (state == Mod4Mask && keysym == XK_Up) { move_window_up(wm); return; }
    if (state == Mod4Mask && keysym == XK_Down) { move_window_down(wm); return; }
    
    // Пользовательские биндинги
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
    
    // Ограничения
    if (c->float_x < 0) c->float_x = 0;
    if (c->float_y < 0) c->float_y = 0;
    if (c->float_x + c->float_width > wm->screen_width) 
        c->float_x = wm->screen_width - c->float_width;
    if (c->float_y + c->float_height > wm->screen_height - 30) 
        c->float_y = wm->screen_height - c->float_height - 30;
    
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
    
    // Проверяем, не запущен ли другой WM
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
    
    // Инициализация десктопов
    init_desktops(wm);
    
    XSelectInput(wm->dpy, wm->root,
                 SubstructureRedirectMask | SubstructureNotifyMask |
                 StructureNotifyMask | PropertyChangeMask |
                 KeyPressMask | KeyReleaseMask | ButtonPressMask |
                 ButtonReleaseMask | UnmapNotify | ColormapChangeMask);
    
    XSync(wm->dpy, False);
    
    XSetWindowBackground(wm->dpy, wm->root, wm->desktop_wallpapers[wm->current_desktop]);
    XClearWindow(wm->dpy, wm->root);
    
    // Граббим хоткеи
    XUngrabKey(wm->dpy, AnyKey, AnyModifier, wm->root);
    
    KeyCode keys[] = {
        XKeysymToKeycode(wm->dpy, XK_q),
        XKeysymToKeycode(wm->dpy, XK_j),
        XKeysymToKeycode(wm->dpy, XK_k),
        XKeysymToKeycode(wm->dpy, XK_space),
        XKeysymToKeycode(wm->dpy, XK_Up),
        XKeysymToKeycode(wm->dpy, XK_Down),
        XKeysymToKeycode(wm->dpy, XK_Left),
        XKeysymToKeycode(wm->dpy, XK_Right),
    };
    
    for (int i = 0; i < 10; i++) {
        KeyCode code = XKeysymToKeycode(wm->dpy, XK_0 + i);
        XGrabKey(wm->dpy, code, Mod4Mask, wm->root, True, GrabModeAsync, GrabModeAsync);
        XGrabKey(wm->dpy, code, Mod4Mask | ShiftMask, wm->root, True, GrabModeAsync, GrabModeAsync);
    }
    
    size_t num_keys = sizeof(keys) / sizeof(keys[0]);
    for (size_t i = 0; i < num_keys; i++) {
        XGrabKey(wm->dpy, keys[i], Mod4Mask, wm->root, True, GrabModeAsync, GrabModeAsync);
    }
    
    // Дополнительные хоткеи
    KeyCode s_code = XKeysymToKeycode(wm->dpy, XK_s);
    XGrabKey(wm->dpy, s_code, Mod4Mask | ControlMask, wm->root, True, GrabModeAsync, GrabModeAsync);
    
    parse_config(wm);
    
    XSync(wm->dpy, False);
    
    printf("=== whxwm initialized successfully ===\n");
    printf("  Desktops: %d\n", wm->desktop_count);
    printf("  Current: %s\n", wm->desktop_names[wm->current_desktop]);
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
    
    printf("Starting whxwm with desktop support...\n");
    
    init_wm(&wm);
    draw_desktop_bar(&wm);
    run_wm(&wm);
    cleanup_wm(&wm);
    
    return 0;
}
