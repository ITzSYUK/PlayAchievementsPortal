# PlayAchievementsPortal

**RetroAchievements on a real PSP — experimental online client, version 0.41.**

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
| Menu and notifications | Tested on PSP; server progress, filters, stable menu and notification rendering |
| Encore | Local replay of previously unlocked achievements tested on PSP |
| Initial Wi-Fi recovery | Logs confirm a failed connection followed by an automatic successful retry |
| Longer recovery scenarios | Implemented and tested with synthetic responses; extended hardware validation remains |

The 0.41 hardware logs show a successful RA login, the 101-entry catalog and repeated menu open/close operations. The owner also reported three successive game launches without rebooting the PSP. One earlier freeze at the PSP launch screen remains unexplained; this is **not a stability guarantee**.

### Features and limits

- Session-long achievement evaluation, with a menu opened by holding **L + R + SELECT** for about 400 ms.
- Server-confirmed progress, an achievement list, filters and status cards. A local trigger or a popup alone does not prove that the server accepted an unlock.
- Separate network worker, certificate verification, bounded responses and queues, and memory-range checks.
- Wi-Fi reconnect and retries for temporary login/set-loading failures. Startup retries use delays of 1, 2, 4, 8, 16, 32 and then 60 seconds. Achievement submissions use the official engine's retry scheduling.
- During an ordinary Wi-Fi outage, the loaded RA client and pending unlocks remain in memory. The pending queue is **not saved across game exit, reboot or power loss**.
- Before the catalog is ready, the hotkey displays **ACHIEVEMENTS LOADING** without pausing the game. Once loaded, it opens the normal menu.

There is no hardcore, leaderboard, multi-account, Vita/Adrenaline, PSP Go internal-storage or general multi-game support. Renaming an image or plugin entry does not make another region, revision, translation or mod compatible. Old offline `.dat` profiles are not uploaded to RA.

Sleep/resume currently requests a safe client stop; automatic recovery after sleep is not implemented. The code has protected `module_stop` cleanup, but full completion during **HOME → Exit Game** is not yet proven. The current game-callback/frame binding is specific to LocoRoco and still marked `CANDIDATE` in diagnostics; broader timing and long-session validation remain necessary.

### Build and install

1. Build the online package using the commands below. The generated archive is `build/client-https/PSP_RA_HTTPS_v0.41.zip`; build outputs are not committed.
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
| Circle | Close |

The menu pauses the game callback and achievement evaluation while network work continues through `rc_client_idle`. Network status cards do not pause the game. **WI-FI CONNECTED** means that an IP was obtained; **RA SERVER REACHABLE** means a verified HTTPS/HTTP 200 response, not an achievement award. **CLIENT WARNING** is a service warning from the set, not an achievement.

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

The client writes `client-bootstrap.log` and `client-user.log` beside the PRX files. Each launch overwrites them. Save both **before launching the game again** when reporting an issue. Include the PSP/CFW, image region/revision, client version and the steps leading to the failure. Do not include `auth.txt`. Logs are bounded to 128 KiB; once full, new lines are discarded without stopping play.

Source map: `online/` contains the portable adapters, `experiments/psp/ingame/https/` the PSP modules, `experiments/ingame/` and `experiments/host/` the host tests, and `plugin/` the inherited offline implementation.

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
| Интерфейс | Проверены список, серверный прогресс, фильтры, стабильные меню и уведомления |
| Encore | Локальный повтор уже открытого достижения проверен на PSP |
| Повтор первого подключения | Логи подтверждают успешное автоматическое подключение после неудачной попытки |
| Другие сценарии восстановления | Реализованы и проверены на синтетических ответах; длительные проверки на PSP ещё нужны |

Логи 0.41 подтверждают вход, загрузку 101 достижения и повторное открытие/закрытие меню. Владелец также сообщил о трёх последовательных запусках без перезагрузки PSP. Причина одного более раннего зависания на заставке PSP пока не установлена; стабильность всех запусков не гарантируется.

### Возможности и ограничения

- Проверки работают всю игровую сессию. Меню открывается удержанием **L + R + SELECT** примерно 400 мс.
- Список использует серверный прогресс. Выполнение условия и появление карточки сами по себе не доказывают зачёт: необходимо подтверждение RA.
- Сеть обслуживает отдельный поток. Есть проверка сертификатов, границ памяти, размеров ответов и очередей.
- При временных ошибках повторяются подключение Wi-Fi, вход и загрузка набора. Задержки запуска: 1, 2, 4, 8, 16, 32 и далее 60 секунд. Для отправки достижений используется расписание повторов официального движка.
- При обычном обрыве Wi-Fi текущая RA-сессия и ожидающие отправки остаются в памяти. Очередь **не сохраняется после выхода из игры, перезагрузки или выключения**.
- До готовности набора сочетание показывает **ACHIEVEMENTS LOADING**, без паузы игры. После загрузки открывается обычное меню.

Не поддерживаются hardcore, таблицы рекордов, несколько аккаунтов, Vita/Adrenaline, внутреннее хранилище PSP Go и произвольные игры. Другой регион, ревизия, перевод или модификация требуют отдельной проверки; переименование файлов совместимость не добавляет. Старые офлайн-профили `.dat` не переносятся на сервер.

Сон/пробуждение сейчас запрашивает безопасную остановку клиента; восстановление после сна не реализовано. Защитная очистка через `module_stop` есть, но её полное выполнение при **HOME → выход из игры** ещё не подтверждено. Привязка проверок к игровому callback предназначена для LocoRoco и остаётся помеченной `CANDIDATE` в диагностике; синхронизацию и долгую игру ещё нужно проверять.

### Сборка и установка

1. Собери онлайн-клиент командами ниже. Архив появится в `build/client-https/PSP_RA_HTTPS_v0.41.zip`; готовые сборки в Git не добавляются.
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

Меню приостанавливает игровой callback и оценку условий; сеть обслуживается через `rc_client_idle`. Сетевые карточки игру не останавливают. **WI-FI CONNECTED** означает получение IP, **RA SERVER REACHABLE** — проверенный HTTPS/HTTP 200, а не зачёт достижения. **CLIENT WARNING** — служебное предупреждение из набора, не достижение.

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

Логи `client-bootstrap.log` и `client-user.log` находятся рядом с PRX и перезаписываются при каждом запуске. При проблеме сохрани оба **до следующего запуска**, укажи PSP/CFW, регион и ревизию игры, версию клиента и действия перед сбоем. Не прикладывай `auth.txt`. Каждый лог ограничен 128 КиБ; после заполнения новые строки отбрасываются, игра продолжается.

Структура: `online/` — переносимые адаптеры; `experiments/psp/ingame/https/` — модули PSP; `experiments/ingame/` и `experiments/host/` — проверки на компьютере; `plugin/` — исходная офлайн-реализация.

- [0.41: исправление раннего меню](docs/ONLINE_CLIENT_041.md)
- [0.40: переподключение и сетевые уведомления](docs/ONLINE_CLIENT_040.md)
- [0.37: Encore](docs/ONLINE_CLIENT_037.md)
- [0.34: подтверждённый softcore-зачёт на PSP](docs/ONLINE_CLIENT_034.md)
- [Контекст разработки](docs/NEXT_CHAT.md) · [Технический аудит](docs/ONLINE_AUDIT.md) · [План](DEVELOPMENT_PLAN.md)

Документы версий описывают состояние на соответствующем этапе; старые инструкции могут быть заменены этим README.

### Благодарности и лицензии

Основа — [avetis-dev/PSPAchievementsSystem](https://github.com/avetis-dev/PSPAchievementsSystem). Для исходного проекта **не выбрана open-source лицензия**; открытый доступ к коду не даёт неограниченных прав на использование и распространение. Форк не заменяет это уведомление и не заявляет новых прав на исходный код. См. [LICENSE_NOTICE.md](LICENSE_NOTICE.md).

Сторонние зависимости сохраняют свои условия: [rcheevos — MIT](vendor/rcheevos/LICENSE), [Mbed TLS — вариант Apache-2.0](vendor/mbedtls/LICENSE), [CA bundle — MPL-2.0](vendor/ca/LICENSE.txt), [PSPSDK](vendor/pspsdk/LICENSE). В публикацию исходников не входят образы игр, захваченные определения достижений, данные аккаунта и секреты.
