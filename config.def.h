/*
 * config.def.h - default configuration template for swm
 *
 * Configuration boundary note
 * ---------------------------
 *
 * The current reconstructed swm.c does not include config.h/config.def.h and
 * does not consume configuration tables for tags, layouts, rules, keys, or
 * buttons.  This file therefore contains only configuration values that have
 * an actual consumer in the current source, plus a clearly marked description
 * of the configuration surfaces that are not yet wired into the source.
 *
 * Do not treat the unwired surfaces below as active defaults.  In particular,
 * this file must not be copied into swm.c or used to imply behavior that the
 * current implementation does not provide.
 *
 * The source filename remains swm.c; internal SWM_* names remain unchanged
 * deliberately.  The public program/version identity is swm.
 */

/* ------------------------------ identity -------------------------------- */

/* Consumed by swm.c for -v/--version and the EWMH supporting-window name. */
#define SWM_VERSION "swm 1.0"

/* ------------------------------ appearance ------------------------------ */

/* The current drawing implementation supports exactly one configured font. */
#define SWM_FONT_COUNT           1u
#define SWM_FONT_PRIMARY         "monospace:size=10"

#define SWM_COLOR_FG             "#ffffff"
#define SWM_COLOR_BG             "#000000"
#define SWM_COLOR_BORDER         "#444444"

#define SWM_COLOR_FG_SELECTED    "#000000"
#define SWM_COLOR_BG_SELECTED    "#ffffff"
#define SWM_COLOR_BORDER_SELECTED "#ffffff"

/*
 * Bar padding is currently hard-coded by swm.c as SWM_BAR_PADDING.  It is
 * therefore documented here as an implementation value, but is not redefined
 * by this template because the source does not make it safely overridable.
 */
/* Consumed by the client-geometry/default-window behavior. */
#define SWM_CLIENT_BORDER_WIDTH  1u
#define SWM_DEFAULT_FLOATING     0
#define SWM_BAR_VISIBLE          1
#define SWM_BAR_TOP              1

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
 * swm.c:
 *
 *     swm_layout_tiled
 *     swm_layout_monocle
 *
 * They are initialized into the two layout slots by the source.  There is no
 * current configuration consumer for a layout-selection table, so none is
 * duplicated here.
 */

/* ------------------------------- rules ---------------------------------- */

/*
 * The Rule structure is defined by swm.c, but the current implementation's
 * rule table is an internal zero-count table.  No configuration include or
 * rule-table handoff exists yet, so no application-specific or synthetic
 * rule table is provided here.
 */

/* ------------------------- keyboard bindings ---------------------------- */

/*
 * Key is a real configuration structure in swm.c, but the active source
 * currently owns an empty swm_keys table and sets swm_key_count to zero.
 * There is therefore no usable configuration table in this file yet.
 *
 * The implemented action entry points are nevertheless real and available
 * to a future explicit configuration boundary; no wrappers are introduced:
 *
 *   swm_action_spawn
 *   swm_action_focus_stack
 *   swm_action_inc_nmaster
 *   swm_action_set_mfact
 *   swm_action_zoom
 *   swm_action_cycle_saved
 *   swm_action_toggle_floating
 *   swm_action_toggle_bar
 *   swm_action_view
 *   swm_action_toggle_view
 *   swm_action_tag
 *   swm_action_toggletag
 *   swm_action_focus_monitor
 *   swm_action_send_monitor
 *   swm_action_move_resize
 *   swm_action_kill
 *   swm_action_quit
 */

/* --------------------------- mouse bindings ----------------------------- */

/*
 * Button and ClickRegion are real configuration interfaces in swm.c.  The
 * active source currently owns an empty swm_buttons table and sets
 * swm_button_count to zero, so no duplicate mouse table is emitted here.
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
 *     SWM_VERSION
 *     SWM_FONT_COUNT
 *     SWM_FONT_PRIMARY
 *     SWM_COLOR_FG
 *     SWM_COLOR_BG
 *     SWM_COLOR_BORDER
 *     SWM_COLOR_FG_SELECTED
 *     SWM_COLOR_BG_SELECTED
 *     SWM_COLOR_BORDER_SELECTED
 *     SWM_CLIENT_BORDER_WIDTH
 *     SWM_DEFAULT_FLOATING
 *     SWM_BAR_VISIBLE
 *     SWM_BAR_TOP
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
