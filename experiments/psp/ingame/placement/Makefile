TARGET = RAPlacementUser
OBJS = main.o
INCDIR =
LIBDIR =
CFLAGS = -O2 -G0 -std=c99 -Wall -Wextra -Werror
CXXFLAGS = $(CFLAGS)
ASFLAGS = $(CFLAGS)
LIBS = -lpspmodinfo -lpspuser
LDFLAGS = -nostartfiles -nostdlib -Wl,-e,module_start
BUILD_PRX = 1
# This target has an empty import range. The SDK fixup tool requires a
# nonempty .lib.stub, so use its documented no-import build rule instead.
NO_FIXUP_IMPORTS = 1
PRX_EXPORTS = exports.exp
PSPSDK := $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
main.o: Makefile
