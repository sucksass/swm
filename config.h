/*
 * config.h - local configuration for swm
 *
 * This configuration follows the requested visual style, while only defining
 * settings that the current wewm.c actually consumes.  Features such as
 * configurable tags, layouts, rules, keys, buttons, gaps, and commands are
 * intentionally not declared here because the current source does not read
 * them from config.h.
 */

/* identity */
#define WEWM_VERSION "swm 1.0"

/* appearance */
#define WEWM_FONT_COUNT            1u
#define WEWM_FONT_PRIMARY          "JetBrainsMono Nerd Font Mono:style=Bold:size=16"

/* TokyoNight-inspired palette */
#define WEWM_COLOR_FG              "#a9b1d6"
#define WEWM_COLOR_BG              "#1a1b26"
#define WEWM_COLOR_BORDER          "#444b6a"
#define WEWM_COLOR_FG_SELECTED     "#0db9d7"
#define WEWM_COLOR_BG_SELECTED     "#1a1b26"
#define WEWM_COLOR_BORDER_SELECTED "#ad8ee6"

/* client/bar behavior */
#define WEWM_CLIENT_BORDER_WIDTH   2u
#define WEWM_DEFAULT_FLOATING      0
#define WEWM_BAR_VISIBLE           1
#define WEWM_BAR_TOP               1
