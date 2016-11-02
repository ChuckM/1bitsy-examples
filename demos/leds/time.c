/*
 * time.c - Simple Time API
 *
 * World's simplest API for 'time', and probably not useful for your
 * project. Three functions:
 *		- simple_time *time_get(uint32_t millis)
 *			return a time structure given a set of milliseconds that
 *			are presumed to represent the time since 1/1/16. So total
 *			dynamic range here, roll over 46 days after the time is set.
 *		- void time_set(void)
 *			Query the user for elements of the time of day from the "console"
 *			serial port.
 *		- char *time_stamp(simple_time *t)
 *			Return a formatted time string of the form:
 *				DDD MMM 99 YYYY, HH:MM:SS TTT
 *			ex: Mon Oct 31 2016, 14:02:00 PDT
 *	
 * Copyright (c) 2016, Chuck McManis <cmcmanis@mcmanis.com>, All Rights Reserved.
 *
 */
#include <stdio.h>
#include <ctype.h>
#include "time.h"
#include "../util/util.h"

static uint32_t	__epoch;
static simple_time local_time;

#define MIN_SEC	(60)
#define HR_SEC	(MIN_SEC * 60)
#define DAY_SEC	(HR_SEC * 24)
#define YEAR_SEC (DAY_SEC * 365)

static const int mlen[12] = {
			/*  JAN      FEB      MAR      APR        MAY        JUN        JUL    */
			/* 0 - 30, 31 - 58, 59 - 89, 90 - 119, 120 - 150, 151 - 180, 181 - 211, */
			       31,      28,      31,       30,        31, 		 30,        31,
			/*    AUG        SEP        OCT        NOV        DEC */
			/* 212 - 242, 243 - 272, 273 - 303, 304 - 333, 334 - 364 */
				      31,        30,        31,        30,        31
};

/* 1/1/16 was Friday, that is day 0 as far as we are concerned */
static const char *days[7] = { 
	"Fri", "Sat", "Sun", "Mon", "Tue", "Wed", "Thu"
};

static const char *months[13] = {
	"Jan", "Feb", "Mar", "Apr", "May", "Jun",
	"Jul", "Aug", "Sep", "Oct", "Nov", "Dec",
	"UNK"
};

/* not thread safe, but not really an issue either 
 *			Regular:  Mon Oct 31 2016, 14:02:05 PDT
 *	High Resolution:  Mon Oct 31 2016, 14:02:05.023 PDT
 */
static char __time_stamp[36];
static char __time_zone[4];

simple_time *
time_get(uint32_t tm) {
	uint32_t now;
	int	day, i;

	/* now is the original date set + number of ms since boot */
	now = __epoch + ((tm + 500) / 1000);
	local_time.ms = tm % 1000;
	local_time.yr = 2016 + (now / YEAR_SEC);

	/* this is the zero based day */
	day = (now % YEAR_SEC) / DAY_SEC;
	local_time.dn = days[day % 7];
	for (i = 0; (i < 12) && (day > mlen[i]); i++) {
		/* doesn't do centuries */
		if ((i == 1) && ((local_time.yr % 4) == 0)) {
			day -= (mlen[i] + 1);
		} else {
			day -= mlen[i];
		}
	}
	local_time.dy = day + 1;
	local_time.mo = i + 1;
	local_time.mn = months[i];
	local_time.hh = (int) (now % 86400) / HR_SEC;
	local_time.mm = (int) (now % 3600) / 60;
	local_time.ss = (int) (now % 60);
	for (i = 0; i < 3; i++) {
		local_time.tz[i] = __time_zone[i];
	}
	local_time.tz[3] = 0;
	return &local_time;
}

/* Basically a cheap implementation of ctime */
char *
time_stamp(simple_time *t, int hires)
{
	if (hires) {
		snprintf(&__time_stamp[0], 36, "%3s %3s %2d %4d, %2d:%02d:%02d.%03d %3s",
			t->dn, t->mn, t->dy, t->yr, t->hh, t->mm, t->ss, t->ms, t->tz);
	} else {
		snprintf(&__time_stamp[0], 36, "%3s %3s %2d %4d, %2d:%02d:%02d %3s",
			t->dn, t->mn, t->dy, t->yr, t->hh, t->mm, t->ss, t->tz);
	}
	return __time_stamp;
}

void
time_set(void)
{
	int year, month, day, hh, mm, ss, i;
	uint32_t tm;
	simple_time *t;
	char timezone[10];

	printf("Please enter the current date and time\n");
	do {
		console_puts("Year [2016 - 9999]: ");
		year = console_getnumber();
	} while (year < 2016);

	do {
		console_puts("\nMonth [1 - 12]: ");
		month = console_getnumber();
	} while ((month < 1) || (month > 12));

	/* number of days, special case Feb in leap year */
	i = mlen[month - 1];
	if ((month == 2) && ((year % 4) == 0)) {
		i++;
	}
	do {
		printf("\nDay [1 - %2d]: ", i);
		fflush(stdout);
		day = console_getnumber();
	} while ((day < 1) || (day > i));

	for (i = 0; i < (month-1); i++) {
		day += mlen[i];
		if ((i == 1) && ((year % 4) == 0)) {
			day++;
		}
	}
	day--; /* actual days are zero based so 1/1/16 is day 0 */
	do {
		console_puts("\nHour [0 - 23]: ");
		hh = console_getnumber();
	} while ((hh < 0) || (hh > 23));

	do {
		console_puts("\nMinute [0 - 59]: ");
		mm = console_getnumber();
	} while ((mm < 0) || (mm > 59));

	do {
		console_puts("\nSecond [0 - 59]: ");
		ss = console_getnumber();
	} while ((ss < 0) || (ss > 59));

	console_puts("\nTime Zone (3 letters) : ");
	console_gets(timezone, 10);
	for (i = 0; i < 3; i++) {
		__time_zone[i] = (char) toupper((unsigned char)timezone[i]);
	}
	__time_zone[3] = 0;

	tm = mtime();
	/* now combine it all together, adjust by what mtime is currently */
	__epoch = ((2016 - year) * YEAR_SEC + day * DAY_SEC + hh * HR_SEC + mm * MIN_SEC + ss) - (tm / 1000);
	t = time_get(tm);
	printf("\nDate set to : %s\n", time_stamp(t, 1));
}

