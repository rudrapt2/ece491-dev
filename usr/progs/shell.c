#include "syscall.h"
#include "string.h"

#define BUFSIZE 1024
#define MAXARGS 8
#define FIN '<'
#define FOUT '>'
#define PIPE '|'

void exec(int c, char** v) {
	int fd = _open(-1, v[0]);
	if(fd < 0) {
		printf("Unable to access %s (Error Code: %d)\n", v[0], fd);
		_exit();
	}
	_exec(fd, c, v);
}

char* find_terminator(char* buf) {
	char* p = buf;
	while(*p) {
		switch(*p) {
			case ' ':
			case '\0':
			case FIN:
			case FOUT:
			case PIPE:
				return p;
			default:
				p++;
				break;
		}
	}
	return p;
}

int parse_and_open(int fd, char** filename, int create) {
	int result;
	char temp;
	char* start = *filename + 1;
	char* end;

	while(*start == ' ') start++;
	end = find_terminator(start);
	temp = *end;
	*end = '\0';
	if (create) _fscreate(start);
	result = _open(fd, start);
	if (result < 0) {
		printf("Could not open file: %s\n", start);
		return result;
	}
	*end = temp;
	*filename = end;
	return 0;
}

int parse(char* buf, char** v) {
	int c = 0;
	int stdin = 0;
	int stdout = 1;
	char temp;
	int wpipe, rpipe;

	while(1) {
		while(*buf == ' ') buf++;
		v[c++] = buf;
		buf = find_terminator(buf);
		for(;;) {
			temp = *buf;
			*buf = '\0';
			switch(temp) {
				case '\0':
					return (v[c-1][0] == '\0' ? c-1 : c); // remove terminating char

				case FOUT:
					_close(stdout);
					if (parse_and_open(stdout, &buf, 1) < 0) return -1; 
					continue;
					
				case FIN:
					_close(stdin);
					if (parse_and_open(stdin, &buf, 0) < 0) return -1;
					continue;

				case PIPE:
					wpipe = -1;
					rpipe = -1;
					if (_pipe(&wpipe, &rpipe) < 0) {
						printf("failed to create pipe\n");
						return -1;
					}
					if (_fork()) { // reader
						_close(stdin);
						_close(wpipe);
						_iodup(rpipe, stdin);
						if (rpipe != stdin)
							_close(rpipe);
						c = 0;
						buf++;
						break;
					}
					else { // writer
						_close(stdout);
						_close(rpipe);
						_iodup(wpipe, stdout);
						if (wpipe != stdout)
							_close(wpipe);
						exec(c, v);
					}

				case ' ':
					while(*(++buf) == ' ') ;
					continue;

				default:
					*buf = temp;
					break;
			}
			break;
		}
	}
}

int main()
{
    char buf[BUFSIZE];
	int c;
	char* v[MAXARGS + 1]; 
	int child;

	_open(2, "/dev/uart1"); // this shouldnt be here
    printf("Starting 391 Shell\n");

    for(;;) {
		printf("LUMON OS> ");
		getsn(buf, BUFSIZE-1);

		if(0 == strcmp(buf, "exit"))
			_exit();

		_iodup(2, 1); // stdout defaults to console

		c = parse(buf, v);
		if (c <= 0) continue;

		child = _fork();
		if(child) 
			_wait(child);
		else {
			exec(c, v);
		}

		_close(0);
		_close(1);
	}
}