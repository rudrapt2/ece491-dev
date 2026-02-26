/*	$NetBSD: screen.c,v screen.c,v 1.19 2004/01/27 20:30:30 jsm Exp $	*/
/* For Linux: still using old termcap interface from version 1.13.  */

/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Chris Torek and Darren F. Provine.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)screen.c	8.1 (Berkeley) 5/31/93
 */

/*
 * Tetris screen control.
 */

#include "usr/setjmp.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef sigmask
#define sigmask(s) (1 << ((s) - 1))
#endif

#include "defs.h"
#include "compat.h"
#include "screen.h"
#include "tetris.h"

#ifdef AEE32
extern size_t snprintf(char *buf, size_t bufsz, const char *fmt, ...);
#endif

/* AEE32: left-align the board with a small margin */
#ifdef AEE32
#undef CTOD
#define CTOD(x) ((x) * 2 + 2)
#endif

static cell curscreen[B_SIZE];	/* 1 => standout (or otherwise marked) */
static int curscore;
static int isset;		/* true => terminal is in game mode */
static struct termios oldtt;
static void (*tstp)(int);

static	void	scr_stop(int);
static	void	stopset(int) __attribute__((__noreturn__));
static	void	draw_side_panel(void);

#ifdef AEE32
/* ANSI background colors for each piece type (indices 0-6) */
static const char *piece_bg[7] = {
	"\033[42m",    /* S-piece: green */
	"\033[41m",    /* Z-piece: red */
	"\033[45m",    /* T-piece: magenta */
	"\033[43m",    /* O-piece: yellow */
	"\033[48;5;208m", /* L-piece: orange */
	"\033[44m",    /* J-piece: blue */
	"\033[46m",    /* I-piece: cyan */
};
/* ANSI foreground colors for ghost pieces */
static const char *piece_fg[7] = {
	"\033[32m",    /* S-piece: green */
	"\033[31m",    /* Z-piece: red */
	"\033[35m",    /* T-piece: magenta */
	"\033[33m",    /* O-piece: yellow */
	"\033[38;5;208m", /* L-piece: orange */
	"\033[34m",    /* J-piece: blue */
	"\033[36m",    /* I-piece: cyan */
};
/* put_cell_color and draw_shape_at are defined after putstr/moveto macros */
#endif


/*
 * Capabilities from TERMCAP.
 */
extern char	PC, *BC, *UP;	/* tgoto requires globals: ugh! */
static char BCdefault[] = "\b";
#ifndef NCURSES_VERSION
short	ospeed;
#endif

static char
	*bcstr,			/* backspace char */
	*CEstr,			/* clear to end of line */
	*CLstr,			/* clear screen */
	*CMstr,			/* cursor motion string */
#ifdef unneeded
	*CRstr,			/* "\r" equivalent */
#endif
	*HOstr,			/* cursor home */
	*LLstr,			/* last line, first column */
	*pcstr,			/* pad character */
	*TEstr,			/* end cursor motion mode */
	*TIstr;			/* begin cursor motion mode */
char
	*SEstr,			/* end standout mode */
	*SOstr;			/* begin standout mode */
static int
	COnum,			/* co# value */
	LInum,			/* li# value */
	MSflag;			/* can move in standout mode */


struct tcsinfo {	/* termcap string info; some abbrevs above */
	char tcname[3];
	char **tcaddr;
} tcstrings[] = {
	{"bc", &bcstr},
	{"ce", &CEstr},
	{"cl", &CLstr},
	{"cm", &CMstr},
#ifdef unneeded
	{"cr", &CRstr},
#endif
	{"le", &BC},		/* move cursor left one space */
	{"pc", &pcstr},
	{"se", &SEstr},
	{"so", &SOstr},
	{"te", &TEstr},
	{"ti", &TIstr},
	{"up", &UP},		/* cursor up */
	{ {0}, NULL}
};

/* This is where we will actually stuff the information */

static char combuf[1024], tbuf[1024];


/*
 * Routine used by tputs().
 */
int
put(c)
	int c;
{

	return (putchar(c));
}

/*
 * putstr() is for unpadded strings (either as in termcap(5) or
 * simply literal strings); putpad() is for padded strings with
 * count=1.  (See screen.h for putpad().)
 */
#define	putstr(s)	(void)fputs(s, stdout)
#define	moveto(r, c)	putpad(tgoto(CMstr, c, r))

#ifdef AEE32
static void put_cell_color(cell so) {
	if (so == 0) {
		putstr("\033[0m");
	} else if (so == WALL_CELL) {
		putstr("\033[90m");
	} else if (so >= 9 && so <= 15) {
		putstr(piece_fg[so - 9]);
	} else if (so >= 1 && so <= 7) {
		putstr(piece_bg[so - 1]);
	} else {
		putstr("\033[0m");
	}
}

static void draw_shape_at(const struct shape *s, int row, int col) {
	int idx, i;
	if (!s) return;
	idx = shape_base[s - shapes];
	putstr(piece_bg[idx]);
	moveto(row, col);
	putstr("  ");
	for (i = 0; i < 3; i++) {
		int off = s->off[i];
		int dr = 0, dc = 0;
		switch (off) {
		case -B_COLS - 1: dr = -1; dc = -1; break; /* TL */
		case -B_COLS:     dr = -1; dc =  0; break; /* TC */
		case -B_COLS + 1: dr = -1; dc =  1; break; /* TR */
		case -1:          dr =  0; dc = -1; break; /* ML */
		case 1:           dr =  0; dc =  1; break; /* MR */
		case B_COLS - 1:  dr =  1; dc = -1; break; /* BL */
		case B_COLS:      dr =  1; dc =  0; break; /* BC */
		case B_COLS + 1:  dr =  1; dc =  1; break; /* BR */
		case 2:           dr =  0; dc =  2; break; /* I horizontal */
		case 2 * B_COLS:  dr =  2; dc =  0; break; /* I vertical */
		default:
			dr = 0;
			dc = 0;
			break;
		}
		moveto(row + dr, col + dc * 2);
		putstr("  ");
	}
	putstr("\033[0m");
}
#endif

/*
 * Set up from termcap.
 */
void
scr_init()
{
	static int bsflag, xsflag, sgnum;
#ifdef unneeded
	static int ncflag;
#endif
	char *term, *fill;
	static struct tcninfo {	/* termcap numeric and flag info */
		char tcname[3];
		int *tcaddr;
	} tcflags[] = {
		{"bs", &bsflag},
		{"ms", &MSflag},
#ifdef unneeded
		{"nc", &ncflag},
#endif
		{"xs", &xsflag},
		{ {0}, NULL}
	}, tcnums[] = {
		{"co", &COnum},
		{"li", &LInum},
		{"sg", &sgnum},
		{ {0}, NULL}
	};
	
	if ((term = getenv("TERM")) == NULL)
		stop("you must set the TERM environment variable");
	if (tgetent(tbuf, term) <= 0)
		stop("cannot find your termcap");
	fill = combuf;
	{
		struct tcsinfo *p;

		for (p = tcstrings; p->tcaddr; p++)
			*p->tcaddr = tgetstr(p->tcname, &fill);
	}
	{
		struct tcninfo *p;

		for (p = tcflags; p->tcaddr; p++)
			*p->tcaddr = tgetflag(p->tcname);
		for (p = tcnums; p->tcaddr; p++)
			*p->tcaddr = tgetnum(p->tcname);
	}
	if (bsflag)
		BC = BCdefault;
	else if (BC == NULL && bcstr != NULL)
		BC = bcstr;
	if (CLstr == NULL)
		stop("cannot clear screen");
	if (CMstr == NULL || UP == NULL || BC == NULL)
		stop("cannot do random cursor positioning via tgoto()");
	PC = pcstr ? *pcstr : 0;
	if (sgnum >= 0 || xsflag)
		SOstr = SEstr = NULL;
#ifdef unneeded
	if (ncflag)
		CRstr = NULL;
	else if (CRstr == NULL)
		CRstr = "\r";
#endif
}

/* this foolery is needed to modify tty state `atomically' */
static jmp_buf scr_onstop;

static void
stopset(sig)
	int sig;
{
	sigset_t sigset;

	(void) signal(sig, SIG_DFL);
	(void) kill(getpid(), sig);
	sigemptyset(&sigset);
	sigaddset(&sigset, sig);
	(void) sigprocmask(SIG_UNBLOCK, &sigset, (sigset_t *)0);
	longjmp(scr_onstop, 1);
}

static void
scr_stop(sig)
	int sig;
{
	sigset_t sigset;

	scr_end();
	(void) kill(getpid(), sig);
	sigemptyset(&sigset);
	sigaddset(&sigset, sig);
	(void) sigprocmask(SIG_UNBLOCK, &sigset, (sigset_t *)0);
	scr_set();
	scr_msg(key_msg, 1);
}

/*
 * Set up screen mode.
 */
void
scr_set()
{
	struct winsize ws;
	struct termios newtt;
	sigset_t sigset, osigset;
	void (*ttou)(int);

	sigemptyset(&sigset);
	sigaddset(&sigset, SIGTSTP);
	sigaddset(&sigset, SIGTTOU);
	(void) sigprocmask(SIG_BLOCK, &sigset, &osigset);
	if ((tstp = signal(SIGTSTP, stopset)) == SIG_IGN)
		(void) signal(SIGTSTP, SIG_IGN);
	if ((ttou = signal(SIGTTOU, stopset)) == SIG_IGN)
		(void) signal(SIGTTOU, SIG_IGN);
	/*
	 * At last, we are ready to modify the tty state.  If
	 * we stop while at it, stopset() above will longjmp back
	 * to the setjmp here and we will start over.
	 */
	(void) setjmp(scr_onstop);
	(void) sigprocmask(SIG_SETMASK, &osigset, (sigset_t *)0);
	Rows = 0, Cols = 0;
	if (ioctl(0, TIOCGWINSZ, &ws) == 0) {
		Rows = ws.ws_row;
		Cols = ws.ws_col;
	}
	if (Rows == 0)
		Rows = LInum;
	if (Cols == 0)
	Cols = COnum;
	if (Rows < MINROWS || Cols < MINCOLS) {
		(void) fprintf(stderr,
		    "the screen is too small: must be at least %dx%d, ",
		    MINCOLS, MINROWS);
		stop("");	/* stop() supplies \n */
	}
	if (tcgetattr(0, &oldtt) < 0)
		stop("tcgetattr() fails");
	newtt = oldtt;
	newtt.c_lflag &= ~(ICANON|ECHO);
	newtt.c_oflag &= ~OXTABS;
	newtt.c_cc[VMIN] = 1;
	newtt.c_cc[VTIME] = 0;
	if (tcsetattr(0, TCSADRAIN, &newtt) < 0)
		stop("tcsetattr() fails");
	ospeed = cfgetospeed(&newtt);
	(void) sigprocmask(SIG_BLOCK, &sigset, &osigset);

	/*
	 * We made it.  We are now in screen mode, modulo TIstr
	 * (which we will fix immediately).
	 */
	if (TIstr)
		putstr(TIstr);	/* termcap(5) says this is not padded */
	if (tstp != SIG_IGN)
		(void) signal(SIGTSTP, scr_stop);
	if (ttou != SIG_IGN)
		(void) signal(SIGTTOU, ttou);

	isset = 1;
	(void) sigprocmask(SIG_SETMASK, &osigset, (sigset_t *)0);
	scr_clear();
}

/*
 * End screen mode.
 */
void
scr_end()
{
	sigset_t sigset, osigset;

	sigemptyset(&sigset);
	sigaddset(&sigset, SIGTSTP);
	sigaddset(&sigset, SIGTTOU);
	(void) sigprocmask(SIG_BLOCK, &sigset, &osigset);
	/* reset attributes, show cursor, clear screen */
	putstr("\033[0m\033[?25h");
	/* move cursor to last line */
	if (LLstr)
		putstr(LLstr);	/* termcap(5) says this is not padded */
	else
		moveto(Rows - 1, 0);
	/* exit screen mode */
	if (TEstr)
		putstr(TEstr);	/* termcap(5) says this is not padded */
	putstr("\033[H\033[J");
	(void) fflush(stdout);
	(void) tcsetattr(0, TCSADRAIN, &oldtt);
	isset = 0;
	/* restore signals */
	(void) signal(SIGTSTP, tstp);
	(void) sigprocmask(SIG_SETMASK, &osigset, (sigset_t *)0);
}

void
stop(why)
	const char *why;
{

	if (isset)
		scr_end();
	(void) fprintf(stderr, "aborting: %s\n", why);
	exit(1);
}

/*
 * Clear the screen, forgetting the current contents in the process.
 */
void
scr_clear()
{

	putpad(CLstr);
	putstr("\033[?25l");
	curscore = -1;
	memset((char *)curscreen, 0, sizeof(curscreen));
}

#if vax && !__GNUC__
typedef int regcell;	/* pcc is bad at `register char', etc */
#else
typedef cell regcell;
#endif

/*
 * Update the screen.
 */
void
scr_update()
{
	cell *bp, *sp;
	regcell so, cur_so = 0;
	int i, ccol, j;
	sigset_t sigset, osigset;
#ifndef AEE32
	static const struct shape *lastshape;
	static const struct shape *lasthold;
#endif

	sigemptyset(&sigset);
	sigaddset(&sigset, SIGTSTP);
	(void) sigprocmask(SIG_BLOCK, &sigset, &osigset);

	/* hide cursor to prevent flicker */
	putstr("\033[?25l");

	/* always leave cursor after last displayed point */
	curscreen[D_LAST * B_COLS - 1] = -1;

	if (score != curscore) {
#ifndef AEE32
		if (HOstr)
			putpad(HOstr);
		else
			moveto(0, 0);
		(void) printf("Score: %d", score);
#endif
		curscore = score;
	}

#ifndef AEE32
	/* draw shape buffer (hold function) */
	/* this is a copy of the next if part. I just adjusted the variables and values a bit. */
	if (usehold && (lasthold != holdshape)) {
		int i;
		static int r=12, c=2;
		int tr, tc, t; 

		lasthold = holdshape;
		
		/* clean */
		putpad(SEstr);
		moveto(r-1, c-1); putstr("          ");
		moveto(r,   c-1); putstr("          ");
		moveto(r+1, c-1); putstr("          ");
		moveto(r+2, c-1); putstr("          ");

		moveto(r-3, c-2);
		putstr("Hold Buffer:");

		/* draw */
		putpad(SOstr);
		moveto(r, 2*c);
		putstr("  ");
		for(i=0; i<3; i++) {
			t = c + r*B_COLS;
			t += holdshape->off[i];

			tr = t / B_COLS;
			tc = t % B_COLS;

			moveto(tr, 2*tc);
			putstr("  ");
		}
		putpad(SEstr);
	}

	/* draw preview of nextpattern */
	if (showpreview && (nextshape != lastshape)) {
		int i;
		static int r=5, c=2;
		int tr, tc, t; 

		lastshape = nextshape;
		
		/* clean */
		putpad(SEstr);
		moveto(r-1, c-1); putstr("          ");
		moveto(r,   c-1); putstr("          ");
		moveto(r+1, c-1); putstr("          ");
		moveto(r+2, c-1); putstr("          ");

		moveto(r-3, c-2);
		putstr("Next shape:");
						
		/* draw */
		putpad(SOstr);
		moveto(r, 2*c);
		putstr("  ");
		for(i=0; i<3; i++) {
			t = c + r*B_COLS;
			t += nextshape->off[i];

			tr = t / B_COLS;
			tc = t % B_COLS;

			moveto(tr, 2*tc);
			putstr("  ");
		}
		putpad(SEstr);
	}
#endif /* !AEE32 */
	
	bp = &board[D_FIRST * B_COLS];
	sp = &curscreen[D_FIRST * B_COLS];
	for (j = D_FIRST; j < D_LAST; j++) {
		ccol = -1;
		for (i = 0; i < B_COLS; bp++, sp++, i++) {
			if (*sp == (so = *bp))
				continue;
			*sp = so;
			if (i != ccol) {
#ifdef AEE32
				if (cur_so) {
					putstr("\033[0m");
					cur_so = 0;
				}
#else
				if (cur_so && MSflag) {
					putpad(SEstr);
					cur_so = 0;
				}
#endif
#ifdef AEE32
				moveto(RTOD(j) + 1, CTOD(i));
#else
				moveto(RTOD(j), CTOD(i));
#endif
			}
#ifdef AEE32
			/* skip wall columns entirely — we draw borders separately */
			if (so == WALL_CELL)
				goto next_cell;
			if (so != cur_so) {
				put_cell_color(so);
				cur_so = so;
			}
			if (so == 0)
				putstr("  ");
			else if (so >= 9 && so <= 15)
				putstr("▒▒");
			else
				putstr("  ");
#else
			if (SOstr) {
				if (so != cur_so) {
					putpad(so ? SOstr : SEstr);
					cur_so = so;
				}
				if (so > 1) /* if ghost tile use ghost character */
					putstr("▒▒"); /* two shaded chars (vim: i_CTRL-K ':S'. see :help digraphs) */
				else
					putstr("  ");
			} else
				putstr(so ? "XX" : "  ");
#endif
			next_cell:
			ccol = i + 1;
			/*
			 * Look ahead a bit, to avoid extra motion if
			 * we will be redrawing the cell after the next.
			 * Motion probably takes four or more characters,
			 * so we save even if we rewrite two cells
			 * `unnecessarily'.  Skip it all, though, if
			 * the next cell is a different color.
			 */
#define	STOP (B_COLS - 3)
			if (i > STOP || sp[1] != bp[1] || so != bp[1])
				continue;
			if (sp[2] != bp[2])
				sp[1] = -1;
			else if (i < STOP && so == bp[2] && sp[3] != bp[3]) {
				sp[2] = -1;
				sp[1] = -1;
			}
		}
	}
#ifdef AEE32
	if (cur_so)
		putstr("\033[0m");
	/* draw thin frame around the playing field */
	{
		int lc = CTOD(1) - 1;   /* column just left of first play col */
		int rc = CTOD(B_COLS-1); /* column just right of last play col */
		int brow = RTOD(D_LAST - 1) + 1; /* align with lowered visible board bottom */
		putstr("\033[90m");       /* dark grey */
		for (j = D_FIRST; j < D_LAST; j++) {
			moveto(RTOD(j) + 1, lc);
			putstr("│");
			moveto(RTOD(j) + 1, rc);
			putstr("│");
		}
		/* bottom border */
		moveto(brow, lc);
		putstr("└");
		{
			int k;
			for (k = lc + 1; k < rc; k++)
				putstr("─");
		}
		putstr("┘");
		putstr("\033[0m");
	}
#else
	if (cur_so)
		putpad(SEstr);
#endif
	draw_side_panel();
	(void) fflush(stdout);
	(void) sigprocmask(SIG_SETMASK, &osigset, (sigset_t *)0);
}

static void
draw_side_panel(void)
{
#ifdef AEE32
	static int panel_inited;
	static int last_score = -1;
	static const struct shape *last_next;
	static const struct shape *last_hold;
	static int last_usehold = -1;
	static int last_showpreview = -1;
	char line[80];
	int left = CTOD(B_COLS) + 3;
	int row = RTOD(D_FIRST);
	int pcol = left + 5;  /* column for shape preview blocks */

	if (left + 24 >= Cols)
		return;

	if (panel_inited &&
	    score == last_score &&
	    nextshape == last_next &&
	    holdshape == last_hold &&
	    usehold == last_usehold &&
	    showpreview == last_showpreview)
		return;

	panel_inited = 1;
	last_score = score;
	last_next = nextshape;
	last_hold = holdshape;
	last_usehold = usehold;
	last_showpreview = showpreview;

	/* ── Score ── */
	{
		char sbuf[16];
		int slen, pad;
		snprintf(sbuf, sizeof(sbuf), "%d", score);
		slen = 0;
		while (sbuf[slen]) slen++;
		pad = 10 - slen;
		if (pad < 0) pad = 0;

		putstr("\033[36;1m"); /* bold cyan */
		moveto(row++, left); putstr("┌─────────────────────┐");
		moveto(row++, left); putstr("│  \033[33;1mScore: ");
		putstr(sbuf);
		while (pad-- > 0) putchar(' ');
		putstr("\033[36;1m  │");
		moveto(row++, left); putstr("├─────────────────────┤");
	}

	/* ── Next shape ── */
	moveto(row++, left); putstr("│  \033[37;1mNext:\033[0;36;1m              │");
	/* clear preview area (4 rows) */
	moveto(row,   left); putstr("│                     │");
	moveto(row+1, left); putstr("│                     │");
	moveto(row+2, left); putstr("│                     │");
	moveto(row+3, left); putstr("│                     │");
	if (nextshape)
		draw_shape_at(nextshape, row + 1, pcol);
	putstr("\033[36;1m");
	row += 4;
	moveto(row++, left); putstr("├─────────────────────┤");

	/* ── Hold ── */
	moveto(row++, left); putstr("│  \033[37;1mHold:\033[0;36;1m              │");
	moveto(row,   left); putstr("│                     │");
	moveto(row+1, left); putstr("│                     │");
	moveto(row+2, left); putstr("│                     │");
	moveto(row+3, left); putstr("│                     │");
	if (usehold && holdshape)
		draw_shape_at(holdshape, row + 1, pcol);
	putstr("\033[36;1m");
	row += 4;
	moveto(row++, left); putstr("├─────────────────────┤");

	/* ── Controls ── */
	moveto(row++, left); putstr("│  \033[37;1mControls\033[0;36;1m           │");
	moveto(row++, left); putstr("├─────────────────────┤");

	snprintf(line, sizeof(line), "│  \033[33;1m%c\033[0;36;1m  │ left          │",
		key_bindings[0]);
	moveto(row++, left); putstr(line);
	snprintf(line, sizeof(line), "│  \033[33;1m%c\033[0;36;1m  │ rotate        │",
		key_bindings[1]);
	moveto(row++, left); putstr(line);
	snprintf(line, sizeof(line), "│  \033[33;1m%c\033[0;36;1m  │ right         │",
		key_bindings[2]);
	moveto(row++, left); putstr(line);
	if (key_bindings[3] == ' ')
		moveto(row++, left), putstr("│ \033[33;1mspc\033[0;36;1m │ drop          │");
	else {
		snprintf(line, sizeof(line), "│  \033[33;1m%c\033[0;36;1m  │ drop          │",
			key_bindings[3]);
		moveto(row++, left); putstr(line);
	}
	snprintf(line, sizeof(line), "│  \033[33;1m%c\033[0;36;1m  │ pause         │",
		key_bindings[4]);
	moveto(row++, left); putstr(line);
	snprintf(line, sizeof(line), "│  \033[33;1m%c\033[0;36;1m  │ quit          │",
		key_bindings[5]);
	moveto(row++, left); putstr(line);
	snprintf(line, sizeof(line), "│  \033[33;1m%c\033[0;36;1m  │ hold          │",
		key_bindings[6]);
	moveto(row++, left); putstr(line);

	moveto(row++, left); putstr("└─────────────────────┘");
	putstr("\033[0m");
#else
	char line[80];
	int left = CTOD(B_COLS) + 3;
	int row = 1;

	if (left + 24 >= Cols)
		return;

	putstr("\033[1m");
	moveto(row++, left); putstr("┌─────────────────────┐");
	moveto(row++, left); putstr("│      Controls       │");
	moveto(row++, left); putstr("├─────────────────────┤");
	putstr("\033[0m");

	snprintf(line, sizeof(line), "│  %c  │ left          │", key_bindings[0]);
	moveto(row++, left); putstr(line);
	snprintf(line, sizeof(line), "│  %c  │ rotate        │", key_bindings[1]);
	moveto(row++, left); putstr(line);
	snprintf(line, sizeof(line), "│  %c  │ right         │", key_bindings[2]);
	moveto(row++, left); putstr(line);
	snprintf(line, sizeof(line), "│  %c  │ drop          │", key_bindings[3]);
	moveto(row++, left); putstr(line);
	snprintf(line, sizeof(line), "│  %c  │ pause         │", key_bindings[4]);
	moveto(row++, left); putstr(line);
	snprintf(line, sizeof(line), "│  %c  │ quit          │", key_bindings[5]);
	moveto(row++, left); putstr(line);
	snprintf(line, sizeof(line), "│  %c  │ hold          │", key_bindings[6]);
	moveto(row++, left); putstr(line);

	moveto(row++, left); putstr("└─────────────────────┘");
#endif
}

/*
 * Write a message (set!=0), or clear the same message (set==0).
 * (We need its length in case we have to overwrite with blanks.)
 */
void
scr_msg(s, set)
	char *s;
	int set;
{
	if (s != NULL && strcmp(s, key_msg) == 0) {
		if (set)
			draw_side_panel();
		return;
	}
	
	if (set || CEstr == NULL) {
		int l = strlen(s);

		moveto(Rows - 2, ((Cols - l) >> 1) - 1);
		if (set)
			putstr(s);
		else
			while (--l >= 0)
				(void) putchar(' ');
	} else {
		moveto(Rows - 2, 0);
		putpad(CEstr);
	}
}
