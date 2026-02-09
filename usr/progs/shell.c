#include "../syscall.h"
#include "../string.h"
#include "../shell.h"
#include "../error.h"

#define BUFSIZE 256
#define MAXARGS 64

#define SKIP_SPACES(buf) while(*buf == ' ') buf++

void exec(int argc, char* argv[]) {
	char full_path[BUFSIZE];
	int fd, result;
    char * path = argv[0];

	// If path doesn't contain '/', prepend '/c/' for relative paths
	if (strchr(argv[0], '/') == NULL) {
		snprintf(full_path, sizeof(full_path), "/c/%s", argv[0]);
        path = full_path;
	}

    fd = _open(-1, path);

	if (fd < 0) {
		printf("Unable to access %s (%s)\n", path, error_desc(fd));
		return;
	}

	result = _exec(fd, argc, argv);
	printf("Failed to exec %s (%s)", path, error_desc(result));
}

static int handle_file_input(char* file) {
    int res;
    _close(STDIN);
    res = _open(STDIN, file);
    if (res < 0) printf("Failed to open file %s (%s)\n", file, error_desc(res));
    return res;
}

static int handle_file_output(char* file) {
    int res;
    _close(STDOUT);
    res = _open(STDOUT, file);
    if (res < 0) {
        res = _create(file);
        if (res < 0) {
            printf("Failed to create file %s (%s)\n", file, error_desc(res));
            return res;
        }
        res = _open(STDOUT, file);
    }
    if (res < 0) printf("Failed to open file %s (%s)\n", file, error_desc(res));
    return res;
}

static int handle_pipe() {
    int wtid;
    int wpipe = -1;
    int rpipe = -1;
    int res = _pipe(&wpipe, &rpipe);

    if (res < 0) {
        printf("Failed to create pipe (%s)\n", error_desc(res));
        return res;
    }

    res = _fork();

    if (res < 0) {
        printf("Failed to fork (%s)\n", error_desc(res));
        return res;
    }

    if (res) { // writer
        _close(rpipe);

        // to make sure everything exits properly, we need a 
        // waiter proc to ensure the main proc sleeps until
        // all child threads are done executing.
        wtid = _fork();
        if (wtid > 0) {
            _close(wpipe);
            _wait(wtid);
            _wait(res);
            _exit();
        }

        _close(STDOUT);
        _iodup(wpipe, STDOUT);
        _close(wpipe);
    }
    else { // reader
        _close(wpipe);
        _close(STDIN);
        _iodup(rpipe, STDIN);
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

    buf[BUFSIZE-1] = '\0'; // terminate

	_open(CONSOLEOUT, "/dev/uart1");    // console device
	_close(STDIN);              	    // close any existing stdin
	_iodup(CONSOLEOUT, STDIN);          // stdin from console
	_close(STDOUT);                     // close any existing stdout
	_iodup(CONSOLEOUT, STDOUT);         // stdout to console

	printf("Starting 391 Shell\n");

	for (;;) {
		printf("LUMON OS> ");
		getsn(buf, BUFSIZE - 1);

		if (0 == strcmp(buf, "exit"))
			return;

		child = _fork();
		if (child) {
			// Parent process: wait for all children
			_wait(child);
		}
		else {
			// Child process: parse and execute
            parse_and_exec(buf);
            _exit();
		}
	}
}