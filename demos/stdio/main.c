#include <stdio.h>
#include <stdlib.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include "../util/util.h"


int
main(void)
{
	int cnt;

	cnt = 0;
	printf("Ok this should come out as expected\n");
	rcc_periph_clock_enable(RCC_GPIOB);
	gpio_mode_setup(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO0 | GPIO1 | GPIO10);
	gpio_set(GPIOB, GPIO10 | GPIO0);
	while(1) 
	{
		gpio_toggle(GPIOB, GPIO0 | GPIO1 | GPIO10);
		msleep(200);
		printf("Count: %d\n", cnt++);
	}
}
