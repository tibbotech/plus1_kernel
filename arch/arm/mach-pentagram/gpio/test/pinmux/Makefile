#
# Makefile for gpio test code
#

CC= arm-none-linux-gnueabi-gcc
CFLAGS  = -O2 -Wall -D_GNU_SOURCE -static
LDFLAGS = -EL -static

SOURCES = pinmux_test.c 
OBJECTS = ${SOURCES:.c=.o}

OUT     = pinmux_test.x

all: $(OUT)
	@echo Build DONE.

$(OUT): $(OBJECTS)
	$(CC) $(LDFLAGS) -o $(OUT) $(OBJECTS) $(LIBS)

clean:
	rm -f $(OBJECTS) *~ $$$$~* *.bak core config.log $(OUT)