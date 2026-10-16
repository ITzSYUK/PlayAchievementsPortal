TARGET = RAProbeEntropy
OBJS = main.o semaphore.o
INCDIR =
LIBDIR =
CFLAGS = -O2 -G0 -std=c99 -Wall -Wextra -Werror
CXXFLAGS = $(CFLAGS)
ASFLAGS = $(CFLAGS)
LIBS =
BUILD_PRX = 1
USE_KERNEL_LIBS = 1
PRX_EXPORTS = exports.exp
PSPSDK := $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
main.o: Makefile entropy_request.h
semaphore.o: Makefile
