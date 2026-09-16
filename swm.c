#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/cursorfont.h>
#ifdef SWM_HAVE_XINERAMA
#include <X11/extensions/Xinerama.h>
#endif
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <locale.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <time.h>

#include "util.h"
#include "drw.h"
#include "config.h"

#define SWM_MAX_TAGS        31u
#define SWM_TAG_MASK_CAPACITY SWM_MAX_TAGS
#define SWM_DEFAULT_TAG_COUNT 9u
#define SWM_TAGMASK_ALL     ((uint32_t)((1u << SWM_MAX_TAGS) - 1u))
#define SWM_TITLE_MAX       256u
#define SWM_STATUS_MAX      256u
#define SWM_STATUS_FALLBACK "swm"
#define SWM_DEFAULT_TITLE   "untitled"
#define SWM_NO_MONITOR      (-1)
#define SWM_BAR_PADDING     2u
#ifndef SWM_CLIENT_BORDER_WIDTH
#define SWM_CLIENT_BORDER_WIDTH 1u
#endif
#ifndef SWM_DEFAULT_FLOATING
#define SWM_DEFAULT_FLOATING 0
#endif
#ifndef SWM_BAR_VISIBLE
#define SWM_BAR_VISIBLE     1
#endif
#ifndef SWM_BAR_TOP
#define SWM_BAR_TOP         1
#endif

/* These configuration values are intentionally overridable by the project
 * configuration/build.  The source file does not invent a version string. */
#ifndef SWM_VERSION
#error "SWM_VERSION must be supplied by the project configuration/build"
#endif
#ifndef SWM_FONT_COUNT
#define SWM_FONT_COUNT 1u
#endif
#ifndef SWM_FONT_PRIMARY
#define SWM_FONT_PRIMARY "monospace:size=10"
#endif
#if SWM_FONT_COUNT != 1u
#error "Component 4 currently requires exactly one configured font; multi-font configuration is defined later"
#endif
#ifndef SWM_COLOR_SCHEME_COUNT
#define SWM_COLOR_SCHEME_COUNT 1u
#endif
#ifndef SWM_COLOR_FG
#define SWM_COLOR_FG "#ffffff"
#endif
#ifndef SWM_COLOR_BG
#define SWM_COLOR_BG "#000000"
#endif
#ifndef SWM_COLOR_BORDER
#define SWM_COLOR_BORDER "#000000"
#endif
#ifndef SWM_COLOR_FG_SELECTED
#define SWM_COLOR_FG_SELECTED SWM_COLOR_FG
#endif
#ifndef SWM_COLOR_BG_SELECTED
#define SWM_COLOR_BG_SELECTED SWM_COLOR_BG
#endif
#ifndef SWM_COLOR_BORDER_SELECTED
#define SWM_COLOR_BORDER_SELECTED SWM_COLOR_BORDER
#endif

#if SWM_MAX_TAGS == 0 || SWM_MAX_TAGS >= 32
#error "SWM_MAX_TAGS must fit in a 32-bit tag mask"
#endif

typedef union Arg Arg;
typedef struct Key Key;
typedef struct Button Button;
typedef struct Layout Layout;
typedef struct Rule Rule;
typedef struct Client Client;
typedef struct Monitor Monitor;
typedef struct Geometry Geometry;
typedef struct SavedGeometry SavedGeometry;
typedef struct SizeHints SizeHints;
typedef void (*ActionFn)(const Arg *arg);
typedef void (*LayoutFn)(Monitor *m);
typedef void (*EventHandler)(XEvent *ev);

/* Generic action argument shared by keyboard and mouse bindings. */
union Arg {
    int i;
    unsigned int ui;
    double f;
    void *v;
};

/* Keyboard binding: modifier, X11 keysym, action, and generic argument. */
struct Key {
    unsigned int mod;
    KeySym keysym;
    ActionFn func;
    Arg arg;
};

enum ClickRegion {
    ClickTagIndicator = 0,
    ClickLayoutSymbol,
    ClickStatusText,
    ClickWindowTitle,
    ClickNewTag,
    ClickClientWindow,
    ClickRootBackground
};

/* Mouse binding: region, modifier, button, action, and generic argument. */
struct Button {
    enum ClickRegion region;
    unsigned int mod;
    unsigned int button;
    ActionFn func;
    Arg arg;
};

/* NULL arrange means a pure-floating/no-automatic-arrangement layout. */
struct Layout {
    const char *symbol;
    LayoutFn arrange;
};

/* NULL class/instance/title means that criterion matches any value. */
struct Rule {
    const char *class_name;
    const char *instance;
    const char *title;
    uint32_t tags;
    bool floating;
    int monitor; /* SWM_NO_MONITOR means no forced monitor. */
};

/* The configuration layer may replace this empty ordered rule set later. */
static const Rule swm_rules[1] = { { NULL, NULL, NULL, 0u, false, SWM_NO_MONITOR } };
static const size_t swm_rule_count = 0;

struct Geometry {
    int x;
    int y;
    unsigned int w;
    unsigned int h;
    unsigned int bw;
};

struct SavedGeometry {
    Geometry geometry;
    bool valid;
};

/* ICCCM size hints; aspect values are valid only when aspect_valid is true. */
struct SizeHints {
    unsigned int basew;
    unsigned int baseh;
    unsigned int incw;
    unsigned int inch;
    unsigned int minw;
    unsigned int minh;
    unsigned int maxw;
    unsigned int maxh;
    double min_aspect;
    double max_aspect;
    bool aspect_valid;
    bool valid;
};

struct Client {
    Window win;
    Monitor *mon;

    char title[SWM_TITLE_MAX];

    Geometry geom;
    SavedGeometry restore;
    SizeHints hints;

    bool fixed;
    bool floating;
    bool urgent;
    bool nofocus;
    bool fullscreen;
    bool was_floating;
    Geometry fullscreen_restore_geometry;
    bool fullscreen_restore_valid;

    /* A managed client must always carry at least one valid tag bit. */
    uint32_t tags;

    /* Independent tiling/order and focus-history linkages. */
    Client *next;
    Client *snext;
};

struct Monitor {
    int num;

    /* Bar state. */
    Window barwin;
    int by;
    bool barvisible;
    bool bartop;

    /* Full monitor geometry and the usable work area. */
    Geometry full;
    Geometry work;

    /* Current layout plus one previous layout slot. */
    const Layout *lt[2];
    unsigned int layout_slot;
    char layout_label[32];
    float mfact;
    unsigned int nmaster;

    /* Two-slot visible-tag history. The active slot must never be zero. */
    uint32_t tagset[2];
    unsigned int tagset_slot;

    /* Client collections are deliberately independent. */
    Client *clients;
    Client *stack;
    Client *sel;

    /* Monitors are linked circularly once discovery has completed. */
    Monitor *next;
};

enum AtomSlot {
    AtomWMProtocols = 0,
    AtomWMDeleteWindow,
    AtomWMWindowState,
    AtomUTF8String,
    AtomWMTakeFocus,
    AtomNetSupported,
    AtomNetWMName,
    AtomNetWMState,
    AtomNetSupportingWMCheck,
    AtomNetWMStateFullscreen,
    AtomNetActiveWindow,
    AtomNetWMWindowType,
    AtomNetWMWindowTypeDialog,
    AtomNetClientList,
    AtomCount
};

struct AtomState {
    Atom slot[AtomCount];
};

/* Event type is an array index; NULL is a safe unhandled-event miss. */
struct EventDispatch {
    EventHandler handlers[LASTEvent + 1];
};

/* ---------------------------- global state ---------------------------- */

static Display *dpy;
static int screen;
static Window root;
static Window wmcheckwin;
static unsigned int sw;
static unsigned int sh;

static Monitor *mons;
static Monitor *selmon;

/* Drawing state. */
static Drw *drw;
static Clr *scheme_normal;
static Clr *scheme_selected;
static Cur *cursor_normal;
static Cur *cursor_resize;
static Cur *cursor_move;

/* Derived later from the active font's line height and small padding. */
static unsigned int bh;
static unsigned int lrpad;
static unsigned int textpad;

/* Configuration/session state. */
static unsigned int tag_count;
static unsigned int numlockmask;

/* Cached root status and deterministic fallback client title. */
static char root_status[SWM_STATUS_MAX];
static const char default_client_title[] = SWM_DEFAULT_TITLE;

/* ICCCM/EWMH atom state. */
static struct AtomState atoms;

/* Event dispatch and X11 error-handler state. */
static struct EventDispatch event_dispatch;
static int (*previous_xerror)(Display *, XErrorEvent *);

/* Later quit behavior changes this to false; startup begins running. */
static bool running;
static Window last_entered;

/* Startup/runtime X error handling. */
static bool startup_xerror_seen;

static int swm_runtime_xerror(Display *display, XErrorEvent *event);
static unsigned int swm_get_numlock_mask(void);

static unsigned int swm_modifier_variant_count(unsigned int mod,
                                                 unsigned int variants[4]);
static void swm_grab_keys(void);
static void swm_regrab_keys(void);
static void swm_grab_buttons(Window win, enum ClickRegion region);
static void swm_grab_client_buttons(Client *c, bool focused);
static void swm_grab_root_buttons(void);
static void swm_grab_bar_buttons(Monitor *m);

static void swm_focus(Monitor *m, Client *target);
static void swm_set_fullscreen(Client *c, bool fullscreen);
static void swm_send_synthetic_configure(Client *c);
static bool swm_client_protocol_supported(Client *c, Atom protocol);
static void swm_send_protocol(Client *c, Atom protocol);
static void swm_create_bar(Monitor *m);
static void swm_update_bars(void);
static void swm_drawbar(Monitor *m);
static void swm_drawbars(void);
static void swm_arrange(Monitor *m);
static void swm_arrange_all(void);
static void swm_arrange_tiled(Monitor *m);
static void swm_arrange_monocle(Monitor *m);
static void swm_dispatch_button_press(XEvent *ev);
static void swm_dispatch_client_message(XEvent *ev);
static void swm_dispatch_configure_request(XEvent *ev);
static void swm_dispatch_configure_notify(XEvent *ev);
static void swm_dispatch_destroy_notify(XEvent *ev);
static void swm_dispatch_enter_notify(XEvent *ev);
static void swm_dispatch_expose(XEvent *ev);
static void swm_dispatch_focus_in(XEvent *ev);
static void swm_dispatch_key_press(XEvent *ev);
static void swm_dispatch_map_request(XEvent *ev);
static void swm_dispatch_mapping_notify(XEvent *ev);
static void swm_dispatch_motion_notify(XEvent *ev);
static void swm_dispatch_property_notify(XEvent *ev);
static void swm_init_status(void);
static void swm_unmanage(Client *c, bool destroyed);
static void swm_action_spawn(const Arg *arg);
static void swm_action_focus_stack(const Arg *arg);
static void swm_action_inc_nmaster(const Arg *arg);
static void swm_action_set_mfact(const Arg *arg);
static void swm_action_zoom(const Arg *arg);
static void swm_action_cycle_saved(const Arg *arg);
static void swm_action_toggle_floating(const Arg *arg);
static void swm_action_toggle_bar(const Arg *arg);
static void swm_action_view(const Arg *arg);
static void swm_action_toggle_view(const Arg *arg);
static void swm_action_tag(const Arg *arg);
static void swm_action_toggletag(const Arg *arg);
static void swm_action_focus_monitor(const Arg *arg);
static void swm_action_send_monitor(const Arg *arg);
static bool swm_transfer_client(Client *c, Monitor *to);
static void swm_action_move_resize(const Arg *arg);
static void swm_action_kill(const Arg *arg);
static void swm_action_quit(const Arg *arg);
static void swm_action_add_tag(const Arg *arg);

/*
 * These checked helpers intentionally reject invalid tag masks rather than
 * silently truncating bits outside the configured mask capacity.
 */
static bool
swm_valid_tag_count(unsigned int count)
{
    return count > 0 && count <= SWM_TAG_MASK_CAPACITY;
}

static bool
swm_valid_tag_mask(uint32_t mask)
{
    return mask != 0 && (mask & ~SWM_TAGMASK_ALL) == 0;
}

static bool
swm_set_tag_count(unsigned int count)
{
    if (!swm_valid_tag_count(count))
        return false;
    tag_count = count;
    return true;
}

static bool
swm_set_client_tags(Client *c, uint32_t mask)
{
    if (!c || !swm_valid_tag_mask(mask))
        return false;
    c->tags = mask;
    return true;
}

static bool
swm_set_visible_tags(Monitor *m, uint32_t mask)
{
    if (!m || !swm_valid_tag_mask(mask))
        return false;
    m->tagset[m->tagset_slot & 1u] = mask;
    return true;
}

/* A populated monitor collection must be one circular list whose cycle
 * returns to its head.  An empty collection is also valid. */
static bool
swm_monitor_list_circular(const Monitor *head)
{
    const Monitor *slow;
    const Monitor *fast;

    if (!head)
        return true;

    slow = head;
    fast = head;
    do {
        if (!fast || !fast->next)
            return false;
        slow = slow->next;
        fast = fast->next->next;
    } while (slow && fast && slow != fast);

    if (!slow || !fast)
        return false;

    /* Verify that the cycle entry is the collection head itself. */
    slow = head;
    while (slow != fast) {
        slow = slow->next;
        fast = fast->next;
        if (!slow || !fast)
            return false;
    }
    return slow == head;
}

/* Invariant 5: no monitors means no active monitor; otherwise exactly one
 * designated active monitor exists, belongs to the circular collection, and
 * the collection has the intended circular-link relationship. */
static bool
swm_active_monitor_valid(const Monitor *head, const Monitor *active)
{
    const Monitor *m;

    if (!head)
        return active == NULL;
    if (!active || !swm_monitor_list_circular(head))
        return false;

    for (m = head; ; m = m->next) {
        if (m == active)
            return true;
        if (m->next == head)
            break;
    }
    return false;
}

static void
swm_set_status(const char *text)
{
    size_t n;

    if (!text || !*text)
        text = SWM_STATUS_FALLBACK;

    n = strlen(text);
    if (n >= sizeof(root_status))
        n = sizeof(root_status) - 1;

    memcpy(root_status, text, n);
    root_status[n] = '\0';
}

static void
swm_set_client_title(Client *c, const char *text)
{
    size_t n;

    if (!c)
        return;

    if (!text || !*text)
        text = default_client_title;

    n = strlen(text);
    if (n >= sizeof(c->title))
        n = sizeof(c->title) - 1;

    memcpy(c->title, text, n);
    c->title[n] = '\0';
}

static void
swm_update_bar_pos(Monitor *m)
{
    if (!m)
        return;

    m->work = m->full;

    if (!m->barvisible) {
        m->by = -(int)bh;
        return;
    }

    if (m->work.h > bh)
        m->work.h -= bh;
    else
        m->work.h = 0;

    if (m->bartop) {
        m->by = m->full.y;
        m->work.y = m->full.y + (int)bh;
    } else {
        m->work.y = m->full.y;
        m->by = m->work.y + (int)m->work.h;
    }
}

static const Layout swm_layout_tiled = { "[]", swm_arrange_tiled };
static const Layout swm_layout_monocle = { "[ ]", swm_arrange_monocle };

static void
swm_init_monitor(Monitor *m, int index)
{
    if (!m)
        return;

    memset(m, 0, sizeof(*m));
    m->num = index;
    m->barvisible = SWM_BAR_VISIBLE != 0;
    m->bartop = SWM_BAR_TOP != 0;
    m->mfact = 0.55f;
    m->nmaster = 1;
    m->layout_slot = 0;
    m->layout_label[0] = '\0';
    m->lt[0] = &swm_layout_tiled;
    m->lt[1] = &swm_layout_monocle;
    m->tagset_slot = 0;
    m->tagset[0] = 1u;
    m->tagset[1] = 1u;
    m->next = m;
}

static bool
swm_same_geometry(const Geometry *a, const Geometry *b)
{
    return a && b && a->x == b->x && a->y == b->y &&
           a->w == b->w && a->h == b->h;
}

static void
swm_set_monitor_geometry(Monitor *m, int x, int y,
                           unsigned int w, unsigned int h)
{
    if (!m)
        return;
    m->full.x = x;
    m->full.y = y;
    m->full.w = w;
    m->full.h = h;
    swm_update_bar_pos(m);
}

static size_t
swm_monitor_count(void)
{
    const Monitor *m;
    size_t n = 0;

    if (!mons)
        return 0;
    for (m = mons; ; m = m->next) {
        n++;
        if (m->next == mons)
            break;
    }
    return n;
}

static Monitor **
swm_monitor_array(size_t count)
{
    Monitor **array;
    Monitor *m;
    size_t i = 0;

    if (!count)
        return NULL;
    array = ecalloc(count, sizeof(*array));
    for (m = mons; i < count; m = m->next)
        array[i++] = m;
    return array;
}

static bool
swm_monitor_geometry_unique(const Geometry *geoms, size_t count,
                             const Geometry *candidate)
{
    size_t i;

    for (i = 0; i < count; i++) {
        if (swm_same_geometry(&geoms[i], candidate))
            return false;
    }
    return true;
}

static void
swm_append_client(Client **head, Client *c, bool stack_link)
{
    Client **p;

    if (!head || !c)
        return;
    if (stack_link)
        c->snext = NULL;
    else
        c->next = NULL;

    p = head;
    while (*p)
        p = stack_link ? &(*p)->snext : &(*p)->next;
    *p = c;
}

static void
swm_migrate_monitor_clients(Monitor *from, Monitor *to)
{
    Client *c, *next;

    if (!from || !to || from == to)
        return;

    for (c = from->clients; c; c = next) {
        next = c->next;
        c->mon = to;
        swm_append_client(&to->clients, c, false);
    }
    from->clients = NULL;

    for (c = from->stack; c; c = next) {
        next = c->snext;
        c->mon = to;
        swm_append_client(&to->stack, c, true);
    }
    from->stack = NULL;

    from->sel = NULL;
    if (!to->sel)
        to->sel = to->clients;
}

static void
swm_link_monitor_list(Monitor **list, size_t count)
{
    size_t i;

    if (!list || !count) {
        mons = NULL;
        return;
    }
    for (i = 0; i < count; i++)
        list[i]->next = list[(i + 1) % count];
    mons = list[0];
}

static Monitor *
swm_find_geometry_match(Monitor **old, bool *used, size_t old_count,
                         const Geometry *geometry)
{
    size_t i;

    for (i = 0; i < old_count; i++) {
        if (!used[i] && swm_same_geometry(&old[i]->full, geometry)) {
            used[i] = true;
            return old[i];
        }
    }
    return NULL;
}

static Monitor *
swm_find_unmatched_old(Monitor **old, bool *used, size_t old_count)
{
    size_t i;

    for (i = 0; i < old_count; i++) {
        if (!used[i]) {
            used[i] = true;
            return old[i];
        }
    }
    return NULL;
}

static bool
swm_discover_monitors(void)
{
    Geometry *geoms = NULL;
    (void)swm_monitor_geometry_unique;
    Monitor **old = NULL;
    Monitor **newlist = NULL;
    bool *used = NULL;
    size_t old_count = swm_monitor_count();
    size_t geom_count = 0, i;
    bool changed = false;
    Monitor *oldsel = selmon;

#ifdef SWM_HAVE_XINERAMA
    if (XineramaIsActive(dpy)) {
        int screen_count = 0;
        XineramaScreenInfo *info = XineramaQueryScreens(dpy, &screen_count);

        if (info && screen_count > 0) {
            geoms = ecalloc((size_t)screen_count, sizeof(*geoms));
            for (i = 0; i < (size_t)screen_count; i++) {
                Geometry g;
                g.x = info[i].x_org;
                g.y = info[i].y_org;
                g.w = (unsigned int)info[i].width;
                g.h = (unsigned int)info[i].height;
                g.bw = 0;
                if (g.w && g.h &&
                    swm_monitor_geometry_unique(geoms, geom_count, &g))
                    geoms[geom_count++] = g;
            }
        }
        if (info)
            XFree(info);
    }
#endif

    if (!geom_count) {
        geoms = ecalloc(1, sizeof(*geoms));
        geoms[0].x = 0;
        geoms[0].y = 0;
        geoms[0].w = sw;
        geoms[0].h = sh;
        geoms[0].bw = 0;
        geom_count = 1;
    }

    old = swm_monitor_array(old_count);
    used = ecalloc(old_count ? old_count : 1, sizeof(*used));
    newlist = ecalloc(geom_count, sizeof(*newlist));

    for (i = 0; i < geom_count; i++) {
        Monitor *m = swm_find_geometry_match(old, used, old_count, &geoms[i]);

        if (!m)
            m = swm_find_unmatched_old(old, used, old_count);
        if (!m) {
            m = ecalloc(1, sizeof(*m));
            swm_init_monitor(m, (int)i);
            changed = true;
        }

        if (m->num != (int)i || !swm_same_geometry(&m->full, &geoms[i]))
            changed = true;

        m->num = (int)i;
        swm_set_monitor_geometry(m, geoms[i].x, geoms[i].y,
                                  geoms[i].w, geoms[i].h);
        newlist[i] = m;
    }

    for (i = 0; i < old_count; i++) {
        if (!used[i]) {
            Monitor *survivor = newlist[0];
            if (survivor)
                swm_migrate_monitor_clients(old[i], survivor);
            if (oldsel == old[i])
                oldsel = survivor;
            if (old[i]->barwin != None)
                XDestroyWindow(dpy, old[i]->barwin);
            free(old[i]);
            changed = true;
        }
    }

    swm_link_monitor_list(newlist, geom_count);
    selmon = oldsel;
    if (!swm_active_monitor_valid(mons, selmon))
        selmon = mons;

    if (mons && !swm_monitor_list_circular(mons))
        die("swm: monitor list is not circular after discovery");
    if (!swm_active_monitor_valid(mons, selmon))
        die("swm: invalid active monitor after discovery");

    free(old);
    free(used);
    free(geoms);
    free(newlist);
    return changed;
}

static Monitor *
swm_monitor_at_rect(int x, int y, unsigned int w, unsigned int h)
{
    Monitor *m, *best = NULL;
    unsigned long long best_area = 0;

    if (!mons)
        return NULL;

    for (m = mons; ; m = m->next) {
        long long right = (long long)x + w;
        long long bottom = (long long)y + h;
        long long mr = (long long)m->full.x + m->full.w;
        long long mb = (long long)m->full.y + m->full.h;
        long long ix1 = x > m->full.x ? x : m->full.x;
        long long iy1 = y > m->full.y ? y : m->full.y;
        long long ix2 = right < mr ? right : mr;
        long long iy2 = bottom < mb ? bottom : mb;
        unsigned long long area = 0;

        if (ix2 > ix1 && iy2 > iy1)
            area = (unsigned long long)(ix2 - ix1) *
                   (unsigned long long)(iy2 - iy1);
        if (area > best_area) {
            best_area = area;
            best = m;
        }
        if (m->next == mons)
            break;
    }
    return best_area ? best : selmon;
}

static Monitor *
swm_monitor_at_window(Window win)
{
    Monitor *m;
    Client *c;
    Window child;
    int x, y, root_x, root_y;
    unsigned int mask;

    if (!mons)
        return NULL;
    if (win == root) {
        if (XQueryPointer(dpy, root, &child, &child, &root_x, &root_y,
                          &x, &y, &mask))
            return swm_monitor_at_rect((int)root_x, (int)root_y, 1, 1);
        return selmon;
    }

    for (m = mons; ; m = m->next) {
        if (m->barwin == win)
            return m;
        for (c = m->clients; c; c = c->next) {
            if (c->win == win)
                return c->mon;
        }
        if (m->next == mons)
            break;
    }
    return selmon;
}


static Client *
swm_wintoclient(Window win)
{
    Monitor *m;
    Client *c;

    if (win == None || !mons)
        return NULL;
    for (m = mons; ; m = m->next) {
        for (c = m->clients; c; c = c->next) {
            if (c->win == win)
                return c;
        }
        if (m->next == mons)
            break;
    }
    return NULL;
}

static bool
swm_get_window_state(Window win, long *state)
{
    Atom actual;
    int format;
    unsigned long nitems, bytes_after;
    unsigned char *data = NULL;
    bool ok = false;

    if (XGetWindowProperty(dpy, win, atoms.slot[AtomWMWindowState], 0, 2,
                           False, atoms.slot[AtomWMWindowState], &actual,
                           &format, &nitems, &bytes_after, &data) == Success &&
        data && actual == atoms.slot[AtomWMWindowState] && format == 32 &&
        nitems >= 1) {
        *state = ((long *)data)[0];
        ok = true;
    }
    if (data)
        XFree(data);
    return ok;
}

static bool
swm_window_is_iconic(Window win)
{
    long state;

    return swm_get_window_state(win, &state) && state == IconicState;
}

static void
swm_set_window_state(Window win, long state)
{
    long data[2] = { state, None };

    XChangeProperty(dpy, win, atoms.slot[AtomWMWindowState],
                    atoms.slot[AtomWMWindowState], 32, PropModeReplace,
                    (unsigned char *)data, 2);
}

static void
swm_update_client_list(void)
{
    Monitor *m;
    Client *c;
    Window *list = NULL;
    size_t count = 0, i = 0;

    if (!mons) {
        XDeleteProperty(dpy, root, atoms.slot[AtomNetClientList]);
        return;
    }

    for (m = mons; ; m = m->next) {
        for (c = m->clients; c; c = c->next)
            count++;
        if (m->next == mons)
            break;
    }

    if (count)
        list = ecalloc(count, sizeof(*list));

    for (m = mons; ; m = m->next) {
        for (c = m->clients; c; c = c->next)
            list[i++] = c->win;
        if (m->next == mons)
            break;
    }

    if (count) {
        XChangeProperty(dpy, root, atoms.slot[AtomNetClientList], XA_WINDOW,
                        32, PropModeReplace, (unsigned char *)list,
                        (int)count);
    } else {
        XDeleteProperty(dpy, root, atoms.slot[AtomNetClientList]);
    }
    free(list);
}

static bool
swm_get_net_wm_name(Window win, char *out, size_t out_size)
{
    Atom actual;
    int format;
    unsigned long nitems, bytes_after;
    unsigned char *data = NULL;
    bool ok = false;
    size_t n;

    if (!out || out_size == 0)
        return false;
    out[0] = '\0';
    if (XGetWindowProperty(dpy, win, atoms.slot[AtomNetWMName], 0,
                           (long)out_size - 1, False,
                           atoms.slot[AtomUTF8String], &actual, &format,
                           &nitems, &bytes_after, &data) != Success)
        return false;
    if (data && actual == atoms.slot[AtomUTF8String] && format == 8 && nitems) {
        n = nitems < out_size ? (size_t)nitems : out_size - 1;
        memcpy(out, data, n);
        out[n] = '\0';
        ok = out[0] != '\0';
    }
    if (data)
        XFree(data);
    return ok;
}

static void
swm_update_client_title(Client *c)
{
    XTextProperty prop;
    char **list = NULL;
    int count = 0;
    char title[SWM_TITLE_MAX];

    if (!c)
        return;
    title[0] = '\0';

    if (swm_get_net_wm_name(c->win, title, sizeof(title))) {
        swm_set_client_title(c, title);
        return;
    }

    memset(&prop, 0, sizeof(prop));
    if (XGetWMName(dpy, c->win, &prop) && prop.value && prop.nitems) {
        if (XmbTextPropertyToTextList(dpy, &prop, &list, &count) >= Success &&
            count > 0 && list && list[0] && *list[0]) {
            swm_set_client_title(c, list[0]);
            XFreeStringList(list);
            XFree(prop.value);
            return;
        }
        if (list)
            XFreeStringList(list);
        if (prop.format == 8) {
            size_t n = prop.nitems < sizeof(title) - 1 ?
                       (size_t)prop.nitems : sizeof(title) - 1;
            memcpy(title, prop.value, n);
            title[n] = '\0';
            if (title[0]) {
                swm_set_client_title(c, title);
                XFree(prop.value);
                return;
            }
        }
    }
    if (prop.value)
        XFree(prop.value);
    swm_set_client_title(c, NULL);
}

static void
swm_update_size_hints(Client *c)
{
    XSizeHints size;
    long supplied = 0;
    bool have = false;

    if (!c)
        return;
    memset(&c->hints, 0, sizeof(c->hints));
    memset(&size, 0, sizeof(size));

    if (XGetWMNormalHints(dpy, c->win, &size, &supplied)) {
        have = true;
        if (size.flags & PBaseSize) {
            c->hints.basew = size.base_width;
            c->hints.baseh = size.base_height;
        }
        if (size.flags & PMinSize) {
            c->hints.minw = size.min_width;
            c->hints.minh = size.min_height;
        }
        if (size.flags & PMaxSize) {
            c->hints.maxw = size.max_width;
            c->hints.maxh = size.max_height;
        }
        if (size.flags & PResizeInc) {
            c->hints.incw = size.width_inc;
            c->hints.inch = size.height_inc;
        }
        if ((size.flags & PAspect) && size.min_aspect.x > 0 &&
            size.min_aspect.y > 0 && size.max_aspect.x > 0 &&
            size.max_aspect.y > 0) {
            c->hints.min_aspect = (double)size.min_aspect.x /
                                  (double)size.min_aspect.y;
            c->hints.max_aspect = (double)size.max_aspect.x /
                                  (double)size.max_aspect.y;
            c->hints.aspect_valid = true;
        }
        c->fixed = (size.flags & PMinSize) && (size.flags & PMaxSize) &&
                   size.min_width == size.max_width &&
                   size.min_height == size.max_height;
    }
    (void)supplied;
    c->hints.valid = have;
}

static void
swm_update_wm_hints(Client *c)
{
    XWMHints *hints;

    if (!c)
        return;
    c->urgent = false;
    c->nofocus = false;
    hints = XGetWMHints(dpy, c->win);
    if (!hints)
        return;
    if (hints->flags & XUrgencyHint)
        c->urgent = true;
    if (hints->flags & InputHint)
        c->nofocus = hints->input == False;
    XFree(hints);
}

static void
swm_update_window_state(Client *c)
{
    Atom actual;
    int format;
    unsigned long nitems, bytes_after, i;
    unsigned char *data = NULL;
    Atom *atoms_list;

    if (!c)
        return;
    c->fullscreen = false;
    if (XGetWindowProperty(dpy, c->win, atoms.slot[AtomNetWMState], 0,
                           32, False, XA_ATOM, &actual, &format, &nitems,
                           &bytes_after, &data) == Success && data &&
        actual == XA_ATOM && format == 32) {
        atoms_list = (Atom *)data;
        for (i = 0; i < nitems; i++) {
            if (atoms_list[i] == atoms.slot[AtomNetWMStateFullscreen]) {
                c->fullscreen = true;
                break;
            }
        }
    }
    if (data)
        XFree(data);

    if (XGetWindowProperty(dpy, c->win, atoms.slot[AtomNetWMWindowType], 0,
                           32, False, XA_ATOM, &actual, &format, &nitems,
                           &bytes_after, &data) == Success && data &&
        actual == XA_ATOM && format == 32) {
        atoms_list = (Atom *)data;
        for (i = 0; i < nitems; i++) {
            if (atoms_list[i] == atoms.slot[AtomNetWMWindowTypeDialog]) {
                c->floating = true;
                break;
            }
        }
    }
    if (data)
        XFree(data);
}

static Monitor *
swm_transient_monitor(Window win, uint32_t *tags, bool *is_transient)
{
    Window parent = None;
    Client *parent_client;

    if (is_transient)
        *is_transient = false;
    if (tags)
        *tags = 0;
    if (!XGetTransientForHint(dpy, win, &parent) || parent == None)
        return selmon;

    if (is_transient)
        *is_transient = true;
    parent_client = swm_wintoclient(parent);
    if (!parent_client)
        return selmon;
    if (tags)
        *tags = parent_client->tags;
    return parent_client->mon;
}

static void
swm_clamp_client_position(Client *c)
{
    long long totalw, totalh, minx, maxx, miny, maxy;
    Monitor *m;

    if (!c || !(m = c->mon))
        return;
    if (!m->work.w || !m->work.h)
        return;

    totalw = (long long)c->geom.w + 2LL * c->geom.bw;
    totalh = (long long)c->geom.h + 2LL * c->geom.bw;
    minx = (long long)m->work.x - totalw + 1;
    maxx = (long long)m->work.x + m->work.w - 1;
    miny = (long long)m->work.y - totalh + 1;
    maxy = (long long)m->work.y + m->work.h - 1;

    if ((long long)c->geom.x < minx)
        c->geom.x = (int)minx;
    if ((long long)c->geom.x > maxx)
        c->geom.x = (int)maxx;
    if ((long long)c->geom.y < miny)
        c->geom.y = (int)miny;
    if ((long long)c->geom.y > maxy)
        c->geom.y = (int)maxy;
}

static void
swm_insert_client(Client *c)
{
    Monitor *m;

    if (!c || !(m = c->mon))
        return;
    if (!m->clients) {
        m->clients = c;
    } else {
        Client *last = m->clients;
        while (last->next)
            last = last->next;
        last->next = c;
    }
    c->snext = m->stack;
    m->stack = c;
    if (!m->sel)
        m->sel = c;
}

static void
swm_remove_client_link(Client **head, Client *c, bool stack_link)
{
    Client **p;

    if (!head || !c)
        return;
    p = head;
    while (*p) {
        Client *cur = *p;
        Client *next = stack_link ? cur->snext : cur->next;
        if (cur == c) {
            *p = next;
            if (stack_link)
                c->snext = NULL;
            else
                c->next = NULL;
            return;
        }
        p = stack_link ? &cur->snext : &cur->next;
    }
}

static bool
swm_rule_matches(const Rule *rule, const char *class_name,
                  const char *instance, const char *title)
{
    if (!rule)
        return false;
    if (rule->class_name &&
        (!class_name || !strstr(class_name, rule->class_name)))
        return false;
    if (rule->instance &&
        (!instance || !strstr(instance, rule->instance)))
        return false;
    if (rule->title && (!title || !strstr(title, rule->title)))
        return false;
    return true;
}

static Monitor *
swm_monitor_by_index(int index)
{
    Monitor *m;

    if (!mons)
        return NULL;
    for (m = mons; ; m = m->next) {
        if (m->num == index)
            return m;
        if (m->next == mons)
            break;
    }
    return NULL;
}

static bool
swm_client_is_linked(const Client *c)
{
    Monitor *m;
    Client *it;

    if (!c || !mons)
        return false;

    for (m = mons; ; m = m->next) {
        for (it = m->clients; it; it = it->next)
            if (it == c)
                return true;
        for (it = m->stack; it; it = it->snext)
            if (it == c)
                return true;
        if (m->next == mons)
            break;
    }
    return false;
}

/*
 * Initial classification only: this routine runs before c is inserted into
 * any monitor collection. A matching monitor rule may therefore change
 * c->mon safely. Already-managed clients must be moved by a dedicated
 * migration operation, not by reapplying initial rules.
 */
static void
swm_apply_rules(Client *c)
{
    XClassHint hint;
    const char *class_name = "";
    const char *instance = "";
    const char *title;
    uint32_t rule_tags = 0;
    uint32_t inherited_tags;
    bool rule_floating = false;
    bool matched_rule = false;
    bool inherent_floating;
    bool transient = false;
    Window parent = None;
    size_t i;

    if (!c || !c->mon)
        return;
    if (swm_client_is_linked(c))
        die("swm: attempted to apply initial client rules after insertion");

    memset(&hint, 0, sizeof(hint));
    if (XGetClassHint(dpy, c->win, &hint)) {
        if (hint.res_class)
            class_name = hint.res_class;
        if (hint.res_name)
            instance = hint.res_name;
    }

    title = c->title;
    inherent_floating = c->floating;

    if (XGetTransientForHint(dpy, c->win, &parent) && parent != None)
        transient = true;

    /*
     * Only tags actually inherited from a transient parent participate in
     * the inherited portion of classification. A non-transient client's
     * initial tag value is not itself an inherited assignment.
     */
    inherited_tags = transient ? c->tags : 0u;

    for (i = 0; i < swm_rule_count; i++) {
        const Rule *rule = &swm_rules[i];
        Monitor *preferred;

        if (!swm_rule_matches(rule, class_name, instance, title))
            continue;

        matched_rule = true;
        rule_tags |= rule->tags & SWM_TAGMASK_ALL;
        rule_floating = rule->floating;
        if (rule->monitor != SWM_NO_MONITOR) {
            preferred = swm_monitor_by_index(rule->monitor);
            if (preferred)
                c->mon = preferred;
        }
    }

    c->floating = inherent_floating || (matched_rule && rule_floating);

    /*
     * Rule tags augment, rather than replace, tags inherited from a
     * transient parent. A zero-tag rule therefore cannot erase inherited
     * tags. If neither source contributes tags, use the monitor's current
     * visible tagset.
     */
    {
        uint32_t final_tags = (inherited_tags | rule_tags) & SWM_TAGMASK_ALL;

        if (final_tags != 0u && swm_valid_tag_mask(final_tags)) {
            if (!swm_set_client_tags(c, final_tags))
                die("swm: failed to apply client classification tags");
        } else if (c->mon &&
                   swm_valid_tag_mask(
                       c->mon->tagset[c->mon->tagset_slot & 1u])) {
            if (!swm_set_client_tags(
                    c, c->mon->tagset[c->mon->tagset_slot & 1u]))
                die("swm: failed to assign client visible tags");
        } else {
            die("swm: client classification produced invalid tags");
        }
    }

    if (hint.res_name)
        XFree(hint.res_name);
    if (hint.res_class)
        XFree(hint.res_class);

    if (!swm_active_monitor_valid(mons, c->mon) ||
        !swm_valid_tag_mask(c->tags))
        die("swm: invalid client state after rule classification");
}

static void
swm_manage(Client *c)
{
    if (!c || !c->mon)
        return;

    c->geom.bw = SWM_CLIENT_BORDER_WIDTH;
    swm_clamp_client_position(c);
    c->restore.valid = true;

    XSelectInput(dpy, c->win, EnterWindowMask | FocusChangeMask |
                 PropertyChangeMask | StructureNotifyMask);
    XSetWindowBorder(dpy, c->win, scheme_normal[2].pixel);
    XSetWindowBorderWidth(dpy, c->win, c->geom.bw);
    swm_grab_client_buttons(c, false);
    swm_insert_client(c);
    swm_update_client_list();
    swm_set_window_state(c->win, NormalState);
    if (c->fullscreen) {
        c->fullscreen_restore_geometry = c->geom;
        c->fullscreen_restore_valid = true;
        c->was_floating = c->floating;
        c->floating = true;
        c->geom = (Geometry){ c->mon->full.x, c->mon->full.y,
                              c->mon->full.w, c->mon->full.h, 0 };
        XMoveResizeWindow(dpy, c->win, c->geom.x, c->geom.y,
                           c->geom.w, c->geom.h);
    }
    swm_send_synthetic_configure(c);
    XMapWindow(dpy, c->win);
}

static Client *
swm_create_client(Window win, XWindowAttributes *wa)
{
    Client *c;
    Monitor *monitor;
    uint32_t inherited_tags = 0;
    bool transient = false;

    if (!wa)
        return NULL;
    c = ecalloc(1, sizeof(*c));
    c->win = win;
    c->geom.x = wa->x;
    c->geom.y = wa->y;
    c->geom.w = (unsigned int)wa->width;
    c->geom.h = (unsigned int)wa->height;
    c->geom.bw = (unsigned int)wa->border_width;
    c->restore.geometry = c->geom;
    c->restore.valid = true;
    c->floating = SWM_DEFAULT_FLOATING != 0;
    c->was_floating = c->floating;
    c->tags = 1u;

    monitor = swm_transient_monitor(win, &inherited_tags, &transient);
    if (!monitor)
        monitor = selmon;
    if (!monitor || !swm_active_monitor_valid(mons, monitor)) {
        free(c);
        return NULL;
    }
    c->mon = monitor;
    if (transient && inherited_tags)
        c->tags = inherited_tags;

    swm_update_client_title(c);
    swm_update_size_hints(c);
    swm_update_wm_hints(c);
    swm_update_window_state(c);
    if (c->fixed || transient)
        c->floating = true;
    c->was_floating = c->floating;

    if (!swm_valid_tag_mask(c->tags)) {
        free(c);
        return NULL;
    }
    return c;
}

static bool
swm_manage_window(Window win, bool map_window)
{
    XWindowAttributes wa;
    Client *c;

    if (win == None || win == root || win == wmcheckwin || swm_wintoclient(win))
        return false;
    if (!XGetWindowAttributes(dpy, win, &wa))
        return false;
    if (wa.override_redirect)
        return false;
    c = swm_create_client(win, &wa);
    if (!c)
        return false;
    swm_apply_rules(c);
    swm_manage(c);
    if (!map_window)
        XUnmapWindow(dpy, win);
    return true;
}

static bool
swm_window_eligible(Window win, bool *transient)
{
    XWindowAttributes wa;
    Window parent = None;

    if (transient)
        *transient = false;
    if (win == None || win == root || win == wmcheckwin || swm_wintoclient(win))
        return false;
    if (!XGetWindowAttributes(dpy, win, &wa) || wa.override_redirect)
        return false;
    if (XGetTransientForHint(dpy, win, &parent) && parent != None) {
        if (transient)
            *transient = true;
    }
    return wa.map_state == IsViewable || swm_window_is_iconic(win);
}

static void
swm_scan_existing_windows(void)
{
    Window root_return, parent_return, *children = NULL;
    unsigned int nchildren = 0, i;
    bool *transients = NULL;

    if (!XQueryTree(dpy, root, &root_return, &parent_return,
                    &children, &nchildren))
        return;
    if (nchildren)
        transients = ecalloc(nchildren, sizeof(*transients));

    for (i = 0; i < nchildren; i++) {
        bool transient;
        if (swm_window_eligible(children[i], &transient)) {
            transients[i] = transient;
            if (!transient)
                (void)swm_manage_window(children[i], true);
        }
    }

    for (i = 0; i < nchildren; i++) {
        XWindowAttributes wa;
        Window parent;

        if (!transients || !transients[i])
            continue;
        if (!XGetWindowAttributes(dpy, children[i], &wa) || wa.override_redirect)
            continue;
        if (!(wa.map_state == IsViewable || swm_window_is_iconic(children[i])))
            continue;
        if (!XGetTransientForHint(dpy, children[i], &parent) || parent == None)
            continue;
        (void)swm_manage_window(children[i], true);
    }

    free(transients);
    if (children)
        XFree(children);
}

static void
swm_unmanage(Client *c, bool destroyed)
{
    Monitor *m;
    bool was_selected;
    int (*old_handler)(Display *, XErrorEvent *);

    if (!c)
        return;
    m = c->mon;
    was_selected = m && m->sel == c;

    if (m) {
        swm_remove_client_link(&m->clients, c, false);
        swm_remove_client_link(&m->stack, c, true);
        if (was_selected)
            m->sel = NULL;
        if (m->sel && m->sel->mon != m)
            m->sel = NULL;
    }
    swm_update_client_list();

    old_handler = XSetErrorHandler(swm_runtime_xerror);
    if (!destroyed) {
        XSetWindowBorderWidth(dpy, c->win, c->restore.geometry.bw);
        XSelectInput(dpy, c->win, NoEventMask);
        swm_set_window_state(c->win, WithdrawnState);
        XUnmapWindow(dpy, c->win);
        XSync(dpy, False);
    }
    XSetErrorHandler(old_handler);
    free(c);
}


/* Built-in bindings for the interactive default configuration. */
static const Key swm_keys[] = {
    { Mod4Mask, XK_1, swm_action_view, { .ui = 1u << 0 } },
    { Mod4Mask, XK_2, swm_action_view, { .ui = 1u << 1 } },
    { Mod4Mask, XK_3, swm_action_view, { .ui = 1u << 2 } },
    { Mod4Mask, XK_4, swm_action_view, { .ui = 1u << 3 } },
    { Mod4Mask, XK_5, swm_action_view, { .ui = 1u << 4 } },
    { Mod4Mask, XK_6, swm_action_view, { .ui = 1u << 5 } },
    { Mod4Mask, XK_7, swm_action_view, { .ui = 1u << 6 } },
    { Mod4Mask, XK_8, swm_action_view, { .ui = 1u << 7 } },
    { Mod4Mask, XK_9, swm_action_view, { .ui = 1u << 8 } }
};
static const size_t swm_key_count = sizeof(swm_keys) / sizeof(swm_keys[0]);

static const Button swm_buttons[] = {
    { ClickTagIndicator, 0u, Button1, swm_action_view, { 0 } },
    { ClickNewTag,       0u, Button1, swm_action_add_tag, { 0 } },
    { ClickClientWindow, 0u, Button1, NULL, { 0 } },
    { ClickClientWindow, Mod4Mask, Button1, swm_action_move_resize, { .i = 1 } },
    { ClickClientWindow, Mod4Mask, Button3, swm_action_move_resize, { .i = -1 } }
};
static const size_t swm_button_count = sizeof(swm_buttons) / sizeof(swm_buttons[0]);

/* -------------------------- focus / protocols ------------------------- */

static bool
swm_client_visible(const Client *c, const Monitor *m)
{
    return c && m && c->mon == m &&
           swm_valid_tag_mask(c->tags) &&
           (c->tags & m->tagset[m->tagset_slot & 1u]) != 0u;
}

static Client *
swm_focus_candidate(Monitor *m, Client *target)
{
    Client *c;

    if (!m)
        return NULL;
    if (target && swm_client_visible(target, m))
        return target;
    for (c = m->stack; c; c = c->snext)
        if (swm_client_visible(c, m))
            return c;
    return NULL;
}

static void
swm_focus(Monitor *m, Client *target)
{
    Client *old, *newsel;

    if (!m)
        return;
    newsel = swm_focus_candidate(m, target);
    old = m->sel;

    if (old && old != newsel) {
        swm_grab_client_buttons(old, false);
        XSetWindowBorder(dpy, old->win, scheme_normal[2].pixel);
    }

    if (newsel) {
        if (newsel != old) {
            swm_remove_client_link(&m->stack, newsel, true);
            newsel->snext = m->stack;
            m->stack = newsel;
        }
        m->sel = newsel;
        XSetWindowBorder(dpy, newsel->win, scheme_selected[2].pixel);
        swm_grab_client_buttons(newsel, true);
        if (!newsel->nofocus)
            XSetInputFocus(dpy, newsel->win, RevertToPointerRoot, CurrentTime);
        {
            Window w = newsel->win;
            XChangeProperty(dpy, root, atoms.slot[AtomNetActiveWindow],
                            XA_WINDOW, 32, PropModeReplace,
                            (unsigned char *)&w, 1);
        }
        if (swm_client_protocol_supported(newsel,
                                           atoms.slot[AtomWMTakeFocus]))
            swm_send_protocol(newsel, atoms.slot[AtomWMTakeFocus]);
    } else {
        m->sel = NULL;
        XSetInputFocus(dpy, root, RevertToPointerRoot, CurrentTime);
        XDeleteProperty(dpy, root, atoms.slot[AtomNetActiveWindow]);
    }
    swm_drawbar(m);
}

static bool
swm_client_protocol_supported(Client *c, Atom protocol)
{
    Atom *list = NULL;
    int count = 0;
    bool found = false;
    int i;

    if (!c || protocol == None)
        return false;
    if (!XGetWMProtocols(dpy, c->win, &list, &count))
        return false;
    for (i = 0; i < count; i++) {
        if (list[i] == protocol) {
            found = true;
            break;
        }
    }
    if (list)
        XFree(list);
    return found;
}

static void
swm_send_protocol(Client *c, Atom protocol)
{
    XEvent ev;

    if (!c || protocol == None)
        return;
    memset(&ev, 0, sizeof(ev));
    ev.xclient.type = ClientMessage;
    ev.xclient.window = c->win;
    ev.xclient.message_type = atoms.slot[AtomWMProtocols];
    ev.xclient.format = 32;
    ev.xclient.data.l[0] = (long)protocol;
    ev.xclient.data.l[1] = CurrentTime;
    XSendEvent(dpy, c->win, False, NoEventMask, &ev);
}

static void
swm_send_synthetic_configure(Client *c)
{
    XEvent ev;

    if (!c)
        return;
    memset(&ev, 0, sizeof(ev));
    ev.xconfigure.type = ConfigureNotify;
    ev.xconfigure.display = dpy;
    ev.xconfigure.event = c->win;
    ev.xconfigure.window = c->win;
    ev.xconfigure.x = c->geom.x;
    ev.xconfigure.y = c->geom.y;
    ev.xconfigure.width = (int)c->geom.w;
    ev.xconfigure.height = (int)c->geom.h;
    ev.xconfigure.border_width = (int)c->geom.bw;
    ev.xconfigure.above = None;
    ev.xconfigure.override_redirect = False;
    XSendEvent(dpy, c->win, False, StructureNotifyMask, &ev);
}

static void
swm_set_fullscreen_property(Client *c, bool enabled)
{
    Atom actual;
    int format;
    unsigned long nitems, bytes_after, i, out = 0;
    Atom *data = NULL;
    Atom *result = NULL;

    if (!c)
        return;
    if (XGetWindowProperty(dpy, c->win, atoms.slot[AtomNetWMState], 0,
                           64, False, XA_ATOM, &actual, &format, &nitems,
                           &bytes_after, (unsigned char **)&data) == Success &&
        data && actual == XA_ATOM && format == 32) {
        result = ecalloc(nitems + (enabled ? 1u : 0u), sizeof(*result));
        for (i = 0; i < nitems; i++) {
            if (data[i] != atoms.slot[AtomNetWMStateFullscreen])
                result[out++] = data[i];
        }
        if (enabled)
            result[out++] = atoms.slot[AtomNetWMStateFullscreen];
        if (out)
            XChangeProperty(dpy, c->win, atoms.slot[AtomNetWMState], XA_ATOM,
                            32, PropModeReplace, (unsigned char *)result,
                            (int)out);
        else
            XDeleteProperty(dpy, c->win, atoms.slot[AtomNetWMState]);
    } else if (enabled) {
        Atom a = atoms.slot[AtomNetWMStateFullscreen];
        XChangeProperty(dpy, c->win, atoms.slot[AtomNetWMState], XA_ATOM,
                        32, PropModeAppend, (unsigned char *)&a, 1);
    }
    if (data)
        XFree(data);
    free(result);
}

static void
swm_set_fullscreen(Client *c, bool fullscreen)
{
    Monitor *m;

    if (!c || !(m = c->mon))
        return;
    if (fullscreen == c->fullscreen)
        return;

    if (fullscreen) {
        c->fullscreen_restore_geometry = c->geom;
        c->fullscreen_restore_valid = true;
        c->was_floating = c->floating;
        c->floating = true;
        c->fullscreen = true;
        swm_set_fullscreen_property(c, true);
        c->geom.x = m->full.x;
        c->geom.y = m->full.y;
        c->geom.w = m->full.w;
        c->geom.h = m->full.h;
        c->geom.bw = 0;
        XMoveResizeWindow(dpy, c->win, c->geom.x, c->geom.y,
                           c->geom.w, c->geom.h);
        XRaiseWindow(dpy, c->win);
    } else {
        swm_set_fullscreen_property(c, false);
        c->fullscreen = false;
        if (c->fullscreen_restore_valid) {
            c->geom = c->fullscreen_restore_geometry;
            c->fullscreen_restore_valid = false;
        }
        c->floating = c->was_floating;
        if (c->geom.bw == 0)
            c->geom.bw = SWM_CLIENT_BORDER_WIDTH;
        XMoveResizeWindow(dpy, c->win, c->geom.x, c->geom.y,
                           c->geom.w, c->geom.h);
    }
    swm_arrange(m);
}

/* --------------------------- bar management --------------------------- */

static Monitor *
swm_monitor_from_bar(Window win)
{
    Monitor *m;
    if (!mons || win == None)
        return NULL;
    for (m = mons; ; m = m->next) {
        if (m->barwin == win)
            return m;
        if (m->next == mons)
            break;
    }
    return NULL;
}

static void
swm_create_bar(Monitor *m)
{
    XSetWindowAttributes wa;

    if (!m || m->barwin != None || !dpy || root == None)
        return;
    memset(&wa, 0, sizeof(wa));
    wa.background_pixel = scheme_normal[1].pixel;
    wa.event_mask = ExposureMask | ButtonPressMask | EnterWindowMask;
    wa.override_redirect = True;
    wa.cursor = cursor_normal->cursor;
    m->barwin = XCreateWindow(dpy, root, m->full.x, m->by,
                              m->full.w, bh ? bh : 1u, 0,
                              DefaultDepth(dpy, screen), InputOutput,
                              DefaultVisual(dpy, screen),
                              CWBackPixel | CWEventMask | CWOverrideRedirect | CWCursor, &wa);
    if (m->barwin == None)
        return;
    XDefineCursor(dpy, m->barwin, cursor_normal->cursor);
    swm_grab_bar_buttons(m);
    if (m->barvisible)
        XMapWindow(dpy, m->barwin);
}

static void
swm_update_bars(void)
{
    Monitor *m;

    if (!mons)
        return;
    for (m = mons; ; m = m->next) {
        if (m->barwin != None) {
            XMoveResizeWindow(dpy, m->barwin, m->full.x, m->by,
                              m->full.w, bh ? bh : 1u);
            if (m->barvisible)
                XMapWindow(dpy, m->barwin);
            else
                XUnmapWindow(dpy, m->barwin);
        } else {
            swm_create_bar(m);
        }
        if (m->next == mons)
            break;
    }
}

static bool
swm_any_urgent_on_tag(unsigned int tag)
{
    Monitor *m;
    Client *c;
    uint32_t bit = 1u << tag;

    if (!mons)
        return false;
    for (m = mons; ; m = m->next) {
        for (c = m->clients; c; c = c->next)
            if (c->urgent && (c->tags & bit))
                return true;
        if (m->next == mons)
            break;
    }
    return false;
}

static void
swm_drawbar(Monitor *m)
{
    unsigned int x = 0, i, statusw = 0;
    char label[32];
    const char *layout = "";

    if (!m || m->barwin == None || !drw || !drw->fonts)
        return;

    drw_setscheme(drw, scheme_normal);
    drw_rect(drw, 0, 0, m->full.w, bh, true, false);

    for (i = 0; i < tag_count; i++) {
        unsigned int w;
        snprintf(label, sizeof(label), "%u", i + 1u);
        w = drw_fontset_getwidth(drw, label) + textpad;
        if (x + w > m->full.w)
            w = m->full.w - x;
        if (w == 0)
            break;
        {
            bool urgent = swm_any_urgent_on_tag(i);
            bool selected = (m->tagset[m->tagset_slot & 1u] & (1u << i)) != 0u;
            drw_setscheme(drw, (urgent || selected) ? scheme_selected : scheme_normal);
            drw_rect(drw, x, 0, w, bh, true, urgent);
            drw_text(drw, x, 0, w, bh, textpad / 2u, label, urgent);
        }
        if (m->sel && (m->sel->tags & (1u << i))) {
            unsigned int markerw = w > 2u ? 2u : w;
            drw_rect(drw, x, bh > 2u ? bh - 2u : 0u, markerw,
                     bh > 2u ? 2u : bh, true, false);
        }
        x += w;
        if (x >= m->full.w)
            break;
    }

    if (tag_count < SWM_TAG_MASK_CAPACITY) {
        unsigned int w = drw_fontset_getwidth(drw, "+") + textpad;
        if (x + w > m->full.w)
            w = m->full.w - x;
        if (w) {
            drw_setscheme(drw, scheme_normal);
            drw_text(drw, x, 0, w, bh, textpad / 2u, "+", false);
            x += w;
        }
    }

    if (m->layout_label[0])
        layout = m->layout_label;
    else if (m->lt[m->layout_slot & 1u] &&
             m->lt[m->layout_slot & 1u]->symbol)
        layout = m->lt[m->layout_slot & 1u]->symbol;
    if (*layout) {
        unsigned int w = drw_fontset_getwidth(drw, layout) + textpad;
        if (x + w > m->full.w)
            w = m->full.w - x;
        drw_setscheme(drw, scheme_normal);
        drw_text(drw, x, 0, w, bh, textpad / 2u, layout, false);
        x += w;
    }

    if (root_status[0])
        statusw = drw_fontset_getwidth(drw, root_status) + textpad;
    if (statusw > m->full.w - x)
        statusw = m->full.w - x;
    if (statusw) {
        drw_setscheme(drw, scheme_normal);
        drw_text(drw, m->full.w - statusw, 0, statusw, bh,
                 textpad / 2u, root_status, false);
    }

    drw_map(drw, m->barwin, 0, 0, m->full.w, bh);
}

static void
swm_drawbars(void)
{
    Monitor *m;
    if (!mons)
        return;
    for (m = mons; ; m = m->next) {
        swm_drawbar(m);
        if (m->next == mons)
            break;
    }
}

/* -------------------------- geometry / arrange ------------------------ */

static void
swm_constrain_size(Client *c, unsigned int *w, unsigned int *h,
                    bool respect_hints)
{
    unsigned int minsize = bh ? bh : 1u;
    unsigned int width, height;
    SizeHints *s;
    double ratio;

    if (!c || !w || !h)
        return;
    width = *w < minsize ? minsize : *w;
    height = *h < minsize ? minsize : *h;
    if (!respect_hints || !c->hints.valid) {
        *w = width;
        *h = height;
        return;
    }

    s = &c->hints;
    if (width < s->basew) width = s->basew;
    if (height < s->baseh) height = s->baseh;

    if (s->aspect_valid && height) {
        ratio = (double)width / (double)height;
        if (ratio < s->min_aspect)
            width = (unsigned int)ceil((double)height * s->min_aspect);
        else if (ratio > s->max_aspect)
            height = (unsigned int)ceil((double)width / s->max_aspect);
    }

    if (s->incw && width > s->basew)
        width = s->basew + ((width - s->basew) / s->incw) * s->incw;
    if (s->inch && height > s->baseh)
        height = s->baseh + ((height - s->baseh) / s->inch) * s->inch;

    if (s->minw && width < s->minw) width = s->minw;
    if (s->minh && height < s->minh) height = s->minh;
    if (s->maxw && width > s->maxw) width = s->maxw;
    if (s->maxh && height > s->maxh) height = s->maxh;

    if (width < minsize) width = minsize;
    if (height < minsize) height = minsize;
    *w = width;
    *h = height;
}

static void
swm_clamp_geometry_to(Monitor *m, Geometry *g, bool full_area)
{
    long long left, top, right, bottom, maxx, maxy;
    unsigned int aw, ah;
    int ax, ay;

    if (!m || !g)
        return;
    ax = full_area ? m->full.x : m->work.x;
    ay = full_area ? m->full.y : m->work.y;
    aw = full_area ? m->full.w : m->work.w;
    ah = full_area ? m->full.h : m->work.h;
    if (!aw || !ah)
        return;
    left = ax;
    top = ay;
    right = (long long)ax + aw;
    bottom = (long long)ay + ah;
    maxx = right - 1;
    maxy = bottom - 1;
    if ((long long)g->x + g->w + 2LL * g->bw <= left)
        g->x = ax;
    else if ((long long)g->x >= right)
        g->x = (int)(right - g->w - 2LL * g->bw);
    if ((long long)g->y + g->h + 2LL * g->bw <= top)
        g->y = ay;
    else if ((long long)g->y >= bottom)
        g->y = (int)(bottom - g->h - 2LL * g->bw);
    if ((long long)g->x > maxx)
        g->x = (int)maxx;
    if ((long long)g->y > maxy)
        g->y = (int)maxy;
}

static void
swm_apply_geometry(Client *c, int x, int y, unsigned int w,
                    unsigned int h, bool tiled)
{
    if (!c)
        return;
    if (w < 1u) w = 1u;
    if (h < 1u) h = 1u;
    if (!tiled)
        swm_constrain_size(c, &w, &h, true);
    c->geom.x = x;
    c->geom.y = y;
    c->geom.w = w;
    c->geom.h = h;
    if (c->win != None && !c->fullscreen)
        XMoveResizeWindow(dpy, c->win, x, y, w, h);
}

static void
swm_arrange_tiled(Monitor *m)
{
    Client *c;
    unsigned int n = 0, master_n, placed = 0;
    unsigned int master_w = 0, stack_w;
    unsigned int my = 0, sy = 0;

    for (c = m->clients; c; c = c->next)
        if (!c->floating && !c->fullscreen && swm_client_visible(c, m))
            n++;
    if (!n)
        return;

    master_n = m->nmaster < n ? m->nmaster : n;
    if (m->nmaster == 0)
        master_n = 0;
    if (master_n == n)
        master_w = m->work.w;
    else if (master_n)
        master_w = (unsigned int)((double)m->work.w * m->mfact);
    stack_w = m->work.w - master_w;

    for (c = m->clients; c; c = c->next) {
        unsigned int rem, h, w;
        int x;
        if (c->floating || c->fullscreen || !swm_client_visible(c, m))
            continue;
        if (placed < master_n) {
            rem = master_n - placed;
            h = rem ? (m->work.h - my) / rem : 0;
            if (rem == 1u)
                h = m->work.h - my;
            w = master_w;
            x = m->work.x;
            if (w > 2u * c->geom.bw) w -= 2u * c->geom.bw; else w = 1u;
            if (h > 2u * c->geom.bw) h -= 2u * c->geom.bw; else h = 1u;
            swm_apply_geometry(c, x, m->work.y + (int)my, w, h, true);
            my += h + 2u * c->geom.bw;
        } else {
            rem = n - master_n - (placed - master_n);
            h = rem ? (m->work.h - sy) / rem : 0;
            if (rem == 1u)
                h = m->work.h - sy;
            w = stack_w;
            x = m->work.x + (int)master_w;
            if (w > 2u * c->geom.bw) w -= 2u * c->geom.bw; else w = 1u;
            if (h > 2u * c->geom.bw) h -= 2u * c->geom.bw; else h = 1u;
            swm_apply_geometry(c, x, m->work.y + (int)sy, w, h, true);
            sy += h + 2u * c->geom.bw;
        }
        placed++;
    }
}

static void
swm_arrange_monocle(Monitor *m)
{
    Client *c;
    unsigned int n = 0;

    for (c = m->clients; c; c = c->next)
        if (!c->floating && !c->fullscreen && swm_client_visible(c, m))
            n++;
    for (c = m->clients; c; c = c->next) {
        unsigned int w, h;
        if (c->floating || c->fullscreen || !swm_client_visible(c, m))
            continue;
        w = m->work.w > 2u * c->geom.bw ?
            m->work.w - 2u * c->geom.bw : 1u;
        h = m->work.h > 2u * c->geom.bw ?
            m->work.h - 2u * c->geom.bw : 1u;
        swm_apply_geometry(c, m->work.x, m->work.y, w, h, true);
    }
    (void)n;
}

static void
swm_arrange(Monitor *m)
{
    Client *c;
    Window stack_array[256];
    unsigned int nstack = 0;
    bool arranged;

    if (!m)
        return;

    /* Hide clients that do not belong to the currently visible desktop.
     * Do not move them to a huge/off-screen coordinate: X11 window
     * coordinates are limited by the protocol and such a move can wrap or
     * otherwise leave the window visible. DWM's model is to unmap clients
     * which are not visible and map them again when their tag is selected. */
    for (c = m->stack; c; c = c->snext) {
        if (c->win == None)
            continue;
        if (!swm_client_visible(c, m)) {
            XUnmapWindow(dpy, c->win);
        } else {
            XMapWindow(dpy, c->win);
        }
    }

    /* Apply geometry to visible clients before final restacking. */
    for (c = m->stack; c; c = c->snext) {
        if (!swm_client_visible(c, m) || c->win == None)
            continue;
        if (c->fullscreen) {
            c->geom.x = m->full.x;
            c->geom.y = m->full.y;
            c->geom.w = m->full.w;
            c->geom.h = m->full.h;
            c->geom.bw = 0;
            XMoveResizeWindow(dpy, c->win, c->geom.x, c->geom.y,
                               c->geom.w, c->geom.h);
        } else if (c->floating || !m->lt[m->layout_slot & 1u] ||
                   !m->lt[m->layout_slot & 1u]->arrange) {
            XMoveResizeWindow(dpy, c->win, c->geom.x, c->geom.y,
                               c->geom.w, c->geom.h);
        }
    }

    m->layout_label[0] = '\0';
    arranged = m->lt[m->layout_slot & 1u] &&
               m->lt[m->layout_slot & 1u]->arrange != NULL;
    if (arranged)
        m->lt[m->layout_slot & 1u]->arrange(m);
    if (m->lt[m->layout_slot & 1u] &&
        m->lt[m->layout_slot & 1u]->arrange == swm_arrange_monocle) {
        unsigned int count = 0;
        for (c = m->clients; c; c = c->next)
            if (!c->floating && !c->fullscreen && swm_client_visible(c, m))
                count++;
        snprintf(m->layout_label, sizeof(m->layout_label), "[%u]", count);
    }

    for (c = m->stack; c && nstack < 256u; c = c->snext)
        if (swm_client_visible(c, m) && !c->floating && !c->fullscreen)
            stack_array[nstack++] = c->win;
    if (nstack)
        XRestackWindows(dpy, stack_array, (int)nstack);
    for (c = m->stack; c; c = c->snext)
        if (swm_client_visible(c, m) && c->floating && !c->fullscreen)
            XRaiseWindow(dpy, c->win);
    if (m->sel && swm_client_visible(m->sel, m) &&
        (m->sel->floating || !arranged))
        XRaiseWindow(dpy, m->sel->win);
    if (m->barwin != None)
        XRaiseWindow(dpy, m->barwin);
    swm_drawbar(m);
}

static void
swm_arrange_all(void)
{
    Monitor *m;
    if (!mons)
        return;
    for (m = mons; ; m = m->next) {
        swm_arrange(m);
        if (m->next == mons)
            break;
    }
}

/* ----------------------------- actions -------------------------------- */

static void
swm_action_spawn(const Arg *arg)
{
    pid_t pid;
    char *const *argv;

    if (!arg || !arg->v)
        return;
    argv = (char *const *)arg->v;
    pid = fork();
    if (pid < 0)
        return;
    if (pid == 0) {
        if (setsid() < 0)
            _exit(127);
        signal(SIGCHLD, SIG_DFL);
        execvp(argv[0], argv);
        fprintf(stderr, "swm: execvp failed: %s\n", strerror(errno));
        _exit(127);
    }
}

static void
swm_action_focus_stack(const Arg *arg)
{
    Monitor *m = selmon;
    Client *c, *first = NULL, *target = NULL;
    int direction;

    if (!m || !m->sel || !arg)
        return;
    direction = arg->i >= 0 ? 1 : -1;
    if (direction > 0) {
        for (c = m->clients; c; c = c->next) {
            if (c == m->sel) { first = c->next; break; }
        }
        if (!first) first = m->clients;
        for (c = first; c; c = c->next)
            if (swm_client_visible(c, m)) { target = c; break; }
        if (!target)
            for (c = m->clients; c && c != first; c = c->next)
                if (swm_client_visible(c, m)) { target = c; break; }
    } else {
        Client *prev = NULL;
        for (c = m->clients; c; c = c->next) {
            if (c == m->sel) break;
            if (swm_client_visible(c, m)) prev = c;
        }
        if (prev) target = prev;
        else {
            for (c = m->clients; c; c = c->next)
                if (swm_client_visible(c, m)) target = c;
        }
    }
    if (target)
        swm_focus(m, target);
    swm_arrange(m);
}

static void
swm_action_inc_nmaster(const Arg *arg)
{
    int next;
    if (!selmon || !arg) return;
    next = (int)selmon->nmaster + arg->i;
    if (next < 0) next = 0;
    selmon->nmaster = (unsigned int)next;
    swm_arrange(selmon);
}

static void
swm_action_set_mfact(const Arg *arg)
{
    double next;
    if (!selmon || !arg) return;
    next = fabs(arg->f) < 1.0 ? (double)selmon->mfact + arg->f : arg->f - 1.0;
    if (next < 0.05 || next > 0.95)
        return;
    selmon->mfact = (float)next;
    swm_arrange(selmon);
}

static void
swm_action_zoom(const Arg *arg)
{
    Client *target = NULL, *c;
    (void)arg;
    if (!selmon || !selmon->sel || !selmon->lt[selmon->layout_slot & 1u] ||
        !selmon->lt[selmon->layout_slot & 1u]->arrange || selmon->sel->floating)
        return;
    for (c = selmon->clients; c; c = c->next)
        if (!c->floating && !c->fullscreen && swm_client_visible(c, selmon)) {
            if (!target) target = c;
            if (c == selmon->sel) break;
        }
    if (target == selmon->sel) {
        for (c = selmon->sel->next; c; c = c->next)
            if (!c->floating && !c->fullscreen && swm_client_visible(c, selmon)) {
                target = c; break;
            }
        if (!target)
            for (c = selmon->clients; c && c != selmon->sel; c = c->next)
                if (!c->floating && !c->fullscreen && swm_client_visible(c, selmon)) {
                    target = c; break;
                }
    }
    if (!target) return;
    swm_remove_client_link(&selmon->clients, target, false);
    target->next = selmon->clients;
    selmon->clients = target;
    swm_focus(selmon, target);
    swm_arrange(selmon);
}

static void
swm_action_cycle_saved(const Arg *arg)
{
    (void)arg;
    if (!selmon) return;
    selmon->tagset_slot ^= 1u;
    selmon->layout_slot ^= 1u;
    swm_focus(selmon, NULL);
    swm_arrange(selmon);
}

static void
swm_action_toggle_floating(const Arg *arg)
{
    (void)arg;
    if (!selmon || !selmon->sel || selmon->sel->fullscreen) return;
    selmon->sel->floating = !selmon->sel->floating;
    swm_arrange(selmon);
}

static void
swm_action_toggle_bar(const Arg *arg)
{
    (void)arg;
    if (!selmon) return;
    selmon->barvisible = !selmon->barvisible;
    swm_update_bar_pos(selmon);
    if (selmon->barwin != None) {
        if (selmon->barvisible) XMapWindow(dpy, selmon->barwin);
        else XUnmapWindow(dpy, selmon->barwin);
    }
    swm_arrange(selmon);
}

static void
swm_action_view(const Arg *arg)
{
    uint32_t mask;
    if (!selmon || !arg) return;
    mask = arg->ui & SWM_TAGMASK_ALL;
    if (!swm_valid_tag_mask(mask) || !swm_set_visible_tags(selmon, mask)) return;
    swm_focus(selmon, NULL);
    swm_arrange(selmon);
}

static void
swm_action_toggle_view(const Arg *arg)
{
    uint32_t mask, result;
    if (!selmon || !arg) return;
    mask = arg->ui & SWM_TAGMASK_ALL;
    result = selmon->tagset[selmon->tagset_slot & 1u] ^ mask;
    if (!swm_valid_tag_mask(result) || !swm_set_visible_tags(selmon, result)) return;
    swm_focus(selmon, NULL);
    swm_arrange(selmon);
}

static void
swm_action_tag(const Arg *arg)
{
    if (!selmon || !selmon->sel || !arg) return;
    if (!swm_set_client_tags(selmon->sel, arg->ui & SWM_TAGMASK_ALL)) return;
    swm_focus(selmon, NULL);
    swm_arrange(selmon);
}

static void
swm_action_toggletag(const Arg *arg)
{
    uint32_t result;
    if (!selmon || !selmon->sel || !arg) return;
    result = selmon->sel->tags ^ (arg->ui & SWM_TAGMASK_ALL);
    if (!swm_valid_tag_mask(result) || !swm_set_client_tags(selmon->sel, result)) return;
    swm_focus(selmon, NULL);
    swm_arrange(selmon);
}

static void
swm_action_focus_monitor(const Arg *arg)
{
    Monitor *old, *newm;
    int direction;
    if (!mons || !selmon || !arg) return;
    direction = arg->i >= 0 ? 1 : -1;
    old = selmon;
    newm = direction > 0 ? old->next : old;
    if (direction < 0) {
        Monitor *m;
        for (m = mons; m->next != old; m = m->next) { }
        newm = m;
    }
    if (newm == old) return;
    swm_focus(old, NULL);
    selmon = newm;
    swm_focus(selmon, selmon->sel);
}

static bool
swm_transfer_client(Client *c, Monitor *to)
{
    Monitor *from;
    bool was_sel;

    if (!c || !to || !(from = c->mon) || from == to)
        return false;
    was_sel = from->sel == c;
    swm_remove_client_link(&from->clients, c, false);
    swm_remove_client_link(&from->stack, c, true);
    if (was_sel)
        from->sel = NULL;
    c->mon = to;
    if (!swm_set_client_tags(c, to->tagset[to->tagset_slot & 1u]))
        return false;
    swm_insert_client(c);
    if (c->fullscreen) {
        c->geom.x = to->full.x;
        c->geom.y = to->full.y;
        c->geom.w = to->full.w;
        c->geom.h = to->full.h;
        c->geom.bw = 0;
        XMoveResizeWindow(dpy, c->win, c->geom.x, c->geom.y,
                           c->geom.w, c->geom.h);
    }
    return true;
}

static void
swm_action_send_monitor(const Arg *arg)
{
    Monitor *from, *to;
    Client *c;
    int direction;
    if (!selmon || !selmon->sel || !arg || !mons || mons->next == mons) return;
    from = selmon;
    c = from->sel;
    direction = arg->i >= 0 ? 1 : -1;
    if (direction > 0) to = from->next;
    else {
        Monitor *m;
        for (m = mons; m->next != from; m = m->next) { }
        to = m;
    }
    if (!swm_transfer_client(c, to))
        return;
    selmon = to;
    swm_focus(from, NULL);
    swm_focus(to, c);
    swm_arrange(from);
    swm_arrange(to);
}

static void
swm_action_move_resize(const Arg *arg)
{
    Client *c;
    XEvent ev;
    Cursor cursor;
    bool resize;
    int startx, starty, base_x, base_y;
    unsigned int base_w, base_h;
    unsigned long last_motion = 0;
    (void)arg;

    if (!selmon || !(c = selmon->sel) || c->fullscreen)
        return;
    resize = arg && arg->i < 0;
    cursor = resize ? cursor_resize->cursor : cursor_move->cursor;
    if (XGrabPointer(dpy, root, False,
                     PointerMotionMask | ButtonReleaseMask | ExposureMask |
                     StructureNotifyMask,
                     GrabModeAsync, GrabModeAsync, None, cursor, CurrentTime)
        != GrabSuccess)
        return;
    XRaiseWindow(dpy, c->win);
    {
        Window child;
        unsigned int mask;
        XQueryPointer(dpy, root, &child, &child, &startx, &starty, &base_x, &base_y, &mask);
    }
    base_x = c->geom.x; base_y = c->geom.y;
    base_w = c->geom.w; base_h = c->geom.h;
    if (resize)
        XWarpPointer(dpy, None, c->win, 0, 0, 0, 0,
                     (int)c->geom.w, (int)c->geom.h);
    while (XMaskEvent(dpy, PointerMotionMask | ButtonReleaseMask | ExposureMask |
                      StructureNotifyMask, &ev)) {
        if (ev.type == MotionNotify) {
            int dx, dy;
            if ((unsigned long)(ev.xmotion.time - last_motion) < 16ul &&
                last_motion != 0ul)
                continue;
            last_motion = ev.xmotion.time;
            dx = ev.xmotion.x_root - startx;
            dy = ev.xmotion.y_root - starty;
            if (!resize) {
                if (!c->floating && selmon->lt[selmon->layout_slot & 1u] &&
                    selmon->lt[selmon->layout_slot & 1u]->arrange) {
                    int dist = abs(dx) > abs(dy) ? abs(dx) : abs(dy);
                    if (dist < 10) continue;
                    c->floating = true;
                }
                c->geom.x = base_x + dx;
                c->geom.y = base_y + dy;
                if (abs(c->geom.x - selmon->work.x) < 10) c->geom.x = selmon->work.x;
                if (abs(c->geom.y - selmon->work.y) < 10) c->geom.y = selmon->work.y;
                swm_clamp_geometry_to(c->mon, &c->geom, true);
                XMoveWindow(dpy, c->win, c->geom.x, c->geom.y);
            } else {
                unsigned int nw = base_w + (dx > 0 ? (unsigned int)dx : 0u);
                unsigned int nh = base_h + (dy > 0 ? (unsigned int)dy : 0u);
                if (dx < 0 && (unsigned int)(-dx) < base_w) nw = base_w - (unsigned int)(-dx);
                if (dy < 0 && (unsigned int)(-dy) < base_h) nh = base_h - (unsigned int)(-dy);
                swm_constrain_size(c, &nw, &nh, true);
                c->geom.w = nw; c->geom.h = nh;
                XMoveResizeWindow(dpy, c->win, c->geom.x, c->geom.y, nw, nh);
            }
        } else if (ev.type == Expose) {
            swm_dispatch_expose(&ev);
        } else if (ev.type == ConfigureRequest) {
            swm_dispatch_configure_request(&ev);
        } else if (ev.type == ButtonRelease) {
            break;
        }
    }
    XUngrabPointer(dpy, CurrentTime);
    if (resize) {
        XWarpPointer(dpy, None, c->win, 0, 0, 0, 0,
                     (int)c->geom.w, (int)c->geom.h);
    }
    {
        Monitor *origin = c->mon;
        Monitor *dest = swm_monitor_at_rect(c->geom.x, c->geom.y,
                                              c->geom.w, c->geom.h);
        if (dest && dest != origin && swm_transfer_client(c, dest)) {
            selmon = dest;
            swm_focus(origin, NULL);
            swm_focus(dest, c);
            swm_arrange(origin);
            swm_arrange(dest);
        } else {
            swm_arrange(c->mon);
        }
    }
}

static void
swm_action_kill(const Arg *arg)
{
    int (*old_handler)(Display *, XErrorEvent *);
    (void)arg;
    if (!selmon || !selmon->sel) return;
    if (swm_client_protocol_supported(selmon->sel,
                                       atoms.slot[AtomWMDeleteWindow])) {
        swm_send_protocol(selmon->sel, atoms.slot[AtomWMDeleteWindow]);
        return;
    }
    old_handler = XSetErrorHandler(swm_runtime_xerror);
    XKillClient(dpy, selmon->sel->win);
    XSync(dpy, False);
    XSetErrorHandler(old_handler);
}

static void
swm_action_add_tag(const Arg *arg)
{
    uint32_t mask;
    (void)arg;

    if (!selmon || tag_count >= SWM_TAG_MASK_CAPACITY)
        return;

    tag_count++;
    mask = 1u << (tag_count - 1u);
    if (!swm_set_visible_tags(selmon, mask))
        return;
    swm_focus(selmon, NULL);
    swm_arrange(selmon);
    swm_drawbars();
}

static void
swm_action_quit(const Arg *arg)
{
    (void)arg;
    running = false;
}

/* ----------------------------- dispatch -------------------------------- */

static unsigned int
swm_clean_modifiers(unsigned int state)
{
    return state & 0xffu & ~(unsigned int)LockMask & ~numlockmask;
}

static bool
swm_button_matches(const Button *button, enum ClickRegion region,
                    unsigned int state, unsigned int detail)
{
    return button && button->region == region && button->button == detail &&
           swm_clean_modifiers(state) == swm_clean_modifiers(button->mod);
}

static enum ClickRegion
swm_bar_region(Monitor *m, int x, unsigned int *tag_index)
{
    unsigned int pos = 0, i;
    char label[32];

    if (tag_index) *tag_index = 0;
    if (!m) return ClickRootBackground;
    for (i = 0; i < tag_count; i++) {
        unsigned int w;
        snprintf(label, sizeof(label), "%u", i + 1u);
        w = drw_fontset_getwidth(drw, label) + textpad;
        if ((unsigned int)x < pos + w) {
            if (tag_index) *tag_index = i;
            return ClickTagIndicator;
        }
        pos += w;
    }
    if (tag_count < SWM_TAG_MASK_CAPACITY) {
        unsigned int w = drw_fontset_getwidth(drw, "+") + textpad;
        if ((unsigned int)x < pos + w) {
            return ClickNewTag;
        }
        pos += w;
    }
    if (m->lt[m->layout_slot & 1u] && m->lt[m->layout_slot & 1u]->symbol) {
        unsigned int w = drw_fontset_getwidth(drw,
                                               m->lt[m->layout_slot & 1u]->symbol) + textpad;
        if ((unsigned int)x < pos + w) return ClickLayoutSymbol;
        pos += w;
    }
    if (root_status[0]) {
        unsigned int w = drw_fontset_getwidth(drw, root_status) + textpad;
        if ((unsigned int)x >= m->full.w - (w < m->full.w ? w : m->full.w))
            return ClickStatusText;
    }
    return ClickWindowTitle;
}

static void
swm_dispatch_button_press(XEvent *ev)
{
    XButtonEvent *e = &ev->xbutton;
    Monitor *m;
    Client *c;
    enum ClickRegion region;
    unsigned int tag = 0, i;

    m = swm_monitor_at_window(e->window);
    if (!m) return;
    if (m != selmon) {
        if (selmon) swm_focus(selmon, NULL);
        selmon = m;
        swm_focus(selmon, selmon->sel);
    }
    c = swm_wintoclient(e->window);
    if (c) {
        region = ClickClientWindow;
        swm_focus(m, c);
        XRaiseWindow(dpy, c->win);
        XAllowEvents(dpy, ReplayPointer, e->time);
    } else if (e->window == m->barwin) {
        region = swm_bar_region(m, e->x, &tag);
    } else if (e->window == root) {
        region = ClickRootBackground;
    } else {
        return;
    }
    for (i = 0; i < swm_button_count; i++) {
        const Button *b = &swm_buttons[i];
        Arg arg;
        if (!swm_button_matches(b, region, e->state, e->button)) continue;
        arg = b->arg;
        if (region == ClickTagIndicator && arg.ui == 0u)
            arg.ui = 1u << tag;
        if (b->func)
            b->func(&arg);
        break;
    }
}

static void
swm_dispatch_client_message(XEvent *ev)
{
    XClientMessageEvent *e = &ev->xclient;
    Client *c = swm_wintoclient(e->window);
    if (e->message_type == atoms.slot[AtomNetWMState] && c) {
        Atom a = (Atom)e->data.l[1];
        Atom b = (Atom)e->data.l[2];
        bool requested = c->fullscreen;
        if (a == atoms.slot[AtomNetWMStateFullscreen] ||
            b == atoms.slot[AtomNetWMStateFullscreen]) {
            if (e->data.l[0] == 0) requested = false;
            else if (e->data.l[0] == 1) requested = true;
            else if (e->data.l[0] == 2) requested = !requested;
            swm_set_fullscreen(c, requested);
        }
    } else if (e->message_type == atoms.slot[AtomNetActiveWindow] && c) {
        if (c != c->mon->sel) {
            c->urgent = true;
            swm_drawbar(c->mon);
        }
    }
}

static void
swm_dispatch_configure_request(XEvent *ev)
{
    XConfigureRequestEvent *e = &ev->xconfigurerequest;
    Client *c = swm_wintoclient(e->window);
    XWindowChanges wc;
    unsigned int mask = e->value_mask;

    if (!c) {
        wc.x = e->x; wc.y = e->y; wc.width = e->width; wc.height = e->height;
        wc.border_width = e->border_width; wc.sibling = e->above; wc.stack_mode = e->detail;
        XConfigureWindow(dpy, e->window, mask, &wc);
        return;
    }
    if ((mask & ~CWBorderWidth) == 0u) {
        if (mask & CWBorderWidth) {
            c->geom.bw = e->border_width;
            wc.border_width = e->border_width;
            XConfigureWindow(dpy, c->win, CWBorderWidth, &wc);
        }
        return;
    }
    if (c->floating || !c->mon->lt[c->mon->layout_slot & 1u] ||
        !c->mon->lt[c->mon->layout_slot & 1u]->arrange) {
        Geometry old = c->geom;
        if (mask & CWX) c->geom.x = e->x;
        if (mask & CWY) c->geom.y = e->y;
        if (mask & CWWidth) c->geom.w = e->width;
        if (mask & CWHeight) c->geom.h = e->height;
        if (mask & CWBorderWidth) c->geom.bw = e->border_width;
        swm_constrain_size(c, &c->geom.w, &c->geom.h, true);
        swm_clamp_geometry_to(c->mon, &c->geom, false);
        if (swm_client_visible(c, c->mon)) {
            if (mask & CWBorderWidth)
                XSetWindowBorderWidth(dpy, c->win, c->geom.bw);
            XMoveResizeWindow(dpy, c->win, c->geom.x, c->geom.y,
                               c->geom.w, c->geom.h);
        }
        if ((old.x != c->geom.x || old.y != c->geom.y) &&
            old.w == c->geom.w && old.h == c->geom.h)
            swm_send_synthetic_configure(c);
    } else {
        swm_send_synthetic_configure(c);
    }
}

static void
swm_dispatch_configure_notify(XEvent *ev)
{
    XConfigureEvent *e = &ev->xconfigure;
    bool changed;
    Monitor *m;
    if (e->window != root) return;
    if ((unsigned int)e->width == sw && (unsigned int)e->height == sh) {
        changed = swm_discover_monitors();
    } else {
        sw = (unsigned int)e->width;
        sh = (unsigned int)e->height;
        changed = swm_discover_monitors();
    }
    if (!changed) return;
    drw_resize(drw, sw, sh);
    swm_update_bars();
    for (m = mons; m; ) {
        Monitor *next = m->next;
        for (Client *c = m->clients; c; c = c->next)
            if (c->fullscreen) {
                c->geom.x = m->full.x; c->geom.y = m->full.y;
                c->geom.w = m->full.w; c->geom.h = m->full.h; c->geom.bw = 0;
                XMoveResizeWindow(dpy, c->win, c->geom.x, c->geom.y, c->geom.w, c->geom.h);
            }
        if (!mons || next == mons) {
            swm_arrange(m);
            break;
        }
        swm_arrange(m);
        m = next;
    }
    swm_focus(selmon, NULL);
}

static void
swm_dispatch_destroy_notify(XEvent *ev)
{
    Client *c = swm_wintoclient(ev->xdestroywindow.window);
    if (c) swm_unmanage(c, true);
}

static void
swm_dispatch_enter_notify(XEvent *ev)
{
    XCrossingEvent *e = &ev->xcrossing;
    Monitor *m;
    Client *c;
    if (e->mode != NotifyNormal || e->detail == NotifyInferior)
        return;
    if (e->window == root)
        return;
    if (e->window == last_entered)
        return;
    last_entered = e->window;
    m = swm_monitor_at_window(e->window);
    if (!m) return;
    c = swm_wintoclient(e->window);
    if (m != selmon) {
        if (selmon) swm_focus(selmon, NULL);
        selmon = m;
    }
    swm_focus(m, c);
}

static void
swm_dispatch_expose(XEvent *ev)
{
    Monitor *m = swm_monitor_from_bar(ev->xexpose.window);
    if (m && ev->xexpose.count == 0)
        swm_drawbar(m);
}

static void
swm_dispatch_focus_in(XEvent *ev)
{
    Monitor *m;
    Client *c;
    if (ev->xfocus.mode != NotifyNormal && ev->xfocus.mode != NotifyWhileGrabbed)
        return;
    c = swm_wintoclient(ev->xfocus.window);
    m = c ? c->mon : selmon;
    if (m && m->sel && ev->xfocus.window != m->sel->win)
        swm_focus(m, m->sel);
}

static void
swm_dispatch_key_press(XEvent *ev)
{
    KeySym sym = XLookupKeysym(&ev->xkey, 0);
    unsigned int state = swm_clean_modifiers(ev->xkey.state);
    size_t i;
    for (i = 0; i < swm_key_count; i++) {
        const Key *k = &swm_keys[i];
        if (k->keysym == sym && swm_clean_modifiers(k->mod) == state) {
            if (k->func) k->func(&k->arg);
            break;
        }
    }
}

static void
swm_dispatch_map_request(XEvent *ev)
{
    XWindowAttributes wa;
    Client *c;
    if (!XGetWindowAttributes(dpy, ev->xmaprequest.window, &wa) || wa.override_redirect)
        return;
    if (swm_wintoclient(ev->xmaprequest.window)) {
        c = swm_wintoclient(ev->xmaprequest.window);
        XMapWindow(dpy, c->win);
        return;
    }
    if (swm_manage_window(ev->xmaprequest.window, true)) {
        c = swm_wintoclient(ev->xmaprequest.window);
        if (c) {
            if (c->mon == selmon) swm_focus(c->mon, c);
            swm_arrange(c->mon);
        }
    }
}

static void
swm_dispatch_mapping_notify(XEvent *ev)
{
    if (ev->xmapping.request == MappingKeyboard ||
        ev->xmapping.request == MappingModifier) {
        XRefreshKeyboardMapping(&ev->xmapping);
        numlockmask = swm_get_numlock_mask();
        swm_regrab_keys();
    }
}

static void
swm_dispatch_motion_notify(XEvent *ev)
{
    Monitor *m;
    if (ev->xmotion.window != root) return;
    m = swm_monitor_at_rect(ev->xmotion.x_root, ev->xmotion.y_root, 1, 1);
    if (m && m != selmon) {
        if (selmon) swm_focus(selmon, NULL);
        selmon = m;
        swm_focus(selmon, selmon->sel);
    }
}

static void
swm_dispatch_property_notify(XEvent *ev)
{
    XPropertyEvent *e = &ev->xproperty;
    Client *c;
    if (e->state == PropertyDelete)
        return;
    if (e->window == root && e->atom == XA_WM_NAME) {
        swm_init_status();
        swm_drawbar(selmon);
        return;
    }
    c = swm_wintoclient(e->window);
    if (!c) return;
    if (e->atom == XA_WM_NORMAL_HINTS) {
        c->hints.valid = false;
    } else if (e->atom == XA_WM_HINTS) {
        swm_update_wm_hints(c);
        swm_drawbars();
    } else if (e->atom == XA_WM_NAME || e->atom == atoms.slot[AtomNetWMName]) {
        swm_update_client_title(c);
        if (c == c->mon->sel) swm_drawbar(c->mon);
    } else if (e->atom == XA_WM_TRANSIENT_FOR) {
        Window parent;
        if (!c->floating && XGetTransientForHint(dpy, c->win, &parent) && parent != None) {
            c->floating = true;
            swm_arrange(c->mon);
        }
    } else if (e->atom == atoms.slot[AtomNetWMWindowType] ||
               e->atom == atoms.slot[AtomNetWMState]) {
        bool was_fullscreen = c->fullscreen;
        swm_update_window_state(c);
        if (c->fullscreen != was_fullscreen)
            swm_set_fullscreen(c, c->fullscreen);
        swm_arrange(c->mon);
    }
}

static void
swm_populate_dispatch(void)
{
    memset(&event_dispatch, 0, sizeof(event_dispatch));
    event_dispatch.handlers[ButtonPress] = swm_dispatch_button_press;
    event_dispatch.handlers[ClientMessage] = swm_dispatch_client_message;
    event_dispatch.handlers[ConfigureRequest] = swm_dispatch_configure_request;
    event_dispatch.handlers[ConfigureNotify] = swm_dispatch_configure_notify;
    event_dispatch.handlers[DestroyNotify] = swm_dispatch_destroy_notify;
    event_dispatch.handlers[EnterNotify] = swm_dispatch_enter_notify;
    event_dispatch.handlers[Expose] = swm_dispatch_expose;
    event_dispatch.handlers[FocusIn] = swm_dispatch_focus_in;
    event_dispatch.handlers[KeyPress] = swm_dispatch_key_press;
    event_dispatch.handlers[MapRequest] = swm_dispatch_map_request;
    event_dispatch.handlers[MappingNotify] = swm_dispatch_mapping_notify;
    event_dispatch.handlers[MotionNotify] = swm_dispatch_motion_notify;
    event_dispatch.handlers[PropertyNotify] = swm_dispatch_property_notify;
}

static int
swm_startup_xerror(Display *display, XErrorEvent *event)
{
    char text[128];

    (void)display;
    startup_xerror_seen = true;
    text[0] = '\0';
    if (event)
        XGetErrorText(display, event->error_code, text, sizeof(text));
    die("swm: another window manager is already managing the root window%s%s",
        text[0] ? ": " : "", text[0] ? text : "");
    return 0;
}

static int
swm_runtime_xerror(Display *display, XErrorEvent *event)
{
    char text[128];

    if (event) {
        XGetErrorText(display, event->error_code, text, sizeof(text));
        fprintf(stderr, "swm: X error: %s\n", text);
    }
    return 0;
}

static void
swm_check_locale(void)
{
    if (!setlocale(LC_CTYPE, "")) {
        fprintf(stderr, "swm: warning: could not initialize the system locale\n");
        return;
    }
    if (!XSupportsLocale())
        fprintf(stderr, "swm: warning: X11 does not support the current locale\n");
}

static void
swm_check_args(int argc, char **argv)
{
    if (argc == 1)
        return;
    if (argc == 2 && argv[1] &&
        (strcmp(argv[1], "-v") == 0 || strcmp(argv[1], "--version") == 0)) {
        printf("%s\n", SWM_VERSION);
        exit(0);
    }
    die("usage: %s [-v|--version]", argv[0] ? argv[0] : "swm");
}

static void
swm_claim_root(void)
{
    startup_xerror_seen = false;
    previous_xerror = XSetErrorHandler(swm_startup_xerror);
    XSelectInput(dpy, root, SubstructureRedirectMask);
    XSync(dpy, False);
    if (startup_xerror_seen)
        die("swm: failed to acquire root-window management ownership");

    XSetErrorHandler(swm_runtime_xerror);
    XSync(dpy, False);
}

static void
swm_init_atoms(void)
{
    static const char *names[AtomCount] = {
        [AtomWMProtocols] = "WM_PROTOCOLS",
        [AtomWMDeleteWindow] = "WM_DELETE_WINDOW",
        [AtomWMWindowState] = "WM_STATE",
        [AtomWMTakeFocus] = "WM_TAKE_FOCUS",
        [AtomUTF8String] = "UTF8_STRING",
        [AtomNetSupported] = "_NET_SUPPORTED",
        [AtomNetWMName] = "_NET_WM_NAME",
        [AtomNetWMState] = "_NET_WM_STATE",
        [AtomNetSupportingWMCheck] = "_NET_SUPPORTING_WM_CHECK",
        [AtomNetWMStateFullscreen] = "_NET_WM_STATE_FULLSCREEN",
        [AtomNetActiveWindow] = "_NET_ACTIVE_WINDOW",
        [AtomNetWMWindowType] = "_NET_WM_WINDOW_TYPE",
        [AtomNetWMWindowTypeDialog] = "_NET_WM_WINDOW_TYPE_DIALOG",
        [AtomNetClientList] = "_NET_CLIENT_LIST"
    };
    unsigned int i;

    for (i = 0; i < AtomCount; i++) {
        atoms.slot[i] = XInternAtom(dpy, names[i], False);
        if (atoms.slot[i] == None)
            die("swm: failed to intern required X atom");
    }
}

static void
swm_init_drawing(void)
{
    static const char *fonts[SWM_FONT_COUNT > 0 ? SWM_FONT_COUNT : 1] = {
        SWM_FONT_PRIMARY
    };
    Fnt *primary;

    drw = drw_create(dpy, screen, root, sw, sh);
    if (!drw)
        die("swm: failed to create drawing context");

    if (SWM_FONT_COUNT == 0)
        die("swm: no configured fonts");
    if (!drw_fontset_create(drw, fonts, SWM_FONT_COUNT))
        die("swm: no configured font could be loaded");

    primary = drw->fonts;
    if (!primary)
        die("swm: drawing context has no active font");

    lrpad = primary->h;
    textpad = lrpad;
    bh = primary->h + SWM_BAR_PADDING;
}

static void
swm_init_cursors(void)
{
    cursor_normal = drw_cur_create(drw, XC_left_ptr);
    cursor_resize = drw_cur_create(drw, XC_sizing);
    cursor_move = drw_cur_create(drw, XC_fleur);
    if (!cursor_normal || !cursor_resize || !cursor_move)
        die("swm: failed to create cursor resources");
}

static void
swm_init_schemes(void)
{
    static const char *normal[3] = {
        SWM_COLOR_FG, SWM_COLOR_BG, SWM_COLOR_BORDER
    };
    static const char *selected[3] = {
        SWM_COLOR_FG_SELECTED, SWM_COLOR_BG_SELECTED,
        SWM_COLOR_BORDER_SELECTED
    };

    scheme_normal = drw_scm_create(drw, normal, 3);
    scheme_selected = drw_scm_create(drw, selected, 3);
    if (!scheme_normal || !scheme_selected)
        die("swm: failed to create color schemes");
}

static void
swm_init_supporting_window(void)
{
    Atom check = atoms.slot[AtomNetSupportingWMCheck];
    Atom utf8 = atoms.slot[AtomUTF8String];
    Atom name = atoms.slot[AtomNetWMName];
    unsigned long value;

    wmcheckwin = XCreateSimpleWindow(dpy, root, 0, 0, 1, 1, 0,
                                     BlackPixel(dpy, screen),
                                     BlackPixel(dpy, screen));
    if (wmcheckwin == None)
        die("swm: failed to create EWMH supporting window");

    value = wmcheckwin;
    XChangeProperty(dpy, wmcheckwin, check, XA_WINDOW, 32,
                    PropModeReplace, (unsigned char *)&value, 1);
    XChangeProperty(dpy, root, check, XA_WINDOW, 32,
                    PropModeReplace, (unsigned char *)&value, 1);
    XChangeProperty(dpy, wmcheckwin, name, utf8, 8, PropModeReplace,
                    (unsigned char *)SWM_VERSION, (int)strlen(SWM_VERSION));
}

static void
swm_init_ewmh_root_properties(void)
{
    Atom supported[] = {
        atoms.slot[AtomNetSupported],
        atoms.slot[AtomNetWMName],
        atoms.slot[AtomNetWMState],
        atoms.slot[AtomNetSupportingWMCheck],
        atoms.slot[AtomNetWMStateFullscreen],
        atoms.slot[AtomNetActiveWindow],
        atoms.slot[AtomNetWMWindowType],
        atoms.slot[AtomNetWMWindowTypeDialog],
        atoms.slot[AtomNetClientList]
    };

    XChangeProperty(dpy, root, atoms.slot[AtomNetSupported], XA_ATOM, 32,
                    PropModeReplace, (unsigned char *)supported,
                    (int)(sizeof(supported) / sizeof(supported[0])));
    XDeleteProperty(dpy, root, atoms.slot[AtomNetClientList]);
}

static void
swm_init_root_input(void)
{
    long mask = SubstructureRedirectMask | SubstructureNotifyMask |
                ButtonPressMask | PointerMotionMask | EnterWindowMask |
                LeaveWindowMask | StructureNotifyMask | PropertyChangeMask;

    XSelectInput(dpy, root, mask);
    XDefineCursor(dpy, root, cursor_normal->cursor);
}

static unsigned int
swm_get_numlock_mask(void)
{
    XModifierKeymap *map;
    KeyCode code;
    unsigned int mask = 0;
    int mod, k;

    code = XKeysymToKeycode(dpy, XK_Num_Lock);
    if (!code)
        return 0;

    map = XGetModifierMapping(dpy);
    if (!map)
        return 0;

    for (mod = 0; mod < 8; mod++) {
        for (k = 0; k < map->max_keypermod; k++) {
            if (map->modifiermap[mod * map->max_keypermod + k] == code) {
                mask = 1u << mod;
                break;
            }
        }
        if (mask)
            break;
    }
    XFreeModifiermap(map);
    return mask;
}

/* X11 modifier masks occupy eight modifier bits.  LockMask and the discovered
 * Num Lock modifier are treated as irrelevant for binding identity. */
static unsigned int
swm_modifier_variant_count(unsigned int mod, unsigned int variants[4])
{
    unsigned int base, numlock, count = 0, i;

    if (!variants || mod > 0xffu)
        return 0;
    numlock = numlockmask & 0xffu;
    base = mod & 0xffu;
    base &= ~(unsigned int)LockMask;
    if (numlock)
        base &= ~numlock;
    variants[count++] = base;
    variants[count++] = base | (unsigned int)LockMask;
    if (numlock) {
        variants[count++] = base | numlock;
        variants[count++] = base | numlock | (unsigned int)LockMask;
    }
    for (i = 0; i < count; i++) {
        unsigned int j;
        for (j = 0; j < i; j++) {
            if (variants[i] == variants[j]) {
                unsigned int k;
                for (k = i; k + 1u < count; k++)
                    variants[k] = variants[k + 1u];
                count--;
                i--;
                break;
            }
        }
    }
    return count;
}

static void
swm_grab_keys(void)
{
    size_t i;
    if (!dpy || root == None)
        return;
    for (i = 0; i < swm_key_count; i++) {
        const Key *key = &swm_keys[i];
        KeyCode code;
        unsigned int variants[4], count, j;
        if (key->keysym == NoSymbol || !(code = XKeysymToKeycode(dpy, key->keysym)))
            continue;
        count = swm_modifier_variant_count(key->mod, variants);
        for (j = 0; j < count; j++)
            XGrabKey(dpy, code, variants[j], root, False,
                     GrabModeAsync, GrabModeAsync);
    }
}

static void
swm_regrab_keys(void)
{
    if (!dpy || root == None)
        return;
    XUngrabKey(dpy, AnyKey, AnyModifier, root);
    swm_grab_keys();
}

static void
swm_grab_buttons(Window win, enum ClickRegion region)
{
    size_t i;
    if (!dpy || win == None)
        return;
    XUngrabButton(dpy, AnyButton, AnyModifier, win);
    for (i = 0; i < swm_button_count; i++) {
        const Button *b = &swm_buttons[i];
        unsigned int variants[4], count, j;
        if (b->region != region || b->button == 0u || b->button > 255u)
            continue;
        count = swm_modifier_variant_count(b->mod, variants);
        for (j = 0; j < count; j++)
            XGrabButton(dpy, b->button, variants[j], win, False,
                        ButtonPressMask, GrabModeAsync, GrabModeAsync,
                        None, None);
    }
}

static void
swm_grab_client_buttons(Client *c, bool focused)
{
    size_t i;
    if (!c || c->win == None || !dpy)
        return;
    XUngrabButton(dpy, AnyButton, AnyModifier, c->win);
    for (i = 0; i < swm_button_count; i++) {
        const Button *b = &swm_buttons[i];
        unsigned int variants[4], count, j;
        if (b->region != ClickClientWindow || b->button == 0u || b->button > 255u)
            continue;
        if (!focused) {
            XGrabButton(dpy, b->button, AnyModifier, c->win, False,
                        ButtonPressMask, GrabModeAsync, GrabModeAsync,
                        None, None);
            continue;
        }
        count = swm_modifier_variant_count(b->mod, variants);
        for (j = 0; j < count; j++)
            XGrabButton(dpy, b->button, variants[j], c->win, False,
                        ButtonPressMask, GrabModeAsync, GrabModeAsync,
                        None, None);
    }
}

static void
swm_grab_root_buttons(void)
{
    swm_grab_buttons(root, ClickRootBackground);
}

static void
swm_grab_bar_buttons(Monitor *m)
{
    if (!m || m->barwin == None)
        return;
    swm_grab_buttons(m->barwin, ClickTagIndicator);
    swm_grab_buttons(m->barwin, ClickNewTag);
    swm_grab_buttons(m->barwin, ClickLayoutSymbol);
    swm_grab_buttons(m->barwin, ClickStatusText);
    swm_grab_buttons(m->barwin, ClickWindowTitle);
}

static void
swm_init_status(void)
{
    Atom actual_type;
    int format;
    unsigned long nitems, bytes_after;
    unsigned char *data = NULL;

    if (XGetWindowProperty(dpy, root, XA_WM_NAME, 0, SWM_STATUS_MAX - 1,
                           False, AnyPropertyType, &actual_type, &format,
                           &nitems, &bytes_after, &data) == Success &&
        data && format == 8 && nitems > 0) {
        data[nitems < SWM_STATUS_MAX ? nitems : SWM_STATUS_MAX - 1] = '\0';
        swm_set_status((const char *)data);
    } else {
        swm_set_status(NULL);
    }
    if (data)
        XFree(data);
}

static void
swm_startup(int argc, char **argv)
{
    swm_check_args(argc, argv);
    swm_check_locale();

    dpy = XOpenDisplay(NULL);
    if (!dpy)
        die("swm: cannot open X display");

    screen = DefaultScreen(dpy);
    root = RootWindow(dpy, screen);
    sw = (unsigned int)DisplayWidth(dpy, screen);
    sh = (unsigned int)DisplayHeight(dpy, screen);

    swm_claim_root();
    swm_init_drawing();
    if (swm_discover_monitors() && !selmon)
        die("swm: monitor discovery produced no active monitor");
    swm_init_atoms();
    swm_init_cursors();
    swm_init_schemes();
    swm_init_status();
    swm_init_supporting_window();
    swm_init_ewmh_root_properties();
    swm_init_root_input();
    swm_update_bars();
    XSync(dpy, False);
    numlockmask = swm_get_numlock_mask();
    swm_regrab_keys();
    swm_grab_root_buttons();
    swm_populate_dispatch();
    swm_scan_existing_windows();
    swm_focus(selmon, NULL);
    swm_arrange_all();
    XSync(dpy, False);
    running = true;
}


static void
swm_cleanup(void)
{
    Monitor *m;

    if (!dpy)
        return;
    while (mons) {
        m = mons;
        if (m->clients)
            swm_unmanage(m->clients, false);
        else {
            Monitor *next = m->next;
            if (m->barwin != None)
                XDestroyWindow(dpy, m->barwin);
            if (next == m) {
                free(m);
                mons = NULL;
            } else {
                Monitor *tail = m;
                while (tail->next != m) tail = tail->next;
                tail->next = next;
                mons = next;
                free(m);
            }
        }
    }
    XUngrabKey(dpy, AnyKey, AnyModifier, root);
    XSetInputFocus(dpy, root, RevertToPointerRoot, CurrentTime);
    XDeleteProperty(dpy, root, atoms.slot[AtomNetActiveWindow]);
    if (wmcheckwin != None) {
        XDestroyWindow(dpy, wmcheckwin);
        wmcheckwin = None;
    }
    if (cursor_normal) drw_cur_free(drw, cursor_normal);
    if (cursor_resize) drw_cur_free(drw, cursor_resize);
    if (cursor_move) drw_cur_free(drw, cursor_move);
    if (scheme_normal) drw_scm_free(drw, scheme_normal, 3);
    if (scheme_selected) drw_scm_free(drw, scheme_selected, 3);
    if (drw) drw_free(drw);
    XSync(dpy, False);
    XCloseDisplay(dpy);
    dpy = NULL;
}

static void
swm_run(void)
{
    XEvent ev;
    while (running) {
        XNextEvent(dpy, &ev);
        if (ev.type >= 0 && ev.type <= LASTEvent &&
            event_dispatch.handlers[ev.type])
            event_dispatch.handlers[ev.type](&ev);
    }
}

static void
swm_init_state(void)
{
    dpy = NULL;
    screen = 0;
    root = None;
    wmcheckwin = None;
    sw = 0;
    sh = 0;

    mons = NULL;
    selmon = NULL;

    drw = NULL;
    scheme_normal = NULL;
    scheme_selected = NULL;
    cursor_normal = NULL;
    cursor_resize = NULL;
    cursor_move = NULL;

    bh = 0;
    lrpad = 0;
    textpad = 0;

    /* Start with the full representable capacity as the default configuration.
     * Later configuration must establish any smaller count through the checked
     * setter; values above the 31-bit mask capacity are rejected. */
    (void)swm_set_tag_count(SWM_DEFAULT_TAG_COUNT);
    numlockmask = 0;
    swm_set_status(NULL);

    /* Keep the built-in action entry points directly linkable even while the
     * configuration tables are intentionally empty. */
    (void)swm_action_spawn;
    (void)swm_action_focus_stack;
    (void)swm_action_inc_nmaster;
    (void)swm_action_set_mfact;
    (void)swm_action_zoom;
    (void)swm_action_cycle_saved;
    (void)swm_action_toggle_floating;
    (void)swm_action_toggle_bar;
    (void)swm_action_view;
    (void)swm_action_toggle_view;
    (void)swm_action_tag;
    (void)swm_action_toggletag;
    (void)swm_action_focus_monitor;
    (void)swm_action_send_monitor;
    (void)swm_action_move_resize;
    (void)swm_action_kill;
    (void)swm_action_quit;
    (void)swm_action_add_tag;

    memset(&atoms, 0, sizeof(atoms));
    memset(&event_dispatch, 0, sizeof(event_dispatch));
    previous_xerror = NULL;
    startup_xerror_seen = false;
    last_entered = None;
    running = true;
}

int
main(int argc, char **argv)
{
    swm_init_state();
    swm_startup(argc, argv);
    swm_run();
    swm_cleanup();
    return 0;
}
