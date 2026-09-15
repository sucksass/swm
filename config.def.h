/*
 * config.def.h - default configuration template for swm
 *
 * Configuration boundary note
 * ---------------------------
 *
 * The current reconstructed wewm.c does not include config.h/config.def.h and
 * does not consume configuration tables for tags, layouts, rules, keys, or
 * buttons.  This file therefore contains only configuration values that have
 * an actual consumer in the current source, plus a clearly marked description
 * of the configuration surfaces that are not yet wired into the source.
 *
 * Do not treat the unwired surfaces below as active defaults.  In particular,
 * this file must not be copied into wewm.c or used to imply behavior that the
 * current implementation does not provide.
 *
 * The source filename remains wewm.c; internal WEWM_* names remain unchanged
 * deliberately.  The public program/version identity is swm.
 */

/* ------------------------------ identity -------------------------------- */

/* Consumed by wewm.c for -v/--version and the EWMH supporting-window name. */
#define WEWM_VERSION "swm 1.0"

/* ------------------------------ appearance ------------------------------ */

/* The current drawing implementation supports exactly one configured font. */
#define WEWM_FONT_COUNT           1u
#define WEWM_FONT_PRIMARY         "monospace:size=10"

#define WEWM_COLOR_FG             "#ffffff"
#define WEWM_COLOR_BG             "#000000"
#define WEWM_COLOR_BORDER         "#444444"

#define WEWM_COLOR_FG_SELECTED    "#000000"
#define WEWM_COLOR_BG_SELECTED    "#ffffff"
#define WEWM_COLOR_BORDER_SELECTED "#ffffff"

/*
 * Bar padding is currently hard-coded by wewm.c as WEWM_BAR_PADDING.  It is
 * therefore documented here as an implementation value, but is not redefined
 * by this template because the source does not make it safely overridable.
 */
/* Consumed by the client-geometry/default-window behavior. */
#define WEWM_CLIENT_BORDER_WIDTH  1u
#define WEWM_DEFAULT_FLOATING     0
#define WEWM_BAR_VISIBLE          1
#define WEWM_BAR_TOP              1

/* ------------------------------- tags ----------------------------------- */

/*
 * The implementation has a fixed 31-bit tag capacity and initializes the
 * active tag set to tag 1.  It does not currently consume a configurable tag
 * label array or a configurable tag count from this file.
 *
 * Consequently no tag table is declared here: doing so would be decorative
 * rather than functional.  The active implementation uses numeric labels
 * 1..tag_count and initializes tag_count to the full 31-bit capacity.
 */

/* ------------------------------ layouts --------------------------------- */

/*
 * The two layout objects are currently defined and selected directly by
 * wewm.c:
 *
 *     wewm_layout_tiled
 *     wewm_layout_monocle
 *
 * They are initialized into the two layout slots by the source.  There is no
 * current configuration consumer for a layout-selection table, so none is
 * duplicated here.
 */

/* ------------------------------- rules ---------------------------------- */

/*
 * The Rule structure is defined by wewm.c, but the current implementation's
 * rule table is an internal zero-count table.  No configuration include or
 * rule-table handoff exists yet, so no application-specific or synthetic
 * rule table is provided here.
 */

/* ------------------------- keyboard bindings ---------------------------- */

/*
 * Key is a real configuration structure in wewm.c, but the active source
 * currently owns an empty wewm_keys table and sets wewm_key_count to zero.
 * There is therefore no usable configuration table in this file yet.
 *
 * The implemented action entry points are nevertheless real and available
 * to a future explicit configuration boundary; no wrappers are introduced:
 *
 *   wewm_action_spawn
 *   wewm_action_focus_stack
 *   wewm_action_inc_nmaster
 *   wewm_action_set_mfact
 *   wewm_action_zoom
 *   wewm_action_cycle_saved
 *   wewm_action_toggle_floating
 *   wewm_action_toggle_bar
 *   wewm_action_view
 *   wewm_action_toggle_view
 *   wewm_action_tag
 *   wewm_action_toggletag
 *   wewm_action_focus_monitor
 *   wewm_action_send_monitor
 *   wewm_action_move_resize
 *   wewm_action_kill
 *   wewm_action_quit
 */

/* --------------------------- mouse bindings ----------------------------- */

/*
 * Button and ClickRegion are real configuration interfaces in wewm.c.  The
 * active source currently owns an empty wewm_buttons table and sets
 * wewm_button_count to zero, so no duplicate mouse table is emitted here.
 *
 * The supported click regions are:
 *   ClickTagIndicator
 *   ClickLayoutSymbol
 *   ClickStatusText
 *   ClickWindowTitle
 *   ClickClientWindow
 *   ClickRootBackground
 */

/* ----------------------- integration boundary --------------------------- */

/*
 * Current status:
 *
 *   Consumed now:
 *     WEWM_VERSION
 *     WEWM_FONT_COUNT
 *     WEWM_FONT_PRIMARY
 *     WEWM_COLOR_FG
 *     WEWM_COLOR_BG
 *     WEWM_COLOR_BORDER
 *     WEWM_COLOR_FG_SELECTED
 *     WEWM_COLOR_BG_SELECTED
 *     WEWM_COLOR_BORDER_SELECTED
 *     WEWM_CLIENT_BORDER_WIDTH
 *     WEWM_DEFAULT_FLOATING
 *     WEWM_BAR_VISIBLE
 *     WEWM_BAR_TOP
 *
 *   Not consumed yet:
 *     configurable tag labels/count
 *     configurable layout table
 *     configurable Rule table
 *     configurable Key table
 *     configurable Button table
 *
 * The latter surfaces require an explicit source-level configuration handoff
 * before this file can safely contain live objects for them.  This template
 * intentionally does not fabricate that handoff or alter the architecture.
 */
