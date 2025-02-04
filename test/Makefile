CC		= gcc
CPP	= g++
LD		= g++
CP		= cp
CFLAGS    = -std=c99 -ggdb -DSTM32F1 -I../include -I../libopeninv/include -I../libopencm3/include
CPPFLAGS    = -ggdb -I../include -I../libopeninv/include -I../libopencm3/include
LDFLAGS     = -g
BINARY		= test_bms
OBJS		= test_main.o bmsalgo.o test_bmsalgo.o \
			  stub_canhardware.o \
			  stub_libopencm3.o picontroller.o
VPATH = ../src ../libopeninv/src

# Check if the variable GITHUB_RUN_NUMBER exists. When running on the github actions running, this
# variable is automatically available.
# Create a compiler define with the content of the variable. Or, if it does not exist, use replacement value 99999.
CPPFLAGS += $(shell \
    if [ -z "$$GITHUB_RUN_NUMBER" ]; then echo "-DGITHUB_RUN_NUMBER=0"; else echo "-DGITHUB_RUN_NUMBER=$$GITHUB_RUN_NUMBER"; fi )

all: $(BINARY)

$(BINARY): $(OBJS)
	$(LD) $(LDFLAGS) -o $(BINARY) $(OBJS)

%.o: ../%.cpp
	$(CPP) $(CPPFLAGS) -o $@ -c $<

%.o: ../%.c
	$(CC) $(CFLAGS) -o $@ -c $<

clean:
	rm -f $(OBJS) $(BINARY)
