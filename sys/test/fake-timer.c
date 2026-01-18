// fake-timer.c - A timer subsystem that says all the right things
//
// Copyright (c) 2026 University of Illinois
// SPDX-License-identifier: NCSA
//

#include "timer.h"
#include "string.h"

char timer_initialized = 0;
unsigned int timer_frequency;

void timer_init(unsigned int freq) {
    timer_frequency = freq;
    timer_initialized = 1;
}

void alarm_init(struct alarm * al, const char * name) {
    memset(al, 0, sizeof(*al));
    al->name = "fakealarm";
}

const char * alarm_name(const struct alarm * al) {
    return "fakealarm";
}

void alarm_sleep_until(struct alarm * al, unsigned long long twake) {
    return;
}

void alarm_sleep_sec(struct alarm * al, unsigned int sec) {
    return;
}

void alarm_sleep_ms(struct alarm * al, unsigned int ms) {
    return;
}

void alarm_sleep_us(struct alarm * al, unsigned int us) {
    return;
}

void sleep_sec(unsigned int sec) {
    return;
}

void sleep_ms(unsigned int ms) {
    return;
}

void sleep_us(unsigned int us) {
    return;
}