64 x 64 LED Example
-------------------

This code let me play with the 1BitSquared 
[64 x 64 LED displays](https://1bitsquared.com/products/led-panel)
I was curious how challenging it would be to keep the display updated. 
The answer is not very.

##Modes

There are several modes here, the [1Bitsy](http://1bitsy.org/) is hooked up
to a TTL to Serial port converter (in my case the
[BlackMagic Probe](https://1bitsquared.com/collections/embedded-hardware/products/black-magic-probe))
which provides the "user interface" (which is generous). Commands are simple:
* T or d - **set the time and date** 
  (this just notes the time so that the clocks show something useful.
* f - **fast mode** which runs the QRClock at 10 frames per second rather 
  than one frame per second
* e - **ecc mode** rotates through the various QR Code ECC modes 
  (Mode M makes the clock reliably readable)
* g - **draw a grid** - this puts up a set of grid lines (4 x 4)
* G - **draw a big grid** - this puts up an 8x8 grid
* c - **change the color** - I use 8 simple colors (3 color LEDs all on or
  all off)
* ' ' (space) - **fill color** - fills the display with one color
* Q - **QR Clock** - run the QR Code clock (point a bar code reader
  at it to read the time and date)
* C - **Wall Clock** - run the wall clock 
  (simply rendered wall clock with a fast mS hand as well)
* i - mirror the display.

##Notes

The display is maintained by a callback that is 'hooked' into the SysTick 
interrupt. Using the API
`set_systick_hook(void (*func)(void), int ticks);` where every
`ticks` number of calls to the interrupt (aka every `ticks` milleseconds) 
the function will be called.  Using the value 1 for ticks it calls every 
time `systick_handler` is called, using the value 0 for `ticks`
disables calling the hook function.

With that setup, and since the display uses 32 pairs of rows, to update a 
total of 32 rows (remember its 64 x 32 nominally) each call to `clock_row` 
happens every mS, and it takes 32 calls
to clock out every pixel. That is 32 mS and inverting that 1/.032 is 30Hz.
