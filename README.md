# Flying ADC BMS
This BMS senses up to 16 channels per board. Boards can be stacked to sense any number of cells. Each board has a current sensor input therefor the current sensor can be located where it is most convenient. In addition each board has two temperature sensor inputs.

For cell voltage sensing a single channel, high precision delta/sigma ADC is used in conjunction with a 16 channel multiplexer. Each cell is sensed with a differential input with an accuracy 0.5 mV. The ADC is paired with a full bridge connected to its 5V isolated power supply. This allows sending current into a low cell and also draining current from a high cell. Only small balancing currents are used over a long time thus omitting large power resistors.
Due to its architecture the cell inputs are somewhat tolerant to miswiring, i.e. a swapped cell lead or multiple cells across one input won't damage the circuitry and will instead be reported by software.

# Wiring
Each board has 17 inputs for cell taps labeled V0 to V16. The very first input of the first board is connected to B-, i.e. the negative pole of the most negative cell. V16 of the first board is connected to the positive pole of the 16th cell. The latter is also connected to V0 of the second board.
Not all channels of a board need to be used if it is more convenient

A common CAN bus and 12V supply is connected to each

# OTA (over the air upgrade)
The firmware is linked to leave the 4 kb of flash unused. Those 4 kb are reserved for the bootloader
that you can find here: https://github.com/jsphuebner/tumanako-inverter-fw-bootloader
When flashing your device for the first time you must first flash that bootloader. After that you can
use the ESP8266 module and its web interface to upload your actual application firmware.
The web interface is here: https://github.com/jsphuebner/esp8266-web-interface

# Compiling
You will need the arm-none-eabi toolchain: https://developer.arm.com/open-source/gnu-toolchain/gnu-rm/downloads
On Ubuntu type

`sudo apt-get install git gcc-arm-none-eabi`

The only external depedencies are libopencm3 and libopeninv. You can download and build these dependencies by typing

`make get-deps`

Now you can compile stm32-<yourname> by typing

`make`

And upload it to your board using a JTAG/SWD adapter, the updater.py script or the esp8266 web interface.

# Editing
The repository provides a project file for Code::Blocks, a rather leightweight IDE for cpp code editing.
For building though, it just executes the above command. Its build system is not actually used.
Consequently you can use your favority IDE or editor for editing files.

# Adding classes or modules
As your firmware grows you probably want to add classes. To do so, put the header file in include/ and the 
source file in src/ . Then add your module to the object list in Makefile that starts in line 43 with .o
extension. So if your files are called "mymodule.cpp" and "mymodule.h" you add "mymodule.o" to the list.

When changing a header file the build system doesn't always detect this, so you have to "make clean" and
then make. This is especially important when editing the "*_prj.h" files.
