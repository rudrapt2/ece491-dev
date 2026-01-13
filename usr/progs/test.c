#include "../syscall.h"
#include "../string.h"
#include "../error.h"
#include "../shell.h"
#include "../heap.h"

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
        result = _fscreate(path);
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
        else {
            dprintf(STDOUT, "SUCCESS: wrote to %s\n", path);
        }
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
        result = _fsdelete(path);
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

struct testcase {
    const char * name;
    void (*main)(int argc, char * argv[]);
};

const struct testcase testcases[] = {
    {.name="malloc",    .main=test_malloc},
    {.name="create",    .main=test_create},
    {.name="write",     .main=test_write},
    {.name="read",      .main=test_read},
    {.name="delete",    .main=test_delete},
    {.name="write_long",.main=test_write_long},
    {.name="read_long", .main=test_read_long},
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