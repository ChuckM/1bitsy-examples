#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include "../util/util.h"
#include "stdint.h"

int
main(void)
{
	clock_setup();

	/* 1Bitsy hooks the LED up to PORT A, Bit 8 */
	rcc_periph_clock_enable(RCC_GPIOA);
	gpio_mode_setup(GPIOA, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO8);

	while (1) {
		gpio_toggle(GPIOA, GPIO8);
/*
		for (i = 0; i < 1000000; i++) {
			__NOP();
		}
*/

		msleep(250); 
	}
}
