/* time.h - simple time API
 *
 *
 * simple_time time_get(uint32_t tm);
 * char *time_date_string(simple_time *t)
 * void time_set(void)
 */
typedef struct  __time_struct {
	int			yr;		/* year */
	int 		mo;		/* month (1 based) */
	const char	*mn;	/* Month name (3 char, mixed case) */
	int 		dy;		/* day (1 based) */
	const char	*dn;	/* Day name (3 char, mixed case) */
	char 		tz[4];	/* Timezone (3 char, upper case) */
	int			hh;		/* hour (0 - 23) */
	int			mm;		/* minute (0 - 59) */
	int			ss;		/* second (0 - 59) */
	int			ms;		/* mS (0 - 999) */
} simple_time;
	
char * time_stamp(simple_time *t);
void time_set(void);
simple_time *time_get(uint32_t tm);

