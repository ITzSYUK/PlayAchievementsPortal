BUILD ?= ../../build/ingame-host
CC ?= cc
CPPFLAGS += -I../../online/include
CFLAGS ?= -O2 -g -std=c99 -Wall -Wextra -Werror
.PHONY: test
test: $(BUILD)/test_log_sync
$(BUILD)/test_log_sync: test_log_sync.c ../psp/ingame/https/user/log_sync.h
	mkdir -p $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) test_log_sync.c $(LDFLAGS) -o $@
test: $(BUILD)/test_client_mode
$(BUILD)/test_client_mode: test_client_mode.c client_mode.h
	mkdir -p $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) test_client_mode.c $(LDFLAGS) -o $@
test: $(BUILD)/test_frame_engine
$(BUILD)/test_frame_engine: test_frame_engine.c client_shared.h ../psp/ingame/https/user/frame_engine.h
	mkdir -p $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) test_frame_engine.c -pthread $(LDFLAGS) -o $@
test: $(BUILD)/test_frame_timeline
$(BUILD)/test_frame_timeline: test_frame_timeline.c frame_timeline.h
	mkdir -p $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) test_frame_timeline.c $(LDFLAGS) -o $@
test: $(BUILD)/test_frame_candidate $(BUILD)/test_game_code_audit $(BUILD)/test_game_memory $(BUILD)/test_policy $(BUILD)/test_format $(BUILD)/test_kernel_log $(BUILD)/test_power $(BUILD)/test_extra_ram $(BUILD)/test_placement2 $(BUILD)/test_lite_placement $(BUILD)/test_client_cleanup $(BUILD)/test_https_bootstrap $(BUILD)/test_https_stop $(BUILD)/test_early_network $(BUILD)/test_apctl_connect $(BUILD)/test_transport_stop
	$(BUILD)/test_frame_candidate
	$(BUILD)/test_game_code_audit
	$(BUILD)/test_game_memory
	$(BUILD)/test_policy
	$(BUILD)/test_format
	$(BUILD)/test_kernel_log
	$(BUILD)/test_power
	$(BUILD)/test_extra_ram
	$(BUILD)/test_placement2
	$(BUILD)/test_lite_placement
	$(BUILD)/test_client_cleanup
	$(BUILD)/test_https_bootstrap
	$(BUILD)/test_https_stop
	$(BUILD)/test_early_network
	$(BUILD)/test_apctl_connect
	$(BUILD)/test_transport_stop
	$(BUILD)/test_frame_timeline
	$(BUILD)/test_frame_engine
	$(BUILD)/test_client_mode
	$(BUILD)/test_log_sync
	python3 test_iso_reference.py
	python3 test_game_code_decoder.py
$(BUILD)/test_policy: test_policy.c probe_policy.c probe_policy.h
	mkdir -p $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) test_policy.c probe_policy.c $(LDFLAGS) -o $@
$(BUILD)/test_format: test_format.c kernel_format.c kernel_format.h
	mkdir -p $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) test_format.c kernel_format.c $(LDFLAGS) -o $@
$(BUILD)/test_kernel_log: ../host/test_probe_log.c ../common/probe_log.c kernel_format.c kernel_format.h ../common/probe_log.h
	mkdir -p $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -I../common -Dvsnprintf=ra_kernel_vsnprintf ../host/test_probe_log.c ../common/probe_log.c kernel_format.c $(LDFLAGS) -o $@
$(BUILD)/test_power: test_power.c probe_power.c probe_power.h
	mkdir -p $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) test_power.c probe_power.c $(LDFLAGS) -o $@
$(BUILD)/test_extra_ram: test_extra_ram.c extra_ram_policy.c extra_ram_policy.h
	mkdir -p $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) test_extra_ram.c extra_ram_policy.c $(LDFLAGS) -o $@
$(BUILD)/test_placement2: test_placement2.c placement2_policy.c placement2_policy.h
	mkdir -p $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) test_placement2.c placement2_policy.c $(LDFLAGS) -o $@
$(BUILD)/test_lite_placement: test_lite_placement.c lite_test_sdk.h ../psp/ingame/lite/main.c placement2_policy.c placement2_policy.h extra_ram_policy.c extra_ram_policy.h ../common/probe_log.c ../common/probe_log.h kernel_format.c kernel_format.h
	mkdir -p $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -I. -I../common -Dvsnprintf=ra_kernel_vsnprintf test_lite_placement.c placement2_policy.c extra_ram_policy.c ../common/probe_log.c kernel_format.c $(LDFLAGS) -o $@
$(BUILD)/test_client_cleanup: test_client_cleanup.c lite_test_sdk.h client_shared.h ../psp/ingame/client/kernel/main.c ../common/probe_log.c ../common/probe_log.h placement2_policy.c placement2_policy.h
	mkdir -p $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -I. -I../common -ffunction-sections -fdata-sections test_client_cleanup.c ../common/probe_log.c placement2_policy.c -Wl,--gc-sections $(LDFLAGS) -o $@
$(BUILD)/test_https_bootstrap: game_code_audit.c game_code_audit.h ../psp/ingame/https/kernel/game_audit.h ../../online/src/ra_game_memory.c ../../online/src/ra_psp_memory.c test_client_cleanup.c lite_test_sdk.h client_shared.h ../psp/ingame/client/kernel/main.c ../common/probe_log.c ../common/probe_log.h placement2_policy.c placement2_policy.h
	mkdir -p $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -DRA_CLIENT_HTTPS -I. -I../common -ffunction-sections -fdata-sections game_code_audit.c ../../online/src/ra_game_memory.c ../../online/src/ra_psp_memory.c test_client_cleanup.c ../common/probe_log.c placement2_policy.c -Wl,--gc-sections $(LDFLAGS) -o $@
$(BUILD)/test_https_stop: test_https_stop.c lite_test_sdk.h client_shared.h ../psp/ingame/https/user/stop_owner.h
	mkdir -p $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -I. test_https_stop.c $(LDFLAGS) -o $@
$(BUILD)/test_early_network: test_early_network.c client_shared.h ../psp/ingame/https/user/early_network.h ../../online/src/ra_net_resources.c ../../online/include/ra_net_resources.h
	mkdir -p $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -I. -I../../online/include test_early_network.c ../../online/src/ra_net_resources.c $(LDFLAGS) -o $@
$(BUILD)/test_apctl_connect: test_apctl_connect.c apctl_events.h client_shared.h ../psp/ingame/https/user/apctl_observer.h ../psp/ingame/https/user/connect_profile.h ../../online/src/ra_net_resources.c ../../online/include/ra_net_resources.h
	mkdir -p $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -I. -I../../online/include test_apctl_connect.c ../../online/src/ra_net_resources.c $(LDFLAGS) -o $@
$(BUILD)/test_transport_stop: test_transport_stop.c lite_test_sdk.h ../psp/ingame/https/user/stop_transport.h
	mkdir -p $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -I. test_transport_stop.c $(LDFLAGS) -o $@

$(BUILD)/test_game_memory: test_game_memory.c frame_observation.c frame_observation.h ../../online/src/ra_game_memory.c ../../online/src/ra_psp_memory.c ../../online/include/ra_game_memory.h ../../online/include/ra_psp_memory.h
	mkdir -p $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) test_game_memory.c frame_observation.c ../../online/src/ra_game_memory.c ../../online/src/ra_psp_memory.c $(LDFLAGS) -o $@

$(BUILD)/test_game_code_audit: test_game_code_audit.c game_code_audit.c game_code_audit.h ../../online/src/ra_game_memory.c ../../online/src/ra_psp_memory.c
	mkdir -p $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) test_game_code_audit.c game_code_audit.c ../../online/src/ra_game_memory.c ../../online/src/ra_psp_memory.c $(LDFLAGS) -o $@

$(BUILD)/test_frame_candidate: test_frame_candidate.c frame_candidate.c frame_candidate.h
	mkdir -p $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) test_frame_candidate.c frame_candidate.c $(LDFLAGS) -o $@
