#include "conf.h"
#include "intr.h"
#include "error.h"
#include "timer.h"
#include "cache.h"
#include "device.h"
#include "thread.h"
#include "memory.h"
#include "string.h"
#include "console.h"
#include "process.h"
#include "dev/rtc.h"
#include "filesys.h"
#include "dev/uart.h"
#include "dev/virtio.h"
#include "testsuite_1.h"
#include "dev/ramdisk.h"
#include "heap.h"
#include "string.h"

// Add args, structs, includes, defines
struct colors {
    uint8_t blue;
    uint8_t green;
    uint8_t red;
    uint8_t x;
};

// Make whatever tests you want.
int test_viogpu_open() {
    int result;
    struct uio * gpu_uio;
    result = open_file("dev", "viogpu0", &gpu_uio);
    return result;
}

#define SLEEP 1000000
// Make whatever tests you want.
int test_viogpu_draw_multi() {
    int result;
    struct uio * gpu_uio;
    result = open_file("dev", "viogpu0", &gpu_uio);

    void * buf;
    result = uio_cntl(gpu_uio, 4, (void**)&buf);

    struct colors * fbuf = (struct colors *) buf;
    kprintf("Frame buffer pointer: %p\n", buf);
    if (result < 0)
        return result;  
    
    for (int j = 0; j < 10; j++) {
        for (int i = 0; i < 640 * 480; i++) {
            fbuf[i].red = 0xFF;
            fbuf[i].green = 0x00;
            fbuf[i].blue = 0x00;
            fbuf[i].x = 0x00;
        }
        uio_write(gpu_uio, NULL, 0); // flush
        kprintf("Drew red to screen, sleeping for %d useconds\n", SLEEP);
        sleep_us(SLEEP);

        for (int i = 0; i < 640 * 480; i++) {
            fbuf[i].red = 0x00;
            fbuf[i].green = 0xFF;
            fbuf[i].blue = 0x00;
            fbuf[i].x = 0x00;
        }
        uio_write(gpu_uio, NULL, 0); // flush
        kprintf("Drew green to screen, sleeping for %d useconds\n", SLEEP);
        sleep_us(SLEEP);

        for (int i = 0; i < 640 * 480; i++) {
            fbuf[i].red = 0x00;
            fbuf[i].green = 0x00;
            fbuf[i].blue = 0xFF;
            fbuf[i].x = 0x00;
        }
        uio_write(gpu_uio, NULL, 0); // flush
        kprintf("Drew blue to screen, sleeping for %d useconds\n", SLEEP);
        sleep_us(SLEEP);
    }
        

    return 0;
}

int test_open_2_viohi() {
    int result;
    struct uio * viohi_uio_0;
    struct uio * viohi_uio_1;
    result = open_file("dev", "viohi0", &viohi_uio_0);
    if (result != 0) {
        return result;
    }
    result = open_file("dev", "viohi1", &viohi_uio_1);
    return result;
}

void viohi_thread_func(void * arg, int instno) {
    kprintf("Starting viohi%d thread\n", instno);
    struct uio * viohi_uio = (struct uio *) arg;
    char buf[128];

    while (1) {
        int result = uio_read(viohi_uio, buf, 128);
        if (result < 0) {
            kprintf("Error reading from viohi%d device: %d\n", instno, result);
            continue;
        }
        buf[127] = '\0';
        kprintf("Viohi%d thread read: %s\n",instno, buf);
    }
}

int test_read_2_viohi() {
    int result;
    struct uio * viohi_uio_0;
    struct uio * viohi_uio_1;
    result = open_file("dev", "viohi0", &viohi_uio_0);
    if (result != 0) {
        return result;
    }
    result = open_file("dev", "viohi1", &viohi_uio_1);
    if (result != 0) {
        return result;
    }

    int tid1 = spawn_thread("viohi0_thread", (void (*) (void)) viohi_thread_func, viohi_uio_0, 0);
    int tid2 = spawn_thread("viohi1_thread", (void (*) (void)) viohi_thread_func, viohi_uio_1, 1);

    thread_join(tid2);

    return result;
}

int test_ktfs() {
    char* buffer = kmalloc(14);
    int result;
    struct uio* ktfs_file_uio;
    struct vioblk* vioblk;
    //* First Critera: the file has to be created

    for (int i = 0; i < 150; i++) {
        snprintf(buffer, 14, "file_%d", i);
        result = create_file("c", buffer);
        if(result < 0) {
            kprintf("Failed to create the %dth file!\n", i + 1);
            return -1; 
        }

        kprintf("Succesfully created %dth file!\n", i + 1);
        fsmgr_flushall();
    }

    //* Second Criteria: the file should be able to open

    for (int i = 0; i < 150; i++) {
        snprintf(buffer, 14, "file_%d", i);
        result = open_file("c", buffer, &ktfs_file_uio);
        if(result < 0) {
            kprintf("Fail to open %dth created file\n", i + 1);
            return -1; 
        }
        kprintf("Successfully opened %dth created file\n", i + 1);
    }

    kfree(buffer);
    return 0;
}

int test_lffs() {
    char buffer[] = "Hello I like Yummy Yummy in my Tummy\n\nGoody be";
    char * ptr = buffer;
    char c;
    struct uio * lffs_file;
    open_file("c", "small", &lffs_file);
    while (*ptr != '\0') {
        uio_read(lffs_file, &c, 1);
        if (*ptr != c)
            return -1;
        ptr++;
    }
    return 0;
}

void run_testsuite_1() {
    int retval = -EINVAL;
    char * test_output;

    // retval = test_viogpu_draw_multi();
    // test_output = (retval == 0) ? "test_viogpu_draw_multi passed!" : "test_viogpu_draw_multi failed!"; 
    // kprintf("%s\n", test_output);

    // retval = test_open_2_viohi();
    // test_output = (retval == 0) ? "test_open_2_viohi passed!" : "test_open_2_viohi failed!"; 
    // kprintf("%s\n", test_output);

    // retval = test_ktfs();
    // test_output = (retval == 0) ? "test_ktfs passed!" : "test_ktfs failed!"; 
    // kprintf("%s\n", test_output);

    retval = test_lffs();
    test_output = (retval == 0) ? "test_lffs passed!" : "test_lffs failed!"; 
    kprintf("%s\n", test_output);
}