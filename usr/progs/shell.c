#include "../syscall.h"
#include "../string.h"
#include "../shell.h"

#define BUFSIZE 1024
#define MAXARGS 8

void exec(int c, char** v) {
	char path[256];
	int fd, result;

	// Null-terminate the argument array
	v[c] = NULL;

	// If path doesn't start with '/', prepend '/c/' for relative paths
	if (strncmp(v[0], "/", 1) != 0 && strncmp(v[0], "c/", 2) != 0) {
		snprintf(path, sizeof(path), "/c/%s", v[0]);
	}
	else {
		strncpy(path, v[0], sizeof(path) - 1);
	}

	// Open the executable file
	fd = _open(-1, path);

	if (fd < 0) {
		printf("Unable to access %s (Error Code: %d)\n", path, fd);
		_exit();
	}

	result = _exec(fd, c, v);
	printf("Failed to exec file (Error Code: %d)", result);
	_exit();
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
					_close(STDOUT);
					if (parse_and_open(STDOUT, &buf, 1) < 0) return -1; 
					continue;
					
				case FIN:
					_close(STDIN);
					if (parse_and_open(STDIN, &buf, 0) < 0) return -1;
					continue;

				case PIPE:
					wpipe = -1;
					rpipe = -1;
					if (_pipe(&wpipe, &rpipe) < 0) {
						printf("failed to create pipe\n");
						return -1;
					}
					if (_fork()) { // reader
						_close(STDIN);
						_close(wpipe);
						_uiodup(rpipe, STDIN);
						if (rpipe != STDIN)
							_close(rpipe);
						c = 0;
						buf++;
						break;
					}
					else { // writer
						_close(STDOUT);
						_close(rpipe);
						_uiodup(wpipe, STDOUT);
						if (wpipe != STDOUT)
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

void main()
{
    char buf[BUFSIZE];
	int c;
	char* v[MAXARGS + 1]; 
	int child;

	_open(CONSOLEOUT, "/dev/uart1");	// console device
	_close(STDIN);              		// close any existing stdin
	_uiodup(CONSOLEOUT, STDIN);       // stdin from console
	_close(STDOUT);              		// close any existing stdout
	_uiodup(CONSOLEOUT, STDOUT);      // stdout to console

	printf("Starting 391 Shell\n");

	for (;;) {
		printf("LUMON OS> ");
		getsn(buf, BUFSIZE - 1);

		if (0 == strcmp(buf, "exit"))
			return;

		child = _fork();
		if (child) {
			// Parent process: just wait for child
			_wait(child);
		}
		else {
			// Child process: parse and execute
			c = parse(buf, v);
			if (c <= 0)
				return;
			exec(c, v);
		}
	}
}