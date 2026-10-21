# PlayAchievementsPortal

**RetroAchievements on a real PSP — experimental online client, version 0.51 (hardware validation pending). Repairs bootstrap log recovery after sleep and records its stop reason independently. Persistent pending records, specific error details and PBAD support remain. DNS cancellation and full HOME cleanup still need hardware validation.**

[English](#english) · [Русский](#русский)

## English

### What this project does

PlayAchievementsPortal brings online RetroAchievements to real PSP hardware running custom firmware. The PSP client is currently named **RAClientLite**. It uses the official `rcheevos` / `rc_client` engine to evaluate achievement conditions, log in, load the server's achievement set and progress, and submit softcore unlocks over verified HTTPS.

This is a focused hardware prototype, not a general-purpose PSP release. Development currently targets one verified LocoRoco image. The project is based on [PSPAchievementsSystem](https://github.com/avetis-dev/PSPAchievementsSystem); the original offline plugin remains in this repository, but the online client has its own modules, configuration and build script. This project is independent of RetroAchievements and is not an official client endorsed by the service.

### Current status

| Item | Status |
|---|---|
| Tested hardware | PSP-3000, firmware 6.60 + ARK 5.1.0, extra memory **+16 MiB**, Memory Stick `ms0:` |
| Supported image | **LocoRoco USA**, PSP ID `UCUS98662`, RetroAchievements game ID **10523** |
| Verified official image hash | `199515744d49ef3620a5f9297c6116f9` |
| Server set | 101 achievements loaded; one currently reported as unsupported |
| Real online unlock | Achievement **397986, “Bu Bu Poruche”**, confirmed by the server in softcore on real hardware |
| Menu and notifications | Earlier UI tested on PSP; 0.49 badges/position/details have host coverage only |
| Encore | Local replay of previously unlocked achievements tested on PSP |
| Initial Wi-Fi recovery | Logs confirm a failed connection followed by an automatic successful retry |
| Longer recovery scenarios | Implemented and tested with synthetic responses; extended hardware validation remains |

The 0.41 hardware logs show a successful RA login, the 101-entry catalog and repeated menu open/close operations. The owner also reported three successive game launches without rebooting the PSP. One earlier freeze at the PSP launch screen remains unexplained; this is **not a stability guarantee**.

### Features and limits

- Session-long achievement evaluation, with a menu opened by holding **L + R + SELECT** for about 400 ms.
- Server-confirmed progress, an achievement list, filters and status cards. A local trigger or a popup alone does not prove that the server accepted an unlock.
- Separate network worker, certificate verification, bounded responses and queues, and memory-range checks.
- Wi-Fi reconnect and retries for temporary login/set-loading failures. Startup retries use delays of 1, 2, 4, 8, 16, 32 and then 60 seconds. Achievement submissions use the official engine's retry scheduling.
- Softcore pending submissions are also saved by the worker in two authenticated `pending-*.raq` files, bound to the authenticated account, image hash and achievement definitions. On restart, the fresh server progress is loaded before replay. Up to 128 entries, maximum age seven days; changed token, damaged records, changed definitions and invalid timestamps are rejected and retained without overwriting. Nothing is imported from offline `.dat` files. A sudden power loss before the worker flush can lose the newest entries.
- Restart replay is experimental: the [RA FAQ](https://docs.retroachievements.org/general/faq.html#can-i-earn-achievements-offline) only describes reconnecting while the emulator stays open. Storage and an HTTP response do not guarantee accepted credit; policy compatibility of restart replay remains unconfirmed.
- Before the catalog is ready, the hotkey displays **ACHIEVEMENTS LOADING** without pausing the game. Once loaded, it opens the normal menu.

There is no hardcore, leaderboard, multi-account, Vita/Adrenaline, PSP Go internal-storage or general multi-game support. Renaming an image or plugin entry does not make another region, revision, translation or mod compatible. Old offline `.dat` profiles are not uploaded to RA.

Sleep immediately pauses achievement evaluation. Recovery starts from RESUMING, waits for transport cancellation, verifies the image/code/memory and resets Delta/hit history without replaying missed frames. Long challenges may need to be restarted. The worker restores its append log; 0.51 also reopens a failed bootstrap log descriptor before treating the write as fatal. Two 0.50 hardware runs passed the resume checks but stopped before reconnection completed; the bootstrap failure path was reproduced on host. Version 0.51 still needs PSP validation. A failed memory check stops the client. Full HOME cleanup and broader validation of the LocoRoco CANDIDATE frame binding remain open.

### Build and install

1. Build the online package using the commands below. The generated archive is `build/client-https/PSP_RA_HTTPS_v0.51.zip`; build outputs are not committed.
2. On the tested PSP configuration, enable ARK's **Use Extra Memory → +16 MiB** and create a working PSP Wi-Fi profile.
3. For a fresh installation, copy the archive's `SEPLUGINS/RAClientLite/` directory to `ms0:/SEPLUGINS/RAClientLite/`. Set `profile.txt` to your actual saved Wi-Fi profile number. The packaged value is `1`.
4. Register **only the bootstrap module**, restricted to the supported game, using the ARK plugin configuration:

   ```text
   UCUS98662, ms0:/seplugins/RAClientLite/RAClientBootstrap.prx, on
   ```

   `RAClientUser.prx` is loaded by the bootstrap; do not register it separately. The same entry is included as `INSTALL_ENTRY.txt` in the package.
5. Create your session-token file with the interactive helper and an existing Python 3 interpreter. On a PC where the Memory Stick is `D:`:

   ```powershell
   python scripts/configure_psp_login.py --output "D:\SEPLUGINS\RAClientLite\auth.txt"
   ```

   Enter the username and password interactively, not as command-line arguments. The helper stores a **session token**, not the password or a Web API key. `auth.txt` is a secret file on the Memory Stick, not protected credential storage. Never commit it or send the whole client directory to someone.
6. Choose the mode, fully restart the PSP, and launch the verified LocoRoco image. Wait for **ACHIEVEMENTS LOADED** before opening the list.

**Upgrading:** replace both PRX files together and keep your existing `auth.txt`, `profile.txt`, `mode.txt`, `duration.txt` and `encore.txt`. Do not overwrite your settings with the archive defaults.

To remove this client's stored login, close the PSP game and run the helper with the same `--output` path plus `--logout`.

### Configuration and controls

All configuration files are plain text in `ms0:/SEPLUGINS/RAClientLite/` and are read when the client starts.

| File | Values and meaning |
|---|---|
| `profile.txt` | Saved PSP Wi-Fi profile number, `1`–`32`; no automatic profile selection |
| `mode.txt` | `spectator` for local condition testing without submissions; `softcore` for real server unlocks. Packaged default: `spectator` |
| `duration.txt` | `session` for the whole session, or `30`–`300` seconds for a timed diagnostic run. Missing file also means `session` |
| `encore.txt` | `0` off, `1` on; requires `softcore`. Packaged default: `0` |
| `auth.txt` | Secret session-token file created by the helper |

Spectator does not load the account's server unlock history in this client. Encore reactivates previously unlocked achievements for **local** replay; known unlocks are not sent to the server again. New achievements can still earn real softcore credit. Encore does not change the game's save or bypass an achievement's save protection: some conditions still require an earlier save or a new playthrough.

| Control | Action |
|---|---|
| Hold L + R + SELECT | Open/close the menu after the catalog loads |
| Up / Down | Select an achievement |
| Left / Right | Change page |
| Triangle | Filter: all / unconfirmed / confirmed |
| Square | Service warning and connection reason/code; press again to return |
| Circle | Close |

The menu pauses the game callback and achievement evaluation while network work continues through `rc_client_idle`. Cards appear at the **top right**. Errors show the Wi-Fi/APCTL, DNS, TCP, TLS/certificate, timeout, login, set, storage or resume reason and numeric code. **WI-FI CONNECTED** means an IP was obtained; **RA SERVER REACHABLE** means verified HTTPS/HTTP 200, not an award. **CLIENT WARNING** shows the actual service warning; Square opens its details.

Badges reuse the original project's **PBAD v1** format. Put a prepared pack at `ms0:/SEPLUGINS/RAClientLite/badges/UCUS-98662.pbad`. Existing `tools/compile_badges.py` converts a local `achievementsets` JSON with BadgeName metadata into a pack (Python 3 + Pillow; it downloads artwork). This client accepts up to 128 badges, 263728 bytes, 32×32 RGB565, matched by achievement ID. The menu scales them to 14×14; popups use 32×32. Locked badges are grayscale. Missing artwork uses a placeholder; invalid/oversized packs report an error. Artwork and automatic PNG downloading are **not included in the ZIP**; without a prepared pack only placeholders appear.

```sh
python3 tools/compile_badges.py /absolute/private/sets.json UCUS-98662 build/UCUS-98662.pbad --cache-dir build/badge-cache
```

### Development and diagnostics

Host tests require GCC, Make and Python 3; development here uses Ubuntu under WSL. The PSP build uses Docker with the immutable PSPDEV image pinned in [dependencies.lock.json](dependencies.lock.json), currently GCC 15.2.0. The image must already be present locally; the build uses `--pull=never` and no container networking.

```sh
# Ubuntu / WSL, from the repository root
make online-test ingame-test public-check
```

```powershell
# Windows PowerShell, from the repository root; requires WSL Ubuntu + Docker
./scripts/build_client_https.ps1
```

The PSP build checks module exports, the heap, imports, PRX file size and the **final PRX PT_LOAD memory**, including padding and BSS. Root `make` still builds the inherited offline plugin; use the script above for the online client. Host tests use synthetic server responses and do not award achievements to a real account. PPSSPP is useful for diagnostics but does not replace testing on a PSP.

The client keeps `client-bootstrap.log` and `client-user.log` plus three earlier pairs (`.1`–`.3`), with matching `LOG_SESSION` IDs. Version 0.42 hardware logs confirmed sessions 1–3 and preservation of the legacy 0.41 pair. Save all eight files when reporting an issue; include PSP/CFW, image revision, client version and preceding actions. Never include `auth.txt`. Each log is bounded to 128 KiB, with space reserved for teardown.

Version 0.43 adds native user `module_reboot_before` and kernel `module_reboot_phase(3)` notifications without replacing the game's exit callback. The user notification requests a stop without waiting. Before user threads are suspended, the kernel writes `EXIT_NOTICE` directly to the bootstrap log, independently of the worker, using a separate 1 KiB reserve. `source=kernel_reboot_phase3` proves receipt of a LoadExec teardown notification; it does not prove that HOME was the cause or cleanup completed. `user_flags` bit 1 means user reboot notification, bit 2 means user `module_stop`; `worker_done=1` reports the owner's cleanup acknowledgement. Early `kernel_module_stop_enter/wait/cleanup` notices distinguish that separate path. Filesystem writeback is best effort; storage I/O has no hard deadline.

Version 0.44 addresses the silent-start regression reported with 0.43: the kernel no longer imports `sceIoSync`; session log creation uses the proven 0.42 flags, followed by a separate append open. A failed append open is recorded in the initial log before any user module is loaded. `EXIT_NOTICE` closes its own descriptor; only the existing user worker performs periodic device sync. The exact failing 0.43 operation remains unknown because no logs were created.

Version 0.44 hardware logs now confirm exactly eight files after launch 5: matching bootstrap/user pairs 5/4/3/2, with pair 1 removed. Launch numbering, rotation, startup stages and exit notifications are verified for this prototype. Full HOME cleanup is a separate open task.

Version 0.47 corrects the socket compatibility regression: successful SO_NONBLOCK setup is required; its value/size readback is logged by SOCKET_MODE without rejecting PSP-specific results. Every send/recv retains `MSG_DONTWAIT`, linger is disabled and poll uses zero timeout plus yields of at most 50 ms. DNS runs in one temporary helper with module-owned buffers and an 8 KiB stack. Transport waits up to five seconds (or the remaining request deadline), then requests sceNetResolverStop and allows one second to confirm STOPPED plus completion before deleting the helper. Only after that can the existing retry use new resources. If completion is not confirmed, -35424 stops the client while retaining live resources; the helper is never forcibly killed. The 30-second request deadline and existing retry scheduling remain; retries use a fresh connection only after the old transport releases its resources. No thread is forcibly terminated and live buffers are never reassigned. This is not a hard deadline guarantee for arbitrary PSP/CFW calls: DNS, TLS computation or cleanup can still require hardware diagnosis.

To test 0.51, install both PRX files together, keep your settings and logs, explicitly set `mode.txt` to `spectator` **and `encore.txt` to `0`**, restart the PSP and run the verified image three times. Encore=1 with spectator aborts startup with -35421 before notifications. Keep Wi-Fi on; allow up to three minutes per launch, record whether **ACHIEVEMENTS LOADED** appears and whether the game remains responsive, then exit through HOME. This three-minute observation window is not a client timeout or success guarantee. Save all eight logs after the third run; never include `auth.txt`. Spectator omits softcore `startsession`, so successful spectator runs alone do not establish that the softcore problem is fixed.

`TRANSPORT_STATE` samples the network thread during startup every five seconds. `location=2*N+1` means a call was entered, `2*N` means it returned; N: 1 resolver create, 2 DNS lookup, 3 socket create, 4 socket setup, 5 connect, 6 connect error query, 7 TLS, 8 send, 9 recv, 10 poll/yield, 11 socket close, 12 resolver delete, 13 resolver Stop. This is the last recorded call, not a complete TLS stack trace. `actions` counts entries; `thread_query`, `status`, `wait_type`, `exit_status` distinguish a query failure or stopped/waiting/running thread. Fields are sampled independently. Frozen counters and an entered call help locate a stall; they do not establish its cause. SOCKET_MODE reports the setter/query results, raw value and returned size. DNS_STATE reports helper UID, completion and cancellation flags; DNS_RESULT reports elapsed time, resolver result and Stop attempts. Result -35425 means no result was collected. The Stop call itself has no proven hard deadline on the tested CFW; retained resources are not a completed cleanup.

`REQUEST_TRACE` uses numeric `kind`: 1 login, 2 achievement sets, 3 start session, 4 ping, 5 award; `stage`: 1 queued, 2 offered to transport, 3 response delivered to adapter (or queued request expired), 4 callback begin, 5 callback end. `age_ms` measures time since enqueue; `time_ms32` wraps after about 49 days. `HTTP_TRANSFER` reports actual transport duration including I/O release. `STARTUP_WAIT` every five seconds reports pending operations, copy errors and mailbox state (0 idle, 1 offered, 2 active, 3 response waiting). The bounded 16-event buffer overwrites oldest events with a `trace_dropped` counter; trace events contain only numbers and are written to disk by the worker, outside game callbacks. Buffered markers may be lost on exit or if the owner stalls; absence of callback-end alone does not prove its cause. REQUEST_TRACE is drained only during startup, with a final drain before frame evaluation begins, to avoid worker contention on the game callback semaphore. Request timeouts, TLS verification and shutdown waits are unchanged.

For the 0.51 sleep check, keep spectator/encore=0. After ACHIEVEMENTS LOADED, open and close the menu, suspend with Power for 10–20 seconds, resume and check reconnection and the menu. Repeat once, then exit through HOME and save all eight logs. POWER_WAKE records callback flags and append-log recovery; RESUME_STAGE: 1 log, 2 transport, 3 display, 4 image, 5 memory/history, 6 checks passed. RESUME_OK alone does not prove Wi-Fi recovery: check GOT_IP/WIFI_RECONNECTED. In spectator with no pending requests, a new HTTPS response may not appear; RA reachability is established by the next permitted request. BOOT_LOG_REOPEN reports restoration of the bootstrap log. EXIT_NOTICE retains user_result/power_flags/resume_stage plus bootstrap_result/bootstrap_log_error/log_reopens/reopen_error if either main log fails.

Interpret each session's matching log pair separately:

| Evidence | Conclusion |
|---|---|
| `EXIT_NOTICE source=kernel_reboot_phase3` | System process-exit notification received; HOME as the cause and completed cleanup are not established |
| `worker_done=1` | User owner acknowledged its cleanup; bootstrap cleanup and final process teardown are not established |
| `LIFECYCLE stage=user_cleanup_end result=0` / `stage=bootstrap_cleanup_end result=0` | That component's cleanup code reached its successful end; final log close and thread termination still follow |
| Cleanup marker with a negative result | Cleanup attempt ended with an error; resources may remain |
| No process-exit notice and no successful cleanup evidence | End cause unknown; a truncated log alone does not prove a crash |

Keep exit notification and cleanup status as separate observations: a session can have a confirmed exit notice and unconfirmed cleanup. `client_finished` or `client_bootstrap_finished` alone is insufficient; inspect `cleanup_result` and subsequent stages. Full cleanup during HOME remains a separate lifecycle task. First measure cleanup duration and identify surviving threads; do not add forced worker waits to the exit notification. The earlier launch-screen freeze remains unexplained; preserve the history if it recurs.

Source map: `online/` contains the portable adapters, `experiments/psp/ingame/https/` the PSP modules, `experiments/ingame/` and `experiments/host/` the host tests, and `plugin/` the inherited offline implementation.

- [0.42: bounded launch history and lifecycle stages](docs/ONLINE_CLIENT_042.md)
- [0.41: startup menu fix](docs/ONLINE_CLIENT_041.md)
- [0.40: Wi-Fi/RA recovery and status cards](docs/ONLINE_CLIENT_040.md)
- [0.37: Encore behavior](docs/ONLINE_CLIENT_037.md)
- [0.34: hardware-confirmed softcore unlock](docs/ONLINE_CLIENT_034.md)
- [Development context](docs/NEXT_CHAT.md) · [Technical audit](docs/ONLINE_AUDIT.md) · [Development plan](DEVELOPMENT_PLAN.md)

Versioned documents describe the state at that point in development; older instructions may be superseded by this README.

### Credits and licensing

Based on [avetis-dev/PSPAchievementsSystem](https://github.com/avetis-dev/PSPAchievementsSystem). The upstream project has **not selected an open-source license**; public source availability does not grant unrestricted reuse or redistribution. This fork does not replace that notice or claim new rights over upstream code. See [LICENSE_NOTICE.md](LICENSE_NOTICE.md).

Pinned third-party dependencies retain their own terms: [rcheevos — MIT](vendor/rcheevos/LICENSE), [Mbed TLS — Apache-2.0 option](vendor/mbedtls/LICENSE), [CA bundle — MPL-2.0](vendor/ca/LICENSE.txt), and [PSPSDK notices](vendor/pspsdk/LICENSE). No game images, captured achievement definitions, account data or credentials are included in the source publication.

## Русский

### О проекте

PlayAchievementsPortal — экспериментальный онлайн-клиент RetroAchievements для настоящей PSP с кастомной прошивкой. На консоли клиент называется **RAClientLite**. Официальный движок `rcheevos` / `rc_client` проверяет условия достижений, выполняет вход, загружает серверный набор и прогресс и отправляет softcore-открытия по HTTPS с проверкой сертификатов.

Сейчас это прототип для одной проверенной игры. Проект основан на [PSPAchievementsSystem](https://github.com/avetis-dev/PSPAchievementsSystem). Исходный офлайн-плагин остаётся в репозитории, но онлайн-клиент использует отдельные модули, настройки и сборку. Проект независим от RetroAchievements и не заявляет официального одобрения сервисом.

### Что подтверждено

| Параметр | Состояние |
|---|---|
| Консоль | PSP-3000, прошивка 6.60 + ARK 5.1.0, дополнительная память **+16 MiB**, Memory Stick `ms0:` |
| Игра | **LocoRoco USA**, PSP ID `UCUS98662`, RetroAchievements ID **10523** |
| Проверенный официальный хеш образа | `199515744d49ef3620a5f9297c6116f9` |
| Набор | Загружается 101 достижение; одно сейчас определяется как неподдерживаемое |
| Онлайн-зачёт | Достижение **397986 «Bu Bu Poruche»** подтверждено сервером в softcore на настоящей PSP |
| Интерфейс | Предыдущий UI проверен на PSP; значки, позиция и подробности 0.49 пока проверены только на ПК |
| Encore | Локальный повтор уже открытого достижения проверен на PSP |
| Повтор первого подключения | Логи подтверждают успешное автоматическое подключение после неудачной попытки |
| Другие сценарии восстановления | Реализованы и проверены на синтетических ответах; длительные проверки на PSP ещё нужны |

Логи 0.41 подтверждают вход, загрузку 101 достижения и повторное открытие/закрытие меню. Владелец также сообщил о трёх последовательных запусках без перезагрузки PSP. Причина одного более раннего зависания на заставке PSP пока не установлена; стабильность всех запусков не гарантируется.

### Возможности и ограничения

- Проверки работают всю игровую сессию. Меню открывается удержанием **L + R + SELECT** примерно 400 мс.
- Список использует серверный прогресс. Выполнение условия и появление карточки сами по себе не доказывают зачёт: необходимо подтверждение RA.
- Сеть обслуживает отдельный поток. Есть проверка сертификатов, границ памяти, размеров ответов и очередей.
- При временных ошибках повторяются подключение Wi-Fi, вход и загрузка набора. Задержки запуска: 1, 2, 4, 8, 16, 32 и далее 60 секунд. Для отправки достижений используется расписание повторов официального движка.
- Worker сохраняет pending softcore в двух файлах `pending-*.raq`, привязанных к подтверждённому аккаунту, хешу образа и определениям достижений. После перезапуска сначала загружается свежий прогресс сервера. Предел — 128 записей и семь дней; смена токена, повреждение, другие условия и некорректное время запрещают повтор и перезапись файлов. Старые `.dat` не импортируются. Выключение до записи worker может потерять последние события.
- Повтор после перезапуска экспериментальный: [FAQ RA](https://docs.retroachievements.org/general/faq.html#can-i-earn-achievements-offline) описывает переподключение при открытом эмуляторе. Совместимость повтора после закрытия с правилами сервиса не подтверждена; сохранение и HTTP-ответ не гарантируют зачёт.
- До готовности набора сочетание показывает **ACHIEVEMENTS LOADING**, без паузы игры. После загрузки открывается обычное меню.

Не поддерживаются hardcore, таблицы рекордов, несколько аккаунтов, Vita/Adrenaline, внутреннее хранилище PSP Go и произвольные игры. Другой регион, ревизия, перевод или модификация требуют отдельной проверки; переименование файлов совместимость не добавляет. Старые офлайн-профили `.dat` не переносятся на сервер.

Сон немедленно приостанавливает оценку достижений. Восстановление запускается по RESUMING, ждёт отмены транспорта, проверяет образ/код/память и сбрасывает Delta/hit-историю без воспроизведения пропущенных кадров. Длительное испытание может потребовать начать заново. Worker восстанавливает журнал на добавление; 0.51 также переоткрывает отказавший дескриптор bootstrap до признания записи фатальной. В двух аппаратных сессиях 0.50 проверки пробуждения прошли, но клиент остановился до завершения переподключения; соответствующий путь отказа bootstrap воспроизведён на ПК. 0.51 ещё требует проверки на PSP. Отказ проверки памяти останавливает клиент. Полная очистка HOME и дальнейшая проверка привязки кадров LocoRoco CANDIDATE остаются открытыми.

### Сборка и установка

1. Собери онлайн-клиент командами ниже. Архив появится в `build/client-https/PSP_RA_HTTPS_v0.51.zip`; готовые сборки в Git не добавляются. Добавлены очередь на карте, проверяемое восстановление после сна, причины ошибок и PBAD-значки. На PSP 0.49 загрузила каталог и меню, но сон/пробуждение не восстановились. В 0.50 проверки пробуждения прошли, но переподключение обрывалось остановкой; исправление 0.51 ещё требует PSP. На PSP в 0.46 выявлены несовместимость проверки сокета и задержка DNS; полная очистка при HOME не подтверждена.
2. На проверенной конфигурации PSP включи в ARK **Use Extra Memory → +16 MiB** и создай рабочий профиль Wi-Fi.
3. При первой установке скопируй папку `SEPLUGINS/RAClientLite/` из архива в `ms0:/SEPLUGINS/RAClientLite/`. В `profile.txt` укажи номер своего сохранённого профиля Wi-Fi. Значение в архиве — `1`.
4. В настройках плагинов ARK добавь запись только для bootstrap и указанной игры:

   ```text
   UCUS98662, ms0:/seplugins/RAClientLite/RAClientBootstrap.prx, on
   ```

   `RAClientUser.prx` загружается автоматически; отдельно включать его не нужно. Эта же строка находится в `INSTALL_ENTRY.txt` архива.
5. Создай файл токена интерактивным помощником, используя имеющийся Python 3. Пример для карты под буквой `D:`:

   ```powershell
   python scripts/configure_psp_login.py --output "D:\SEPLUGINS\RAClientLite\auth.txt"
   ```

   Имя и пароль вводятся по запросу, а не в командной строке. Сохраняется **токен сессии**, не пароль и не Web API key. `auth.txt` — секретный файл на карте, без защищённого хранилища. Не публикуй его и не передавай всю папку клиента.
6. Выбери режим, полностью перезагрузи PSP и запусти проверенный образ LocoRoco. Дождись **ACHIEVEMENTS LOADED**, затем открывай список.

**Обновление:** заменяй оба PRX вместе, сохраняя свои `auth.txt`, `profile.txt`, `mode.txt`, `duration.txt` и `encore.txt`. Не перезаписывай настройки значениями из архива.

Для удаления сохранённого входа закрой игру и запусти помощник с тем же `--output`, добавив `--logout`.

### Настройки и управление

Текстовые файлы лежат в `ms0:/SEPLUGINS/RAClientLite/` и читаются при запуске клиента.

| Файл | Значение |
|---|---|
| `profile.txt` | Номер сохранённого профиля Wi-Fi, `1`–`32`; автоматического выбора нет |
| `mode.txt` | `spectator` — проверка условий без отправки; `softcore` — настоящий серверный зачёт. В архиве: `spectator` |
| `duration.txt` | `session` — вся сессия; число `30`–`300` — ограниченная диагностика в секундах. Отсутствие файла тоже означает `session` |
| `encore.txt` | `0` — выключен, `1` — включён; требует `softcore`. В архиве: `0` |
| `auth.txt` | Секретный токен сессии, создаваемый помощником |

В spectator этот клиент не загружает историю серверных открытий аккаунта. Encore повторно активирует уже полученные достижения **локально**, без повторной отправки известных открытий. Новые достижения по-прежнему могут реально засчитываться в softcore. Encore не меняет сохранение и не отменяет защиту условий: иногда требуется более раннее сохранение или новое прохождение.

| Кнопки | Действие |
|---|---|
| Удержание L + R + SELECT | Открыть/закрыть меню после загрузки набора |
| Вверх / вниз | Выбор достижения |
| Влево / вправо | Переключение страницы |
| Triangle | Все / неподтверждённые / подтверждённые |
| Circle | Закрыть меню |

Меню приостанавливает игровой callback и оценку условий; сеть обслуживается через `rc_client_idle`. Карточки находятся **справа сверху** и игру не останавливают. Ошибки показывают причину Wi-Fi/APCTL, DNS, TCP, TLS/сертификата, тайм-аута, входа, набора, карты памяти или пробуждения и числовой код. **WI-FI CONNECTED** означает получение IP, **RA SERVER REACHABLE** — проверенный HTTPS/HTTP 200, а не зачёт. **CLIENT WARNING** показывает текст служебного предупреждения. **Квадрат** открывает подробности предупреждения и соединения, повторное нажатие возвращает список.

Значки используют исходный **PBAD v1**. Подготовленный пакет положи в `ms0:/SEPLUGINS/RAClientLite/badges/UCUS-98662.pbad`. Штатный `tools/compile_badges.py` создаёт его из локального JSON `achievementsets` с BadgeName (Python 3 + Pillow, изображения скачиваются). Онлайн-клиент принимает до 128 значков, 263728 байт, 32×32 RGB565 и сопоставляет их по ID достижения. В меню размер 14×14, в уведомлении 32×32; закрытые значки серые. При отсутствии изображения выводится заглушка, повреждённый/слишком большой пакет даёт ошибку. **В ZIP нет изображений и автоматического скачивания PNG: без подготовленного PBAD будут заглушки.**

### Разработка и диагностика

Для переносимых тестов нужны GCC, Make и Python 3; в этой разработке используется Ubuntu WSL. PSP-сборка выполняется через Docker с закреплённым в [dependencies.lock.json](dependencies.lock.json) образом PSPDEV, GCC 15.2.0. Образ должен быть заранее доступен локально: сборка использует `--pull=never` и отключённую сеть контейнера.

```sh
# Ubuntu / WSL, из корня репозитория
make online-test ingame-test public-check
```

```powershell
# Windows PowerShell, из корня; нужны WSL Ubuntu и Docker
./scripts/build_client_https.ps1
```

Проверяются exports, heap, импорты, размер PRX и память **конечного сегмента PT_LOAD**, включая промежутки и BSS. Обычный `make` в корне собирает исходный офлайн-плагин; для онлайн-клиента нужен указанный скрипт. Тесты на компьютере используют синтетические серверные ответы и ничего не начисляют реальному аккаунту. PPSSPP помогает диагностике, но не заменяет настоящую PSP.

Рядом с PRX сохраняются `client-bootstrap.log`, `client-user.log` и три предыдущие пары (`.1`–`.3`) с общим номером `LOG_SESSION`. На PSP в 0.42 подтверждены сессии 1–3 и сохранение старой пары 0.41. При проблеме сохрани все восемь файлов, укажи PSP/CFW, ревизию образа, версию клиента и предшествующие действия. Не прикладывай `auth.txt`. Каждый лог ограничен 128 КиБ с резервом для остановки.

В 0.43 добавлены штатные user `module_reboot_before` и kernel `module_reboot_phase(3)`, без замены exit-callback игры. User-обработчик запрашивает остановку без ожидания. До приостановки user-потоков kernel пишет `EXIT_NOTICE` непосредственно в bootstrap-лог независимо от worker, с отдельным резервом 1 КиБ. `source=kernel_reboot_phase3` подтверждает получение уведомления LoadExec о завершении процесса; это не доказательство выхода именно через HOME или полной очистки. В `user_flags` бит 1 означает user reboot-уведомление, бит 2 — user `module_stop`; `worker_done=1` означает подтверждение очистки владельцем. Ранние `kernel_module_stop_enter/wait/cleanup` различают отдельный путь остановки модуля. Запись и синхронизация файлов выполняются по возможности, жёсткого тайм-аута I/O нет.

В 0.44 устранены два отличия раннего старта от рабочей 0.42: импорт `sceIoSync` из kernel убран, лог создаётся прежними флагами и затем отдельно открывается для дописывания. Ошибка второго открытия записывается в исходный лог до загрузки user-модуля. `EXIT_NOTICE` закрывает свой дескриптор; периодическую синхронизацию устройства по-прежнему выполняет существующий user-worker. Точная отказавшая операция 0.43 неизвестна: новые логи не появились.

Логи 0.44 на PSP подтвердили после пятого запуска ровно восемь файлов: совпадающие пары bootstrap/user с ID 5/4/3/2, пара 1 удалена. Нумерация, ротация, этапы старта и уведомления о выходе проверены для прототипа. Полная очистка при HOME — отдельная незакрытая задача.

0.47 исправляет несовместимость проверки сокета: успешный SO_NONBLOCK обязателен, но прочитанные значение и размер записываются в SOCKET_MODE без отказа из-за отличий PSP. Каждый send/recv сохраняет `MSG_DONTWAIT`, linger выключен, poll вызывается с нулевым ожиданием и паузами до 50 мс. DNS выполняется в одном временном потоке с буферами внутри модуля и стеком 8 КиБ. Транспорт ждёт до пяти секунд (либо оставшегося срока запроса), затем запрашивает sceNetResolverStop и отводит секунду на подтверждение STOPPED и завершения перед удалением потока. Только после этого существующий повтор может использовать новые ресурсы. Если завершение не подтверждено, -35424 останавливает клиент с удержанием живых ресурсов; поток принудительно не уничтожается. Срок запроса 30 секунд и существующий повтор сохранены; новый запрос использует новое соединение только после освобождения ресурсов старого. Принудительного завершения потока и передачи занятых буферов нет. Это не гарантия жёсткого срока произвольных PSP/CFW-вызовов: DNS, вычисления TLS и очистка ещё могут потребовать диагностики на железе.

Для проверки 0.51 замени оба PRX вместе, сохрани настройки и логи, явно запиши `spectator` в `mode.txt` **и `0` в `encore.txt`**, перезагрузи PSP и выполни три запуска проверенного образа. Encore=1 вместе со spectator останавливает запуск с -35421 до появления уведомлений. Wi-Fi не переключай; в каждом запуске наблюдай до трёх минут, отметь появление **ACHIEVEMENTS LOADED** и отзывчивость игры, затем выйди через HOME. Три минуты — окно наблюдения, а не тайм-аут клиента или гарантия успеха. После третьего запуска сохрани восемь логов, без `auth.txt`. В spectator нет softcore-запроса `startsession`: успех в spectator сам по себе не доказывает исправление проблемы softcore.

`TRANSPORT_STATE` раз в пять секунд при запуске показывает сетевой поток. `location=2*N+1` — вход в вызов, `2*N` — возврат; N: 1 — создание resolver, 2 — DNS, 3 — создание сокета, 4 — настройка сокета, 5 — connect, 6 — проверка ошибки connect, 7 — TLS, 8 — send, 9 — recv, 10 — poll/пауза, 11 — close, 12 — удаление resolver, 13 — Stop resolver. Это последний отмеченный вызов, а не полный стек TLS. `actions` — счётчик входов; `thread_query`, `status`, `wait_type`, `exit_status` помогают отличить ошибку опроса, завершённый, ожидающий и выполняющийся поток. Поля считываются по отдельности. Неизменный счётчик и отметка входа локализуют остановку, но не доказывают причину. SOCKET_MODE показывает результаты установки/чтения, значение и размер. DNS_STATE показывает UID потока, флаги завершения и отмены; DNS_RESULT — длительность, результат resolver и попытки Stop. -35425 означает, что результат не получен. Жёсткий срок самого Stop на CFW владельца не доказан; удержание ресурсов не считается завершённой очисткой.

В `REQUEST_TRACE` поле `kind`: 1 — вход, 2 — набор, 3 — старт сессии, 4 — ping, 5 — зачёт; `stage`: 1 — очередь, 2 — передача транспорту, 3 — ответ передан адаптеру (либо запрос просрочен в очереди), 4 — начало callback, 5 — конец callback. `age_ms` — время от постановки в очередь; `time_ms32` оборачивается примерно через 49 суток. `HTTP_TRANSFER` показывает длительность транспорта, включая освобождение I/O. `STARTUP_WAIT` раз в пять секунд показывает pending, ошибки копирования и mailbox: 0 — свободен, 1 — предложен запрос, 2 — транспорт работает, 3 — ответ ожидает передачи. Буфер на 16 числовых событий вытесняет старые со счётчиком `trace_dropped`; в файл пишет worker вне игрового callback. При выходе или остановке владельца буферизованные маркеры могут не сохраниться; отсутствие конца callback само по себе не устанавливает причину. REQUEST_TRACE выгружается только при запуске, с последней выгрузкой до включения оценки кадров, чтобы worker не конкурировал за семафор игрового callback. Тайм-ауты, TLS-проверка и ожидания остановки не изменены.

Проверка сна в 0.51: spectator/encore=0; дождись ACHIEVEMENTS LOADED, открой и закрой меню, усыпи Power на 10–20 секунд, разбуди и проверь переподключение и меню. Повтори цикл, затем выйди через HOME и сохрани восемь логов. POWER_WAKE содержит флаги и восстановление журнала; RESUME_STAGE: 1 — журнал, 2 — транспорт, 3 — display, 4 — образ, 5 — память/история, 6 — проверки пройдены. RESUME_OK сам по себе не доказывает восстановление Wi-Fi: проверь GOT_IP/WIFI_RECONNECTED. В spectator без ожидающих запросов нового HTTPS-ответа может не быть; доступность RA подтверждается следующим разрешённым запросом. BOOT_LOG_REOPEN показывает восстановление журнала bootstrap. В EXIT_NOTICE сохранены user_result/power_flags/resume_stage и bootstrap_result/bootstrap_log_error/log_reopens/reopen_error на случай отказа основного журнала.

Разбирай каждую пару с совпадающим номером сессии отдельно:

| Свидетельство | Вывод |
|---|---|
| `EXIT_NOTICE source=kernel_reboot_phase3` | Получено системное уведомление о завершении процесса; причина HOME и завершение очистки не установлены |
| `worker_done=1` | Владелец user подтвердил свою очистку; очистка bootstrap и окончательное завершение процесса не установлены |
| `LIFECYCLE stage=user_cleanup_end result=0` / `stage=bootstrap_cleanup_end result=0` | Код очистки соответствующего компонента дошёл до успешного конца; закрытие лога и завершение потока идут далее |
| Маркер очистки с отрицательным результатом | Попытка очистки закончилась ошибкой; ресурсы могут остаться |
| Нет уведомления о завершении процесса и свидетельств успешной очистки | Причина окончания неизвестна; обрыв лога сам по себе не доказывает сбой |

Уведомление о выходе и состояние очистки — отдельные признаки: уведомление может быть подтверждено при неподтверждённой очистке. Одной строки `client_finished` или `client_bootstrap_finished` недостаточно: проверяй `cleanup_result` и последующие этапы. Полная очистка при HOME остаётся отдельной задачей жизненного цикла. Сначала нужны замеры времени очистки и проверка оставшихся потоков; принудительное ожидание worker в уведомлении о выходе не добавляется. Старое зависание на заставке остаётся необъяснённым; при повторении сохрани историю.

Структура: `online/` — переносимые адаптеры; `experiments/psp/ingame/https/` — модули PSP; `experiments/ingame/` и `experiments/host/` — проверки на компьютере; `plugin/` — исходная офлайн-реализация.

- [0.42: история запусков и этапы жизненного цикла](docs/ONLINE_CLIENT_042.md)
- [0.41: исправление раннего меню](docs/ONLINE_CLIENT_041.md)
- [0.40: переподключение и сетевые уведомления](docs/ONLINE_CLIENT_040.md)
- [0.37: Encore](docs/ONLINE_CLIENT_037.md)
- [0.34: подтверждённый softcore-зачёт на PSP](docs/ONLINE_CLIENT_034.md)
- [Контекст разработки](docs/NEXT_CHAT.md) · [Технический аудит](docs/ONLINE_AUDIT.md) · [План](DEVELOPMENT_PLAN.md)

Документы версий описывают состояние на соответствующем этапе; старые инструкции могут быть заменены этим README.

### Благодарности и лицензии

Основа — [avetis-dev/PSPAchievementsSystem](https://github.com/avetis-dev/PSPAchievementsSystem). Для исходного проекта **не выбрана open-source лицензия**; открытый доступ к коду не даёт неограниченных прав на использование и распространение. Форк не заменяет это уведомление и не заявляет новых прав на исходный код. См. [LICENSE_NOTICE.md](LICENSE_NOTICE.md).

Сторонние зависимости сохраняют свои условия: [rcheevos — MIT](vendor/rcheevos/LICENSE), [Mbed TLS — вариант Apache-2.0](vendor/mbedtls/LICENSE), [CA bundle — MPL-2.0](vendor/ca/LICENSE.txt), [PSPSDK](vendor/pspsdk/LICENSE). В публикацию исходников не входят образы игр, захваченные определения достижений, данные аккаунта и секреты.
