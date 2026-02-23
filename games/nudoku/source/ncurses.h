// ncurses.h - Minimal ncurses compatibility layer for nudoku on bare-metal RISC-V
//
// Provides the ncurses subset used by nudoku using VT100 escape sequences.
//

#ifndef _NCURSES_H_
#define _NCURSES_H_

#include <stddef.h>
#include <stdarg.h>
#include <stdbool.h>

// Pull in our bare-metal stdio stub so that source files get FILE*,
// fopen(), fgetc() etc. without needing to #include <stdio.h> themselves
// (matching the behaviour of a standard ncurses.h which includes stdio.h).
#include <stdio.h>

// ── Basic types ───────────────────────────────────────────────────────────────

typedef unsigned int chtype;    // character + attribute combined
typedef unsigned int attr_t;    // attribute only

// Opaque window handle
typedef struct _win_st WINDOW;

// ── Return codes ─────────────────────────────────────────────────────────────

#define OK   0
#define ERR  (-1)

// ── Attribute bits (stored in chtype / attr_t) ────────────────────────────────
//
// Layout of a chtype:
//   bits  0-7  : character value (0-255)
//   bit   8    : ACS flag (VT100 alternate character set)
//   bits  9-15 : reserved
//   bit  16    : A_BOLD
//   bit  17    : A_REVERSE
//   bit  18    : A_UNDERLINE
//   bit  19    : A_DIM
//   bit  20    : A_BLINK
//   bits 21-23 : reserved
//   bits 24-31 : color-pair number (0-255)

#define A_NORMAL     0x00000000U
#define A_BOLD       0x00010000U
#define A_REVERSE    0x00020000U
#define A_UNDERLINE  0x00040000U
#define A_DIM        0x00080000U
#define A_BLINK      0x00100000U
#define A_STANDOUT   A_REVERSE      // compatibility alias

// Encode/decode color-pair number in bits 24-31
#define COLOR_PAIR(n)   ((attr_t)((unsigned)(n) << 24))
#define PAIR_NUMBER(a)  (((a) >> 24) & 0xFFU)

// ── Standard colors ───────────────────────────────────────────────────────────

#define COLOR_BLACK    0
#define COLOR_RED      1
#define COLOR_GREEN    2
#define COLOR_YELLOW   3
#define COLOR_BLUE     4
#define COLOR_MAGENTA  5
#define COLOR_CYAN     6
#define COLOR_WHITE    7

// ── ACS (Alternate Character Set) box-drawing glyphs ─────────────────────────
//
// Bit 8 set → VT100 ACS mode (\033(0 ... \033(B).
// The lower byte is the VT100 ACS glyph code.

#define ACS_FLAG      0x00000100U
#define ACS_ULCORNER  (ACS_FLAG | (chtype)'l')   // ┌
#define ACS_URCORNER  (ACS_FLAG | (chtype)'k')   // ┐
#define ACS_LLCORNER  (ACS_FLAG | (chtype)'m')   // └
#define ACS_LRCORNER  (ACS_FLAG | (chtype)'j')   // ┘
#define ACS_LTEE      (ACS_FLAG | (chtype)'t')   // ├
#define ACS_RTEE      (ACS_FLAG | (chtype)'u')   // ┤
#define ACS_TTEE      (ACS_FLAG | (chtype)'w')   // ┬
#define ACS_BTEE      (ACS_FLAG | (chtype)'v')   // ┴
#define ACS_PLUS      (ACS_FLAG | (chtype)'n')   // ┼
#define ACS_HLINE     (ACS_FLAG | (chtype)'q')   // ─
#define ACS_VLINE     (ACS_FLAG | (chtype)'x')   // │

// ── Special key codes ────────────────────────────────────────────────────────

#define KEY_DOWN      0x102
#define KEY_UP        0x103
#define KEY_LEFT      0x104
#define KEY_RIGHT     0x105
#define KEY_HOME      0x106
#define KEY_BACKSPACE 0x107
#define KEY_F0        0x108
#define KEY_DC        0x14a   // Delete key
#define KEY_ENTER     0x157
#define KEY_RESIZE    0x19a

// ── globals ───────────────────────────────────────────────────────────────────

extern int     LINES;   // terminal height (default 30)
extern int     COLS;    // terminal width  (default 90)
extern WINDOW *stdscr;  // full-screen window

// ── Window management ─────────────────────────────────────────────────────────

WINDOW *initscr(void);
int     endwin(void);
WINDOW *newwin(int nlines, int ncols, int begy, int begx);
int     delwin(WINDOW *win);

// ── Screen update ─────────────────────────────────────────────────────────────

int     refresh(void);
int     wrefresh(WINDOW *win);
int     werase(WINDOW *win);
int     redrawwin(WINDOW *win);
int     clear(void);

// ── Cursor movement ───────────────────────────────────────────────────────────

int     wmove(WINDOW *win, int y, int x);

// ── Character output ─────────────────────────────────────────────────────────

int     waddch(WINDOW *win, chtype ch);
int     wstandend(WINDOW *win);
int     wattron(WINDOW *win, int attr);
int     wattroff(WINDOW *win, int attr);
int     wbkgd(WINDOW *win, chtype ch);

int     wprintw(WINDOW *win, const char *fmt, ...);
int     mvwprintw(WINDOW *win, int y, int x, const char *fmt, ...);
int     printw(const char *fmt, ...);
int     mvprintw(int y, int x, const char *fmt, ...);

// ── Input ─────────────────────────────────────────────────────────────────────

int     getch(void);

// ── Terminal mode (no-ops on this OS) ────────────────────────────────────────

int     cbreak(void);
int     noecho(void);
int     nonl(void);
int     keypad(WINDOW *win, bool bf);

// ── Color ─────────────────────────────────────────────────────────────────────

int     start_color(void);
bool    has_colors(void);
int     use_default_colors(void);
int     init_pair(short pair, short fg, short bg);

#endif // _NCURSES_H_
