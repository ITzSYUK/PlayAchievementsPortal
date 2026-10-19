TARGET = RAClientUser
RUNTIME_PROBE = 0
TLS_ROOT = ../../../../../vendor/mbedtls
RC_ROOT = ../../../../../vendor/rcheevos
include ../../../../tls/mbedtls.mk
RC_SOURCES = $(RC_ROOT)/src/rc_client.c $(RC_ROOT)/src/rc_version.c $(filter-out $(RC_ROOT)/src/rcheevos/rc_validate.c,$(wildcard $(RC_ROOT)/src/rcheevos/*.c)) $(filter-out $(RC_ROOT)/src/rapi/rc_api_editor.c $(RC_ROOT)/src/rapi/rc_api_runtime.c $(RC_ROOT)/src/rapi/rc_api_common.c,$(wildcard $(RC_ROOT)/src/rapi/*.c))
TLS_OBJS = $(patsubst $(TLS_ROOT)/library/%.c,objects/%.o,$(TLS_SOURCES))
ifeq ($(RUNTIME_PROBE),1)
OBJS = main.o ra_game_memory.o ra_psp_memory.o probe_log.o kernel_format.o
else
OBJS = main.o ra_https.o ra_http_body.o ra_net_resources.o ra_client_session.o ra_state_test.o ra_auth_file.o ra_game_memory.o ra_psp_memory.o frame_observation.o frame_candidate.o rtc_epoch.o probe_log.o kernel_format.o probe_power.o md5.o rc_api_runtime.o rc_api_common.o rc_compat.o rc_util.o $(TLS_OBJS)
OBJS += $(notdir $(RC_SOURCES:.c=.o))
OBJS += ra_ui.o ra_ui_draw.o
OBJS += ra_badge.o
OBJS += ra_pending.o
endif
VPATH = ../../../../../online/src ../../../../common ../../../../tls ../../../../ingame $(RC_ROOT)/src/rapi $(RC_ROOT)/src $(RC_ROOT)/src/rhash $(RC_ROOT)/src/rcheevos
INCDIR = ../../../../../online/include ../../../../common ../../../../tls ../../../../ingame $(RC_ROOT)/include $(RC_ROOT)/src $(RC_ROOT)/src/rhash $(TLS_ROOT)/include $(TLS_ROOT)/library
CFLAGS = -Os -G0 -std=c99 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -DRC_NO_THREADS -DRA_CLIENT_HTTPS -DRA_SESSION_ONLINE -D_GNU_SOURCE -DMBEDTLS_CONFIG_FILE=\"compact_tls_config.h\" -Dsnprintf=ra_kernel_snprintf
CXXFLAGS = $(CFLAGS)
ASFLAGS = -G0
rc_client.o: CFLAGS += -Wno-unused-parameter
$(notdir $(RC_SOURCES:.c=.o)): CFLAGS += -include strings.h
# Full set parsing needs the real formatter (including long/float formats).
# Only diagnostic logging retains the small bounded kernel formatter.
$(notdir $(RC_SOURCES:.c=.o)) rc_api_runtime.o rc_api_common.o rc_compat.o rc_util.o: CFLAGS += -Usnprintf
format.o runtime.o rc_api_runtime.o rc_api_common.o rc_api_info.o: CFLAGS += -Wno-format
ifeq ($(RUNTIME_PROBE),1)
LIBS = -lpspdisplay
else
LIBS = -lm -lpsppower -lpsprtc -lpspnet_inet -lpspnet_resolver -lpspwlan -lpspdisplay -lpspctrl -lpspaudio -lpspge
endif
LDFLAGS = -nostartfiles -Wl,-e,module_start,--gc-sections,--undefined=__library_exports,--undefined=module_info,--undefined=sce_newlib_heap_kb_size
BUILD_PRX = 1
PRX_EXPORTS = exports.exp
PSPSDK := $(shell psp-config --pspsdk-path)
.DEFAULT_GOAL := all
include $(PSPSDK)/lib/build.mak
objects/%.o: $(TLS_ROOT)/library/%.c ../../../../tls/compact_tls_config.h ../../../../tls/psp_tls_config.h
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(addprefix -I,$(INCDIR)) -c $< -o $@
$(OBJS): Makefile ../../../../ingame/client_shared.h
main.o: ../../../../ingame/client_mode.h
main.o: reconnect.h ../../../../ingame/reconnect_policy.h
main.o: ../../../../ingame/session_mailbox.h
main.o ra_client_session.o: ../../../../../online/include/ra_client_session.h
main.o: log_sync.h
main.o: socket_io.h
main.o: dns_job.h
main.o: ../../../../ingame/log_lifecycle.h
main.o: online_ui.h ../../../../../online/include/ra_ui.h
main.o: pending_store.h resume_owner.h
main.o ra_pending.o ra_client_session.o: ../../../../../online/include/ra_pending.h
main.o ra_badge.o: ../../../../../online/include/ra_badge.h
main.o: ../../../../ingame/ui_pause.h ../../../../ingame/ui_display.h
ra_ui.o ra_ui_draw.o ra_client_session.o: ../../../../../online/include/ra_ui.h
ra_ui.o: ../../../../../online/include/ra_https.h ../../../../../online/include/ra_response_limits.h
# Historical probe helpers share this source with the online worker.
# Unused experiments are removed by --gc-sections.
main.o: CFLAGS += -Wno-unused-function -Wno-unused-variable -Wno-unused-const-variable
main.o: frame_engine.h ../../../../ingame/frame_timeline.h stop_owner.h stop_transport.h early_network.h connect_profile.h apctl_observer.h ../../../../ingame/apctl_events.h ../../../../tls/compact/gts_root.h ../../../../../online/include/ra_https.h ../../../../../online/include/ra_client_session.h ../../../../../online/include/ra_auth_file.h
ra_client_session.o: ../../../../../online/include/ra_client_session.h ../../../../../online/include/ra_response_limits.h
ra_http_body.o ra_https.o main.o: ../../../../../online/include/ra_http_body.h ../../../../../online/include/ra_response_limits.h
ra_auth_file.o: ../../../../../online/include/ra_auth_file.h
main.o ra_game_memory.o: ../../../../../online/include/ra_game_memory.h ../../../../../online/include/ra_psp_memory.h
main.o frame_observation.o: ../../../../ingame/frame_observation.h
probe_log.o: ../../../../common/probe_log.c ../../../../common/probe_log.h
	$(CC) $(CFLAGS) -Dvsnprintf=ra_kernel_vsnprintf -c $< -o $@

ra_state_test.o ra_client_session.o main.o: ../../../../../online/include/ra_state_test.h

main.o frame_candidate.o: ../../../../ingame/frame_candidate.h
