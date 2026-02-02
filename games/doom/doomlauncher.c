#include "usr/heap.h"
#include "usr/viohi.h"
#include "usr/string.h"
#include "usr/syscall.h"

static void input_handler();
static void alarm_handler();
int input_pipe_in = -1;
int input_pipe_out = -1;
int alarm_pipe_in = -1;
int alarm_pipe_out = -1;

void main(int argc, char **argv) {

    int result;

    argc += 2; // passing in the pipes
    char* argv2[argc];

    _open(2, "dev/uart1");

    result = _pipe(&input_pipe_in, &input_pipe_out);
    if (result < 0) {
        printf("failed to create input pipe");
        _exit();
    }

    result = _pipe(&alarm_pipe_in, &alarm_pipe_out);
    if (result < 0) {
        printf("failed to create alarm pipe");
        _exit();
    }

    result = _open(-1, "c/doom");
    if (result < 0) {
        printf("Failed to open doom");
        _exit();
    }

    argv2[0] = malloc(2);
    argv2[1] = malloc(2);

    snprintf(argv2[0], 2, "%c", input_pipe_out);
    snprintf(argv2[1], 2, "%c", alarm_pipe_in);

    printf("argv2[0] is %d and argv2[1] is %d\n", argv2[0][0], argv2[1][0]);

    for (int i = 2; i < argc; i++)
        argv2[i] = argv[i - 2];

    if (_fork() == 0) input_handler();

    if (_fork() == 0) alarm_handler();

    _close(input_pipe_in);
    _close(alarm_pipe_out);

    _exec(result, argc, argv2);
}

static void alarm_handler() {
    int result;
    uint32_t us;
    struct viohi_event evt;
    evt.type = 0;

    _close(input_pipe_out);
    _close(alarm_pipe_in);

    while (_read(alarm_pipe_out, &us, sizeof(uint32_t)) > 0) {
        if (us > 0)
            _usleep(us);
        result = _write(input_pipe_in, &evt, sizeof(evt));
        if (result <= 0) break; // broken pipe 
    }
    _exit();
}

static void input_handler() {
    _close(input_pipe_out);
    _close(alarm_pipe_in);
    _close(alarm_pipe_out);

    int input_fd;
    int result;

    if (_fork())
        input_fd = _open(-1, "dev/viohi0");
    else
        input_fd = _open(-1, "dev/viohi1");

    if (input_fd < 0) {
        printf("failed to open viohi\n");
        _exit();
    }

    struct viohi_event evt;
    evt.type = 0;
    while (1) {
        result = _read(input_fd, &evt, sizeof(evt));

        if (result == 0)
            continue; // read returned 0? Should never happen

        if (evt.type == EV_REL || evt.type == EV_KEY) {
            result = _write(input_pipe_in, &evt, sizeof(evt));
            if (result <= 0) 
                _exit(); // broken pipe
        }
    }
}