64 x 32 LED Example
-------------------

This code let me play with a couple of the AdaFruit [64 x 32 LED displays](https://www.adafruit.com/product/2279).
I was curious how challenging it would be to keep the display updated. The answer
was not as bad as I thought.

##Modes

There are several modes here, the [1Bitsy](http://1bitsy.org/) is hooked up to a TTL to Serial
port converter (in my case the [BlackMagic Probe](https://1bitsquared.com/collections/embedded-hardware/products/black-magic-probe)) which provides the "user interface"
(which is generous). Commands are simple
* T or d - **set the time and date** (this just notes the time so that the clocks show something useful.
* f - **fast mode** which runs the QRClock at 10 frames per second rather than one frame per second
* e - **ecc mode** rotates through the various QR Code ECC modes (Mode M makes the clock reliably readable)
* g - **draw a grid** - this puts up a set of grid lines (4 x 4)
* G - **draw a big grid** - this puts up an 8x8 grid
* c - **change the color** - I use 8 simple colors (3 color LEDs all on or all off)
* ' ' (space) - **fill color** - fills the display with one color
* Q - **QR Clock** - run the QR Code clock (point a bar code reader at it to read the time and date)
* C - **Wall Clock** - run the wall clock (simply rendered wall clock with a fast mS hand as well)

##Notes

The display is maintained by a callback that is 'hooked' into the SysTick interrupt (which is being called
once per millesecond). The API for that is `set_systick_hook(void (*func)(void), int ticks);` where every
`ticks` number of calls to the interrupt (aka every `ticks` milleseconds) the function will be called. 
Using the value 1 for ticks it calls every time `systick_handler` is called, using the value 0 for `ticks`
disables calling the hook function.

With that setup, and since the display uses 16 pairs of rows, to update a total of 32 rows
(remember its 64 x 32 nominally) each call to `clock_two_rows` happens every mS, and it takes 16 calls
to clock out every pixel. That is 16 mS and inverting that 1/.016 is 60Hz.

When you put the output of one display into the input of the next display it becomes a 128 x 32 display,
which in my case I've turned the second one "upside down" and put it below the first one, to give the
appearance of a 64 x 64 display. In order to not have half the display be upside down, the code in `clock_two_rows` 
plays some games with the order of pixels it sends out so that they end up in the right place.

The last bit is that I clock a GPIO pin on, when I start my call to `next_pair()` and then turn it off when I'm done.
By measuring that on my oscilloscope I can see that it takes about 120uS to clock out all of the pixels for 1/16th
of the display. That is 12% of one Systick interval. Since the display is on when it isn't being clocked, 
when a row is active it is on 88% of the time which makes it pretty bright.

[leds]: http://www.adafruit.com/product/2279

[1bitsy]: http://1bitsy.org/

[bmp]: https://1bitsquared.com/collections/embedded-hardware/products/black-magic-probe
