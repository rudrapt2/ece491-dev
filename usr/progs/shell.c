#include "../syscall.h"
#include "../string.h"
#include "../shell.h"
#include "../error.h"

#define BUFSIZE 1024
#define MAXARGS 64

#define SKIP_SPACES(buf) while(*buf == ' ') buf++

void exec(int argc, char* argv[]) {
	char path[BUFSIZE];
	int fd, result;

	// If path doesn't start with '/', prepend '/c/' for relative paths
	if (strncmp(argv[0], "/", 1) != 0 && strncmp(argv[0], "c/", 2) != 0) {
		snprintf(path, sizeof(path), "/c/%s", argv[0]);
        fd = _open(-1, path);
	}
	else {
	    fd = _open(-1, argv[0]);
	}

	if (fd < 0) {
		printf("Unable to access %s (%s)\n", path, error_name(fd));
		return;
	}

	result = _exec(fd, argc, argv);
	printf("Failed to exec file (%s)", error_name(result));
}

static int handle_file_input(char* file) {
    int res;
    _close(STDIN);
    res = _open(STDIN, file);
    if (res < 0) printf("Failed to open file %s (%s)\n", file, error_name(res));
    return res;
}

static int handle_file_output(char* file) {
    int res;
    _close(STDOUT);
    res = _open(STDOUT, file);
    if (res < 0) {
        res = _fscreate(file);
        if (res < 0) {
            printf("Failed to create file %s (%s)\n", file, error_name(res));
            return res;
        }
        res = _open(STDOUT, file);
    }
    if (res < 0) printf("Failed to open file %s (%s)\n", file, error_name(res));
    return res;
}

static int handle_pipe() {
    int wpipe = -1;
    int rpipe = -1;
    int res = _pipe(&wpipe, &rpipe);

    if (res < 0) {
        printf("Failed to create pipe (%s)\n", error_name(res));
        return res;
    }

    res = _fork();

    if (res < 0) {
        printf("Failed to fork (%s)\n", error_name(res));
        return res;
    }

    if (res) { // writer
        _close(rpipe);
        _close(STDOUT);
        _uiodup(wpipe, STDOUT);
        _close(wpipe);
    }
    else { // reader
        _close(wpipe);
        _close(STDIN);
        _uiodup(rpipe, STDIN);
        _close(rpipe);
    }

    return res;
}

static int is_terminator(char c) {
    switch (c) {
        case ' ':
        case '\0':
        case FIN:
        case FOUT:
        case PIPE:
            return 1;
        default:
            return 0;
    }
}

static char find_terminator(char* head, char** end) {
    *end = head;
    while (!is_terminator(**end)) (*end)++;
    return **end;
}

void parse_and_exec(char* head) {
    int argc;
    char* argv[MAXARGS + 1]; // +1 for NULL termination
    char* end;
    char term;
    int res;

    SKIP_SPACES(head);

    // handle args
    for (argc = 0; argc < MAXARGS;) {
        term = find_terminator(head, &end);
        *end = '\0';
        
        if (head != end)
            argv[argc++] = head;
        
        if (term != ' ') break;

        end++;
        SKIP_SPACES(end);
        head = end;
    }

    if (argc == 0) return; // nothing to do

	// Null-terminate the argument array
	argv[argc] = NULL;

    // at this point, anything remaining should be redirection
    while (term != '\0') {
        head = end + 1;
        SKIP_SPACES(head);
        switch (term) {
            case FIN:
                term = find_terminator(head, &end);
                *end = '\0';
                res = handle_file_input(head);
                if (res < 0) return;
                break;
            case FOUT:
                term = find_terminator(head, &end);
                *end = '\0';
                res = handle_file_output(head);
                if (res < 0) return;
                break;
            case PIPE:
                res = handle_pipe();
                if (res < 0) return;
                if (res)
                    exec(argc, argv); // writer
                else 
                    parse_and_exec(head); // reader
                return;
        }

        if (term == ' ') {
            end++;
            SKIP_SPACES(end);
            term = *end;
        }
    }

    exec(argc, argv);
}

void main()
{
    char buf[BUFSIZE];
    int child;

    /**************************************************/
    // NOTE: REMOVE FROM STUDENT RELEASE
    // this makes waiting for all children easier.
    // if we dont do this we have to deal with children
    // spawned by the main thread (ie cache thrfn)
    // and would need to keep count of children spawned.
    child = _fork();
    if (child) {
        _wait(child);
        return;
    }
    /**************************************************/

    buf[BUFSIZE-1] = '\0'; // terminate

	_open(CONSOLEOUT, "/dev/uart1");    // console device
	_close(STDIN);              	    // close any existing stdin
	_uiodup(CONSOLEOUT, STDIN);         // stdin from console
	_close(STDOUT);                     // close any existing stdout
	_uiodup(CONSOLEOUT, STDOUT);        // stdout to console

	printf("Starting 391 Shell\n");

	for (;;) {
		printf("LUMON OS> ");
		getsn(buf, BUFSIZE - 1);

		if (0 == strcmp(buf, "exit"))
			return;

		child = _fork();
		if (child) {
			// Parent process: wait for all children
			while (_wait(0) > 0);
		}
		else {
			// Child process: parse and execute
            parse_and_exec(buf);
            _exit();
		}
	}
}