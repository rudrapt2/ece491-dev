#include "../syscall.h"
#include "../string.h"
#include "../error.h"
#include "../shell.h"
#include "../heap.h"
#include "../io.h"

#define FBUFSZ 256

unsigned long long rand_state;

// helper functions
static unsigned long long rand(void) {
    rand_state *= 25214903917UL;
    rand_state += 11;
    return (rand_state >> 12);
}

static size_t fprintf(int fd, const char * fmt, ...) {
    va_list ap;
    char buf[FBUFSZ];
    va_start(ap, fmt);
    size_t n = vsnprintf(buf, FBUFSZ, fmt, ap);
    _print(buf);
    va_end(ap);
    return n;
}

// test cases

void test_malloc(int argc, char * argv[]) {
    char* arr;
    int dev_fd;
    int result;
    unsigned long N;
    unsigned long iters;

    if (argc < 2) {
        printf("USAGE: %s [N] [iters=1]\n", argv[0]);
        return;
    }

    N = strtoul(argv[1], NULL, 10);
    iters = (argc > 2) ? strtoul(argv[2], NULL, 10) : 1;

    arr = malloc((N + 1) * sizeof(char));
    dev_fd = _open(-1, "/dev/viorng0");

    for (int iterations = 0; iterations < iters; iterations++) {
        for (int i = 0; i < N; i++) {
            result = _read(dev_fd, &arr[i], sizeof(char));
            if (result < 0) {
                printf("Could not read from /dev/viorng0: %s\n", error_name(result));
                return;
            }
            while (arr[i] < (char)32 || arr[i] > (char)127) {
                result = _read(dev_fd, &arr[i], sizeof(char));
                if (result < 0) {
                    printf("Could not read from /dev/viorng0: %s\n", error_name(result));
                    return;
                }
            }
        }

        arr[N] = '\0';

        dprintf(STDOUT, "(iteration %d) Random string is:\n%s\n(%d chars)\n", iterations+1, arr, strlen(arr));
    }
}

void test_create(int argc, char * argv[]) {
    int num_files;
    char path[26];
    int result;
    char* mp = "/c/";

    if (argc < 2) {
        printf("USAGE: %s [NUM_FILES] [MOUNTPOINT (c)]\n", argv[0]);
        return;
    }

    num_files = strtoul(argv[1], NULL, 10);
    if (argc >= 3)
        mp = argv[2];

    // testing create
    for (int i = 1; i <= num_files; i++) {
        snprintf(path, 26, "%s%d", mp, i);
        result = _create(path);
        if (result < 0) {
            printf("Failed to create %s: %s\n", path, error_name(result));
            return;
        }
        else {
            dprintf(STDOUT, "SUCCESS: created %s\n", path);
        }
    }
}

void test_write(int argc, char * argv[]) {
    int num_files;
    char path[26];
    int result,fd;
    size_t written;
    char* mp = "/c/";

    if (argc < 2) {
        printf("USAGE: %s [NUM_FILES] [MOUNTPOINT (c)]\n", argv[0]);
        return;
    }

    num_files = strtoul(argv[1], NULL, 10);
    if (argc >= 3)
        mp = argv[2];

    // testing write
    for (int i = 1; i <= num_files; i++) {
        written = snprintf(path, 26, "%s%d", mp, i);
        fd = _open(-1, path);
        if (fd < 0) {
            printf("Failed to open %s: %s\n", path, error_name(fd));
            return;
        }
        result = _write(fd, path, written);
        if (result < 0) {
            printf("Failed to write to %s: %s\n", path, error_name(result));
            return;
        }
        if (result == 0) {
            printf("Failed to write to %s: %s\n", path, "Wrote 0 bytes");
            return;
        }
        if (result < written) {
            printf("WARNING: wrote %d bytes to %s instead of %d\n", 
                result, path, written);
        }
        dprintf(STDOUT, "SUCCESS: wrote to %s\n", path);
        _close(fd);
    }
}

void test_read(int argc, char * argv[]) {
    int num_files;
    char path[26];
    char buffer[26];
    int result,fd;
    size_t written;
    char* mp = "/c/";

    if (argc < 2) {
        printf("USAGE: %s [NUM_FILES] [MOUNTPOINT (c)]\n", argv[0]);
        return;
    }

    num_files = strtoul(argv[1], NULL, 10);
    if (argc >= 3)
        mp = argv[2];

    // testing read
    for (int i = 1; i <= num_files; i++) {
        written = snprintf(path, 26, "%s%d", mp, i);
        fd = _open(-1, path);
        if (fd < 0) {
            printf("Failed to open %s: %s\n", path, error_name(fd));
            return;
        }
        result = _read(fd, buffer, 26);
        if (result <= 0) {
            printf("Failed to read from %s: %s\n", path, error_name(result));
            return;
        }
        if (strncmp(path, buffer, written) != 0) {
            printf("FAIL: Expected %s but got %s\n", path, buffer);
            return;
        }
        dprintf(STDOUT, "SUCCESS: read from %s\n", path);
        _close(fd);
    }
}

void test_delete(int argc, char * argv[]) {
    int num_files;
    char path[26];
    int result;
    char* mp = "/c/";

    if (argc < 2) {
        printf("USAGE: %s [NUM_FILES] [MOUNTPOINT (c)]\n", argv[0]);
        return;
    }

    num_files = strtoul(argv[1], NULL, 10);
    if (argc >= 3)
        mp = argv[2];

    // testing delete
    for (int i = 1; i <= num_files; i++) {
        snprintf(path, 26, "%s%d", mp, i);
        result = _delete(path);
        if (result < 0) {
            printf("Failed to delete %s: %s\n", path, error_name(result));
            return;
        }
        else {
            dprintf(STDOUT, "SUCCESS: deleted %s\n", path);
        }
    }
}

void test_write_long(int argc, char * argv[]) {
    int num_files;
    int num_writes;
    char path[26];
    int result,fd;
    size_t written;
    char* mp = "/c/";

    if (argc < 3) {
        printf("USAGE: %s [NUM_FILES] [NUM_WRITES] [MOUNTPOINT (c)]\n", argv[0]);
        return;
    }

    num_files = strtoul(argv[1], NULL, 10);
    num_writes = strtoul(argv[2], NULL, 10);
    if (argc >= 4)
        mp = argv[3];

    // testing write
    for (int i = 1; i <= num_files; i++) {
        written = snprintf(path, 26, "%s%d", mp, i);
        fd = _open(-1, path);
        if (fd < 0) {
            printf("Failed to open %s: %s\n", path, error_name(fd));
            return;
        }
        for (int j = 0; j < num_writes; j++) {
            result = _write(fd, path, written);
            if (result < 0) {
                printf("Failed to write to %s on iteration %d: %s\n", path, j, error_name(result));
                return;
            }
            if (result == 0) {
                printf("Failed to write to %s on iteration %d: %s\n", path, j, "Wrote 0 bytes");
                return;
            }
            if (result < written) {
                printf("WARNING: wrote %d bytes to %s instead of %d\n", 
                    result, path, written);
            }
        }
        dprintf(STDOUT, "SUCCESS: wrote long to %s\n", path);
        _close(fd);
    }
}

void test_read_long(int argc, char * argv[]) {
    int num_files;
    int num_reads;
    char path[26];
    char buffer[26];
    int result,fd;
    size_t written;
    char* mp = "/c/";

    if (argc < 3) {
        printf("USAGE: %s [NUM_FILES] [NUM_READS] [MOUNTPOINT (c)]\n", argv[0]);
        return;
    }

    num_files = strtoul(argv[1], NULL, 10);
    num_reads = strtoul(argv[2], NULL, 10);
    if (argc >= 4)
        mp = argv[3];

    for (int i = 1; i <= num_files; i++) {
        written = snprintf(path, 26, "%s%d", mp, i);
        fd = _open(-1, path);
        if (fd < 0) {
            printf("Failed to open %s: %s\n", path, error_name(fd));
            return;
        }
        for (int j = 0; j < num_reads; j++) {
            result = _read(fd, buffer, written);
            if (result <= 0) {
                printf("Failed to read from %s on iteration %d: %s\n", path, j, error_name(result));
                return;
            }
            if (strncmp(path, buffer, written) != 0) {
                printf("FAIL: Expected %s but got %s on iteration %d\n", path, buffer, j);
                return;
            }
        }
        dprintf(STDOUT, "SUCCESS: read long from %s\n", path);
        _close(fd);
    }
}

void test_set_end(int argc, char * argv[]) {
    int num_files;
    char path[26];
    int result,fd;
    unsigned long end = 0;
    char* mp = "/c/";

    if (argc < 2) {
        printf("USAGE: %s [NUM_FILES] [MOUNTPOINT (c)]\n", argv[0]);
        return;
    }

    num_files = strtoul(argv[1], NULL, 10);
    if (argc >= 3)
        mp = argv[2];

    // testing set end
    for (int i = 1; i <= num_files; i++) {
        snprintf(path, 26, "%s%d", mp, i);
        fd = _open(-1, path);
        if (fd < 0) {
            printf("Failed to open %s: %s\n", path, error_name(fd));
            return;
        }
        result = _ioctl(fd, IOC_SETEND, &end);
        if (result < 0) {
            printf("Failed to set end of %s: %s\n", path, error_name(result));
            return;
        }
        else {
            dprintf(STDOUT, "SUCCESS: set end of %s\n", path);
        }
        _close(fd);
    }
}

void test_write_multi(int argc, char * argv[]) {
    int num_iters, write_size, num_uios;
    char path[26];
    int * fds;
    int result;
    char* mp = "/c/";
    char* buffer;
    char c = 0;
    int fd_idx = 0;

    if (argc < 4) {
        printf("USAGE: %s [NUM_ITERS] [NUM_UIOS] [WRITE_SIZE] [MOUNTPOINT (c)]\n", argv[0]);
        return;
    }

    num_iters = strtoul(argv[1], NULL, 10);
    num_uios = strtoul(argv[2], NULL, 10);
    write_size = strtoul(argv[3], NULL, 10);

    if (argc >= 5)
        mp = argv[4];

    snprintf(path, 26, "%stestfile", mp);
    _delete(path);
    result = _create(path);
    if (result < 0) {
        printf("Failed to create %s (%s)\n", path, error_name(result));
        return;
    }

    fds = calloc(num_uios, sizeof(int));

    fds[0] = _open(-1, path);
    if (fds[0] < 0) {
        printf("Failed to open %s (%s)\n", path, error_name(fds[0]));
        return;        
    }

    for (int i = 1; i < num_uios; i++) {
        fds[i] = _iodup(fds[0], -1);
        if (fds[0] < 0) {
            printf("Failed dup %d (%s)\n", i, error_name(fds[i]));
            return;        
        }
    }

    buffer = calloc(1, write_size);

    for (int i = 0; i < num_iters; i++) {
        for (int j = 0; j < write_size; j++)
            buffer[j] = c++;
        result = _write(fds[fd_idx], buffer, write_size);

        if (result < 0) {
            printf("Failed write at iteration %d, fd=%d (%s)\n", i, fd_idx, error_name(result));
            return;
        }

        if (result < write_size) {
            printf("Failed full write at iteration %d, fd=%d (wrote %d bytes)\n", i, fd_idx, result);
            return;
        }

        fd_idx = (fd_idx+1) % num_uios;
    }

    dprintf(STDOUT, "SUCCESS: wrote %d bytes with %d uios\n", num_iters*write_size, num_uios);
}

void test_read_multi(int argc, char * argv[]) {
    int num_iters, read_size, num_uios;
    char path[26];
    int * fds;
    int result;
    char* mp = "/c/";
    char* buffer;
    char c = 0;
    int fd_idx = 0;

    if (argc < 4) {
        printf("USAGE: %s [NUM_ITERS] [NUM_UIOS] [READ_SIZE] [MOUNTPOINT (c)]\n", argv[0]);
        return;
    }

    num_iters = strtoul(argv[1], NULL, 10);
    num_uios = strtoul(argv[2], NULL, 10);
    read_size = strtoul(argv[3], NULL, 10);

    if (argc >= 5)
        mp = argv[4];

    snprintf(path, 26, "%stestfile", mp);

    fds = calloc(num_uios, sizeof(int));

    fds[0] = _open(-1, path);
    if (fds[0] < 0) {
        printf("Failed to open %s (%s)\n", path, error_name(fds[0]));
        return;        
    }

    for (int i = 1; i < num_uios; i++) {
        fds[i] = _iodup(fds[0], -1);
        if (fds[0] < 0) {
            printf("Failed dup %d (%s)\n", i, error_name(fds[i]));
            return;        
        }
    }

    buffer = calloc(1, read_size);

    for (int i = 0; i < num_iters; i++) {
        result = _read(fds[fd_idx], buffer, read_size);

        if (result < 0) {
            printf("Failed read at iteration %d, fd=%d (%s)\n", i, fd_idx, error_name(result));
            return;
        }

        if (result < read_size) {
            printf("Failed full read at iteration %d, fd=%d (read %d bytes)\n", i, fd_idx, result);
            return;
        }

        for (int j = 0; j < read_size; j++) {
            if (c++ != buffer[j]) {
                printf("Read incorrect value at iteration %d\n", i);
                return;
            }
        }
        fd_idx = (fd_idx+1) % num_uios;
    }

    dprintf(STDOUT, "SUCCESS: read %d bytes with %d uios\n", num_iters*read_size, num_uios);
}

void test_race(int argc, char * argv[]) {
    int num_iters, num_forks;
    int fd, result;
    char path[26];
    char* mp = "/c/";
    int proc_idx = 0;
    char c;
    unsigned long long pos;

    if (argc < 3) {
        fprintf(CONSOLEOUT, "USAGE: %s [NUM_FORKS] [NUM_ITERS] [MOUNTPOINT (c)]\r\n", argv[0]);
        return;
    }

    num_forks = strtoul(argv[1], NULL, 10);
    num_iters = strtoul(argv[2], NULL, 10);

    if (argc >= 4)
        mp = argv[3];

    snprintf(path, 26, "%stestfile", mp);
    _delete(path);
    result = _create(path);
    if (result < 0) {
        fprintf(CONSOLEOUT, "Failed to create %s (%s)\r\n", path, error_name(result));
        return;
    }

    for (int i = 0; i < num_forks; i++) {
        proc_idx *= 2;
        if (_fork() > 0) proc_idx++;
    }

    rand_state = proc_idx;

    fd = _open(-1, path);
    if (fd < 0) {
        fprintf(CONSOLEOUT, "proc%d failed to open %s (%s)\r\n", proc_idx, path, error_name(fd));
        return;
    }

    fprintf(STDOUT, "proc%d initialized\r\n", proc_idx);

    for (int i = 0; i < num_iters; i++) {
        switch (rand() % 4) {
            case 0: // read
                result = _read(fd, &c, sizeof(c));
                if (result < 0) 
                    fprintf(CONSOLEOUT, "proc%d failed to read (%s)\r\n", proc_idx, error_name(result));
                if (result == 0)
                    fprintf(CONSOLEOUT, "proc%d read 0 bytes\r\n", proc_idx);
                else
                    fprintf(CONSOLEOUT, "proc%d read %c\r\n", proc_idx, c);
                continue;
            case 1: // write
                c = '0' + proc_idx;
                result = _write(fd, &c, sizeof(c));
                if (result < 0) 
                    fprintf(CONSOLEOUT, "proc%d failed to write (%s)\r\n", proc_idx, error_name(result));
                if (result == 0)
                    fprintf(CONSOLEOUT, "proc%d wrote 0 bytes\r\n", proc_idx);
                else
                    fprintf(CONSOLEOUT, "proc%d wrote %c\r\n", proc_idx, c);
                continue;
            case 2: // setpos
                _ioctl(fd, IOC_GETEND, &pos);
                pos = rand() % pos;
                result = _ioctl(fd, IOC_SETPOS, &pos);
                if (result < 0)
                    fprintf(CONSOLEOUT, "proc%d failed to setpos to %llu (%s)\r\n", proc_idx, pos, error_name(result));
                else
                    fprintf(CONSOLEOUT, "proc%d setpos to %llu\r\n", proc_idx, pos);
                continue;
            case 3: // setend
                _ioctl(fd, IOC_GETEND, &pos);
                pos = rand() % (pos + 4096);
                result = _ioctl(fd, IOC_SETEND, &pos);
                if (result < 0)
                    fprintf(CONSOLEOUT, "proc%d failed to setend to %llu (%s)\r\n", proc_idx, pos, error_name(result));
                else
                    fprintf(CONSOLEOUT, "proc%d setend to %llu\r\n", proc_idx, pos);
                continue;
        }
    }

    _close(fd);
    result = _delete(path);
    if (result < 0) 
        fprintf(CONSOLEOUT, "proc%d failed to delete %s (%s)\r\n", proc_idx, path, error_name(result));
    else
        fprintf(CONSOLEOUT, "proc%d deleted %s\r\n", proc_idx, path);
}

void test_fs_race(int argc, char * argv[]) {
    int num_iters, num_forks;
    int ls, result;
    char path[26];
    char* mp = "/c/";
    int proc_idx = 0;

    if (argc < 3) {
        fprintf(CONSOLEOUT, "USAGE: %s [NUM_FORKS] [NUM_ITERS] [MOUNTPOINT (c)]\r\n", argv[0]);
        return;
    }

    num_forks = strtoul(argv[1], NULL, 10);
    num_iters = strtoul(argv[2], NULL, 10);

    if (argc >= 4)
        mp = argv[3];

    snprintf(path, 26, "%stestfile", mp);
    _delete(path);
    result = _create(path);
    if (result < 0) {
        fprintf(CONSOLEOUT, "Failed to create %s (%s)\r\n", path, error_name(result));
        return;
    }

    for (int i = 0; i < num_forks; i++) {
        proc_idx *= 2;
        if (_fork() > 0) proc_idx++;
    }

    ls = _open(-1, mp);

    rand_state = proc_idx;
    fprintf(STDOUT, "proc%d initialized\r\n", proc_idx);

    for (int i = 0; i < num_iters; i++) {
        switch (rand() % 3) {
            case 0: // create
                result = _create(path);
                if (result < 0) 
                    fprintf(CONSOLEOUT, "[Iteration %d] proc%d failed to create %s (%s)\r\n", i, proc_idx, path, error_name(result));
                else
                    fprintf(CONSOLEOUT, "[Iteration %d] proc%d created %s\r\n", i, proc_idx, path);
                continue;
            case 1: // delete
                result = _delete(path);
                if (result < 0) 
                    fprintf(CONSOLEOUT, "[Iteration %d] proc%d failed to delete %s (%s)\r\n", i, proc_idx, path, error_name(result));
                else
                    fprintf(CONSOLEOUT, "[Iteration %d] proc%d deleted %s\r\n", i, proc_idx, path);
                continue;
            case 2: // ls increment
                result = _read(ls, path + strlen(mp), 26 - strlen(mp));
                if (result == 0) {
                    fprintf(CONSOLEOUT, "[Iteration %d] proc%d reached end of listing, restarting...\r\n", i, proc_idx);
                    _close(ls);
                    ls = _open(-1, mp);
                    result = _read(ls, path + strlen(mp), 26 - strlen(mp));
                }
                if (result == 0) {
                    fprintf(CONSOLEOUT, "[Iteration %d] FAIL: proc%d read 0 from listing twice\r\n", i, proc_idx);
                }
                if (result < 0)
                    fprintf(CONSOLEOUT, "[Iteration %d] proc%d failed to read from listing (%s)\r\n", i, proc_idx, error_name(result));
                else
                    fprintf(CONSOLEOUT, "[Iteration %d] proc%d read %s from listing\r\n", i, proc_idx, path);
                continue;
        }
    }

    _close(ls);
    fprintf(CONSOLEOUT, "proc%d finished\r\n", proc_idx);
}

void test_race_evil(int argc, char * argv[]) {
    if (_fork()) 
        return test_race(argc, argv);
    
    return test_fs_race(argc, argv);
}

struct testcase {
    const char * name;
    void (*main)(int argc, char * argv[]);
};

const struct testcase testcases[] = {
    {.name="malloc",        .main=&test_malloc},
    {.name="create",        .main=&test_create},
    {.name="write",         .main=&test_write},
    {.name="read",          .main=&test_read},
    {.name="delete",        .main=&test_delete},
    {.name="write_long",    .main=&test_write_long},
    {.name="read_long",     .main=&test_read_long},
    {.name="set_end",       .main=&test_set_end},
    {.name="write_multi",   .main=&test_write_multi},
    {.name="read_multi",    .main=&test_read_multi},
    {.name="race",          .main=&test_race},
    {.name="fs_race",       .main=&test_fs_race},
    {.name="race_evil",     .main=&test_race_evil},
};

void main (int argc, char* argv[]) {
    if (argc < 2) {
        printf("USAGE: %s [TEST CASE]\n", argv[0]);
        return;
    }

    for(int i = 0; i < sizeof(testcases) / sizeof(testcases[0]); i++) {
        if (strcmp(testcases[i].name, argv[1]) == 0) {
            return testcases[i].main(--argc, ++argv);
        }
    }

    printf("Could not find test case: %s\n", argv[1]);
}