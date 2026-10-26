#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/jiffies.h>

/*
 * NOTE: this uses the legacy integer GPIO interface. On older RPi kernels the
 * GPIO number equals the BCM number. On newer kernels (6.6+) the gpiochip has
 * a base offset (e.g. 512), so BCM4 may appear as 516. Check with:
 *     sudo cat /sys/kernel/debug/gpio
 * (look at the "gpiochip0: GPIOs <first>-<last>" line) and set GPIO_BASE
 * below to <first> if it is not 0.
 */
#define GPIO_BASE 0

static unsigned int ledGreen   = GPIO_BASE + 4;   /* GPIO4  (header pin 7)  - Green LED */
static unsigned int pushButton = GPIO_BASE + 11;  /* GPIO11 (header pin 23) - push button */
static unsigned int irqNumber;                    /* share IRQ num within file */
static bool ledOn = 0;                            /* used to toggle state of LED */

/* The GPIO IRQ handler function */
static irqreturn_t rpi_gpio_isr(int irq, void *dev_id)
{
    static unsigned long last_jiffies = 0;

    /* software debounce: ignore edges within 200 ms of the last accepted one */
    if (time_before(jiffies, last_jiffies + msecs_to_jiffies(200)))
        return IRQ_HANDLED;
    last_jiffies = jiffies;

    ledOn = !ledOn;                    /* toggle the LED state */
    gpio_set_value(ledGreen, ledOn);   /* set LED accordingly */
    printk(KERN_ALERT "GPIO Interrupt!\n");  /* announce IRQ handled */
    return IRQ_HANDLED;
}

/* The LKM exit function */
static void __exit rpi_gpio_exit(void)
{
    free_irq(irqNumber, NULL);         /* free the IRQ number, no *dev_id */
    gpio_set_value(ledGreen, 0);       /* turn the LED off */
    gpio_free(ledGreen);               /* free the LED GPIO */
    gpio_free(pushButton);             /* free the Button GPIO */
    printk(KERN_ALERT "Goodbye from the GPIO LKM!\n");
}

/* The LKM initialization function */
static int __init rpi_gpio_init(void)
{
    int result = 0;

    printk(KERN_ALERT "Initializing the GPIO LKM\n");

    if (!gpio_is_valid(ledGreen) || !gpio_is_valid(pushButton)) {
        printk(KERN_ALERT "Invalid GPIO number\n");
        return -ENODEV;
    }

    ledOn = true;                              /* default for LED is ON */
    result = gpio_request(ledGreen, "sysfs");  /* request for LED GPIO */
    if (result) return result;
    gpio_direction_output(ledGreen, ledOn);    /* output mode, LED on */

    result = gpio_request(pushButton, "sysfs");/* request for push button GPIO */
    if (result) { gpio_free(ledGreen); return result; }
    gpio_direction_input(pushButton);          /* set up as input */
    /* gpio_set_debounce() is gone from newer kernels; debounce in the ISR instead */

    irqNumber = gpio_to_irq(pushButton);       /* map pushbutton to IRQ number */
    printk(KERN_ALERT "Button mapped to IRQ: %d\n", irqNumber);

    /* Request an interrupt line */
    result = request_irq(irqNumber,            /* interrupt number requested */
                         rpi_gpio_isr,         /* isr handler function */
                         IRQF_TRIGGER_RISING,  /* trigger on rising edge */
                         "rpi_gpio_handler",   /* used in /proc/interrupts */
                         NULL);                /* *dev_id for shared lines - NULL */
    if (result) {
        gpio_free(pushButton);
        gpio_free(ledGreen);
    }
    return result;
}

module_init(rpi_gpio_init);
module_exit(rpi_gpio_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("SC3103");
MODULE_DESCRIPTION("GPIO LKM: push button IRQ toggles green LED");
MODULE_VERSION("V1");
