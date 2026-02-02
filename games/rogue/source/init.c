/*	$NetBSD: init.c,v 1.4 1995/04/28 23:49:19 mycroft Exp $	*/

/*
 * Copyright (c) 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Timothy C. Stoehr.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 */

#ifndef lint
#if 0
static char sccsid[] = "@(#)init.c	8.1 (Berkeley) 5/31/93";
#else
static char rcsid[] = "$NetBSD: init.c,v 1.4 1995/04/28 23:49:19 mycroft Exp $";
#endif
#endif /* not lint */

/*
 * init.c
 *
 * This source herein may be modified and/or distributed by anybody who
 * so desires, with the following restrictions:
 *    1.)  No portion of this notice shall be removed.
 *    2.)  Credit shall not be taken for the creation of this source.
 *    3.)  This code is not to be traded, sold, or used for personal
 *         gain or profit.
 *
 */

#include "glue.h"
#include "rogue.h"

char login_name[MAX_OPT_LEN];
char *nick_name = (char *) 0;
char *rest_file = 0;
boolean cant_int = 0;
boolean did_int = 0;
boolean score_only;
boolean init_curses = 0;
boolean save_is_interactive = 1;
boolean ask_quit = 1;
boolean no_skull = 0;
boolean passgo = 0;
char *error_file = "rogue.esave";
char *byebye_string = "Okay, bye bye!";

extern char *fruit;
extern char *save_file;
extern short party_room;
extern boolean jump;
extern int exited;

init(argc, argv)
int argc;
char *argv[];
{
	printf("Press any key ... ");
	getchar();

	do_args(argc, argv);
	do_opts();

	initscr();
	if ((LINES < DROWS) || (COLS < DCOLS)) {
		clean_up("must be played on 24 x 80 screen");
		return;
	}
	start_window();
	init_curses = 1;

	if (score_only) {
		put_scores((object *) 0, 0);
	}
	(void) srrandom(random_seed());
	
	if (rest_file) {
		restore(rest_file);
		return(1);
	}
	mix_colors();
	get_wand_and_ring_materials();
	make_scroll_titles();

	level_objects.next_object = (object *) 0;
	level_monsters.next_monster = (object *) 0;
	player_init();
	ring_stats(0);
	return(0);
}

player_init()
{
	object *obj;

	rogue.pack.next_object = (object *) 0;

	obj = alloc_object();
	get_food(obj, 1);
	(void) add_to_pack(obj, &rogue.pack, 1);

	obj = alloc_object();		/* initial armor */
	obj->what_is = ARMOR;
	obj->which_kind = RINGMAIL;
	obj->class = RINGMAIL+2;
	obj->is_protected = 0;
	obj->d_enchant = 1;
	(void) add_to_pack(obj, &rogue.pack, 1);
	do_wear(obj);

	obj = alloc_object();		/* initial weapons */
	obj->what_is = WEAPON;
	obj->which_kind = MACE;
	obj->damage = "2d3";
	obj->hit_enchant = obj->d_enchant = 1;
	obj->identified = 1;
	(void) add_to_pack(obj, &rogue.pack, 1);
	do_wield(obj);

	obj = alloc_object();
	obj->what_is = WEAPON;
	obj->which_kind = BOW;
	obj->damage = "1d2";
	obj->hit_enchant = 1;
	obj->d_enchant = 0;
	obj->identified = 1;
	(void) add_to_pack(obj, &rogue.pack, 1);

	obj = alloc_object();
	obj->what_is = WEAPON;
	obj->which_kind = ARROW;
	obj->quantity = get_rand(25, 35);
	obj->damage = "1d2";
	obj->hit_enchant = 0;
	obj->d_enchant = 0;
	obj->identified = 1;
	(void) add_to_pack(obj, &rogue.pack, 1);
}

clean_up(estr)
char *estr;
{
	if (save_is_interactive) {
		if (init_curses) {
			move(DROWS-1, 0);
			refresh();
			stop_window();
		}
		printf("\n%s\n", estr);
	}
	exited = 1;
	exit(); // prevent ice monster from keeping us locked after dying
//	md_exit(0);
}

start_window()
{
	crmode();
	noecho();
#ifndef BAD_NONL
	nonl();
#endif
}

stop_window()
{
	endwin();
}

void
byebye()
{
//	md_ignore_signals();
	if (ask_quit) {
		quit(1);
	} else {
		clean_up(byebye_string);
	}
//	md_heed_signals();
}

void
onintr()
{
//	md_ignore_signals();
	if (cant_int) {
		did_int = 1;
	} else {
		check_message();
		message("interrupt", 1);
	}
//	md_heed_signals();
}

void
error_save()
{
	save_is_interactive = 0;
	save_into_file(error_file);
	clean_up("");
}

do_args(argc, argv)
int argc;
char *argv[];
{
	short i, j;

	for (i = 1; i < argc; i++) {
		if (argv[i][0] == '-') {
			for (j = 1; argv[i][j]; j++) {
				switch(argv[i][j]) {
				case 's':
					score_only = 1;
					break;
				}
			}
		} else {
			rest_file = argv[i];
		}
	}
}

do_opts()
{
	init_str(&nick_name, "391er");
	init_str(&save_file, "rogue.sv");
	init_str(&fruit, "apple");
}

init_str(str, dflt)
char **str, *dflt;
{
	static char strheap[100][MAX_OPT_LEN+2];
	static int pos = 0;

	if (!(*str)) {
		*str = strheap[pos++];
		strncpy(*str, dflt, MAX_OPT_LEN+1);
	}
}
