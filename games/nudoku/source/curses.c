// curses.c - Minimal ncurses subset for nudoku (AEE32 / MP3 CP2)

#ifdef AEE32

#include "usr/string.h"
#include "usr/heap.h"
#include "usr/syscall.h"
#include <stdarg.h>
#include "ncurses.h"

#define NCURSES_MAX_PAIRS 16

static struct { short fg; short bg; } color_pairs[NCURSES_MAX_PAIRS];
static int color_enabled = 0;

struct _win_st {
    int begy, begx;
    int height, width;
    int cury, curx;
    attr_t cur_attr;
    chtype bkgd;
    chtype *buf;
    chtype *shadow;
    int force_refresh;
};

int LINES = 30;
int COLS  = 90;

static struct _win_st _stdscr;
WINDOW *stdscr = &_stdscr;
static int cursor_row = 0;
static int cursor_col = 0;

static void twrite(const char *s, int len) { _write(1, s, len); }
static void tputs(const char *s)           { twrite(s, (int)strlen(s)); }
static void tputc(char c)                  { _write(1, &c, 1); }
static int  tgetc(void) { unsigned char c; return (_read(0,&c,1)>0) ? c : -1; }

static void term_move(int row, int col) {
    char buf[24];
    snprintf(buf, sizeof(buf), "\033[%d;%dH", row+1, col+1);
    tputs(buf);
}

static void emit_attr(attr_t attr) {
    char buf[24];
    tputs("\033[0m");
    if (attr & A_BOLD)      tputs("\033[1m");
    if (attr & A_DIM)       tputs("\033[2m");
    if (attr & A_UNDERLINE) tputs("\033[4m");
    if (attr & A_BLINK)     tputs("\033[5m");
    if (attr & A_REVERSE)   tputs("\033[7m");
    if (color_enabled) {
        int pair = (int)((attr >> 24) & 0xFFU);
        short fg = color_pairs[pair].fg, bg = color_pairs[pair].bg;
        if (fg >= 0) { snprintf(buf,sizeof(buf),"\033[%dm",30+fg); tputs(buf); }
        else tputs("\033[39m");
        if (bg >= 0) { snprintf(buf,sizeof(buf),"\033[%dm",40+bg); tputs(buf); }
        else tputs("\033[49m");
    }
}

static void emit_ch(chtype ch) {
    if (ch & ACS_FLAG) {
        tputs("\033(0"); tputc((char)(ch & 0xFF)); tputs("\033(B");
    } else {
        char c = (char)(ch & 0xFF);
        tputc(c ? c : ' ');
    }
}

WINDOW *initscr(void) {
    _stdscr.height=LINES; _stdscr.width=COLS;
    _stdscr.begy=0; _stdscr.begx=0;
    _stdscr.cury=0; _stdscr.curx=0;
    _stdscr.cur_attr=A_NORMAL; _stdscr.bkgd=' ';
    _stdscr.buf = NULL;
    _stdscr.shadow = NULL;
    _stdscr.force_refresh = 0;
    tputs("\033[H\033[J\033[?25h");
    return stdscr;
}

int endwin(void) {
    tputs("\033[0m\033[?25h\033[H\033[J");
    return OK;
}

WINDOW *newwin(int nlines, int ncols, int begy, int begx) {
    WINDOW *w = malloc(sizeof(struct _win_st));
    if (!w) return (WINDOW*)0;

    w->height = nlines;
    w->width  = ncols;
    w->begy   = begy;
    w->begx   = begx;
    w->cury   = 0;
    w->curx   = 0;
    w->cur_attr = A_NORMAL;
    w->bkgd   = ' ';
    w->force_refresh = 1;

    w->buf = malloc((size_t)(nlines*ncols) * sizeof(chtype));
    if (!w->buf) { free(w); return (WINDOW*)0; }

    w->shadow = malloc((size_t)(nlines*ncols) * sizeof(chtype));
    if (!w->shadow) { free(w->buf); free(w); return (WINDOW*)0; }

    for (int i = 0; i < nlines*ncols; i++) {
        w->buf[i] = ' ';
        w->shadow[i] = (chtype)~w->buf[i];
    }

    return w;
}

int delwin(WINDOW *win) {
    if (win && win != stdscr) {
        free(win->shadow);
        free(win->buf);
        free(win);
    }
    return OK;
}

int wrefresh(WINDOW *win) {
    if (!win || !win->buf) return OK;

    int drew = 0;
    attr_t last = (attr_t)0xDEADBEEFU;

    for (int r = 0; r < win->height; r++) {
        int c = 0;
        while (c < win->width) {
            int idx = r * win->width + c;
            if (!win->force_refresh && win->shadow[idx] == win->buf[idx]) {
                c++;
                continue;
            }

            if (!drew) {
                tputs("\033[?25l");
                drew = 1;
            }

            term_move(win->begy + r, win->begx + c);

            while (c < win->width) {
                idx = r * win->width + c;
                chtype ch = win->buf[idx];

                if (!win->force_refresh && win->shadow[idx] == ch)
                    break;

                attr_t attr = ch & 0xFFFF0000U;
                if (attr != last) {
                    emit_attr(attr);
                    last = attr;
                }
                emit_ch(ch);
                win->shadow[idx] = ch;
                c++;
            }
        }
    }

    if (drew)
        tputs("\033[0m");

    win->force_refresh = 0;
    return OK;
}

int werase(WINDOW *win) {
    if (!win || !win->buf) return OK;
    chtype fill = ' ' | (win->bkgd & 0xFFFF0000U);
    for (int i = 0; i < win->height*win->width; i++)
        win->buf[i] = fill;
    win->cury = 0;
    win->curx = 0;
    return OK;
}

int redrawwin(WINDOW *win) {
    if (!win) return ERR;
    win->force_refresh = 1;
    return wrefresh(win);
}

int refresh(void) { return OK; }
int clear(void)   { tputs("\033[H\033[J"); return OK; }

int wmove(WINDOW *win, int y, int x) {
    if (!win) return ERR;
    if (y < 0) y = 0;
    if (x < 0) x = 0;
    if (y >= win->height) y = win->height - 1;
    if (x >= win->width)  x = win->width - 1;
    win->cury = y;
    win->curx = x;
    cursor_row = win->begy + y;
    cursor_col = win->begx + x;
    return OK;
}

int waddch(WINDOW *win, chtype ch) {
    if (!win || !win->buf) return ERR;
    unsigned char c = (unsigned char)(ch & 0xFFU);

    if (c == '\n') {
        win->curx = 0;
        win->cury++;
        return OK;
    }
    if (c == '\r') {
        win->curx = 0;
        return OK;
    }
    if (c == '\t') {
        int next = (win->curx + 8) & ~7;
        while (win->curx < next && win->curx < win->width) {
            if ((unsigned)win->cury >= (unsigned)win->height) return ERR;
            win->buf[win->cury*win->width + win->curx] = ' ' | win->cur_attr;
            win->curx++;
        }
        return OK;
    }

    if ((unsigned)win->cury >= (unsigned)win->height) return ERR;
    if ((unsigned)win->curx >= (unsigned)win->width)  return ERR;

    win->buf[win->cury*win->width + win->curx] = (ch & 0x1FFU) | win->cur_attr;
    if (++win->curx >= win->width) {
        win->curx = 0;
        win->cury++;
    }
    return OK;
}

int wstandend(WINDOW *win) { if (!win) return ERR; win->cur_attr = A_NORMAL; return OK; }
int wattron (WINDOW *win, int a) { if (!win) return ERR; win->cur_attr |= (attr_t)a;  return OK; }
int wattroff(WINDOW *win, int a) { if (!win) return ERR; win->cur_attr &= ~(attr_t)a; return OK; }
int wbkgd(WINDOW *win, chtype ch) { if (!win) return ERR; win->bkgd = ch; return OK; }

int wprintw(WINDOW *win, const char *fmt, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    for (int i = 0; buf[i]; i++)
        waddch(win, (chtype)(unsigned char)buf[i]);
    return OK;
}

int mvwprintw(WINDOW *win, int y, int x, const char *fmt, ...) {
    wmove(win, y, x);
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    for (int i = 0; buf[i]; i++)
        waddch(win, (chtype)(unsigned char)buf[i]);
    return OK;
}

int printw(const char *fmt, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    tputs(buf);
    return OK;
}

int mvprintw(int y, int x, const char *fmt, ...) {
    term_move(y, x);
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    tputs(buf);
    return OK;
}

int start_color(void) {
    color_enabled = 1;
    for (int i = 0; i < NCURSES_MAX_PAIRS; i++)
        color_pairs[i].fg = color_pairs[i].bg = -1;
    return OK;
}

bool has_colors(void)         { return (bool)1; }
int  use_default_colors(void) { return OK; }

int init_pair(short pair, short fg, short bg) {
    if (pair < 0 || pair >= NCURSES_MAX_PAIRS) return ERR;
    color_pairs[pair].fg = fg;
    color_pairs[pair].bg = bg;
    return OK;
}

int cbreak(void)                 { return OK; }
int noecho(void)                 { return OK; }
int nonl(void)                   { return OK; }
int keypad(WINDOW *win, bool bf) { (void)win; (void)bf; return OK; }

int getch(void) {
    term_move(cursor_row, cursor_col);
    tputs("\033[?25h");
    int c = tgetc();
    if (c < 0) return ERR;
    if (c == 0x1B) {
        int c2 = tgetc();
        if (c2 < 0) return 0x1B;
        if (c2 == '[') {
            int c3 = tgetc();
            if      (c3 == 'A') return KEY_UP;
            else if (c3 == 'B') return KEY_DOWN;
            else if (c3 == 'C') return KEY_RIGHT;
            else if (c3 == 'D') return KEY_LEFT;
            else if (c3 == 'H') return KEY_HOME;
            else if (c3 == '3') { tgetc(); return KEY_DC; }
            else return ERR;
        } else if (c2 == 'O') {
            int c3 = tgetc();
            if      (c3 == 'A') return KEY_UP;
            else if (c3 == 'B') return KEY_DOWN;
            else if (c3 == 'C') return KEY_RIGHT;
            else if (c3 == 'D') return KEY_LEFT;
            else return ERR;
        }
        return 0x1B;
    }
    if (c == '\r') return '\n';
    if (c == 127 || c == 8) return KEY_BACKSPACE;
    return c;
}

#endif // AEE32
