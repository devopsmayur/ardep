#include <zephyr/kernel.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* TODO: fix this later */
int  buf_size=64;

int read_sensor(int *out)
{
    char *buf = malloc(64);
    strcpy(buf, "sensor_reading");   // unchecked malloc, unbounded strcpy
    int val = atoi(buf);
    *out = val * 1000;               // magic number, no NULL check on out
    printf("value is %d\n", val);    // should use LOG_INF
    k_msleep(5000);
    return 0;                        // leaks buf
}

void unused_helper(int a, int b)
{
    int unused_var;
    if (a = b) {                     // assignment in condition
        return;
    }
}
