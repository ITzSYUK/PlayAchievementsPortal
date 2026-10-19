include ../rcheevos.mk

BUILD ?= ../../build/host
PYTHON ?= python3
CFLAGS ?= -O2 -g -std=c99 -Wall -Wextra -Werror
CPPFLAGS += $(RC_DEFINES) -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64 $(RC_INCLUDES) -I../../online/include -I../common
LDLIBS += -lm
RC_OBJECTS = $(patsubst $(RC_ROOT)/%.c,$(BUILD)/rcheevos/%.o,$(RC_SOURCES))
LOCAL_OBJECTS = $(BUILD)/ra_psp_memory.o $(BUILD)/ra_psp_hash.o $(BUILD)/offline_probe.o $(BUILD)/offline_main.o $(BUILD)/hash_probe.o $(BUILD)/probe_log.o $(BUILD)/test_probe_log.o $(BUILD)/ra_client_session.o $(BUILD)/test_client_session.o

.PHONY: all test
all: $(BUILD)/test_ui
$(BUILD)/test_ui: test_ui.c ../../online/src/ra_ui.c ../../online/src/ra_ui_draw.c ../../online/include/ra_ui.h
	$(CC) $(CPPFLAGS) $(CFLAGS) test_ui.c ../../online/src/ra_ui.c ../../online/src/ra_ui_draw.c -pthread $(LDFLAGS) -o $@
test: ui-test
.PHONY: ui-test
ui-test: $(BUILD)/test_ui
	$(BUILD)/test_ui
all: $(BUILD)/test_session_mailbox
$(BUILD)/test_session_mailbox: test_session_mailbox.c ../ingame/session_mailbox.h ../../online/src/ra_client_session.c ../../online/include/ra_client_session.h $(BUILD)/librcheevos.a
	$(CC) $(CPPFLAGS) -I../ingame $(CFLAGS) -DRA_SESSION_ONLINE test_session_mailbox.c ../../online/src/ra_client_session.c $(BUILD)/librcheevos.a -pthread $(LDFLAGS) $(LDLIBS) -o $@
all: $(BUILD)/test_online_session
$(BUILD)/test_online_session: test_online_session.c ../../online/src/ra_client_session.c ../../online/include/ra_client_session.h $(BUILD)/librcheevos.a
	$(CC) $(CPPFLAGS) $(CFLAGS) -DRA_SESSION_ONLINE test_online_session.c ../../online/src/ra_client_session.c $(BUILD)/librcheevos.a $(LDFLAGS) $(LDLIBS) -o $@
all: $(BUILD)/test_rcheevos_regressions $(BUILD)/client_memory_bench
$(BUILD)/test_rcheevos_regressions: test_rcheevos_regressions.c $(BUILD)/librcheevos.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(BUILD)/librcheevos.a $(LDFLAGS) $(LDLIBS) -o $@

$(BUILD)/client_memory_bench: client_memory_bench.c ../../online/src/ra_client_session.c ../../online/include/ra_client_session.h $(BUILD)/librcheevos.a
	$(CC) $(CPPFLAGS) $(CFLAGS) -fno-builtin-malloc -fno-builtin-calloc -fno-builtin-realloc -fno-builtin-free client_memory_bench.c ../../online/src/ra_client_session.c $(BUILD)/librcheevos.a $(LDFLAGS) $(LDLIBS) -Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=free,--wrap=strdup -o $@

.PHONY: memory-bench
memory-bench: $(BUILD)/client_memory_bench
	$(BUILD)/client_memory_bench $(if $(SETS),--sets "$(SETS)")
all: $(BUILD)/test_state_test $(BUILD)/test_target_session $(BUILD)/offline_probe $(BUILD)/hash_probe $(BUILD)/test_probe_log $(BUILD)/test_client_session $(BUILD)/test_client_session_noframes $(BUILD)/test_auth_file $(BUILD)/test_large_session $(BUILD)/test_game_session

$(BUILD):
	mkdir -p $@

$(BUILD)/rcheevos/%.o: $(RC_ROOT)/%.c
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Wno-unused-parameter -MMD -MP -c $< -o $@

$(BUILD)/librcheevos.a: $(RC_OBJECTS)
	$(AR) rcs $@ $^

$(BUILD)/ra_psp_%.o: ../../online/src/ra_psp_%.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD)/ra_client_session.o: ../../online/src/ra_client_session.c ../../online/include/ra_client_session.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD)/test_client_session.o: test_client_session.c ../../online/include/ra_client_session.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -pthread -MMD -MP -c $< -o $@

$(BUILD)/offline_probe.o: ../common/offline_probe.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD)/probe_log.o: ../common/probe_log.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD)/%.o: %.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD)/offline_probe: $(BUILD)/offline_main.o $(BUILD)/offline_probe.o $(BUILD)/ra_psp_memory.o $(BUILD)/librcheevos.a
	$(CC) $(CFLAGS) $^ $(LDFLAGS) $(LDLIBS) -o $@

$(BUILD)/hash_probe: $(BUILD)/hash_probe.o $(BUILD)/ra_psp_hash.o $(BUILD)/librcheevos.a
	$(CC) $(CFLAGS) $^ $(LDFLAGS) $(LDLIBS) -o $@

$(BUILD)/test_probe_log: $(BUILD)/test_probe_log.o $(BUILD)/probe_log.o
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@

$(BUILD)/test_client_session: $(BUILD)/test_client_session.o $(BUILD)/ra_client_session.o $(BUILD)/librcheevos.a
	$(CC) $(CFLAGS) $^ $(LDFLAGS) $(LDLIBS) -pthread -o $@

$(BUILD)/test_auth_file: test_auth_file.c ../../online/src/ra_auth_file.c ../../online/include/ra_auth_file.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) test_auth_file.c ../../online/src/ra_auth_file.c $(LDFLAGS) -o $@

$(BUILD)/test_client_session_noframes: test_client_session.c ../../online/src/ra_client_session.c ../../online/include/ra_client_session.h $(BUILD)/librcheevos.a
	$(CC) $(CPPFLAGS) $(CFLAGS) -DRA_SESSION_NO_FRAMES test_client_session.c ../../online/src/ra_client_session.c $(BUILD)/librcheevos.a $(LDFLAGS) $(LDLIBS) -pthread -o $@

$(BUILD)/test_large_session: test_large_session.c ../../online/src/ra_client_session.c ../../online/include/ra_client_session.h ../../online/include/ra_response_limits.h $(BUILD)/librcheevos.a
	$(CC) $(CPPFLAGS) $(CFLAGS) -DRA_SESSION_NO_FRAMES test_large_session.c ../../online/src/ra_client_session.c $(BUILD)/librcheevos.a $(LDFLAGS) $(LDLIBS) -Wl,--wrap=malloc,--wrap=free -o $@

$(BUILD)/test_game_session: test_game_session.c ../../online/src/ra_client_session.c ../../online/src/ra_game_memory.c ../../online/src/ra_psp_memory.c ../../online/include/ra_game_memory.h ../../online/include/ra_client_session.h $(BUILD)/librcheevos.a
	$(CC) $(CPPFLAGS) $(CFLAGS) -DRA_SESSION_NO_FRAMES test_game_session.c ../../online/src/ra_client_session.c ../../online/src/ra_game_memory.c ../../online/src/ra_psp_memory.c $(BUILD)/librcheevos.a $(LDFLAGS) $(LDLIBS) -o $@

$(BUILD)/test_state_test: test_state_test.c ../../online/src/ra_state_test.c ../../online/include/ra_state_test.h $(BUILD)/librcheevos.a
	$(CC) $(CPPFLAGS) $(CFLAGS) test_state_test.c ../../online/src/ra_state_test.c $(BUILD)/librcheevos.a $(LDFLAGS) $(LDLIBS) -o $@

$(BUILD)/test_target_session: test_game_session.c ../../online/src/ra_client_session.c ../../online/src/ra_state_test.c ../../online/src/ra_game_memory.c ../../online/src/ra_psp_memory.c ../../online/include/ra_state_test.h ../../online/include/ra_client_session.h $(BUILD)/librcheevos.a
	$(CC) $(CPPFLAGS) -I$(RC_ROOT)/src $(CFLAGS) -DRA_SESSION_NO_FRAMES -DRA_SESSION_STATE_TEST test_game_session.c ../../online/src/ra_client_session.c ../../online/src/ra_state_test.c ../../online/src/ra_game_memory.c ../../online/src/ra_psp_memory.c $(BUILD)/librcheevos.a $(LDFLAGS) $(LDLIBS) -o $@

test: all
	$(BUILD)/test_rcheevos_regressions
	$(BUILD)/client_memory_bench
	$(BUILD)/test_session_mailbox
	$(BUILD)/test_online_session
	$(BUILD)/test_state_test
	$(BUILD)/test_target_session
	$(BUILD)/offline_probe
	$(PYTHON) test_hash_probe.py $(BUILD)/hash_probe
	$(BUILD)/test_probe_log
	$(BUILD)/test_client_session
	$(BUILD)/test_client_session_noframes
	$(BUILD)/test_large_session
	$(BUILD)/test_game_session
	$(BUILD)/test_auth_file
	$(PYTHON) test_configure_login.py

-include $(RC_OBJECTS:.o=.d) $(LOCAL_OBJECTS:.o=.d)

$(RC_OBJECTS) $(LOCAL_OBJECTS): Makefile ../rcheevos.mk
