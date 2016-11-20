/*
 * Lets see if we can drive the 64 x 32 LED panel
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <gfx.h>
#include <qrencode.h>
#include "time.h"
#include "../util/util.h"


/* ### prototypes ### */
void gpio_init(void);
void set_row(int row);
void clock_two_rows(uint8_t *q1, uint8_t *q2, uint8_t *q3, uint8_t *q4);
void next_pair(void);
void draw_pixel(int x, int y, uint16_t color);
void draw_clock(uint32_t time);

/*
 * ### Signals matching ###

	PB0	- G2
	PB1 - G1
	PB10 - B2
	PB11 - B1
	PB12 - R2
	PB13 - R1

	PC9 - CLK
	PC8 - LAT
	PC7 - OE

	PB5 - A
	PB6 - B
	PB7 - C
	PB8 - D
 */

#define LED_LAT	GPIO8
#define LED_CLK GPIO9
#define LED_OE	GPIO7


/* ### code ### */

/* Simple helper to set state of a given GPIO pin */
static void
set_pin(uint32_t gpio, uint16_t pin, int state)
{
	if (state) {
		gpio_set(gpio, pin);
	} else {
		gpio_clear(gpio, pin);
	}
}


/* set both portC pins and all of port B to output + pullup*/
void
gpio_init(void)
{
	rcc_periph_clock_enable(RCC_GPIOB);
	gpio_mode_setup(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_PULLUP, 0xffff);
	gpio_clear(GPIOB, 0xffff);
	rcc_periph_clock_enable(RCC_GPIOC);
	gpio_mode_setup(GPIOC, GPIO_MODE_OUTPUT, GPIO_PUPD_PULLUP, GPIO9 | GPIO8 | GPIO7);
	gpio_mode_setup(GPIOC, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO2);
	gpio_clear(GPIOC, GPIO2 | GPIO9 | GPIO8 | GPIO7);
}

/* R, G, B, bits for Pixel 1 and Pixel 2 */
static const uint16_t p1[3] = {
	GPIO13, GPIO1, GPIO11
};

static const uint16_t p2[3] = {
	GPIO12, GPIO0, GPIO10
};


/*
 * Clock in two rows of data, pixel values are 0 - 7.
 * My modules are connected together into a 64 x 64
 * array with one output feeding the other's input.
 * But the second one is "upside down" with respect
 * to the top one. As a result we want to clock in
 * the data to reflect a transform from pixmap space
 * into array space.
 */
void
clock_two_rows(uint8_t *q1, uint8_t *q2, uint8_t *q3, uint8_t *q4)
{
	int	i;
	/* clock in the new row(s) data */
	q1 += 63;
	q2 += 63;
	for (i = 0; i < 64; i++) {
		set_pin(GPIOB, p1[0], (*q1 & 0x4) != 0);
		set_pin(GPIOB, p1[1], (*q1 & 0x2) != 0);
		set_pin(GPIOB, p1[2], (*q1 & 0x1) != 0);
		set_pin(GPIOB, p2[0], (*q2 & 0x4) != 0);
		set_pin(GPIOB, p2[1], (*q2 & 0x2) != 0);
		set_pin(GPIOB, p2[2], (*q2 & 0x1) != 0);
		gpio_set(GPIOC, GPIO9);
		gpio_clear(GPIOC, GPIO9);
		q1--; q2--;
	}

/*	q3 += 63;
	q4 += 63;
*/
	for (i = 0; i < 64; i++) {
		set_pin(GPIOB, p1[0], (*q3 & 0x4) != 0);
		set_pin(GPIOB, p1[1], (*q3 & 0x2) != 0);
		set_pin(GPIOB, p1[2], (*q3 & 0x1) != 0);
		set_pin(GPIOB, p2[0], (*q4 & 0x4) != 0);
		set_pin(GPIOB, p2[1], (*q4 & 0x2) != 0);
		set_pin(GPIOB, p2[2], (*q4 & 0x1) != 0);
		gpio_set(GPIOC, GPIO9);
		gpio_clear(GPIOC, GPIO9);
		q3++; q4++;
	}

}

/* This is the LED display's "buffer" */
uint8_t buf1[64 * 64];
uint8_t buf2[64 * 64];
uint8_t *draw_buf;
uint8_t *display_buf;
volatile uint8_t do_swap;

void
draw_pixel(int x, int y, uint16_t color)
{
	*(draw_buf + y * 64 + x) = color & 0x7;
}

static int current_pair = 0;

/*
 * This function is called every millisecond. The idea
 * is that each time it is called it displays the next
 * two rows of data, and since there are 16 pairs it
 * will have displayed a complete frame every 16mS which is
 * convieniently 60hz
 */
void
next_pair(void)
{
	int top_pair;
	uint8_t *t;

	gpio_set(GPIOC, GPIO2);
	current_pair = (current_pair + 1) & 0xf;

	/* implement double buffering */
	if ((current_pair == 0) && (do_swap != 0)) {
		t = draw_buf;
		draw_buf = display_buf;
		display_buf = t;
		do_swap = 0;
	}
	
	/* turn off LEDs */
	gpio_set(GPIOC, LED_OE);
	/* update row pair */
	top_pair = 15 - current_pair;
	/* Set the next pair */
	set_row(current_pair);
	/* prep the next pair of rows */
	clock_two_rows(display_buf + (top_pair + 16) * 64, 
				   display_buf + (top_pair + 0) * 64,
				   display_buf + (current_pair+32) * 64,
				   display_buf + (current_pair+48) * 64);
	/* latch previously clocked in data */
	gpio_clear(GPIOC, LED_LAT);
	gpio_set(GPIOC, LED_LAT);

	/* turn the LEDs back on */
	gpio_clear(GPIOC, LED_OE); 
	gpio_clear(GPIOC, GPIO2);
}

/* pin ordering for A, B, C, D on the LED panel */
const uint16_t row_gpios[4] = {
	GPIO5, GPIO6, GPIO7, GPIO8
};

void
set_row(int row)
{
	int i;
	uint8_t bits = row % 16;

	for (i = 0; i< 4; i++) {
		set_pin(GPIOB, row_gpios[i], (bits & 1) != 0);
		bits = bits >> 1;
	}
}

int gmt_clock;
void draw_24hr_clock(uint32_t tm);

void
draw_24hr_clock(uint32_t tm)
{
	simple_time *t; 
	int hh, mm, ss, ms;
	int x0, x1, y0, y1;
	int i;

	t = time_get(tm);
	
	hh = t->hh;
	mm = t->mm;
	ss = t->ss;
	ms = t->ms;


	gfx_fillScreen(0);
#ifdef GFX_CIRCLE
	gfx_drawCircle(32,32,31, 2);
#else
#define CIRCLE_INC	5
	for (i = 0; i < 360; i += CIRCLE_INC) {
		x0 = 32.5 + 31.0 * (sin((float) i / 180.0 * M_PI));
		y0 = 32.5 + 31.0 * (cos((float) i / 180.0 * M_PI));
		x1 = 32.5 + 31.0 * (sin((float) (i + CIRCLE_INC) / 180.0 * M_PI));
		y1 = 32.5 + 31.0 * (cos((float) (i + CIRCLE_INC) / 180.0 * M_PI));
		gfx_drawLine(x0, y0, x1, y1, 2);
	}
#endif
	gfx_setFont(GFX_FONT_SMALL);
	gfx_setTextColor(4, 0);
	gfx_setCursor(26, 11);
	gfx_puts((unsigned char *)"24");
	gfx_setCursor(54, 36);
	gfx_puts((unsigned char *)"6");
	gfx_setCursor(26, 60);
	gfx_puts((unsigned char *)"12");
	gfx_setCursor(4, 36);
	gfx_puts((unsigned char *)"18");
	for (i = 1; i < 60; i++) {

		if ((i == 0) || (i == 15) || (i == 30) || (i == 45)) {
			continue;
		}
		x1 = (int) (32.5 + 30.0 * (sin((float) i / 30.0 * M_PI)));
		y1 = (int) (32.5 + 30.0 * (cos((float) i / 30.0 * M_PI)));
		if ((i % 5) == 0) {
			x0 = (int) (32.5 + 25.0 * (sin((float) i / 30.0 * M_PI)));
			y0 = (int) (32.5 + 25.0 * (cos((float) i / 30.0 * M_PI)));
			gfx_drawLine(x0, y0, x1, y1, 1);
		} else {
			gfx_drawPixel(x1, y1, 1);
		}
	}
	/* rotate hands */
	gfx_drawLine(32, 32, 32 + 25 * (sin((float) ss/30.0 * M_PI)),
						 32 - 25 * (cos((float) ss/30.0 * M_PI)), 5);
	gfx_drawLine(32, 32, 32 + 20 * (sin((float) mm/30.0 * M_PI)),
						 32 - 20 * (cos((float) mm/30.0 * M_PI)), 2);
	gfx_drawLine(32, 32, 32 + 15 * (sin(((float) hh + (float) mm / 60.0) / 12.0 * M_PI)),
						 32 - 15 * (cos(((float) hh + (float) mm / 60.0) / 12.0 * M_PI)), 3);
	gfx_drawLine(32, 32, 32 + 10 * (sin((float) ms/500.0 * M_PI)),
						 32 - 10 * (cos((float) ms/500.0 * M_PI)), 1);
	gfx_fillCircle(32,32,3,6);
	do_swap++;
}

void
draw_clock(uint32_t tm)
{
	simple_time *t; 
	int hh, mm, ss, ms;
	int x0, x1, y0, y1;
	int i;

	t = time_get(tm);
	
	hh = t->hh;
	mm = t->mm;
	ss = t->ss;
	ms = t->ms;


	gfx_fillScreen(0);
#ifdef GFX_CIRCLE
	gfx_drawCircle(32,32,31, 2);
#else
#define CIRCLE_INC	5
	for (i = 0; i < 360; i += CIRCLE_INC) {
		x0 = 32.5 + 31.0 * (sin((float) i / 180.0 * M_PI));
		y0 = 32.5 + 31.0 * (cos((float) i / 180.0 * M_PI));
		x1 = 32.5 + 31.0 * (sin((float) (i + CIRCLE_INC) / 180.0 * M_PI));
		y1 = 32.5 + 31.0 * (cos((float) (i + CIRCLE_INC) / 180.0 * M_PI));
		gfx_drawLine(x0, y0, x1, y1, 2);
	}
#endif
	gfx_setFont(GFX_FONT_SMALL);
	gfx_setTextColor(4, 0);
	gfx_setCursor(26, 11);
	gfx_puts((unsigned char *)"12");
	gfx_setCursor(54, 36);
	gfx_puts((unsigned char *)"3");
	gfx_setCursor(30, 60);
	gfx_puts((unsigned char *)"6");
	gfx_setCursor(4, 36);
	gfx_puts((unsigned char *)"9");
	for (i = 1; i < 60; i++) {

		if ((i == 0) || (i == 15) || (i == 30) || (i == 45)) {
			continue;
		}
		x1 = (int) (32.5 + 30.0 * (sin((float) i / 30.0 * M_PI)));
		y1 = (int) (32.5 + 30.0 * (cos((float) i / 30.0 * M_PI)));
		if ((i % 5) == 0) {
			x0 = (int) (32.5 + 25.0 * (sin((float) i / 30.0 * M_PI)));
			y0 = (int) (32.5 + 25.0 * (cos((float) i / 30.0 * M_PI)));
			gfx_drawLine(x0, y0, x1, y1, 1);
		} else {
			gfx_drawPixel(x1, y1, 1);
		}
	}
	/* rotate hands */
	gfx_drawLine(32, 32, 32 + 25 * (sin((float) ss/30.0 * M_PI)),
						 32 - 25 * (cos((float) ss/30.0 * M_PI)), 5);
	gfx_drawLine(32, 32, 32 + 20 * (sin((float) mm/30.0 * M_PI)),
						 32 - 20 * (cos((float) mm/30.0 * M_PI)), 2);
	gfx_drawLine(32, 32, 32 + 15 * (sin(((float) hh + (float) mm / 60.0) / 6.0 * M_PI)),
						 32 - 15 * (cos(((float) hh + (float) mm / 60.0) / 6.0 * M_PI)), 3);
	gfx_drawLine(32, 32, 32 + 10 * (sin((float) ms/500.0 * M_PI)),
						 32 - 10 * (cos((float) ms/500.0 * M_PI)), 1);
	gfx_fillCircle(32,32,3,6);
	do_swap++;
}

int color;
int fast_mode = 0; /* .1 sec or .0 sec mode */
uint32_t last_time = 0;

void qr_clock(uint32_t tm);

int qr_ecc = QR_ECLEVEL_Q;
void rotate_ecc_level(void);

void
rotate_ecc_level(void)
{
	switch (qr_ecc) {
	case QR_ECLEVEL_L :
		qr_ecc = QR_ECLEVEL_M;
		break;
	case QR_ECLEVEL_M :
		qr_ecc = QR_ECLEVEL_Q;
		break;
	case QR_ECLEVEL_Q :
		qr_ecc = QR_ECLEVEL_H;
		break;
	default:
		qr_ecc = QR_ECLEVEL_L;
		break;
	}
}

int color_mode = 0;

void
qr_clock(uint32_t tm)
{
	int x, y, inset, size;
	uint32_t this_time;
	QRcode *my_qr;
	simple_time *t;

	t = time_get(tm);
	this_time = t->ss * 10;
	if (fast_mode) {
		this_time += (t->ms / 100);
	}
	/* only update if the second changes */
	if (last_time != this_time) {
		last_time = this_time;
		my_qr = QRcode_encodeString(time_stamp(t, fast_mode), 0, qr_ecc, QR_MODE_8, 1);
		if (my_qr != NULL) {
			if ((my_qr->width * 2) < 64) {
				inset = (64 - (my_qr->width * 2)) / 2;
				size = 2;
			} else {
				inset = (64 - my_qr->width) / 2;
				size = 1;
			}
			gfx_fillScreen(color);
			for (x = 0; x < my_qr->width; x++) {
				for (y = 0; y < my_qr->width; y++) {
					int foo = *(my_qr->data + y * my_qr->width + x);
					int cc = color;;
					if (foo & 1) {
						cc = 0;
					}
					if (color_mode) {
						if (foo & 2) {
							cc = 4;
						}
						if (foo & 0x80) {
							cc = 1;
						}
					}
					if (*((my_qr->data) + y * my_qr->width + x) & 1) {
						if (size == 1) {
							gfx_drawPixel(x + inset, y + inset, cc);
						} else {
							gfx_drawRect((x*2)+inset, (y*2)+inset, 2, 2, cc);
						}
					}
				}
			}
			QRcode_free(my_qr);
			do_swap++;
		}
	}
}

int flip_it = 0;

int
main(void)
{
	int cnt;
	int clock_running = 0;
	int qclock_running = 0;
	int refresh = 1;

	printf("LED Panel Demo\n");
	draw_buf = &buf1[0];
	display_buf = &buf2[0];

	gfx_init(draw_pixel, 64, 64, GFX_FONT_LARGE);
	gfx_fillScreen(0);
	gfx_setCursor(0, 12);
	gfx_setTextColor(0x2, 0x0);
	gfx_puts((unsigned char *) "1Bitsy");
	gfx_setCursor(0, 24);
	gfx_setTextColor(0x4, 0x0);
	gfx_puts((unsigned char *) " @esden");
	gfx_drawRoundRect(0, 0, 64, 64, 5, 1);
	gfx_setCursor(4, 48); gfx_setTextColor(0x6, 0x0);
	gfx_puts((unsigned char *)"Chuck");
	gfx_setCursor(6, 62);
	gfx_puts((unsigned char *)"McManis");
	do_swap++;

	gpio_init();
	cnt = 0;
	gpio_set(GPIOC, LED_OE);
	gpio_set(GPIOC, LED_LAT);
	gpio_set(GPIOC, LED_CLK);
	/* this should start drawing the buffer */
	current_pair = 0;
	set_clock_hook(next_pair, 1);
	
	gpio_clear(GPIOC, GPIO3);
	color = 3;
	while(1) 
	{
		if (do_swap) {
			continue;
		}
		switch (console_getc(0)) {
			case ' ':
				clock_running = 0;
				qclock_running = 0;
				gfx_fillScreen(color);
				do_swap++;
				break;
			case 'g':
				clock_running = 0;
				qclock_running = 0;
				gfx_fillScreen(0);
				for (cnt = 0; cnt < 64; cnt++) {
					if (((cnt % 4) == 0) || (cnt == 63)) {
						gfx_drawLine(cnt, 0, cnt, 63, color);
						gfx_drawLine(0, cnt, 63, cnt, color);
					}
				}
				do_swap++;
				break;
			case 'G':
				qclock_running = 0;
				clock_running = 0;
				gfx_fillScreen(0);
				for (cnt = 0; cnt < 64; cnt++) {
					if (((cnt % 8) == 0) || (cnt == 63)) {
						gfx_drawLine(cnt, 0, cnt, 63, color);
						gfx_drawLine(0, cnt, 63, cnt, color);
					}
				}
				do_swap++;
				break;
			case 'c':
				color = (color + 1) & 7;
				if (color == 0) color = 1;
				printf("Color is now : %d\n", color);
				break;
			case 'C':
				qclock_running = 0;
				clock_running = 1;
				break;
			case 'Q':
				clock_running = 0;
				qclock_running++;
				break;
			case 'f':
				fast_mode = (fast_mode != 0) ? 0 : 1;
				printf("Fast mode: %s\n", (fast_mode) ? "ON" : "OFF");
				break;
			case 't':
				printf(" TIME: %s\n", time_stamp(time_get(mtime()),1));
				break;
			case 'e':
				rotate_ecc_level();
				break;
			case 'r':
				console_puts("Enter delay count (refresh) [1+]: ");
				refresh = console_getnumber();
				printf("\nRefresh set to %d\n", refresh);
				set_clock_hook(next_pair, refresh);
				break;
			case 'T':
			case 'd':
				time_set();
				break;
			case 'P':
				color_mode = (color_mode == 0) ? 1 : 0;
				break;
			case '2':
				gmt_clock = (gmt_clock == 0) ? 1 : 0;
				break;
			case 'i':
				flip_it = (flip_it == 0) ? 1 : 0;
				gfx_setMirrored(flip_it);
				break;
				
			default:
				break;
		}
		if (clock_running) {
			if (gmt_clock) {
				draw_24hr_clock(mtime());
			} else {
				draw_clock(mtime());
			}
		} else if (qclock_running) {
			qr_clock(mtime());
		}

	}
}
