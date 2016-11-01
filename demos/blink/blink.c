#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include "../util/util.h"
#include "stdint.h"

int
main(void)
{
	clock_setup();

	rcc_periph_clock_enable(RCC_GPIOB);
	gpio_mode_setup(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO0 | GPIO1);

	gpio_set(GPIOB, GPIO1);
	while (1) {
		gpio_toggle(GPIOB, GPIO0|GPIO1);
		msleep(250);
	}
}
