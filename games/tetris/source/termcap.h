#ifndef _TERMCAP_H_
#define _TERMCAP_H_

#ifdef __cplusplus
extern "C" {
#endif

extern char PC;
extern char *BC;
extern char *UP;

int tgetent(char *bp, const char *name);
int tgetflag(const char *id);
int tgetnum(const char *id);
char *tgetstr(const char *id, char **area);
char *tgoto(const char *cm, int destcol, int destline);
int tputs(const char *str, int affcnt, int (*putc_fn)(int));

#ifdef __cplusplus
}
#endif

#endif
