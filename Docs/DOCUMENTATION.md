# swm Technical Documentation

**Project:** swm, Static Window Manager  
**Scope:** Current source tree in `swm.zip`  
**Purpose:** Implementation reference, not a behavioral specification  
**Inspected:** 2026-09-16

> This file documents the implementation that is actually present in the inspected tree. `CLEANROOM.md` covers how the implementation was produced and reviewed; this file covers what the resulting program does.

---

## 1. Overview

`swm` is an X11 window manager written in C. Its core responsibility is to own the X root window, receive X events, maintain a model of clients and monitors, and keep the X server synchronized with that model.

The current implementation contains the usual window-management primitives:

- tiled and monocle layouts,
- floating and fullscreen clients,
- tags and views,
- keyboard and mouse actions,
- multiple monitors,
- a status bar,
- ICCCM/EWMH integration,
- Xft/fontconfig text rendering,
- optional Xinerama monitor discovery.

The implementation is intentionally more explicit than the compact style associated with traditional suckless software. It names several pieces of state that could otherwise be represented as ordinary fields or helper logic, including `Geometry`, `SavedGeometry`, `SizeHints`, `AtomState`, and `EventDispatch`.

That is also one of the project's recurring jokes: if a rectangle can become an abstraction, swm will absolutely give the rectangle a job title.

---

## 2. Source tree and build boundary

The normal executable is built from:

```text
swm.c
drw.c
util.c
```

Their responsibilities are:

| File | Responsibility |
|---|---|
| `swm.c` | Window-manager state, X11 events, clients, monitors, actions, layouts, protocols |
| `drw.c` / `drw.h` | Drawing, fonts, colors, text, cursors |
| `util.c` / `util.h` | Allocation and common utility/error functions |
| `config.def.h` | Compile-time configuration defaults and tables |
| `config.mk` | Compiler, linker, and dependency settings |
| `Makefile` | Build, install, uninstall, distribution |
| `swm.1` | Manual page |
| `transient.c` | Standalone X11 transient-window test program |
| `README.md` | Project introduction |
| `CLEANROOM.md` | Clean-room development/review history |

`wewm.c` is also present in the archive, but it is not part of the normal `SRC` list in the current Makefile. It should therefore be treated as a repository artifact/parallel implementation, not as code compiled into the current `swm` executable.

---

## 3. Startup and shutdown

The program is organized around four broad stages:

```text
swm_init_state()
      |
swm_startup()
      |
swm_run()
      |
swm_cleanup()
```

### Initialization

The initialization phase clears global state, establishes initial tag state, prepares protocol and dispatch state, initializes status text, and marks the main loop as running.

### X11 startup

The startup path establishes the pieces required for a working WM:

1. validate command-line arguments,
2. initialize locale support,
3. open the X display,
4. identify the screen and root window,
5. claim the root window,
6. initialize drawing,
7. discover monitors,
8. initialize X11 atoms,
9. create cursors and color schemes,
10. initialize status state,
11. create the EWMH supporting window,
12. publish root properties,
13. select root events,
14. create/update bars,
15. determine the Num Lock modifier,
16. install input grabs,
17. populate event handlers,
18. scan existing windows,
19. establish initial focus,
20. arrange monitors.

Only after that setup does the program enter the main event loop.

### Main loop

`swm_run()` obtains an `XEvent` with `XNextEvent()` and dispatches it through the `EventDispatch` table.

Conceptually:

```text
XEvent
  |
  v
event type
  |
  v
EventDispatch
  |
  v
handler
```

### Shutdown

Cleanup unmanages remaining clients, destroys bar windows, releases grabs, restores root focus, clears active-window state, destroys the EWMH supporting window, releases drawing resources and cursors, synchronizes with X, and closes the display.

---

## 4. Global state

The implementation keeps the main X11/session state in file-static globals.

Important groups include:

### X state

- `dpy`: X display connection
- `screen`: X screen
- `root`: root window
- `wmcheckwin`: EWMH supporting/check window
- screen dimensions

### Monitor state

- `mons`: monitor-list head
- `selmon`: selected monitor

### Drawing state

- drawing context
- normal and selected schemes
- cursors
- bar height
- text padding

### Runtime state

- current tag count,
- Num Lock mask,
- root status text,
- atom state,
- event dispatch table,
- running flag,
- last entered window.

This is intentionally centralized: most operations eventually need access to the same X connection, monitor set, selected monitor, or drawing context.

---

## 5. Core data model

### `Arg`

The generic action argument is a union containing integer, unsigned integer, floating-point, and pointer forms. Keyboard and mouse bindings can therefore invoke the same action functions with different argument types.

### `Key`

A key binding combines:

- modifier mask,
- X11 keysym,
- action function,
- generic argument.

### `Button`

A mouse binding combines:

- semantic click region,
- modifier mask,
- X11 button,
- action function,
- generic argument.

### `Layout`

A layout contains a display symbol and an arrangement function. A layout without an arrangement function represents a non-tiling/floating-style arrangement.

### `Rule`

A rule can match client properties such as class, instance, and title, and can specify tags, floating state, and monitor placement.

The current tree contains the rule machinery, but the runtime rule configuration is empty, so the existence of the structure should not be confused with a populated set of application-specific rules.

### `Client`

A managed X11 window is represented by a `Client`.

Its state includes:

- X window ID,
- monitor,
- title,
- current geometry,
- saved geometry,
- size hints,
- floating state,
- urgency/focus-related state,
- fullscreen state,
- previous floating state,
- fullscreen restoration state,
- tag mask,
- client-list linkage,
- focus-stack linkage.

Two linked-list relationships are important:

```text
next  -> monitor client ordering
snext -> focus/stack ordering
```

### `Monitor`

A monitor stores:

- monitor index,
- bar state,
- full geometry,
- work geometry,
- two layout slots,
- selected layout slot,
- monitor label,
- master factor,
- master count,
- visible-tag state,
- client list,
- focus stack,
- selected client,
- circular-list link.

### `Geometry`

`Geometry` is a first-class rectangle:

```c
struct Geometry {
    int x;
    int y;
    unsigned int w;
    unsigned int h;
    unsigned int bw;
};
```

It is reused for client placement, monitor/work areas, saved placement, and fullscreen restoration.

### `SavedGeometry`

This stores geometry plus validity state so fullscreen and similar transitions can be reversed safely.

### `SizeHints`

Normalized client size constraints are kept separately so geometry calculations can apply minimum/maximum sizes, increments, base sizes, and aspect constraints.

### `AtomState`

ICCCM/EWMH atoms are grouped into named slots rather than being scattered through the source.

### `EventDispatch`

The event table maps X11 event numbers to compatible handlers. This creates an explicit boundary between protocol events and state-changing operations.

---

## 6. Geometry and work areas

The monitor has a full rectangle and a usable work rectangle.

The bar occupies part of the monitor surface, so the work area is derived from the full monitor geometry after accounting for the bar.

Clients have their own geometry, including border width.

The implementation also contains geometry validation/clamping logic so interactive movement does not blindly place clients outside the relevant monitor area.

The general flow is:

```text
monitor geometry
      |
bar reservation
      |
work geometry
      |
layout calculation
      |
client geometry
      |
X11 configure
```

This is one of the clearest examples of swm's tendency to turn ordinary window-manager bookkeeping into named architecture.

---

## 7. Tags and views

Tags use a bitmask. A client's tag mask and a monitor's visible-tag mask are compared by intersection.

Conceptually:

```text
visible = (client_tags & monitor_tags) != 0
```

The current implementation has a practical 31-bit tag capacity while maintaining a separate current tag count.

The initial view is tag 1.

View operations replace or toggle the monitor's active tag mask, then refocus a suitable visible client and rearrange the monitor.

Client-tag operations replace or toggle a client's tag mask. Invalid zero masks are rejected where a valid tag set is required.

The implementation can increase the current tag count until its bit capacity is reached.

This means the program distinguishes between:

```text
configured/current tag count
```

and:

```text
maximum representable tag bits
```

rather than treating them as one number.

---

## 8. Layouts

The current implementation contains tiled and monocle arrangements.

### Tiled

The tiled arrangement separates visible, non-floating clients into a master area and a stack area.

The default state uses one master client and a master factor around 0.55.

Conceptually:

```text
+----------------------+------------------+
|                      |                  |
|       MASTER         |      STACK       |
|                      |                  |
|                      |                  |
+----------------------+------------------+
```

### Monocle

Monocle gives visible clients the usable monitor area.

### Arrangement boundary

Actions generally mutate state and then invoke arrangement instead of directly placing every client themselves:

```text
action
  -> state change
  -> arrange
  -> geometry application
  -> X11 update
```

This keeps placement policy in the layout subsystem.

---

## 9. Client lifecycle

The client lifecycle connects X11 events to the internal `Client` model.

### Discovery

At startup, existing windows are scanned. New windows enter through the X event system.

### Management

Management creates client state, reads relevant X properties, applies classification, assigns the client to a monitor and tag set, selects required X events, and inserts the client into the monitor's structures.

### Visibility and mapping

A client is mapped when it should be visible under the current monitor/view/layout state.

### Configuration

When geometry changes, the WM sends the appropriate X11 configuration to the client. Size hints and work-area boundaries are considered rather than assuming every client accepts arbitrary dimensions.

### Unmanagement

When a client is withdrawn or destroyed, its internal state and X11 relationships are removed.

The `UnmapNotify` path is particularly important because a window being temporarily unmapped is not always equivalent to a client disappearing permanently. Confusing those states can leave a stale client object behind.

---

## 10. Rules and classification

Rules can classify clients using:

- class,
- instance,
- title,
- tags,
- floating state,
- monitor.

The current source contains this machinery, but the active rule table is empty. Therefore this documentation treats rules as an implemented mechanism rather than claiming that the default configuration currently contains a large application-specific policy.

---

## 11. Focus and selection

Focus is more than a single `XSetInputFocus()` call.

The WM must keep several pieces synchronized:

```text
selected client
      |
      +--> internal focus state
      +--> X input focus
      +--> active-window property
      +--> visual/border state
```

The focus stack is maintained separately from the client list so that selection history/order can be manipulated without redefining monitor membership.

The implementation validates selected monitor/client relationships before applying focus transitions.

---

## 12. Keyboard and mouse actions

Actions use function pointers and `Arg` values.

Keyboard bindings are:

```text
modifier + keysym -> action + argument
```

Mouse bindings are:

```text
click region + modifier + button -> action + argument
```

The action layer covers operations such as:

- view/tag changes,
- floating toggles,
- focus changes,
- client movement,
- interactive resizing,
- monitor changes,
- promotion/zoom,
- fullscreen,
- layout changes,
- adding tags.

The shared action mechanism means the input layer describes *when* something happens while the action function describes *what* happens.

---

## 13. Bar interaction

The bar is both a display and an input surface.

Current click regions distinguish semantic areas such as:

- tag indicators,
- new-tag control,
- layout symbol,
- status text.

A click is therefore interpreted in two stages:

```text
X coordinate
    |
    v
bar hit testing
    |
    v
semantic click region
    |
    v
configured action
```

Tag clicks can select the corresponding tag rather than treating the entire tag strip as one undifferentiated button.

The project apparently spent enough engineering effort on this that the bar now has a taxonomy. Suckless would probably have called it "x coordinate."

---

## 14. Event handling

Major X11 events are routed through explicit handlers for lifecycle, focus, input, configuration, mapping, properties, and monitor/root changes.

The architecture is:

```text
X11 event
  |
  v
EventDispatch
  |
  v
event-specific handler
  |
  +--> inspect event
  +--> mutate WM state
  +--> update X11
  +--> arrange/focus/redraw
```

This makes the event-to-state path relatively easy to trace and gives the source a single place where the set of supported event handlers is visible.

---

## 15. ICCCM and EWMH

The implementation participates in standard X11 WM protocols.

### ICCCM-related behavior

The program handles standard client protocol/property mechanisms needed for lifecycle and graceful interaction with clients.

### EWMH-related behavior

The source maintains selected EWMH properties for concepts including:

- supported atoms,
- active window,
- client list,
- WM state,
- fullscreen,
- window type,
- dialog type,
- supporting-window identification.

The purpose is interoperability with clients and desktop tools that communicate with the WM through standardized X properties and client messages.

---

## 16. Fullscreen

Fullscreen is a reversible state transition.

Conceptually:

```text
normal geometry
      |
save geometry/state
      |
fullscreen geometry/state
      |
restore saved geometry/state
```

`SavedGeometry` exists specifically to make the transition reversible.

Fullscreen also needs protocol synchronization. The visible state and the corresponding EWMH fullscreen state must agree.

---

## 17. Size hints and interactive geometry

X11 clients can impose constraints such as:

- minimum size,
- maximum size,
- base size,
- resize increments,
- aspect ratio.

`SizeHints` normalizes those values for use by the WM.

Interactive movement/resizing therefore has to reconcile:

```text
pointer movement
+ client constraints
+ monitor boundaries
+ WM policy
```

rather than simply adding a delta to `x` and `y`.

---

## 18. Multi-monitor behavior

The implementation maintains monitors in a circular list and supports optional Xinerama-based discovery.

Each monitor independently owns:

- full/work geometry,
- bar,
- tag view,
- layouts,
- master settings,
- clients,
- focus state.

Moving a client between monitors therefore involves more than changing its monitor pointer. Visibility, focus, geometry, and arrangement can all be affected.

The circular monitor list makes traversal naturally wrap from the last monitor to the first.

---

## 19. Rendering subsystem

`drw.c`/`drw.h` isolate drawing from window-management policy.

The subsystem handles:

- drawing contexts,
- fonts,
- font sets/fallback,
- colors,
- text measurement,
- rectangles,
- cursors,
- text rendering.

The WM can therefore ask for a piece of text to be drawn without owning every low-level Xft/fontconfig detail.

This separation also mattered during the clean-room process. The rejected first `drw.c` implementation accidentally reproduced reference-specific internal cache details; the final subsystem was rebuilt with a different internal mechanism. That history belongs in `CLEANROOM.md`, not in the runtime architecture itself.

---

## 20. Configuration

Configuration is primarily compile-time.

The configuration surface includes defaults for:

- version,
- fonts,
- colors,
- client border width,
- default floating state,
- bar placement/visibility,
- keyboard/mouse bindings,
- layouts,
- rules.

This retains the general source-configured philosophy associated with dwm while deliberately making the implementation around it more elaborate.

The joke is straightforward:

> dwm says "edit C and rebuild."  
> swm says "edit C and rebuild, but first let us define the architectural implications of your semicolon."

---

## 21. Utilities and test client

`util.c` contains generic allocation/error helpers.

`transient.c` is a standalone X11 program for exercising transient-window behavior. It is not part of the main WM executable.

These files are deliberately outside the core client/monitor/event machinery.

---

## 22. Invariants and defensive checks

The implementation explicitly validates several state relationships.

Examples include:

- tag masks must remain valid,
- empty tag masks are rejected where invalid,
- selected monitors must be valid,
- selected clients must belong to the expected monitor,
- geometry must remain representable,
- X11 resources must be released by their owner,
- event handlers must correspond to the expected event types,
- fullscreen restoration must have valid saved state.

This is a real architectural difference from the deliberately terse style being parodied.

There is nothing inherently magical about validation, but swm makes a point of having more of it visible in the source.

---

## 23. Current implementation boundaries

Three distinctions are especially important when extending the code.

### Source presence is not runtime use

A type or subsystem can exist without being exercised by the current configuration. Rules are the clearest example.

### Repository artifacts are not executable components

`wewm.c` exists in the archive but is not compiled by the current Makefile.

### Protocol support is not universal desktop support

Implementing selected ICCCM/EWMH properties does not mean every desktop-specific extension or optional EWMH feature is implemented.

---

## 24. Maintenance map

```text
swm.c
 ├── startup/shutdown
 ├── global state
 ├── event dispatch
 ├── client lifecycle
 ├── monitors
 ├── tags/views
 ├── layouts
 ├── focus
 ├── actions
 ├── geometry
 ├── ICCCM/EWMH
 └── bar policy

drw.c / drw.h
 ├── fonts
 ├── font fallback
 ├── colors
 ├── text
 ├── rectangles
 └── cursors

util.c / util.h
 └── common helpers

config.def.h
 └── compile-time policy

transient.c
 └── standalone X11 test client
```

---

## 25. Closing note

swm is intentionally caught between a conventional X11 implementation and a parody of minimalist engineering culture.

Technically, it is still a window manager with clients, monitors, tags, layouts, focus, protocols, and geometry.

Architecturally, it keeps naming things that could have remained unnamed.

So there is `Geometry`.

Then `SavedGeometry`.

Then geometry validation.

Then geometry restoration.

At some point the joke stops being "this is over-engineered" and becomes "this is now an architecture."

That is the point.
