/*
 * gpio.c - User space GPIO access on Raspberry Pi (Pibrella board)
 *   Green LED  : GPIO4
 *   Yellow LED : GPIO17
 *   Red LED    : GPIO27
 *   Push button: GPIO11 (reads 1 when pressed)
 *
 * Green/Yellow blink alternately every second.
 * Red LED blinks while the push button is held down.
 *
 * Build: gcc -o gpio gpio.c      Run: sudo ./gpio  (or as a user in 'gpio' group)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/gpio.h>

#define TICK_US      100000   /* poll the button every 100 ms            */
#define TICKS_PER_S  10       /* 10 ticks = 1 s -> GY toggle period      */
#define RUN_SECONDS  20       /* total run time                          */

int main(int argc, char *argv[])
{
    int fd0 = open("/dev/gpiochip0", O_RDWR);
    if (fd0 < 0) { perror("open /dev/gpiochip0"); exit(EXIT_FAILURE); }

    struct gpiochip_info cinfo;
    if (ioctl(fd0, GPIO_GET_CHIPINFO_IOCTL, &cinfo) < 0) {
        perror("GPIO_GET_CHIPINFO_IOCTL"); exit(EXIT_FAILURE);
    }
    fprintf(stdout, "GPIO chip 0: %s, \"%s\", %u lines\n",
            cinfo.name, cinfo.label, cinfo.lines);

    /* ---- Green + Yellow LEDs (outputs) ---- */
    struct gpiohandle_request req_GY;
    struct gpiohandle_data    data_GY;
    memset(&req_GY, 0, sizeof(req_GY));
    memset(&data_GY, 0, sizeof(data_GY));
    req_GY.lines          = 2;
    req_GY.lineoffsets[0] = 4;     /* Green  */
    req_GY.lineoffsets[1] = 17;    /* Yellow */
    req_GY.flags          = GPIOHANDLE_REQUEST_OUTPUT;
    strcpy(req_GY.consumer_label, "sc3103_gy");
    data_GY.values[0] = 1;         /* Green ON  */
    data_GY.values[1] = 0;         /* Yellow OFF */
    req_GY.default_values[0] = 1;
    req_GY.default_values[1] = 0;
    if (ioctl(fd0, GPIO_GET_LINEHANDLE_IOCTL, &req_GY) < 0) {
        perror("request GY lines"); exit(EXIT_FAILURE);
    }

    /* ---- Red LED (output) ---- */
    struct gpiohandle_request req_R;
    struct gpiohandle_data    data_R;
    memset(&req_R, 0, sizeof(req_R));
    memset(&data_R, 0, sizeof(data_R));
    req_R.lines          = 1;
    req_R.lineoffsets[0] = 27;
    req_R.flags          = GPIOHANDLE_REQUEST_OUTPUT;
    strcpy(req_R.consumer_label, "sc3103_red");
    if (ioctl(fd0, GPIO_GET_LINEHANDLE_IOCTL, &req_R) < 0) {
        perror("request Red line"); exit(EXIT_FAILURE);
    }

    /* ---- Push button (input) ---- */
    struct gpiohandle_request req_B;
    struct gpiohandle_data    data_B;
    memset(&req_B, 0, sizeof(req_B));
    memset(&data_B, 0, sizeof(data_B));
    req_B.lines          = 1;
    req_B.lineoffsets[0] = 11;
    req_B.flags          = GPIOHANDLE_REQUEST_INPUT;
    strcpy(req_B.consumer_label, "sc3103_button");
    if (ioctl(fd0, GPIO_GET_LINEHANDLE_IOCTL, &req_B) < 0) {
        perror("request Button line"); exit(EXIT_FAILURE);
    }

    for (int tick = 0; tick < RUN_SECONDS * TICKS_PER_S; ++tick) {
        /* Every second: output GY state, then toggle for next round */
        if (tick % TICKS_PER_S == 0) {
            ioctl(req_GY.fd, GPIOHANDLE_SET_LINE_VALUES_IOCTL, &data_GY);
            data_GY.values[0] = !data_GY.values[0];
            data_GY.values[1] = !data_GY.values[1];
        }

        /* Every tick: poll the button */
        ioctl(req_B.fd, GPIOHANDLE_GET_LINE_VALUES_IOCTL, &data_B);
        if (data_B.values[0])
            data_R.values[0] = !data_R.values[0];   /* blink while pressed */
        else
            data_R.values[0] = 0;                   /* off when released   */
        ioctl(req_R.fd, GPIOHANDLE_SET_LINE_VALUES_IOCTL, &data_R);

        usleep(TICK_US);
    }

    /* turn everything off, release lines */
    data_GY.values[0] = data_GY.values[1] = 0;
    ioctl(req_GY.fd, GPIOHANDLE_SET_LINE_VALUES_IOCTL, &data_GY);
    data_R.values[0] = 0;
    ioctl(req_R.fd, GPIOHANDLE_SET_LINE_VALUES_IOCTL, &data_R);

    close(req_GY.fd);
    close(req_R.fd);
    close(req_B.fd);
    close(fd0);
    exit(EXIT_SUCCESS);
}
