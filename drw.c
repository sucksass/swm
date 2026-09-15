#include "drw.h"
#include "util.h"

#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>
#include <fontconfig/fontconfig.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


typedef struct FailCache FailCache;

struct FailCache {
	Drw *drw;
	FcChar32 *cp;
	size_t len;
	size_t cap;
	FailCache *next;
};

static FailCache *failcaches;

static FailCache *failcache_get(Drw *drw, int create);
static void failcache_remove(Drw *drw);
static int failcache_has(Drw *drw, FcChar32 cp);
static void failcache_add(Drw *drw, FcChar32 cp);

static Fnt *drw_font_create(Drw *drw, const char *fontname,
                            FcPattern *pattern);
static void drw_font_free(Fnt *font);

static int utf8decode(const char *s, long *u, int *err);
static Fnt *font_for_codepoint(Drw *drw, FcChar32 cp);

static unsigned int text_measure(Drw *drw, const char *text,
                                 unsigned int limit, int limited);

static int text_draw(Drw *drw, int x, int y, unsigned int w,
                     unsigned int h, unsigned int lpad,
                     const char *text, int invert);


/* failed-font cache */

static FailCache *
failcache_get(Drw *drw, int create)
{
	FailCache *c;

	for (c = failcaches; c; c = c->next)
		if (c->drw == drw)
			return c;

	if (!create)
		return NULL;

	c = ecalloc(1, sizeof(*c));
	c->drw = drw;
	c->next = failcaches;
	failcaches = c;

	return c;
}

static void
failcache_remove(Drw *drw)
{
	FailCache **pp;
	FailCache *c;

	for (pp = &failcaches; (c = *pp); pp = &c->next) {
		if (c->drw != drw)
			continue;

		*pp = c->next;
		free(c->cp);
		free(c);
		return;
	}
}

static int
failcache_has(Drw *drw, FcChar32 cp)
{
	FailCache *c;
	size_t i;

	c = failcache_get(drw, 0);
	if (!c)
		return 0;

	for (i = 0; i < c->len; i++)
		if (c->cp[i] == cp)
			return 1;

	return 0;
}

static void
failcache_add(Drw *drw, FcChar32 cp)
{
	FailCache *c;
	FcChar32 *p;
	size_t cap;

	if (failcache_has(drw, cp))
		return;

	c = failcache_get(drw, 1);

	if (c->len == c->cap) {
		cap = c->cap ? c->cap * 2 : 32;

		p = realloc(c->cp, cap * sizeof(*p));
		if (!p)
			die("realloc:");

		c->cp = p;
		c->cap = cap;
	}

	c->cp[c->len++] = cp;
}


/* drawing context */

Drw *
drw_create(Display *dpy, int screen, Window root,
           unsigned int w, unsigned int h)
{
	Drw *drw;

	if (!dpy)
		return NULL;

	drw = ecalloc(1, sizeof(*drw));

	drw->w = w;
	drw->h = h;
	drw->dpy = dpy;
	drw->screen = screen;
	drw->root = root;

	drw->drawable = XCreatePixmap(
		dpy, root, w, h, DefaultDepth(dpy, screen));

	drw->gc = XCreateGC(dpy, root, 0, NULL);

	XSetLineAttributes(
		dpy, drw->gc, 1, LineSolid, CapButt, JoinMiter);

	return drw;
}

void
drw_resize(Drw *drw, unsigned int w, unsigned int h)
{
	if (!drw)
		return;

	drw->w = w;
	drw->h = h;

	XFreePixmap(drw->dpy, drw->drawable);

	drw->drawable = XCreatePixmap(
		drw->dpy, drw->root, w, h,
		DefaultDepth(drw->dpy, drw->screen));
}

void
drw_free(Drw *drw)
{
	if (!drw)
		return;

	failcache_remove(drw);

	XFreePixmap(drw->dpy, drw->drawable);
	XFreeGC(drw->dpy, drw->gc);

	drw_fontset_free(drw->fonts);

	free(drw);
}


/* fonts */

static Fnt *
drw_font_create(Drw *drw, const char *fontname, FcPattern *pattern)
{
	Fnt *font;

	if (!drw)
		return NULL;

	if (!fontname && !pattern)
		die("no font specified.");

	font = ecalloc(1, sizeof(*font));
	font->dpy = drw->dpy;

	if (fontname) {
		font->xfont = XftFontOpenName(
			drw->dpy, drw->screen, fontname);

		if (!font->xfont) {
			fprintf(stderr,
			        "drw: cannot load font '%s'\n",
			        fontname);
			free(font);
			return NULL;
		}

		font->pattern = FcNameParse(
			(const FcChar8 *)fontname);

		if (!font->pattern) {
			fprintf(stderr,
			        "drw: cannot parse font '%s'\n",
			        fontname);
			XftFontClose(drw->dpy, font->xfont);
			free(font);
			return NULL;
		}
	} else {
		font->pattern = FcPatternDuplicate(pattern);

		if (!font->pattern) {
			free(font);
			return NULL;
		}

		font->xfont = XftFontOpenPattern(
			drw->dpy, pattern);

		if (!font->xfont) {
			FcPatternDestroy(font->pattern);
			free(font);
			return NULL;
		}
	}

	font->h = (unsigned int)(
		font->xfont->ascent + font->xfont->descent);

	return font;
}

static void
drw_font_free(Fnt *font)
{
	if (!font)
		return;

	drw_font_free(font->next);

	if (font->pattern)
		FcPatternDestroy(font->pattern);

	if (font->xfont)
		XftFontClose(font->dpy, font->xfont);

	free(font);
}

Fnt *
drw_fontset_create(Drw *drw, const char *fonts[], size_t fontcount)
{
	Fnt *head = NULL;
	Fnt *tail = NULL;
	Fnt *font;
	Fnt *oldfonts;
	size_t i;

	if (!drw || !fonts)
		return NULL;

	for (i = 0; i < fontcount; i++) {
		if (!fonts[i])
			continue;

		font = drw_font_create(drw, fonts[i], NULL);

		if (!font)
			continue;

		if (!head)
			head = font;
		else
			tail->next = font;

		tail = font;
	}

	/*
	 * The newly-created set is ready before the old set is
	 * released, so a failed individual font load does not
	 * accidentally destroy the currently active set.
	 */
	oldfonts = drw->fonts;

	drw->fonts = head;

	/*
	 * The available font set has changed, so failed fallback
	 * results associated with the old set are no longer valid.
	 */
	failcache_remove(drw);

	drw_font_free(oldfonts);

	return head;
}

void
drw_fontset_free(Fnt *set)
{
	drw_font_free(set);
}


/*
 * Find the first font in the current ordered set containing cp.
 * If none does, ask Fontconfig for a fallback and append it to
 * the current set so that it remains available for later use.
 */
static Fnt *
font_for_codepoint(Drw *drw, FcChar32 cp)
{
	Fnt *font;
	Fnt *tail;
	Fnt *fallback;

	FcCharSet *charset;
	FcPattern *pattern;
	FcPattern *match;
	FcResult result;

	if (!drw || !drw->fonts)
		return NULL;

	for (font = drw->fonts; font; font = font->next) {
		if (XftCharExists(
			    drw->dpy, font->xfont, cp))
			return font;
	}

	if (failcache_has(drw, cp))
		return NULL;

	charset = FcCharSetCreate();

	if (!charset)
		return NULL;

	if (!FcCharSetAddChar(charset, cp)) {
		FcCharSetDestroy(charset);
		return NULL;
	}

	pattern = FcPatternDuplicate(
		drw->fonts->pattern);

	if (!pattern) {
		FcCharSetDestroy(charset);
		return NULL;
	}

	FcPatternAddCharSet(
		pattern, FC_CHARSET, charset);

	FcPatternAddBool(
		pattern, FC_SCALABLE, FcTrue);

	FcConfigSubstitute(
		NULL, pattern, FcMatchPattern);

	FcDefaultSubstitute(pattern);

	match = XftFontMatch(
		drw->dpy, drw->screen, pattern, &result);

	FcPatternDestroy(pattern);
	FcCharSetDestroy(charset);

	if (!match) {
		failcache_add(drw, cp);
		return NULL;
	}

	fallback = drw_font_create(
		drw, NULL, match);

	if (!fallback) {
		failcache_add(drw, cp);
		return NULL;
	}

	if (!XftCharExists(
		    drw->dpy, fallback->xfont, cp)) {
		drw_font_free(fallback);
		failcache_add(drw, cp);
		return NULL;
	}

	tail = drw->fonts;

	while (tail->next)
		tail = tail->next;

	tail->next = fallback;

	return fallback;
}


/* text metrics */

unsigned int
drw_fontset_getwidth(Drw *drw, const char *text)
{
	if (!drw)
		return 0;

	return text_measure(drw, text, 0, 0);
}

unsigned int
drw_fontset_getwidth_clamp(Drw *drw, const char *text,
                           unsigned int n)
{
	unsigned int width;

	if (!drw || !text || !n)
		return 0;

	width = text_measure(drw, text, n, 1);

	return MIN(n, width);
}

void
drw_font_getexts(Fnt *font, const char *text,
                 unsigned int len, unsigned int *w,
                 unsigned int *h)
{
	XGlyphInfo ext;

	if (!font || !font->xfont || !text)
		return;

	XftTextExtentsUtf8(
		font->dpy,
		font->xfont,
		(const FcChar8 *)text,
		len,
		&ext);

	if (w)
		*w = ext.xOff;

	if (h)
		*h = font->h;
}


/* colors */

void
drw_clr_create(Drw *drw, Clr *dest, const char *clrname)
{
	if (!drw || !dest || !clrname)
		return;

	if (!XftColorAllocName(
		    drw->dpy,
		    DefaultVisual(drw->dpy, drw->screen),
		    DefaultColormap(drw->dpy, drw->screen),
		    clrname,
		    dest))
		die("cannot allocate color '%s':", clrname);
}

void
drw_clr_free(Drw *drw, Clr *clr)
{
	if (!drw || !clr)
		return;

	XftColorFree(
		drw->dpy,
		DefaultVisual(drw->dpy, drw->screen),
		DefaultColormap(drw->dpy, drw->screen),
		clr);
}

Clr *
drw_scm_create(Drw *drw, const char *clrnames[],
               size_t clrcount)
{
	Clr *scm;
	size_t i;

	if (!drw || !clrnames || clrcount < 2)
		return NULL;

	scm = ecalloc(clrcount, sizeof(*scm));

	for (i = 0; i < clrcount; i++)
		drw_clr_create(
			drw, &scm[i], clrnames[i]);

	return scm;
}

void
drw_scm_free(Drw *drw, Clr *scm, size_t clrcount)
{
	size_t i;

	if (!drw || !scm)
		return;

	for (i = 0; i < clrcount; i++)
		drw_clr_free(drw, &scm[i]);

	free(scm);
}


/* cursors */

Cur *
drw_cur_create(Drw *drw, int shape)
{
	Cur *cursor;

	if (!drw)
		return NULL;

	cursor = ecalloc(1, sizeof(*cursor));

	cursor->cursor =
		XCreateFontCursor(drw->dpy, shape);

	return cursor;
}

void
drw_cur_free(Drw *drw, Cur *cursor)
{
	if (!drw || !cursor)
		return;

	XFreeCursor(drw->dpy, cursor->cursor);
	free(cursor);
}


/* setters */

void
drw_setfontset(Drw *drw, Fnt *set)
{
	if (!drw)
		return;

	failcache_remove(drw);
	drw->fonts = set;
}

void
drw_setscheme(Drw *drw, Clr *scm)
{
	if (!drw)
		return;

	drw->scheme = scm;
}


/* rectangle */

void
drw_rect(Drw *drw, int x, int y,
         unsigned int w, unsigned int h,
         int filled, int invert)
{
	if (!drw || !drw->scheme)
		return;

	XSetForeground(
		drw->dpy,
		drw->gc,
		drw->scheme[invert ? ColFg : ColBg].pixel);

	if (filled) {
		XFillRectangle(
			drw->dpy, drw->drawable, drw->gc,
			x, y, w, h);
	} else {
		XDrawRectangle(
			drw->dpy, drw->drawable, drw->gc,
			x, y, w - 1, h - 1);
	}
}


/* UTF-8 */

static int
utf8decode(const char *s, long *u, int *err)
{
	unsigned char c;
	unsigned long cp;
	int len;
	int i;

	*u = 0xFFFD;
	*err = 1;

	if (!s || !*s)
		return 0;

	c = (unsigned char)s[0];

	if (c < 0x80) {
		*u = c;
		*err = 0;
		return 1;
	}

	if (c >= 0xC2 && c <= 0xDF) {
		cp = c & 0x1F;
		len = 2;
	} else if (c >= 0xE0 && c <= 0xEF) {
		cp = c & 0x0F;
		len = 3;
	} else if (c >= 0xF0 && c <= 0xF4) {
		cp = c & 0x07;
		len = 4;
	} else {
		return 1;
	}

	for (i = 1; i < len; i++) {
		c = (unsigned char)s[i];

		if (!c)
			return i;

		if ((c & 0xC0) != 0x80)
			return i;

		cp = (cp << 6) | (c & 0x3F);
	}

	if ((len == 2 && cp < 0x80) ||
	    (len == 3 && cp < 0x800) ||
	    (len == 4 && cp < 0x10000) ||
	    (cp >= 0xD800 && cp <= 0xDFFF) ||
	    cp > 0x10FFFF)
		return len;

	*u = (long)cp;
	*err = 0;

	return len;
}


/* shared measurement engine */

static unsigned int
text_measure(Drw *drw, const char *text,
             unsigned int limit, int limited)
{
	const char *p;
	Fnt *font;
	long cp;
	int invalid;
	int len;
	unsigned int width;
	unsigned int cw;

	if (!drw || !text || !drw->fonts)
		return 0;

	width = 0;
	p = text;

	while (*p) {
		len = utf8decode(
			p, &cp, &invalid);

		if (!len)
			break;

		if (invalid) {
			cp = 0xFFFD;

			font = font_for_codepoint(
				drw, (FcChar32)cp);

			if (!font)
				return width;

			drw_font_getexts(
				font,
				"\xEF\xBF\xBD",
				3,
				&cw,
				NULL);
		} else {
			font = font_for_codepoint(
				drw, (FcChar32)cp);

			if (!font) {
				cp = 0xFFFD;

				font = font_for_codepoint(
					drw, (FcChar32)cp);

				if (!font)
					return width;

				drw_font_getexts(
					font,
					"\xEF\xBF\xBD",
					3,
					&cw,
					NULL);
			} else {
				drw_font_getexts(
					font,
					p,
					(unsigned int)len,
					&cw,
					NULL);
			}
		}

		if (limited && width + cw > limit)
			return width + cw;

		width += cw;
		p += len;
	}

	return width;
}


/* text drawing */

static int
text_draw(Drw *drw, int x, int y,
          unsigned int w, unsigned int h,
          unsigned int lpad, const char *text,
          int invert)
{
	XftDraw *draw;
	Clr *fg;

	const char *p;
	long cp;

	Fnt *font;
	Fnt *ellipsis_font;

	int invalid;
	int len;

	unsigned int available;
	unsigned int fullwidth;
	unsigned int ellipsis_width;
	unsigned int prefix_width;
	unsigned int cw;
	unsigned int used;

	const char *cut;
	int cutlen;

	if (!drw || !drw->scheme ||
	    !drw->fonts || !text)
		return x;

	XSetForeground(
		drw->dpy,
		drw->gc,
		drw->scheme[invert ? ColFg : ColBg].pixel);

	XFillRectangle(
		drw->dpy,
		drw->drawable,
		drw->gc,
		x, y, w, h);

	if (w < lpad)
		return x + w;

	x += lpad;
	available = w - lpad;

	draw = XftDrawCreate(
		drw->dpy,
		drw->drawable,
		DefaultVisual(drw->dpy, drw->screen),
		DefaultColormap(drw->dpy, drw->screen));

	if (!draw)
		return x + available;

	fg = &drw->scheme[invert ? ColBg : ColFg];

	fullwidth = text_measure(
		drw, text, 0, 0);

	/*
	 * Select the font used for the literal ASCII ellipsis once.
	 * The actual three-byte string is then measured as one UTF-8
	 * string in that exact font, matching the eventual draw call.
	 */
	ellipsis_font =
		font_for_codepoint(drw, (FcChar32)'.');

	if (ellipsis_font) {
		drw_font_getexts(
			ellipsis_font,
			"...",
			3,
			&ellipsis_width,
			NULL);
	} else {
		ellipsis_width = 0;
	}

	/*
	 * If everything fits, render the complete string.
	 */
	if (fullwidth <= available) {
		p = text;
		used = 0;

		while (*p) {
			len = utf8decode(
				p, &cp, &invalid);

			if (!len)
				break;

			if (invalid) {
				font = font_for_codepoint(
					drw, 0xFFFD);

				if (!font)
					break;

				XftDrawStringUtf8(
					draw,
					fg,
					font->xfont,
					x + used,
					y + (h - font->h) / 2 +
					font->xfont->ascent,
					(const FcChar8 *)
					"\xEF\xBF\xBD",
					3);

				drw_font_getexts(
					font,
					"\xEF\xBF\xBD",
					3,
					&cw,
					NULL);
			} else {
				font = font_for_codepoint(
					drw, (FcChar32)cp);

				if (!font) {
					font = font_for_codepoint(
						drw, 0xFFFD);

					if (!font)
						break;

					XftDrawStringUtf8(
						draw,
						fg,
						font->xfont,
						x + used,
						y + (h - font->h) / 2 +
						font->xfont->ascent,
						(const FcChar8 *)
						"\xEF\xBF\xBD",
						3);

					drw_font_getexts(
						font,
						"\xEF\xBF\xBD",
						3,
						&cw,
						NULL);
				} else {
					XftDrawStringUtf8(
						draw,
						fg,
						font->xfont,
						x + used,
						y + (h - font->h) / 2 +
						font->xfont->ascent,
						(const FcChar8 *)p,
						(unsigned int)len);

					drw_font_getexts(
						font,
						p,
						(unsigned int)len,
						&cw,
						NULL);
				}
			}

			used += cw;
			p += len;
		}

		XftDrawDestroy(draw);
		return x + used;
	}

	/*
	 * The text does not fit. If the ellipsis itself cannot fit,
	 * omit the text.
	 */
	if (!ellipsis_font || ellipsis_width > available) {
		XftDrawDestroy(draw);
		return x + available;
	}

	/*
	 * Determine the largest valid UTF-8 prefix that leaves room
	 * for the exact ellipsis string measured above.
	 */
	prefix_width = 0;
	cut = text;
	cutlen = 0;
	p = text;

	while (*p) {
		len = utf8decode(
			p, &cp, &invalid);

		if (!len)
			break;

		font = font_for_codepoint(
			drw,
			(FcChar32)(invalid ? 0xFFFD : cp));

		if (!font)
			font = font_for_codepoint(
				drw, 0xFFFD);

		if (!font)
			break;

		if (invalid)
			drw_font_getexts(
				font,
				"\xEF\xBF\xBD",
				3,
				&cw,
				NULL);
		else
			drw_font_getexts(
				font,
				p,
				(unsigned int)len,
				&cw,
				NULL);

		if (prefix_width + cw + ellipsis_width > available)
			break;

		prefix_width += cw;
		p += len;
		cut = p;
		cutlen = (int)(cut - text);
	}

	/*
	 * Render the retained prefix.
	 */
	p = text;
	used = 0;

	while ((int)(p - text) < cutlen) {
		len = utf8decode(
			p, &cp, &invalid);

		if (!len)
			break;

		font = font_for_codepoint(
			drw,
			(FcChar32)(invalid ? 0xFFFD : cp));

		if (!font)
			font = font_for_codepoint(
				drw, 0xFFFD);

		if (!font)
			break;

		if (invalid) {
			XftDrawStringUtf8(
				draw,
				fg,
				font->xfont,
				x + used,
				y + (h - font->h) / 2 +
				font->xfont->ascent,
				(const FcChar8 *)
				"\xEF\xBF\xBD",
				3);

			drw_font_getexts(
				font,
				"\xEF\xBF\xBD",
				3,
				&cw,
				NULL);
		} else {
			XftDrawStringUtf8(
				draw,
				fg,
				font->xfont,
				x + used,
				y + (h - font->h) / 2 +
				font->xfont->ascent,
				(const FcChar8 *)p,
				(unsigned int)len);

			drw_font_getexts(
				font,
				p,
				(unsigned int)len,
				&cw,
				NULL);
		}

		used += cw;
		p += len;
	}

	/*
	 * Render the same three-byte ellipsis string using the exact
	 * font whose width was used in the truncation calculation.
	 */
	XftDrawStringUtf8(
		draw,
		fg,
		ellipsis_font->xfont,
		x + used,
		y + (h - ellipsis_font->h) / 2 +
		ellipsis_font->xfont->ascent,
		(const FcChar8 *)"...",
		3);

	used += ellipsis_width;

	XftDrawDestroy(draw);

	return x + used;
}


int
drw_text(Drw *drw, int x, int y,
         unsigned int w, unsigned int h,
         unsigned int lpad, const char *text,
         int invert)
{
	if (!drw || !text || !drw->fonts)
		return 0;

	if (x == 0 && y == 0 && w == 0 && h == 0)
		return (int)text_measure(
			drw, text, 0, 0);

	return text_draw(
		drw, x, y, w, h,
		lpad, text, invert);
}


/* pixmap -> window */

void
drw_map(Drw *drw, Window win, int x, int y,
        unsigned int w, unsigned int h)
{
	if (!drw)
		return;

	XCopyArea(
		drw->dpy,
		drw->drawable,
		win,
		drw->gc,
		x, y, w, h,
		x, y);

	XSync(drw->dpy, False);
}