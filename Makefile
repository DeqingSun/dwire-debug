#PATH := /bin
TARGET := dwdebug




ifdef WINDIR
BINARY := $(TARGET).exe
else
BINARY := $(TARGET)
endif




ifndef  TOOLCHAIN_DIR

CC = gcc

# macOS: libusb + libusb-compat. Search Homebrew, a user-local prefix, then /usr/local.
ifeq ($(shell uname -s),Darwin)
  USB_PREFIX := $(shell for p in "$$(brew --prefix 2>/dev/null)" "$(HOME)/.local" /opt/homebrew /usr/local; do \
    if [ -f "$$p/include/usb.h" ] && [ -f "$$p/lib/libusb.a" ] && [ -f "$$p/lib/libusb-1.0.a" ]; then echo "$$p"; exit 0; fi; \
  done; echo "$(HOME)/.local")
endif

#all: run
all: build

build: $(BINARY)

run: $(BINARY)
	./$(BINARY)

install:
	cp -p $(BINARY) /usr/local/bin

else

  # set variable TOOLCHAIN_DIR to cross compile
  # example: https://github.com/raspberrypi/tools
  # make TOOLCHAIN_DIR=<tools>/arm-bcm2708/arm-rpi-4.9.3-linux-gnueabihf
  # this toolchain has package libusb-1.0-0-dev installed, but
  # we need files from the pi package libusb-dev (previous version):
  # /usr/include/usb.h => <toolchain_dir>/arm-linux-gnueabihf/sysroot/usr/include
  # /usr/lib/arm-linux-gnueabihf/libusb.a => <toolchain_dir>/arm-linux-gnueabihf/sysroot/lib
  CC := $(wildcard $(TOOLCHAIN_DIR)/bin/*-gcc)

endif




$(BINARY): */*/*.c */*.c Makefile
ifdef WINDIR
	# Lowercase -l names are required for Linux-hosted mingw cross-link (case-sensitive fs).
	i686-w64-mingw32-gcc -std=gnu99 -Wall -o $(BINARY) -Dwindows src/$(TARGET).c -lkernel32 -lwinmm -lcomdlg32 -lws2_32
else
ifeq ($(shell uname -s),Darwin)
	# Mac OS X
	$(CC) -std=gnu99 -g -fno-pie -rdynamic -fPIC -Wall -o $(BINARY) src/$(TARGET).c -I$(USB_PREFIX)/include $(USB_PREFIX)/lib/libusb.a $(USB_PREFIX)/lib/libusb-1.0.a -lobjc -ldl -Wl,-framework,IOKit -Wl,-framework,CoreFoundation -Wl,-framework,Security
else
	# Linux
	$(CC) -std=gnu99 -g -fno-pie -rdynamic -fPIC -Wall -o $(BINARY) src/$(TARGET).c -lusb -ldl
endif
endif
	ls -lap $(BINARY)


clean:
	-rm -rf *.o *.map *.list $(BINARY)

