#include <stdio.h>
#include <stdint.h>
#include "../util/util.h"


int
main(void)
{
	clock_setup();
	console_setup(115200);

	console_puts("This is a test message for our console.\n");
	while (1) ;
}
