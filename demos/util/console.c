/*
 * console.c
 *
 * Copyright (C) 2013-2016, Chuck McManis <cmcmanis@mcmanis.com>
 *
 * Console utility library - uses the serial port made available
 * by the debug port on the evaluation board as a default console.
 *
 * This version is interrupt driven.
 */

#include <stdint.h>
#include <ctype.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/usart.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/stm32/iwdg.h>
#include <libopencm3/cm3/scb.h>
#include <libopencm3/cm3/cortex.h>
#include "../util/util.h"

/*
 * Some definitions of our console "functions" attached to the
 * USART.
 *
 * These define sort of the minimum "library" of functions which
 * we can use on a serial port.
 */

#define CONSOLE_USART		USART3
#define CONSOLE_USART_CLOCK	RCC_USART3
#define CONSOLE_USART_AF	GPIO_AF7
#define CONSOLE_ISR			usart3_isr
#define CONSOLE_GPIO		GPIOC
#define CONSOLE_PINS		(GPIO10 | GPIO11)
#define CONSOLE_GPIO_CLOCK	RCC_GPIOC
#define CONSOLE_USART_INT	NVIC_USART3_IRQ

/* Default Color state (enabled) */
static int __console_color_state = 1;

/* This is a ring buffer to holding characters as they are typed
 * it maintains both the place to put the next character received
 * from the USART, and the place where the last character was
 * read by the program.
 */

#define RECV_BUF_SIZE	128		/* Arbitrary buffer size */
char recv_buf[RECV_BUF_SIZE];
volatile int recv_ndx_nxt;		/* Next place to store */
volatile int recv_ndx_cur;		/* Next place to read */

/* For interrupt handling we add a new function which is called
 * when recieve interrupts happen. The name (usart3_isr) is created
 * by the irq.json file in libopencm3 calling this interrupt for
 * USART3 'usart3', adding the suffix '_isr', and then weakly binding
 * it to the 'do nothing' interrupt function in vec.c.
 *
 * By defining it in this file the linker will override that weak
 * binding and instead bind it here, but you have to get the name
 * right or it won't work. And you'll wonder where your interrupts
 * are going.
 */
void CONSOLE_ISR(void)
{
	uint32_t	reg;
	int			i;

	do {
		reg = USART_SR(CONSOLE_USART);
		if (reg & USART_SR_RXNE) {
			recv_buf[recv_ndx_nxt] = USART_DR(CONSOLE_USART);
#ifdef RESET_ON_CTRLC
			/*
			 * This bit of code will jump to the ResetHandler if you
			 * hit ^C
			 */
			if (recv_buf[recv_ndx_nxt] == '\003') {
				scb_reset_core();
				return; /* never actually reached */
			}
#endif
			/* Check for "overrun" */
			i = (recv_ndx_nxt + 1) % RECV_BUF_SIZE;
			if (i != recv_ndx_cur) {
				recv_ndx_nxt = i;
			}
		}
	/* can read back-to-back interrupts */
	} while ((reg & USART_SR_RXNE) != 0);
}

/*
 * console_putc(char c)
 *
 * Send the character 'c' to the USART, wait for the USART
 * transmit buffer to be empty first.
 */
void console_putc(char c)
{
	uint32_t	reg;
	do {
		reg = USART_SR(CONSOLE_USART);
	} while ((reg & USART_SR_TXE) == 0);
	USART_DR(CONSOLE_USART) = (uint16_t) c & 0xff;
}

/*
 * char = console_getc(int wait)
 *
 * Check the console for a character. If the wait flag is
 * non-zero. Continue checking until a character is received
 * otherwise return 0 if called and no character was available.
 */
char console_getc(int wait)
{
	char		c = 0;

	while ((wait != 0) && (recv_ndx_cur == recv_ndx_nxt));
	if (recv_ndx_cur != recv_ndx_nxt) {
		c = recv_buf[recv_ndx_cur];
		recv_ndx_cur = (recv_ndx_cur + 1) % RECV_BUF_SIZE;
	}
	return c;
}

/*
 * void console_puts(char *s)
 *
 * Send a string to the console, one character at a time, return
 * after the last character, as indicated by a NUL character, is
 * reached.
 *
 * Translate '\n' in the string (newline) to \n\r (newline + 
 * carraige return)
 */
void console_puts(char *s)
{
	while (*s != '\000') {
		console_putc(*s);
		/* Add in a carraige return, after sending line feed */
		if (*s == '\n') {
			console_putc('\r');
		}
		s++;
	}
}

/*
 * int console_gets(char *s, int len)
 *
 * Wait for a string to be entered on the console, with
 * support for editing characters (delete letter, word,
 * entire line). It returns when the length is reached
 * or a carrige return is entered. <CR> is changed to newline
 * before the buffer is returned.
 */
int console_gets(char *s, int len)
{
	char *t = s;
	char c;

	*t = '\000';
	/* read until a <CR> is received */
	while (((c = console_getc(1)) != '\r') && ((t - s) < len) ) {
		if ((c == 0x8) || (c == 0x7f)) {
			if (t > s) {
				/* send ^H ^H to erase previous character */
				console_puts("\010 \010");
				t--;
			}
		} else if (c == 0x17) {	// ^W erase a word
			while ((t > s) &&  (!(isspace((int) (*t))))) {
				t--;
				console_puts("\010 \010");
			}
		} else if (c == 0x15) { // ^U erase the line
			while (t > s) {
				t--;
				console_puts("\010 \010");
			}
		} else {
			*t = c;
			console_putc(c);
			if ((t - s) < len) {
				t++;
			}
		}
		/* update end of string with NUL */
		*t = '\000';
	}
	if ((t < s) < len) {
		*t++ = '\n';
		*t = 0;
	}
	return t - s;
}

/*
 * Set up the GPIO subsystem with an "Alternate Function"
 * on some of the pins, in this case connected to a
 * USART.
 */
void console_setup(int baud)
{

	/* MUST enable the GPIO clock in ADDITION to the USART clock */
	rcc_periph_clock_enable(CONSOLE_GPIO_CLOCK);

	/* This example uses PB10 and PB11 for Tx and Rx respectively
	 */
	gpio_mode_setup(CONSOLE_GPIO, GPIO_MODE_AF, GPIO_PUPD_NONE, CONSOLE_PINS);

	/* Actual Alternate function number (in this case 7) is part
	 * depenedent, check the data sheet for the right number to
	 * use.
	 */
	gpio_set_af(CONSOLE_GPIO, CONSOLE_USART_AF, CONSOLE_PINS);


	/* This then enables the clock to the USART3 peripheral.
	 */
	rcc_periph_clock_enable(CONSOLE_USART_CLOCK);

	/* Set up USART/UART parameters using the libopencm3 helper functions */
	usart_set_baudrate(CONSOLE_USART, baud);
	usart_set_databits(CONSOLE_USART, 8);
	usart_set_stopbits(CONSOLE_USART, USART_STOPBITS_1);
	usart_set_mode(CONSOLE_USART, USART_MODE_TX_RX);
	usart_set_parity(CONSOLE_USART, USART_PARITY_NONE);
	usart_set_flow_control(CONSOLE_USART, USART_FLOWCONTROL_NONE);
	usart_enable(CONSOLE_USART);

	/* Enable interrupts from the USART */
	nvic_enable_irq(CONSOLE_USART_INT);

	/* Specifically enable recieve interrupts */
	usart_enable_rx_interrupt(CONSOLE_USART);
}

/*
 * Set a different baud rate for the console.
 */
void console_baud(int baud_rate)
{
	usart_set_baudrate(CONSOLE_USART, baud_rate);
}

char *
console_color(TERM_COLOR c)
{
	if (__console_color_state == 0) {
		return "";
	}
	switch (c) {
		case RED:
			return ("\033[31;40;1m");
		case GREEN:
			return ("\033[32;40;1m");
		case BLUE:
			return ("\033[34;40;1m");
		case YELLOW:
			return ("\033[33;40;1m");
		case CYAN:
			return ("\033[35;40;1m");
		case MAGENTA:
			return ("\033[36;40;1m");
		case WHITE:
			return ("\033[37;40;1m");
		default:
			return ("\033[0m");
	}
}

void
console_color_enable(void)
{
	__console_color_state = 1;
}

void
console_color_disable(void)
{
	__console_color_state = 0;
}

/*
 * Convert a string to a number, it accepts base modifiers
 * so 0x<digits> is hex 0b<digits> is binary, 0<digits> octal.
 * <digits> and -<digits> decimal.
 *
 * Longest string is 0b00000000000000000000000000000000
 * 35 characters (0b<32 digits>NUL)
 * 
 * Assumes:
 *      - Stops at first non-legal digit in guessed
 *        base or NUL character.
 *      - only does signed input on decimal numbers
 * Returns:
 *      - 0 even on failure.
 */
uint32_t
console_getnumber (void) {
	char holding_buf[40];
	char *buf;
    uint32_t res = 0;
    int     base = 10;
    int     sign_bit = 0;
    uint8_t digit;

	if (console_gets(holding_buf, sizeof(holding_buf)) == 0) {
		return 0;
	}

	holding_buf[sizeof(holding_buf) - 1] = 0; /* forced NUL */
	buf = &holding_buf[0];

    /* check for different base indicators */
    if (((*buf >= '1') && (*buf <= '9')) || (*buf == '-')) {
        if (*buf == '-') {
            sign_bit = 1;
            buf++;
        }
        base = 10;
    } else if ((*buf) == '0') {
        buf++;
        if (*buf == 'b') {
            base = 2;
            buf++;
        } else if (*buf == 'x') {
            base = 16;
            buf++;
        } else if ((*buf >= '0') && (*buf <= '7')) {
            base = 8;
        } else if (*buf == '\000') {
            return 0;
        }
    }
    while (*buf != '\000') {
        digit = (*buf > '9') ? ((*buf & 0xdf) - '7') : (*buf - '0');
        if (((base == 2) && (digit > 1)) ||
            ((base == 8) && (digit > 7)) ||
            ((base == 10) && (digit > 9)) ||
            ((base == 16) && (digit > 15))) {
            return (sign_bit) ? -res : res;
        }
        res *= base;
        res += digit;
        buf++;
    }
    return (sign_bit) ? -res : res;
}
