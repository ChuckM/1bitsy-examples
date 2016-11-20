/*
 * clock.c -- clock tree setup code and systick init
 *
 */

/*
 * Now this is just the clock setup code from systick-blink as it is the
 * transferrable part.
 */

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/flash.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/cm3/systick.h>

/* Common function descriptions */
#include "../util/util.h"

void term_draw_cursor(int);

static void
null_func(void)
{ 
	return;
};

static void (*clk_hook)(void);
static volatile int hook_interval;
static volatile int hook_count;
static volatile int mini_count;

/* milliseconds since boot */
static volatile uint32_t system_millis;
/* millisecond count down when sleeping */
static volatile uint32_t system_delay;

/* Called when systick fires */
void sys_tick_handler(void)
{
	mini_count++;
	mini_count &= 0x3;
	if (mini_count == 0) {
		system_millis++;
		if (system_delay != 0) {
			system_delay--;
		}
	}
	/* process the systick hook function if set */
	if (hook_interval != 0) {
		if (hook_count <= 0) {
			clk_hook();
			hook_count = hook_interval - 1;
		} else {
			hook_count--;
		}
	}

}

/* simple sleep for delay milliseconds */
void msleep(uint32_t delay)
{
	system_delay = delay;
	while (system_delay > 0);
}

/* Getter function for the current time */
uint32_t mtime(void)
{
	return system_millis;
}

/*
 * Set a hook function to be called every 'interval' clock
 * ticks. If interval is 0 then stop calling the hook function.
 */
void
set_clock_hook(void (*hook_function)(void), int interval) {
	clk_hook = hook_function;
	hook_interval = interval;
	hook_count = interval - 1;
}

/*
 * clock_setup(void)
 *
 * This function sets up both the base board clock rate
 * and a 1khz "system tick" count. The SYSTICK counter is
 * a standard feature of the Cortex-M series.
 */
void clock_setup(void)
{

	rcc_clock_setup_hse_3v3(&rcc_hse_25mhz_3v3[RCC_CLOCK_3V3_168MHZ]);

	set_clock_hook(null_func, 0);
	/* clock rate / 168000 to get 1mS interrupt rate */
	systick_set_reload(168000/4);
	systick_set_clocksource(STK_CSR_CLKSOURCE_AHB);
	systick_counter_enable();

	/* this done last */
	systick_interrupt_enable();
}
