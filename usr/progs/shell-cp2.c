#include "syscall.h"
#include "string.h"

#define BUFSIZE 1024
#define MAXARGS 8

int count(char* buf, char** v) {
	int c = 0;

	while (1) {
		while (*buf == ' ') buf++;
		v[c++] = buf;
		if (c > MAXARGS) return MAXARGS;
		buf = strchr(buf, ' ');
		if (buf != NULL) *(buf++) = '\0';
		else return (v[c-1][0] == '\0' ? c-1 : c); // remove terminating char
	}
}

int main ()
{
    int fd;
    char buf[BUFSIZE];
	int c;
	char* v[MAXARGS + 1]; 
	_devopen(2, "uart", 1); // console
    printf ("Starting 391 Shell\n");

    while (1) {
		printf ("LUMON OS> ");
		getsn (buf, BUFSIZE-1);
		if (0 == strcmp (buf, "exit"))
			return 0;

		c = count(buf, v);
		v[c] = NULL;
		
		if (v[0] == NULL)
			continue;

		fd = _fsopen (-1, v[0]);
		if (fd < 0) {
			printf ("%s not recognized\n");
			continue;
		}
		_exec (fd, c-1, v+1);
		_close (fd);
	}
}