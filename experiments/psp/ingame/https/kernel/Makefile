TARGET = RAClientBootstrap
OBJS = main.o semaphore.o probe_log.o kernel_format.o placement2_policy.o ra_game_memory.o ra_psp_memory.o game_code_audit.o
VPATH = ../../../../common ../../../../ingame ../../../entropy ../../../../../online/src
INCDIR = ../../../../common ../../../../ingame ../../../../../online/include
CFLAGS = -Os -G0 -std=c99 -Wall -Wextra -Werror -DRA_CLIENT_HTTPS
CXXFLAGS = $(CFLAGS)
ASFLAGS = -G0
LIBS =
BUILD_PRX = 1
USE_KERNEL_LIBS = 1
PRX_EXPORTS = exports.exp
PSPSDK := $(shell psp-config --pspsdk-path)
.DEFAULT_GOAL := all
include $(PSPSDK)/lib/build.mak
main.o: notice_hook.h ../../../../ingame/display_import.h
main.o: ../../../../ingame/log_history.h ../../../../ingame/log_lifecycle.h
main.o: exit_notice.h
main.o: main.c game_audit.h ../../client/kernel/main.c ../../../../ingame/client_shared.h ../../../../ingame/game_code_audit.h ../../../../../online/include/ra_game_memory.h
game_code_audit.o: ../../../../ingame/game_code_audit.h
ra_game_memory.o: ../../../../../online/include/ra_game_memory.h
probe_log.o: ../../../../common/probe_log.c ../../../../common/probe_log.h
	$(CC) $(CFLAGS) -Dvsnprintf=ra_kernel_vsnprintf -c $< -o $@
