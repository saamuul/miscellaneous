/*
 * gpio.c - User space GPIO access on Raspberry Pi (Pibrella board)
 *   Green LED  : GPIO4
 *   Yellow LED : GPIO17
 *   Red LED    : GPIO27
 *   Push button: GPIO11 (reads 1 when pressed)
 *
 * Phase 1 (5 s) : Green and Yellow LEDs blink alternately, 1 s each.
 * Phase 2 (10 s): each press of the push button toggles the Red LED.
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

#define BLINK_SECONDS   5        /* phase 1 duration                       */
#define BUTTON_SECONDS  10       /* phase 2 duration                       */
#define POLL_US         20000    /* poll button every 20 ms (also debounces) */

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
    data_GY.values[0] = 1;         /* Green ON   */
    data_GY.values[1] = 0;         /* Yellow OFF */
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

    /* ---- Phase 1: blink Green/Yellow for 5 seconds ---- */
    printf("Phase 1: Green/Yellow blinking for %d s\n", BLINK_SECONDS);
    for (int i = 0; i < BLINK_SECONDS; ++i) {
        ioctl(req_GY.fd, GPIOHANDLE_SET_LINE_VALUES_IOCTL, &data_GY);
        usleep(1000000);                          /* 1 second */
        data_GY.values[0] = !data_GY.values[0];   /* toggle */
        data_GY.values[1] = !data_GY.values[1];
    }
    data_GY.values[0] = data_GY.values[1] = 0;    /* both off */
    ioctl(req_GY.fd, GPIOHANDLE_SET_LINE_VALUES_IOCTL, &data_GY);

    /* ---- Phase 2: button toggles Red LED for 10 seconds ---- */
    printf("Phase 2: press the button to toggle Red LED (%d s)\n", BUTTON_SECONDS);
    int prev = 0;                                 /* previous button state */
    long polls = (long)BUTTON_SECONDS * 1000000L / POLL_US;
    for (long i = 0; i < polls; ++i) {
        ioctl(req_B.fd, GPIOHANDLE_GET_LINE_VALUES_IOCTL, &data_B);
        if (data_B.values[0] && !prev) {          /* rising edge = new press */
            data_R.values[0] = !data_R.values[0];
            ioctl(req_R.fd, GPIOHANDLE_SET_LINE_VALUES_IOCTL, &data_R);
            printf("Button pressed: Red LED %s\n", data_R.values[0] ? "ON" : "OFF");
        }
        prev = data_B.values[0];
        usleep(POLL_US);
    }

    /* turn Red off, release lines */
    data_R.values[0] = 0;
    ioctl(req_R.fd, GPIOHANDLE_SET_LINE_VALUES_IOCTL, &data_R);

    close(req_GY.fd);
    close(req_R.fd);
    close(req_B.fd);
    close(fd0);
    exit(EXIT_SUCCESS);
}
