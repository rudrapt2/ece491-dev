#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/select.h>

#include "rbuf.h"

#define CTLCH0 0xC0 // Select channel 0
#define CTLCH1 0xC1 // Select channel 1
#define CTLCH2 0xC2 // Select channel 2
#define CTLCH3 0xC3 // Select channel 3
#define CTLESC 0xCE // Escape

#define ISCTLCHn(c) (((c) & 0xfc) == 0xc0)

#define RXBUFSZ 64 // must be 64

int ufd;
FILE * ufp;
pthread_t rthr;
pthread_mutex_t wlock;

struct channel {
    int fd;
    FILE * fp;
    struct rbuf rxbuf;
    char buf[RXBUFSZ];
    pthread_mutex_t rlock;
    pthread_cond_t rxbupdt;
    pthread_t rthr;
    pthread_t wthr;
    int ovf;
};

static struct channel ch[4];

void * chan_reader (void * arg) {
    struct channel * const chan = arg;
    int const chno = chan - ch;
    int c;

    for (;;) {

        c = getc(ch[chno].fp);

        if (c == EOF) {
            if (errno == EAGAIN) {
                // Non-blocking fd, no data available rn; use select to wait
                // until fd is readable
                fd_set rfds;
                FD_ZERO(&rfds);
                FD_SET(ch[chno].fd, &rfds);
                select(ch[chno].fd + 1, &rfds, NULL, NULL, NULL);
                clearerr(ch[chno].fp);  // clear the error so we can use getc
                continue;
            } else {
                perror("chan_reader - getc error");
                exit(1);
            }
        }
        
        // Acquire wlock before writing to fd bc mult channel threads
        // could be writing
        pthread_mutex_lock(&wlock);

        // Prepend with channel ctrl char
        putc(CTLCH0 + chno, ufp);

        // If data stream contains a ctrl char, prepend with escape char
        if(ISCTLCHn(c) || c == CTLESC) {
            putc(CTLESC, ufp);
        }
        putc(c, ufp);

        pthread_mutex_unlock(&wlock);
    }
}

void * chan_writer(void * arg) {
    struct channel * const chan = arg;
    int const chno = chan - ch;
    int byte;
    int result;

    for (;;) {
    
        pthread_mutex_lock(&ch[chno].rlock);

        // Block if rxbuf is currently empty
        while(rbuf_empty(&ch[chno].rxbuf)){
            pthread_cond_wait(&ch[chno].rxbupdt, &ch[chno].rlock);
        }

        while (!rbuf_empty(&ch[chno].rxbuf)) {
            byte = rbuf_getc(&ch[chno].rxbuf);
            
            // Release rlock so uart_reader can continue writing to rxbuf
            pthread_mutex_unlock(&ch[chno].rlock);
            
            result = putc(byte, ch[chno].fp);
            if(result == EOF) {
                if(errno == EAGAIN) {
                    fd_set wfds;
                    FD_ZERO(&wfds);
                    FD_SET(ch[chno].fd, &wfds);
                    select(ch[chno].fd + 1, NULL, &wfds, NULL, NULL);
                    clearerr(ch[chno].fp);
                    pthread_mutex_lock(&ch[chno].rlock);
                    continue;
                } else {
                    perror("chan_writer - putc error");
                    exit(1);
                }
            }

            // Acquire rlock again before checking rbuf_empty
            pthread_mutex_lock(&ch[chno].rlock);
        }
        pthread_mutex_unlock(&ch[chno].rlock);
    }
}

void * uart_reader(void * arg) {
    int chno = -1;
    int esc = 0;
    int byte;
    
    for (;;) {

        byte = getc(ufp);

        if(byte == EOF){
            if (errno == EAGAIN) {
                fd_set rfds;
                FD_ZERO(&rfds);
                FD_SET(ufd, &rfds);
                select(ufd + 1, &rfds, NULL, NULL, NULL);
                clearerr(ufp);
                continue;
            } else {
                perror("uart_reader - getc error");
                exit(1);
            }
        }

        // If it's an escape character then we skip it
        if(!esc && (ISCTLCHn(byte) || byte == CTLESC)) {
            if(ISCTLCHn(byte)) {
                chno = byte & 3; // Get lower two bits - chno
            } else {
                esc = 1;
            }
            continue;
        }
        esc = 0;

        if (chno < 0) {
            continue;   // If we haven't selected any channel yet then just wait
        }

        // Write data to appropriate channel
        pthread_mutex_lock(&ch[chno].rlock);
        if(!rbuf_full(&ch[chno].rxbuf)){
            rbuf_putc(&ch[chno].rxbuf, (char)byte);
        } else {
            ch[chno].ovf = 1;
        }
        pthread_cond_signal(&ch[chno].rxbupdt);
        pthread_mutex_unlock(&ch[chno].rlock);      
    }
}

static int open_pts(void) {
    int result;

    int fd = posix_openpt(O_RDWR | O_NOCTTY);
    
    result = grantpt(fd);
    if(result < 0) {
        perror("open_pts - grantpt");
        exit(1);
    }

    result = unlockpt(fd);
    if(result < 0){
        perror("open_pts - unlockpt");
        exit(1);
    }

    int flags = fcntl(fd, F_GETFL);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK); // make fd non-blocking
    printf("pts: %s\n", ptsname(fd));
    return fd;
}

int main(int argc, char * const argv[]) {
    int result;

    pthread_mutex_init(&wlock, NULL);

    ufd = open("/dev/ttyUSB0", O_RDWR|O_NOCTTY|O_NONBLOCK);

    // Make the UART fd a file with read+write perms
    ufp = fdopen(ufd, "r+");
    setvbuf(ufp, NULL, _IONBF, 0); // unbuffered - flush on each write
    
    for (int i = 0; i < 4; i++) {
        ch[i].fd = open_pts(); // make them non-blocking
        rbuf_init(&ch[i].rxbuf, ch[i].buf, RXBUFSZ);
        ch[i].ovf = 0;

        // each channel fd gets treated as unbuffered FILE ptr
        ch[i].fp = fdopen(ch[i].fd, "r+");
        setvbuf(ch[i].fp, NULL, _IONBF, 0);

        pthread_mutex_init(&ch[i].rlock, NULL);
        pthread_cond_init(&ch[i].rxbupdt, NULL);

        // Create channel reader/writer threads for each channel
        result = pthread_create(&ch[i].rthr, NULL, &chan_reader, ch+i);
        if (result < 0) {
            perror("main - pthread_create chan_reader");
            exit(1);
        }
        
        result = pthread_create(&ch[i].wthr, NULL, &chan_writer, ch+i);
        if (result < 0) {
            perror("main - pthread_create chan_writer");
            exit(1);
        }
    }

    // Create one UART reader thread
    result = pthread_create(&rthr, NULL, &uart_reader, NULL);
    if (result < 0) {
        perror("main - pthread_create uart_reader");
        exit(1);
    }

    pthread_exit(NULL);
}