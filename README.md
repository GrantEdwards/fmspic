## fmspic: An FMSPIC Joystick Driver

This is a joystick driver for an RC transmitter connected to a serial
port via a 9600 baud "FMS PIC" adapter cable.  It was developed using
3.3 and 3.5 kernels, but seems to work fine with 4.x kernels and probably
isn't too version-specific.

I use it with both the crrcsim RC airplane flight simulator, and the
Heli-X RC helicopter and airplane flight simulator. It provides a standard
joystick device and should be usable with any application that uses the
normal joystick API.

Note: there are two mutually-incompatible types of "FMS PIC" serial
cables:

 1. Sync byte of 0XF0+(#channels) running at 9600 baud.

 2. Sync byte of 0xFF running at 19200 baud.

This driver only supports the 9600 baud 0xF0+#channels variety. It
shouldn't be hard to add support for the other variety, but I don't have
one to test with.

This driver was based on the "zhenhau.c" driver found in the the
kernel source tree.

The included Makefile will build against the currently running kernel
and also also provides these make targets:

### insert

`make insert` will load the fmspic.ko module into the running kernel
(removing it first if it's already loaded).

### remove

`make remove` will unload the fmspic module from the running kernel.

### attach

`make attach` runs the "inputattach" command to connect to the serial
port specified by the SERPORT variable (default /dev/ttyUSB0) to the
fmspic module.  On Gentoo, this provided by the games-util/joystick
package.
