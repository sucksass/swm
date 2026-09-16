# swm vs dwm

**Scope:** Current `swm` source tree compared with upstream dwm's documented design and current public project information  
**Inspected:** 2026-09-16

> This comparison is technical rather than a universal winner/loser verdict. The projects make different design tradeoffs, and the useful question is what those tradeoffs actually change.

---

## 1. At a glance

Both projects are X11 window managers built around the same fundamental problem: manage client windows, monitors, focus, layouts, tags/views, and X11 protocol state.

| Area | dwm | swm |
|---|---|---|
| X11 window manager | Yes | Yes |
| Tiled layout | Yes | Yes |
| Monocle layout | Yes | Yes |
| Floating clients | Yes | Yes |
| Tags/views | Yes | Yes |
| Multi-monitor | Yes | Yes |
| Keyboard/mouse actions | Yes | Yes |
| ICCCM/EWMH | Yes | Yes |
| Xft drawing | Yes | Yes |
| Xinerama | Yes | Optional/current build support |
| Source-level configuration | Yes | Yes |
| Explicit `Geometry` type | Mostly direct fields | Yes |
| Explicit saved-geometry type | Embedded state | Yes |
| Explicit event-dispatch abstraction | Compact handler table | Yes |
| Explicit atom grouping | Less centralized | `AtomState` |
| Explicit size-hint abstraction | More directly attached to clients | `SizeHints` |
| Design emphasis | Small/simple | Explicit/defensive/over-engineered |

The upstream dwm site explicitly describes dwm as a small, fast, simple X11 window manager with tiled, monocle, floating, tags, a status bar, and multi-monitor support. citeturn0search0turn0search1

---

## 2. The common foundation

A clean way to see the relationship is to ignore names and look at responsibilities.

Both need roughly:

```text
X server
   |
   v
events
   |
   v
client/monitor state
   |
   +--> tags/views
   +--> focus
   +--> layout
   +--> geometry
   +--> protocols
   |
   v
X server updated
```

That common structure is not surprising. An X11 WM cannot escape X11's event model, client windows, monitor geometry, focus semantics, or protocol conventions.

The interesting difference is what happens between the event and the state transition.

---

## 3. dwm's design philosophy

dwm's official documentation emphasizes smallness, speed, simplicity, and source-level configuration. It also describes its compact single-binary model and its lack of additional configuration languages or runtime machinery. citeturn0search0

Its philosophy can be summarized as:

```text
complex problem
      |
      v
keep the implementation small
      |
      v
avoid machinery that does not earn its place
```

This does **not** mean dwm has no complexity. X11 window management is full of protocol and state edge cases.

It means the project tries to keep that complexity represented with as little surrounding machinery as practical.

---

## 4. swm's answer to that philosophy

swm solves much of the same problem while taking the opposite architectural joke:

```text
complex problem
      |
      v
name the concept
      |
      v
give it a structure
      |
      v
give the structure validation
      |
      v
document the validation
```

Hence:

- `Geometry`
- `SavedGeometry`
- `SizeHints`
- `AtomState`
- `EventDispatch`

The behavior can remain similar while the source becomes more explicit.

This is the central contrast throughout the project.

---

## 5. Client representation

### dwm

dwm's client structure directly contains the information required for management: window identity, monitor, geometry, tags, floating/fullscreen state, size hints, and list relationships.

Its surrounding functions operate directly on that model.

### swm

swm has the same fundamental client model but separates more concepts into explicit structures and helpers.

The result is roughly:

```text
Client
 ├── Monitor
 ├── Geometry
 ├── SavedGeometry
 ├── SizeHints
 ├── tags
 ├── focus relationship
 └── protocol/window state
```

### Practical effect

The two implementations still answer the same questions:

- Which monitor owns this window?
- Is it visible?
- What tags does it have?
- What rectangle should it occupy?
- Can it be resized to that rectangle?
- Should it receive focus?

swm simply makes more of the answers into named objects.

### The jab

dwm has fields that mean things.

swm has fields that mean things, plus a structure explaining that the fields mean things.

Minimalism: "it's a rectangle."

swm: "it is a `Geometry`, and we have considered the rectangle's lifecycle."

---

## 6. Geometry

This is one of the clearest differences.

### dwm

Geometry is represented close to the client/monitor data and manipulated by compact placement functions.

### swm

swm defines:

```c
struct Geometry {
    int x;
    int y;
    unsigned int w;
    unsigned int h;
    unsigned int bw;
};
```

It reuses that concept for clients, monitors, saved state, and fullscreen restoration.

The practical algorithm remains:

```text
calculate rectangle
    |
apply rectangle
    |
configure X window
```

The difference is whether the rectangle gets to become a named subsystem.

swm votes yes.

---

## 7. Tags and views

Both projects use bitmasks for tags and views.

The visibility relationship is essentially:

```text
client_tags & active_view != 0
```

dwm's tutorial documents nine default tags and direct tag selection/toggling through keyboard and mouse interaction. citeturn0search1

swm preserves the bitmask model but adds explicit validation and distinguishes current tag count from maximum representable tag bits.

That means swm has more state around a concept that, at its core, is still:

```text
bit 0 = tag 1
bit 1 = tag 2
...
```

The bitmask did not ask for an architecture review. It got one anyway.

---

## 8. Layouts

The high-level layout model is very close.

dwm officially documents tiled, monocle, and floating behavior, with tiled windows split into master and stacking areas. citeturn0search0

swm implements the same family.

### Tiled

Both use a master area and a stack area controlled by a master factor and master count.

### Monocle

Both use the available work area for visible clients.

### Floating

Both allow individual clients to escape automatic tiling.

The key point is that identical high-level behavior does not require identical source structure. These are constrained algorithms with obvious behavioral requirements.

---

## 9. Focus and client ordering

Both need at least two ideas:

```text
which clients belong to the monitor?
which client should be focused next?
```

dwm keeps client and stack relationships in its compact client/monitor model.

swm makes the distinction especially visible through separate client and focus-stack links.

The resulting behavior can still be summarized simply:

```text
choose
  -> select
  -> focus
  -> update X11
```

swm just puts more names between those verbs.

---

## 10. Event architecture

### dwm

The classic dwm structure uses a compact event-handler table to associate X11 event types with functions.

The implementation remains close to the X11 event vocabulary.

### swm

swm formalizes the same idea through `EventDispatch`.

Conceptually:

```text
XEvent
  |
  v
EventDispatch
  |
  v
handler
  |
  v
state transition
```

This is not a radically different event-driven model. It is a more explicit wrapper around the same idea.

### The joke

dwm:

```text
event -> handler
```

swm:

```text
event -> dispatch layer -> handler contract -> validation -> state transition
```

The X server sends exactly the same event either way.

---

## 11. Monitor architecture

dwm supports multiple monitors and Xinerama, with a view associated with each Xinerama screen. citeturn0search0

swm likewise represents each monitor as an explicit `Monitor` object containing:

- full geometry,
- work geometry,
- bar,
- tag view,
- layouts,
- master settings,
- clients,
- focus state.

Both therefore follow the same broad model:

```text
discover monitor
    |
create state
    |
assign/manage clients
    |
arrange independently
```

swm's monitor state is simply more heavily named and validated.

---

## 12. Rules

dwm uses source-level rules to classify windows by properties such as class, instance, and title and to determine tags, floating behavior, or monitor placement.

swm has a comparable rule structure.

However, the current swm tree has an empty active rule configuration. Therefore the correct comparison is:

> swm contains rule infrastructure comparable in purpose to dwm's rule mechanism, but the current configuration does not populate it with a large set of application-specific rules.

That distinction matters. A structure existing in C is not the same thing as a feature being actively configured.

---

## 13. Fullscreen

Both implementations have to solve the same reversible state problem:

```text
normal
  |
save previous state
  |
fullscreen
  |
restore previous state
```

swm makes the saved rectangle explicit with `SavedGeometry`.

dwm keeps the corresponding information inside its client state and fullscreen logic.

The X11/EWMH side also has to stay synchronized, because fullscreen is not merely a different rectangle. It is a protocol-visible state.

---

## 14. Size hints

Both WMs must account for client-provided X11 size constraints.

These can include:

- minimum dimensions,
- maximum dimensions,
- base dimensions,
- resize increments,
- aspect ratio.

dwm keeps these values close to its client representation.

swm wraps the normalized values in `SizeHints`.

The algorithmic requirement is the same:

```text
requested geometry
      +
client constraints
      +
WM boundaries
      |
      v
valid geometry
```

The difference is how loudly the source announces that this calculation exists.

---

## 15. Status bar

dwm's official documentation describes the bar as a compact surface for tags, layout information, visible-window information, focused title, and root-window status text. citeturn0search0turn0search1

swm has the same basic concept but also treats the bar as an explicit interaction surface.

Its click regions distinguish semantic targets such as tags, the add-tag control, layout symbol, and status area.

So the bar participates in both:

```text
rendering
```

and:

```text
input routing
```

That is a small architectural expansion over the basic status-bar concept.

---

## 16. Rendering

Both projects separate Xft/font rendering from the main WM logic.

swm's `drw.c`/`drw.h` provides a dedicated drawing layer for:

- fonts,
- fallback,
- colors,
- text measurement,
- rectangles,
- cursors,
- rendering.

This is structurally similar to the purpose of dwm's drawing subsystem.

The interesting difference is provenance rather than runtime behavior: the clean-room record documents a rejected swm renderer whose internal cache mechanism reproduced reference-specific constants and naming. The final implementation was rebuilt with a different mechanism.

That is evidence about the development process, not proof that the resulting drawing API is unrelated to every internal idea in dwm.

---

## 17. Configuration

dwm explicitly embraces source-level configuration. The official documentation describes customizing the WM by editing its C configuration and rebuilding. citeturn0search0

swm does the same.

So the comparison is not:

```text
dwm source configuration
vs
swm runtime configuration
```

It is:

```text
dwm source configuration
vs
swm source configuration with substantially more architectural commentary
```

The joke is therefore aimed at the philosophy, not a fake technical difference.

dwm:

> edit C and rebuild.

swm:

> edit C and rebuild, then consider whether your change has sufficiently defensible architectural semantics.

---

## 18. Patches and feature growth

The official dwm ecosystem distributes many optional patches, including tab bars, systray support, scratchpads, and multi-monitor enhancements. citeturn0search3turn0search5turn0search8turn0search9

That creates an interesting maintenance philosophy.

### dwm ecosystem

```text
small upstream core
       |
       +--> optional patch
       |
       +--> another patch
       |
       +--> user's customized tree
```

### swm

The current project tends to favor putting more behavior into explicit internal subsystems:

```text
behavior
   |
implementation
   |
abstraction
   |
validation
   |
documentation
```

Neither approach is inherently universal. They optimize for different things.

The parody version is:

> dwm keeps the core small by applying patches.  
> swm keeps the core "clean" by inventing a subsystem.

---

## 19. Source organization

The architectural difference becomes clearest when reduced to nouns.

### dwm

```text
Client
Monitor
Layout
functions
configuration
```

### swm

```text
Client
Monitor
Geometry
SavedGeometry
SizeHints
AtomState
EventDispatch
validation helpers
actions
layouts
drawing
```

The second list is not automatically "better." It is simply more explicit.

That means swm can be easier to discuss in terms of named responsibilities, while also carrying more conceptual overhead.

---

## 20. What the dwm philosophy gets right, technically

dwm's compactness has a real engineering benefit: there is less machinery between a requirement and the code implementing it.

The official project explicitly emphasizes small source and a single binary. citeturn0search0

For someone maintaining a small X11 WM, that can mean:

- fewer layers to understand,
- fewer abstractions to keep synchronized,
- fewer places for state to become inconsistent,
- easier whole-program inspection.

The joke should not obscure that.

Making fun of minimalism works better when the minimalism is actually competent.

---

## 21. What swm deliberately changes

swm makes more of the state relationships visible.

Examples:

- geometry becomes a named type,
- saved geometry becomes explicit,
- size hints become a dedicated concept,
- atoms are grouped,
- event dispatch is explicit,
- validity checks are more visible,
- client and focus relationships are separated,
- actions are organized behind reusable handlers.

This gives the implementation a stronger "architecture diagram in C" feeling.

It also creates more code and more relationships that can themselves fail.

More structure is not automatically more correctness.

---

## 22. Performance and memory

Source size or abstraction count alone cannot establish that either WM is faster, slower, smaller in RAM, or more efficient at runtime.

dwm's compactness strongly suggests a design priority, but performance claims require measurements.

Likewise, swm's additional validation and abstraction layers may add work on some paths, but without controlled benchmarks it would be irresponsible to turn that into a quantitative claim.

So this comparison deliberately does not invent performance numbers.

---

## 23. Compatibility

Both target the same broad X11 ecosystem:

- client windows,
- tags/views,
- layouts,
- focus,
- floating,
- fullscreen,
- monitors,
- bars,
- ICCCM,
- EWMH.

That makes dwm a useful behavioral reference point.

But compatibility must be tested at the behavior level.

For example, two WMs can both support fullscreen while differing in:

- restoration geometry,
- property update order,
- focus changes,
- monitor transitions,
- client-message handling.

Shared feature names are not proof of identical semantics.

---

## 24. Suckless philosophy, translated into the swm joke

The upstream project explicitly describes dwm as small, fast, simple, source-configured software. citeturn0search0

swm deliberately exaggerates the opposite instinct.

Examples:

- dwm can keep coordinates as fields; swm gives them `Geometry`.
- dwm can dispatch an event; swm gives the dispatch mechanism a structure.
- dwm can keep protocol atoms close to the code; swm groups them into `AtomState`.
- dwm can store size hints with the client; swm promotes them into `SizeHints`.
- dwm can patch a feature; swm may build a subsystem and then write documentation explaining the subsystem.

The important thing is that the jabs are based on real design differences, not invented claims about how dwm works.

---

## 25. The philosophical split

The difference can be summarized as:

### dwm

**Minimize the machinery surrounding the solution.**

### swm

**Make the machinery explicit, name it, validate it, and then make fun of yourself for naming it.**

That produces two different styles around the same X11 fundamentals.

dwm asks:

> Does this abstraction need to exist?

swm asks:

> Does this abstraction need to exist?

Then:

> What should we call it?

Then:

> What invariants should it have?

Then:

> Should the invariant itself be an abstraction?

Then, somewhere in the corner, `Geometry` is still waiting for its next promotion.

---

## 26. Bottom line

The projects are closer at the behavioral level than they are at the architectural level.

At the behavioral level, both revolve around:

```text
X11 events
  -> clients/monitors
  -> tags/views
  -> focus
  -> layouts
  -> geometry
  -> X11 state
```

At the architectural level:

```text
dwm:
compact representation
       |
       v
direct manipulation
       |
       v
small core
```

versus:

```text
swm:
explicit representation
       |
       v
helper/subsystem
       |
       v
validation
       |
       v
state transition
       |
       v
documentation
```

That is the most useful way to compare them.

dwm is deliberately minimal.

swm is deliberately a tiny bit less minimal while pretending this is somehow a research program.

And yes, both still have to deal with the same X11 nonsense at the end of the day.
