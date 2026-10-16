# Подтверждено:0.34, реальный softcore-зачёт на PSP

397986 Bu Bu Poruche за первое прохождение1-2 подтверждено сервером:
CONDITION_MET292395ms -> SERVER_CONFIRMED295655ms.17914frames/300с,
earned1 confirmed1 requests1 pending0 errors0 fault0, failed чтений0.
Завершение и очистка0. Конфигурация:PSP-3000/6.60+ARK5.1.0/+16MiB,
LocoRoco USA UCUS98662. Предшествующий spectator также сработал.
Подробности docs/ONLINE_CLIENT_034.md. Окно300с остаётся диагностическим;
постоянная работа, уведомления и другие игры не объявлены готовыми.
Далее история; её «0.34 ещё не проверена» устарело.

---
# Обновление:0.33 проверена,0.34 — диагностика условий

0.33:7188 do_frame/120с, fault/errors0, HTTPS/login/set101 успешны,
failed чтений0, rejected1 прежнего запрещённого адреса0979E4AC. Завершение
и очистка штатные. Владелец подтвердил окончание первого уровня.
Старое earned1 означает предупреждение101000001, не игровое открытие;
confirmed0. Предположение397986=первый уровень не доказано.
0.34 отделяет предупреждение и наблюдает уже оценённые условия397986 без
дополнительных чтений/оценки. См.docs/ONLINE_CLIENT_034.md и NEXT_CHAT.md.
На PSP0.34 пока не проверена. Далее история.

---
# Текущее0.33 — короткое окно, настраиваемое без пересборки

Пользователь считает7мин слишком долгим ожиданием. Теперь стандарт120с
ПОСЛЕ загрузки набора вместо240с. Необязательный duration.txt: ASCII30..300
секунд, в пакете120; без файла120. Например60 для быстрой проверки кадров,
но короткого окна может не хватить для ачивки за прохождение уровня.
Проверки начинают работать сразу после набора; ожидание нужно только для
автоматического завершения диагностики. Нормальная сеть даёт итог около
2,5–3мин от старта, но медленная сеть не позволяет гарантировать фиксированные3мин.

Добавлен worker-only sceIoSync(ms0:,0) каждые5с и перед штатным закрытием
user-log, без удержания engine_gate. Измеряются sync_calls/max_ms/last_result.
Это запрос writeback, а не доказательство причины прежнего обрыва и не
гарантия финальных записей при досрочном HOME. Главные изменения не трогают
кадровой/сетевой протокол, mode и правила начислений. На карту не писали.
Инструкция docs/ONLINE_CLIENT_033.md. Новый пакет0.33 на PSP ещё не проверен.

Уточнённая история0.32: первый запуск APCTL80410106 до HTTP, очистка успешна.
Второй: PRX совпадают, bootstrap заканчивается second11, user на HTTPS_READY,
нет результата чтения токена/подключения/набора/кадров/финала. Владелец играл
нормально около3мин и вышел HOME. Точная причина раннего обрыва неизвестна.
НЕ говорить, что клиент точно остановился на последней строке, что хвост
точно потерял кэш или что3мин объясняют ранний обрыв: это не доказано.
Не объявлять0.32 полноценной успешной проверкой движка на PSP.

Проверки0.33: make ingame-test в Ubuntu WSL PASS; тесты duration bounds,
BOM/NUL/размера/CRLF и sync(period/force/error/64-bit clock) PASS;
public-tree PASS; PSPDEV GCC15.2.0 offline ELF/build PASS.
Kernel ELF19056, user322108, лимит327680. ZIP18 файлов побайтно сверены.
Пакет build/client-https/PSP_RA_HTTPS_v0.33.zip,256327байт.
ZIP f36a1ad14a75c0fb22d7288abb6885b668286f6ac274f0ec804f2a3b176d79ea.
Kernel ac302694fe2308df101ec98a0499607cee3a869542a1b5b8948ef6069c8b5856.
User060aec8ad329d27ef0e41291faa8dd02688836530659e937264fca55b8d77130.
Реальный зачёт на PSP не подтверждён, spectator пока сохраняется.

---
# Актуальное состояние: исправление0.32

0.31 проверена на PSP, оба PRX совпали по SHA256. HTTPS/login/set101 успешны,
unsupported=1, target397986 active. Выполнено219 do_frame, accepted133320,
rejected1 (запрещённый0179E4AC), failed0. earned/confirmed/requests=0.
После ~3,6с оценок fault=3, user result=-35413; kernel=-35106 (увидел stop
до завершения worker). RESTORE/RETAIN и очистка сети/client успешны.
Причина в адаптере0.31: сетевой опрос/диагностика делили queue_guard с игрой.
Не повторять0.31 и не включать в ней softcore.

Исправлена0.32: только сериализованный владелец rc_client вызывает методы
адаптера очереди. HTTPS-поток получает запрос/возвращает неизменяемый ответ
через SPSC mailbox с acquire/release; report тоже передаётся без queue_guard.
Ни ожиданий сети, ни пропуска/догоняния кадров не добавлено. После join
незавершённая передача возвращается владельцу до закрытия сессии.
Убран старый лимит4 HTTP-запросов, мешавший ping/последующим открытиям.
Инструкция: docs/ONLINE_CLIENT_032.md. Режим spectator по умолчанию, затем
softcore тем же пакетом только после проверки корректности срабатывания.

Проверки: make ingame-test и make online-test PASS. Новый test_session_mailbox:
настоящий rc_client, два потока,30тыс итераций владельца, постоянный транспортный
опрос, задержанные ответы, кадры во время I/O, >4 запросов включая ping,
softcore award/ack, отмена в REQUEST/ACTIVE/RESPONSE. Lock адаптера допускается
только в owner. Этот тест с ASan/UBSan PASS; public-tree PASS.
PSPDEV GCC15.2.0 offline build/ELF PASS: kernel19056/user321128 байт.
ZIP16 файлов побайтно сверены с источниками; auth.txt отсутствует.

Пакет build/client-https/PSP_RA_HTTPS_v0.32.zip,252945 байт.
ZIP SHA256 b8fec77457289b135d53edede7a2f0501219e66dcc5f7afd10448ccc0f0ef52b.
Kernel48e7916ca85456a5f4a57d3796b48001bb92132d0d1c827ec944180e57d3ecfa.
User d5de5ebd46a7183039585ad80f52b2c0f917014f5224dadf817fd6df513178b6.

0.32 на PSP ещё не проверена. Карту не меняли, auth.txt не читали,
аккаунт с компьютера не трогали. Следующий шаг: заменить только два PRX,
сохранить mode=spectator/auth/profile, выполнить тот же объединённый сценарий
прохождения уровня+START+продолжение, передать два журнала.
Далее — история предыдущих состояний.

---
# Онлайн-клиент 0.31 — 16 октября 2026

Актуальная инструкция: [ONLINE_CLIENT_031.md](ONLINE_CLIENT_031.md).
Реализован протокол softcore-зачёта через rc_client; по умолчанию spectator.
Host-тесты и новые ASan/UBSan тесты протокола/кадрового gate/mode PASS.
PSPDEV offline build и ELF guards PASS; kernel19056, user320740 байт.
ZIP15 файлов сверены побайтно, public-tree PASS. SHA256 пакета:
2a19732e04391211586c73230a1d6ebbcc22206e538406b152d6e84ca38bb16c.
На PSP 0.31 ещё не проверена. Следующий шаг — единый spectator-сценарий
с прохождением уровня, затем softcore тем же пакетом. Старые .dat не отправляются.
Карта D: не менялась, auth.txt на компьютере не читался, аккаунт не трогали.
Ниже — история предыдущих этапов.

---

# Игровая проба: состояние работы

Актуально16 октября2026:0.29 прошла на PSP (7161 вход за120с, оригинальный
слот восстановлен, очистка без ошибок). Владелец подтвердил START-паузу,
нормальное управление и продолжение. Объект0A7B2E8C вне разрешённого диапазона
не читался; FFFFFFFF — маркер отсутствия данных. Подготовлена0.30 для
корреляции ввода/VCount/framebuffer; на PSP не проверена. User PRX/control
после восстановления намеренно остаются до выхода из игры из-за риска
сохранённого адреса обёртки. do_frame/аккаунт/начисления выключены.
Текущая инструкция — [HTTPS_CLIENT.md](HTTPS_CLIENT.md),
аудит — [PPSSPP_FRAME_AUDIT.md](PPSSPP_FRAME_AUDIT.md).

Проверки0.30,16 октября2026:

- `wsl -d Ubuntu -- make -s -C /mnt/c/Users/itzsy/Documents/ChatGPT/PortableAchievementsSystem ingame-test` — PASS,
  включая test_frame_timeline (wrap времени/VCount, повтор/пропуск/reset,
  недоступные метаданные и предел4 traces), а также отказ записи при чужом
  слоте/несовпадении инструкции и удержание user/control после восстановления.
- ASan/UBSan: из experiments/ingame сборка целей
  `../../build/https-030-sanitized/test_https_bootstrap` и
  `../../build/https-030-sanitized/test_frame_timeline` с
  `BUILD=../../build/https-030-sanitized`,
  `CFLAGS='-O1 -g -std=c99 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer'`,
  `LDFLAGS='-fsanitize=address,undefined'`; оба бинарника выполнены — PASS.
- `./scripts/build_client_https.ps1` — PASS: закреплённый PSPDEV без сети,
  GCC15.2.0, метаданные0.30/start/stop, heap pin2048КиБ, размерные ограничения,
  отсутствие account/network/rc_client/controller и изменяющих display API.
  GetVcount/GetFrameBuf разрешены; heap metadata не означает его выделение
  минимальным worker. Kernel ELF19000/20/40=19060, user8860/12/300=9172.
- ZIP13 entries: все имена по whitelist, каждый SHA256 совпал с источником;
  public-tree check PASS. ZIP SHA256
  `89564f43fa4311b664a0a8acdd74611b69ccc2185dfa2810b8ad19fd6f9ec9d9`.
  Kernel PRX `746a9edd8ac1664d2da899e4e879942340e7e8faf66ac5b80373ab6a6e94b7dc`,
  user PRX `d1ce61cc3f6487d0baeddad43b1165396b7f2d7f6ac88fc5a149cd14e62f3fa1`.

PSP0.30 ещё не проверена. Карта/auth/аккаунт не затронуты. Это одна локальная
сборка для корреляции; full do_frame/онлайн-зачёт по-прежнему не включены.

Ниже история; сведения «подготовлена0.29» относятся к предыдущему этапу.

15 октября 2026 года. PSP-3000, 6.60 + ARK 5.1.0 по сообщению владельца.

0.28 проверена на PSP: аудит завершился за22,126с, запросов/входа/кадров не
было, очистка успешна. 08844440 оказался обработчиком очереди событий, а
08841880 — переключателем бита0x02 для диапазона объектов; найдено6 callers.
Подготовлена0.29: двухминутный runtime-счётчик метода ввода с временной заменой
только слота089B4EE4 и обязательным восстановлением. Аккаунт/сеть/do_frame/
award отсутствуют. На PSP0.29 ещё не проверена; игровой кадр/пауза UNPROVEN.
Инструкция — [HTTPS_CLIENT.md](HTTPS_CLIENT.md), актуальный результат — в последнем разделе.

Подтверждено на PSP: Offline Probe PASS; Mbed TLS Probe 0.2 принимает
положительные сертификаты и отклоняет expired/name mismatch/self-signed с
ожидаемыми флагами. RA endpoint без параметров отвечает HTTP 422 по TLS.
KIRK helper работает, RTC исправлен; пик TLS-выделений 453134 байта, остаток 0.

Образ владельца: LocoRoco (USA).iso, 589758464 байта; официальный rcheevos
v12.5.0 RA-хеш на компьютере: `199515744d49ef3620a5f9297c6116f9`, совпадает с
поддерживаемым хешем игры 10523. Образ не включается в проект или пакет.

Первоначальный план игровой пробы (история):
1. Получить из ISO только справочные размеры/MD5/имя модуля для сравнения.
2. Подготовить отдельный kernel bootstrap/наблюдатель и user TLS/file worker.
3. Проверить границы сегментов, отмену и правила владения сетевым стеком.
4. Собрать закреплённым SDK, упаковать PRX/CA/лицензии/инструкцию.
5. Владелец проверит игру, pause/resume и логи на PSP.

Выполнено локально:
- Из оригинального ISO прочитаны только метаданные/хеши. ID `UCUS-98662`,
  модуль `LocoRoco`; независимый потоковый MD5 совпал с официальным хешером.
  Данные файла не изменялись и не извлекались в проект.
- Созданы kernel bootstrap/наблюдатель и user worker с отдельным 2 МиБ heap.
  Последний использует уже проверенный общий Mbed TLS/RTC код. В его ELF
  отсутствует sceKernelExitGame; стандартный CRT main/exit не включён.
- Чтение RA-адресов дополнительно ограничено загруженными сегментами игры;
  короткое/запрещённое чтение не считается чтением нулей. Framebuffer только
  наблюдается, display/GE hooks и изменения игрового кода не выполняются.
- Существующий стек сети не переинициализируется. Перед запросом проверяются
  модули; успешные собственные load/init отслеживаются для очистки. Параллельный
  запуск Game Sharing игрой не проверен и исключён из первого сеанса.
- Stop/suspend отменяют I/O между вызовами. Unload/control-free допускаются
  только после остановки worker; иначе его код и control block удерживаются
  до выхода из игры. Границы и журнал проверены локально, PSP shutdown/sleep
  всё ещё требуют проверки владельцем.

Проверки: `make ingame-test` в Ubuntu WSL и тот же набор под ASan/UBSan прошли.
Проверены границы сегментов/overflow, правила пропуска чужого стека и отмены,
ограниченный kernel-форматтер, частичная/неуспешная запись журнала и справочный
ISO-парсер на синтетических входах. Компиляция обоих PRX: закреплённый SDK из
dependencies.lock.json, GCC 15.2.0, `-Werror`, без предупреждений импортов.
Команда сборки пакета: `scripts/build_ingame_probe.ps1`; инструкции —
`docs/INGAME_PROBE.md`.

## Результат первого запуска и исправление 0.2

Свежий `bootstrap.log` владельца от 14 октября подтверждает правильную
GAME-запись, обнаружение LocoRoco (`game_uid=0x03E8C727`, text `0x08804000`,
text_size 1417108, один сегмент) и успешное выделение user control block.
Следующий вызов `scePowerRegisterCallback(-1, ...)` через `scePower_driver`
вернул `0x80000004`. Проба 0.1 перешла в cleanup до загрузки user worker,
поэтому `worker.log` отсутствовал. В этом логе нет проверки памяти, хеша или
HTTPS внутри игры. Это ошибка пути запуска пробы, а не установки владельца.
Предыдущий лог из двух строк был недостаточен для установления этой причины.

В версии 0.2:
- Убрана регистрация power callback из kernel; создание, регистрация и
  освобождение выполняются user-потоком через публичную библиотеку `scePower`.
  API auto-slot `-1` описан в закреплённом `psppower.h`; используем собственный
  callback, не перезаписывая слот игры. Основной пример SDK также регистрирует
  callback из user-программы:
  https://github.com/pspdev/pspsdk/blob/e2b2b313d4175823a263c2c6cf175311e824eb77/src/samples/power/main.c
- User-поток обслуживает callback через `sceKernelCheckCallback` между
  операциями и CB-ожидание в цикле команд. Обработчик только ставит stop/suspend.
  Наблюдатель kernel тоже завершает сеанс при shared stop.
- Ошибка регистрации оставляет диагностическую запись в `worker.log` до
  начала хеширования/TLS. Ошибка unregister/delete удерживает код и shared block
  (worker_done остаётся 0); выгрузка живого callback не допускается.
- Версии обоих PRX/логов и архива изменены на 0.2. При обновлении нужны оба PRX.

Команды проверки 0.2 в Ubuntu WSL, выполнены успешно:
`make ingame-test`
и
`make -C experiments/ingame BUILD=../../build/ingame-host-sanitize CFLAGS="-O1 -g -std=c99 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer" LDFLAGS="-fsanitize=address,undefined" test`.
Добавлена проверка отказов create/register/unregister/delete, владения слотом 0,
повторной очистки и запрета удаления зарегистрированного callback.
Сборка `scripts/build_ingame_probe.ps1` завершилась успешно в закреплённом
контейнере PSPSDK, GCC 15.2.0, `-Werror`, без предупреждений. Kernel PRX —
15238 байт, user PRX — 442586 байт. `psp-nm` подтвердил отсутствие power API
в kernel и наличие `scePowerRegisterCallback`, `scePowerUnregisterCallback`,
`sceKernelCheckCallback` в user; `sceKernelExitGame` отсутствует.
Архив `build/ingame/PSP_RA_InGameProbe_v0.2.zip` — 355404 байта, SHA256
`8fd7df42fd64cdf86dcfe399978a96ff338b42200265c27203db8a1ba28f5aa6`.
Проверены точный набор 12 файлов и соответствие исходным бинарникам, CA,
инструкции и лицензиям. Public tree validation и `git diff --check` прошли;
последняя проверка ограничена тем, что проект пока состоит из untracked файлов.
Работа версии 0.2 на PSP описана ниже; доставка sleep/resume ещё не проверена.
Файлы на карте владельца не изменялись.

## Выключение с 0.2 и облегчённая проба

Логи владельца с карты от 14 октября 10:40 подтверждают успешные load/start
user worker, create/register user power callback (slot 0), профиль 1 и
`DISC_HASH_MATCH`: исходные PARAM.SFO (472 байта) и EBOOT.BIN (2329168 байт)
через disc0 дали официальный PC-хеш `199515744d49ef3620a5f9297c6116f9`.
Worker успел перейти в ожидание команды, HTTPS не запускался. Наблюдатель
записал два замера ~1003/2003 мс: VBlank 60, смены буфера 60, прочитан один
сегмент, fingerprint A004B77F. Логи обрываются, cleanup отсутствует.
Это подтверждение узких чтений и хеша, но не стабильности или frame binding.

Владелец сообщил о выключении консоли при переходе от вступительного ролика
к новой игре, независимо от пропуска ролика. После выключения записи плагина
в PLUGINS.TXT он снова пропустил ролик, начал игру и подтвердил нормальную
работу без выключения. Версия 0.2 исключена из дальнейшего тестирования.
Связь с включённой пробой подтверждается сравнением запусков; OOM или
конкретная аварийная инструкция ещё не установлены. Приблизительный минимум
ресурсов старого worker: heap 2 МиБ, стек 128 КиБ, ELF code/data/bss ~404 КиБ,
служебные ресурсы runtime; реальный расход и запас игры не были замерены.

Следующий шаг — отдельный `RAInGameLite` 0.3, без автозагрузки worker,
newlib/heap, user control allocation, power callback, TLS, pad/display,
disc0 и прямого чтения памяти игры. Один kernel-поток с 8 КиБ stack и
приоритетом 0x60 измеряет user partition 2 раз в секунду в течение 120
интервалов, пишет отдельный `RAInGameLite/lite.log`. Проверяется допустимость
ответов API памяти. На ошибке поток завершает пробу, module_stop отказывается
выгрузить незавершившийся поток. Сборщик исключает опасные импорты и включает
в отдельный ZIP только PRX, инструкцию, строку установки и PSPSDK license.
Результат проверки 0.3-lite на PSP приведён ниже; полный worker пока не возвращается.

Локальная проверка 0.3-lite: `scripts/build_ingame_lite.ps1` завершилась
успешно в том же закреплённом PSPSDK-контейнере без сети, GCC 15.2.0,
`-Werror`, предупреждений нет. ELF text/data/bss — 4572/8/20 байт,
PRX — 7678 байт. Сборщик проверил отсутствие символов загрузки модулей,
user allocation, ExitGame, display/pad/power/network, malloc/newlib и
адаптера прямого чтения RAM. `make ingame-test` в Ubuntu WSL прошёл,
включая применяемые здесь ограниченный форматтер и частичные/ошибочные записи
logger. Эти проверки не подтверждают работу системных вызовов на PSP.
Архив `build/ingame-lite/PSP_RA_InGameLite_v0.3.zip` — 7563 байта, SHA256
`0460b7a238b54b3ca7a668611bb658111fbcabcf4c099d97050535672221c361`.
Проверены точные четыре файла, соответствие PRX, инструкции, install-entry
и лицензии; public tree validation и `git diff --check` прошли
(diff по-прежнему ограничен отсутствием tracked исходников).

## Успешный запуск 0.3 и план размещения worker

Владелец сообщил, что игра работала хорошо. `lite.log` от 14 октября 11:14
содержит замеры 0–120 без пропусков и `lite_finished stop=0 result=0
log_error=0x00000000`. User partition 2: start `0x08800000`, size 25165824
байта (24 МиБ). Свободный объём и крупнейший свободный блок:
- замеры 0–2: 12601856 байт (~12,02 МиБ);
- 3–61: 864512 байт (844,25 КиБ);
- 62–70: 569600 байт (556,25 КиБ);
- 71–120: вновь 864512 байт.

Выделяемые до уровня ресурсы worker 0.2 не помещаются в измеренный запас
игры: один только его heap 2 МиБ превышает максимум после замера 3.
Нехватка основного user RAM — наиболее вероятная причина старого выключения,
но не доказанный exception/OOM игры. Минимумы относятся только к моментам
опроса; занятый игрой внутренний heap и его свободные байты здесь не видны.
Чтение metadata SDK в kernel, лёгкий поток и запись журнала проверены одним
обычным запуском; sleep/resume, display/pad и TLS внутри игры ещё не проверены.

Изучен справочный `SystemControl/src/sysmem.c` того же закреплённого ARK5
`a9b74547c0d1785cc77981462cf095bd5a76434f`:
https://github.com/PSP-Arkfive/ARK-5/blob/a9b74547c0d1785cc77981462cf095bd5a76434f/SystemControl/src/sysmem.c
Он рассматривает дополнительные partitions 8–11; этого недостаточно для
обещания их доступности на установленной CFW. Справочный исходник сохранён
только в игнорируемом кэше, код ARK не включён в PRX.

В 0.4-layout лёгкая проба на метках 0/3/10/30/60 запрашивает размеры,
attributes и свободное место partitions 1/2/5/8/9/10/11 через уже проверенные
в 0.3 SDK API. При failed query optional-раздел пропускается до запросов
free/maximum. Размеры проверяются; полученные адреса не разыменовываются.
Ни выделения RAM, ни unprotect, ни настроек CFW нет. Журнал теперь layout.log,
предыдущий lite.log не заменяется. Инструкция: docs/INGAME_MEMORY_LAYOUT.md.

Локальная сборка 0.4: `scripts/build_ingame_lite.ps1`, тот же закреплённый
Docker/WSL, GCC 15.2.0, `-Werror`; предупреждений нет. ELF text/data/bss:
5316/8/20 байт; PRX 8574 байта. Проверка forbidden symbols прошла, включая
отсутствие API изменения защиты, allocation, callbacks, сети, дискового
чтения и sctrl. Архив `build/ingame-lite/PSP_RA_MemoryLayout_v0.4.zip`
8144 байта, SHA256
`2475e3c58cd5206065fadcd6f8535d8234988b365ce2cc2b32ccf86074578c70`.
Проверены ровно четыре файла, соответствие бинарнику/инструкции/install-entry/
лицензии, версия в PRX и отдельное имя журнала. Результат 0.4 на PSP приведён ниже.

## Результат карты RAM и резервирование 0.5

`layout.log` владельца содержит все 121 замер, пять снимков карты 0/3/10/30/60
и нормальное `lite_finished stop=0 result=0 log_error=0x00000000`.
Partition 2: те же 24 МиБ, free 864512 байт после метки 3, минимум 569600
на метках 40–53. Partition 9: start 0x0A000000, size 25165824, attr 0x0C;
free/largest первоначально 25165824, затем 16774912 байт на 3/10/30/60.
Partition 8: start 0x8BB00000, size 1 МиБ, free/largest 697344; partition 11:
start 0x8BC00000, size/free/largest 4 МиБ. Partition 10 отсутствует,
0x800200D2 ожидаемо обработан без последующего обращения к нему. Остальные
снимки valid=1. Attributes и свободные байты не подтверждают user-доступ.

Подготовлена 0.5-reserve: лёгкий kernel PRX после 10 интервалов пытается
резервировать 2 МиБ только в partition 9, PSP_SMEM_High. Допускается только
измеренная область 0x0A000000, размер <=24 МиБ, free >=6 МиБ, largest >=2 МиБ;
на отказе нет fallback в partition 2. Это временный guard эксперимента,
не бюджет для всех игр. Полный адресный диапазон выделенного блока и slices
проверяются отдельно с overflow/alias rejection. Пишутся только 384 байта
собственного блока, проверяются через 10 интервалов и перед освобождением.
Раздел 9 выделяется штатным allocator, свободные адреса из старого лога не
используются напрямую. Free выполняется при нормальном/раннем завершении;
на ошибке module_stop повторяет после join и не выгружает модуль до освобождения.
Журнал reserve.log, старые журналы сохраняются. Worker/TLS и изменение
защиты отсутствуют; пригодность user исполнения/heap/stack всё ещё не доказана.

Локальные проверки: `make ingame-test` и тот же набор ASan/UBSan успешно,
добавлены граничные случаи extra block/slice, alias, overflow, запрет диапазона
partition 2. `scripts/build_ingame_lite.ps1`: закреплённый контейнер без сети,
GCC 15.2.0, `-Werror`, предупреждений нет; проверены отсутствие запрещённых
функций и присутствие allocation/free. ELF text/data/bss 7388/12/24 байта,
PRX 11594 байта. Архив `build/ingame-lite/PSP_RA_ExtraReserve_v0.5.zip` —
9886 байт, SHA256
`104f118cbcce38c1aa75d5bad11197a89c75e3c5fa0e08a7a37f3e8acec1cc3f`.
Проверены точный набор четырёх файлов, бинарник, версия, инструкции, лицензия
и отдельное имя журнала. Результат 0.5 на PSP приведён ниже.

## Успешное резервирование 0.5 и подготовка размещения кода 0.6

Владелец подтвердил нормальную работу игры. В reserve.log все 121 замер,
10 успешных periodic checks (20–110), успешная финальная проверка,
free partition9 test block 0 и нормальное завершение без ошибок журнала.
2 МиБ выделены по 0x0ADFF700: extra free 16774912 ->14677760, после free
вернулось 16774912. Основной user free до/после allocation и free — 864512.
Его минимум в ходе игры вновь 569600. Это подтверждённый kernel allocation/
доступ к 384 собственным байтам/free, не весь блок и не user-доступ.

Для следующего шага подготовлена 0.6-placement. После резервирования она
загружает (без StartModule) отдельный tiny user PRX RAPlacementUser в
partition 9 для text/data. Проверяет metadata QueryModuleInfo: имя,
user attribute, непустой text, 1–4 сегмента, нахождение непустых сегментов
в extra RAM, отсутствие kernel aliases. Никакой fallback в partition 2.
В конце UnloadModule unstarted child, затем free отдельного scratch блока.
При ошибках unload/free bootstrap отказывается выгрузиться до очистки;
повтор в module_stop выполняется только после join observer.
Loader может отвергнуть размещение; это результат теста, не повод
форсировать запуск. Исполнение, stack и DDR user permissions ещё не проверены.

PSPSDK позволяет отдельно задать mpidtext/mpiddata в LMOption и mpidstack
в SMOption; здесь используется только загрузка. Tiny C user PRX без CRT,
runtime/syscalls, callbacks и потоков имеет по 1 КиБ data/BSS. Используются
служебные libpspmodinfo и штатный режим SDK NO_FIXUP_IMPORTS=1: пустой import
range подтверждён по ModuleInfo, настоящих .lib.stub/.sceStub.text/.sceNid
нет. Kernel bootstrap отсутствие StartModule/StopModule подтверждено по
символам, изменения защиты и MMIO не включены.

Локальная проверка: `make ingame-test` и набор ASan/UBSan прошли,
добавлены проверки user segment с запретом cached kernel alias.
`scripts/build_ingame_lite.ps1` завершилась успешно в закреплённом
PSPSDK Docker/WSL, GCC 15.2.0, -Werror. ELF bootstrap text/data/bss
8960/16/24, tiny user 172/1024/1024 байта. PRX 13606 и 2574 байта.
Архив build/ingame-lite/PSP_RA_CodePlacement_v0.6.zip — 11334 байта,
SHA256 `bb764008afb17ec0def72e1d9ee83339c8b69e232fb54a49f8740342f9642b69`.
Проверены точные 5 файлов, версия и имя журнала, соответствие бинарников/
инструкции/install-entry/license, user flags и пустой import range в PRX.
Результат последующего запуска 0.6 приведён ниже.

## Отказ размещения 0.6 и исправление диагностики 0.7

Изучен placement.log владельца от 14 октября: на метке 10 резервирование
успешно, LoadModule вернул 0x80020132 PARTITION_MISMATCH. Partition 9 attr
0x0C несовместим с user-проверкой attr & 3 в исследованном ModuleMgr 6.60.
Финальная проверка scratch и free успешны, free 16774912 восстановлен,
user free остаётся 864512. Итог result=-2147352270 соответствует этому коду,
log_error=0; журнал не оборван. Размещение сегментов, user-исполнение и стек
не проверены. Работа игры в сеансе 0.6 отдельно владельцем не подтверждена.
Оба PRX на карте совпали по SHA256 с локальным пакетом 0.6.

0.7 проверяет user-атрибуты и диапазон раздела до LoadModule. При отказе
возвращает собственный result=2/PLACEMENT_UNSUPPORTED, не вызывает загрузчик
и освобождает scratch. Итог содержит hex-код, имя, этап и outcome; cleanup
отдельно сохраняет первоначальный результат и результаты unload/free.
Новый журнал placement-preflight.log сохраняет прежний placement.log.
Оба PRX имеют версию 0.7, архив PSP_RA_CodePlacement_v0.7.zip создаёт тот же
сборщик. Защита, MMIO и размещение в partition 2 не менялись.
Это исправление диагностики и раннего отказа, не разрешение user-доступа к
extra RAM. Инструкция: docs/INGAME_CODE_PLACEMENT.md. Результат запуска 0.7 ниже.

Проверка 0.7 на компьютере: команда
`wsl -d Ubuntu -- make -C /mnt/c/Users/itzsy/Documents/ChatGPT/PortableAchievementsSystem ingame-test`
прошла (GCC, -Wall -Wextra -Werror). Добавлены случаи с реально измеренной
partition 9 attr 0x0C и запретом partition 2/kernel alias/некорректного размера
даже при user-атрибутах. Сборка `scripts/build_ingame_lite.ps1` прошла в
закреплённом dependencies.lock.json контейнере, без сети, PSP GCC 15.2.0,
-Werror. ELF bootstrap text/data/bss 10776/16/24, user 172/1024/1024 байта.
PRX 16462/2574 байта. Проверены отсутствие StartModule/StopModule/DDR/network
в bootstrap, runtime/imports у user, версии обоих PRX 0.7 и точные пять файлов
ZIP с побайтным соответствием исходным бинарникам/документам.
Архив `build/ingame-lite/PSP_RA_CodePlacement_v0.7.zip` — 14045 байт,
SHA256 `c3dde040d38b465b0ab1476c7eda8044726c66b74fd42bb8e33b658ab51573ef`.
Проверены пробелы в конце строк изменённых файлов; git diff ограничен тем,
что исходники проекта пока untracked. Старый архив 0.6 сохранён, файлы карты
PSP не менялись. Полный TLS worker и права user RAM этим не проверены.

## Результат 0.7 и отдельная проба partition 2 (0.8)

Из placement-preflight.log владельца: оба PRX соответствуют пакету 0.7,
на метке 10 PLACEMENT_UNSUPPORTED attr=0x0C reason=USER_PARTITION_MISMATCH,
load=NOT_ATTEMPTED. Проверка собственных байтов и free 0, free partition 9
вернулось к 16774912, user free 864512. Итог result=2 outcome=UNSUPPORTED,
log_error=0, cleanup без ошибок. Работа раннего отказа подтверждена на PSP;
загрузки/исполнения user-кода и его доступа к extra RAM в этом запуске нет.

План разрешённого следующего эксперимента:
1. Отдельный пакет 0.8 и placement-user2.log, только tiny user PRX в partition 2.
2. Kernel scratch 2 МиБ в partition 9, замеры 2/9 до/после load и unload.
3. Консервативные memory guards, строгие границы/размеры metadata, сохранение
   владения ресурсами при ошибках и завершении observer.
4. Переносимые проверки, mocks реального load/unload пути, pinned PSP-сборка,
   проверка ZIP. Затем один запуск владельцем по docs/INGAME_USER_PLACEMENT.md.

Реализован load-only: mpidtext=mpiddata=2, без StartModule/StopModule, user
heap/threads/imports и без изменений DDR. Требуются free >=512+32 КиБ и
largest >=32 КиБ перед загрузкой; при удержании child free >=512 КиБ.
Суммы text/data/BSS и сегментов ограничены 32 КиБ. Размеры не ограничивают
временную память загрузчика; guards не являются доказанным бюджетом игры.
Разница free неатомарна и помечена attribution=UNPROVEN. Замеры не блокируют
cleanup: даже при их отказе child выгружается. На момент сборки 0.8 на PSP ещё не проверялась.
Результат последующего запуска приведён в разделе минимального клиента ниже.

Проверки 0.8 выполнены 14 октября 2026 года:

- До изменений воспроизведена текущая 0.7 через `scripts/build_ingame_lite.ps1`;
  исходники для сравнения сохранены локально в .work/placement2-before.
  Upstream e0cd8a9b3ccf439532b24b4f1faf2cfc8c0198ab, rcheevos v12.5.0
  (1433173220a7eaede6a9ed7a18e94117be1821e0), SDK и остальные pins не менялись.
- `wsl -d Ubuntu -- make -C /mnt/c/Users/itzsy/Documents/ChatGPT/PortableAchievementsSystem ingame-test`
  прошёл, GCC с -Wall -Wextra -Werror. Новые тесты: диапазоны/aliases/overflow,
  нулевые data/BSS и budget, recorded free 864512/569600, fragmentation/low RAM;
  реальный load/unload код bootstrap с подменёнными API проверяет mpid=2,
  разницу free, отказ до загрузки и после роста расхода, wrong segment, ошибки
  load/query/log, отказ замера перед unload, повтор unload и stop до загрузки.
- `scripts/build_ingame_lite.ps1` прошёл в закреплённом Docker/WSL SDK без сети,
  PSP GCC 15.2.0, -Werror. Kernel ELF text/data/bss 13464/16/24 байта,
  user 172/1024/1024; PRX 20126/2574 байта. Проверки символов исключили запуск
  модулей, network/DDR/newlib/heap; у user PRX пустой import range.
- `build/ingame-lite/PSP_RA_UserPlacement_v0.8.zip`: ровно 5 ожидаемых файлов,
  15091 байт, SHA256 baa498aa58e72570138a15ad6c59d0f858bc38a2fa0c6768201888e4be2926e4.
  Содержимое побайтно соответствует исходным PRX/инструкции/install-entry/license;
  версии обоих PRX 0.8, user attribute 0. Локальные ссылки и whitespace проверены.
- Просмотрен diff с локальной копией 0.7; архив 0.7 сохранил прежний SHA256.
  Файлы на карте PSP не менялись. Остаётся один запуск владельца с журналом
  placement-user2.log и сообщением о работе игры. User-исполнение, стек,
  доступ к extra RAM, HTTPS/FPS/sleep и полный клиент этим не подтверждены.

Измерение display VBlank и наблюдаемых смен framebuffer не определяет
игровой кадр. В этом шаге нет hooks, rc_client_do_frame, условий достижений,
входа в аккаунт или unlock. При несоответствии disc0-хеша это результат
диагностики, а не повод подставить справочный хеш в онлайн-клиент.

## Минимальный клиент: результат 0.8 и интеграция 0.9

В placement-user2.log 0.8 все 121 замер и нормальный итог result=0,
outcome=COMPLETE, log_error=0. User PRX attr=0 загружен по 0x094F6B00,
text/data/BSS 148/1024/1024, один segment 2488 байт. Наблюдаемая разница
user free 864512 ->861952 составляет 2560 байт и полностью восстановилась
при unload (возвращён положительный UID 0x042BA355). Минимум free 567040,
largest 566784. Scratch 2 МиБ по 0x0B600000 проверен и освобождён, partition 9
free 25165824 ->23068672 ->25165824. В отличие от прежних запусков раздел 9
показывал 24 МиБ свободных; причина различия распределения из лога не известна.
Оба PRX соответствуют пакету 0.8; владелец подтвердил хорошую работу игры.
Размещение и очистка подтверждены, user-код в 0.8 не запускался.

План интеграции зафиксирован в docs/MINIMAL_CLIENT.md. Реализован настоящий
rc_client v12.5.0 в online/src/ra_client_session.c: immutable spectator/hardcore=0,
owner-only RC API, HTTPS URL allowlist, queued/active/complete tickets, bounded
POST/response/общая память, timeout/cancel, owner delivery и close после завершения
активного транспорта. Буферы стираются, raw errors/requests не логируются.
Есть вход по токену и загрузка по явно переданному проверенному хешу; файлового
хранения токена, пароля или реального аккаунта в пакете нет. Pause/frame gate
не вызывает do_frame без подтверждённого игрового кадра.

0.9 включает отдельные RAClientBootstrap/RAClientUser в SEPLUGINS/RAClientLite.
User owner исполняет rc_client create/idle/destroy со stack 16 КиБ, fixed newlib
heap 32 КиБ; entry/stop stack 8 КиБ в partition 2. Прежний worker 0.2 и old lite
выключены для запуска 0.9. PRX file <=320 КиБ, image <=288 КиБ; до load free
>=823296 с запасом на возможные file/image buffers, при наблюдении >=192 КиБ.
Это guards эксперимента, не доказанные бюджеты игры/временной памяти loader.
Entry/text/segments проверены до StartModule, живой user thread не выгружается;
ошибки stop/unload/free/join удерживают код/control до успешной очистки.
Libc socket/file read/write/close отключены с ENOSYS, чтобы idle runtime не
импортировал NetInet и не закрывал stdio игры; явные PSP I/O остаются для логов.
Сеть, login/set download и game frame processing в PSP runtime пока отключены.

Выполненные проверки 14 октября 2026 года:

- Исходный проект e0cd8a9b3ccf439532b24b4f1faf2cfc8c0198ab, rcheevos v12.5.0
  1433173220a7eaede6a9ed7a18e94117be1821e0, pinned SDK из dependencies.lock.json
  не менялись. До интеграции scripts/build_ingame_lite.ps1 воспроизвёл 0.8.
- `wsl -d Ubuntu -- make -s -C /mnt/c/Users/itzsy/Documents/ChatGPT/PortableAchievementsSystem online-test`
  прошёл: отдельный pthread транспорта не вызывает RC callbacks, размеры/expiry/
  cancel/tickets/close проверены. Синтетический login + game set + условие
  дали ровно одно achievement event только на разрешённом frame, без award
  запроса в spectator. Это собственная тестовая логика, не условия сервиса.
- `wsl -d Ubuntu -- make -s -C /mnt/c/Users/itzsy/Documents/ChatGPT/PortableAchievementsSystem ingame-test`
  прошёл: actual bootstrap cleanup с mocks, positive unload UIDs, ошибки stop/
  unload/free/join/delete и retry, запрет неверных entry/text/segments/aliases.
- Целевой session test собран под ASan/UBSan:
  `wsl -d Ubuntu -- make -s -C /mnt/c/Users/itzsy/Documents/ChatGPT/PortableAchievementsSystem/experiments/host BUILD=../../build/client-sanitized 'CFLAGS=-O1 -g -std=c99 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer' 'LDFLAGS=-fsanitize=address,undefined' ../../build/client-sanitized/test_client_session`
  и запущен `wsl -d Ubuntu -- /mnt/c/Users/itzsy/Documents/ChatGPT/PortableAchievementsSystem/build/client-sanitized/test_client_session`.
  Прошёл без диагностик sanitizer.
- `scripts/build_minimal_client.ps1`: pinned Docker/WSL, --network none,
  GCC 15.2.0. Own code -Werror; пять pinned vendor files имеют документированное
  исключение format warning для одинакового 32-bit ABI int/long. Vendor source
  не менялся. ELF bootstrap text/data/BSS 8048/16/28, user 246524/1860/25652.
  Symbol checks исключили ExitGame/network/DDR/display/pad; heap 32 КиБ
  подтверждён по sce_newlib_heap_kb_size в ELF.
- ZIP build/minimal-client/PSP_RA_MinimalClient_v0.9.zip содержит ровно шесть
  ожидаемых файлов (2 PRX, инструкция/install-entry и 2 лицензии). PRX версии
  0.9, kernel/user attributes проверены, source/ZIP bytes совпадают. Старый
  архив 0.8 сохранил SHA256. Public-check прошёл, файлы карты PSP не менялись.
  Размер ZIP 155902 байта, SHA256
  7eb9727cff9c65afd992325ae285447d9d1cf93fb4d4e2a0ab6ce03962e855dd.
  PRX bootstrap/user 12454/307462 байта. Проверены syntax сборщика, локальные
  ссылки и whitespace; git diff ограничен untracked-состоянием исходников.

На момент сборки оставалось на железе: один запуск 0.9 с двумя логами и сообщением о работе игры.
После подтверждения runtime/памяти — bounded HTTPS в игре, game memory adapter
и проверенный frame source, реальный набор LocoRoco в spectator. Softcore-зачёт
потребует отдельного разрешения. Не объявлять синтетический host event зачётом
игры или готовностью полного клиента; реальная сеть/условия пока не интегрированы.

## Runtime 0.9: отказ SMOption и исправление 0.10

14 октября изучена D:/SEPLUGINS/RAClientLite. PRX bootstrap/user совпали с
пакетом 0.9 по SHA256. В client-bootstrap.log: LoadModule вернул UID 0x04291E3B,
проверены RAClientUser attr=0, text/data/BSS 246500/1836/25592 и segment 274296.
Free перед load 864512, после 589824. StartModule и StopModule вернули
0x80020001, status=-1; ready/done/idle/frames=0. client-user.log отсутствует.
Очистка не прошла, unload/free не вызывались: расход 274688 байт остался.
Метка after_cleanup second=120 была константой, отказ произошёл на интервале 10.

Причина в общем stack_option: PSP_THREAD_ATTR_USER=0x80000000 недопустим
для SceKernelSMOption. В uOFW _checkSMOptionConditions отвергает этот бит
с SCE_ERROR_KERNEL_ERROR; StartModule и StopModule вызывают проверку до операции.
Источники: [проверка](https://github.com/uofw/uofw/blob/master/src/kd/modulemgr/loadModuleChecks_inline.h#L64),
[маска](https://github.com/uofw/uofw/blob/master/src/kd/modulemgr/modulemgr_int.h#L11),
[вызовы](https://github.com/uofw/uofw/blob/master/src/kd/modulemgr/start-stopModule.c#L40).
Ревизия CFW владельца не зафиксирована; совпадение пути и обоих кодов объясняет
наблюдение, успешное исполнение исправленного варианта требует PSP.

0.10 задаёт attribute=0 и логирует SMOption; ModuleMgr определяет user-режим
по PRX. CreateThread owner сохраняет USER. При отказе запуска StopModule с
явным SCE_KERNEL_ERROR_NOT_STARTED (0x80020134, подтверждён SDK header)
разрешает UnloadModule; control освобождается только после успешной выгрузки.
Другие отказы stop удерживают модуль/control, исходный результат запуска
не подменяется результатом cleanup. after_cleanup использует текущий интервал.
Сеть, аккаунт, кадры, условия, бюджет RAM и pinned зависимости не менялись.

Проверки и пакет:

- До правки scripts/build_minimal_client.ps1 воспроизвёл 0.9 с прежним SHA256.
  Upstream e0cd8a9b3ccf439532b24b4f1faf2cfc8c0198ab и rcheevos v12.5.0
  1433173220a7eaede6a9ed7a18e94117be1821e0 не менялись. SDK — digest из
  dependencies.lock.json, PSP GCC 15.2.0, Docker/WSL --network none.
- `wsl -d Ubuntu -- make -s -C /mnt/c/Users/itzsy/Documents/ChatGPT/PortableAchievementsSystem ingame-test`
  прошёл. Реальный start_client и observer с mocks проверяют SMOption, отказ
  запуска до записи status, NOT_STARTED -> unload/free, отказ unload и повтор,
  запрет очистки при неопределённом stop и итоговый second=10 вместо 120.
- `wsl -d Ubuntu -- make -s -C /mnt/c/Users/itzsy/Documents/ChatGPT/PortableAchievementsSystem/experiments/ingame BUILD=../../build/client-lifecycle-sanitized 'CFLAGS=-O1 -g -std=c99 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer' 'LDFLAGS=-fsanitize=address,undefined' ../../build/client-lifecycle-sanitized/test_client_cleanup`
  и `wsl -d Ubuntu -- /mnt/c/Users/itzsy/Documents/ChatGPT/PortableAchievementsSystem/build/client-lifecycle-sanitized/test_client_cleanup`
  прошли без диагностик ASan/UBSan.
- scripts/build_minimal_client.ps1 собрал 0.10, проверки символов/лимитов прошли.
  ELF bootstrap text/data/BSS 8260/16/28, user 246524/1860/25652 байта.
  PRX bootstrap/user 12718/307462 байта, версии обоих 0.10.
- build/minimal-client/PSP_RA_MinimalClient_v0.10.zip: 156961 байт, шесть
  ожидаемых файлов, hashes записей совпали с исходными PRX/документом/лицензиями.
  SHA256 6c0662b6aca9c20f5f2e7a447218a58a25b6ecec43d13a295a1d8382b09d543a.
  Архивы 0.8/0.9 сохранили прежние SHA256. Синтаксис PowerShell проверен.
- `wsl -d Ubuntu -- make -s -C /mnt/c/Users/itzsy/Documents/ChatGPT/PortableAchievementsSystem public-check`
  прошёл. В ELF подтверждены версия 0.10 и kernel/user attributes 0x1000/0;
  проверены отсутствие trailing whitespace и соответствие файлов ZIP исходникам.

0.10 ещё не запущена на PSP. Следующий шаг владельца — сохранить журналы 0.9,
заменить оба PRX пакетом 0.10 и проверить create/idle/destroy с двумя логами.
Файлы D: не менялись; сетевых обращений к RA и зачёта достижений не было.

## 0.10 на PSP и компактный HTTPS 0.11

Журналы D:/SEPLUGINS/RAClientLite от 14 октября: оба PRX совпали с 0.10.
StartModule вернул положительный UID 0x04293E65, status=0; libc init вернулся,
RC_CLIENT_READY spectator=1 hardcore=0, ready=1 с интервала 11. В 121 замере
frames=0; idle достиг 5075 на интервале 120. User журнал завершён result=0,
cleanup_result=0, log_error=0. StopModule/status=0, UnloadModule вернул UID,
free control=0. User free/largest восстановились до 864512/864512 байт.
Минимум free 245760 и largest 237056 на интервалах 19–21; обычно 540672/523776.
Kernel free до load 903680, после cleanup 903424: разница 256 байт не доказывает
утечку или её отсутствие, выделения игры параллельны. Оценка FPS/работы игры
владельцем для этого запуска не получена. Запуск/idle/очистка rc_client доказаны,
сеть, набор и условия в 0.10 не исполнялись.

Подготовлен отдельный следующий пакет 0.11; инструкция docs/HTTPS_CLIENT.md.
Установочный каталог прежний, оба PRX заменяются. Это TLS-worker вместо idle
rc_client: одновременное размещение полного клиента и TLS ещё не доказано.
Никакие пароль/токен/набор/award в пакете не используются. User вычисляет
disc0-хеш, сравнивает с ранее официально проверенным хешем образа и только
при совпадении отправляет реально вычисленное значение через rc_api resolve_hash.
Ответ полностью читается и разбирается официальным JSON parser; GameID берётся
с сервера, не подставляется. Исходные vendor-файлы не менялись.

Новый bounded POST transport online/src/ra_https.c: TLS 1.2 ECDHE/ECDSA/AES-GCM,
required certificate/hostname/UTC checks, общий deadline, cancel, heap cap 64 КиБ,
HTTP framing/size bounds, отсутствие raw request/response logging. Counted TLS
allocations стираются при освобождении. online/src/ra_http_body.c принимает
length/chunked/аутентифицированный close, отвергает ambiguous/overflow/truncation.
online/src/ra_net_resources.c удерживает неосвобождённые ресурсы/зависимости
после первой ошибки и позволяет повтор без двойного освобождения.

Для памяти user heap 64 КиБ и stack 32 КиБ, network pool 64 КиБ. Убраны
неиспользуемые newlib timezone/CWD/descriptor adapters, heap/locks сохранены.
Форматирование ограничено проверенным integer/string subset. -Os/sections GC
с явным удержанием syslib exports; отдельный ELF checker проверяет start/stop/
module_info NID, user/kernel attrs, версию и heap. DDR/MMIO/extra RAM не меняются.
Kernel генерирует 80 байт KIRK seed в принадлежащий user control; ошибка или
stuck-output запрещает сеть, seed стирается volatile-записями. Чужие сетевые
модули запрещают запуск; последующие ресурсы учитываются только после успеха.

Read-only проверка текущей TLS-цепочки:
`wsl -d Ubuntu -- sh -c 'printf "" | openssl s_client -connect retroachievements.org:443 -servername retroachievements.org -showcerts 2>&1 | head -n 14'`
показала GTS Root R4 -> WE1 -> retroachievements.org, verify return:1.
Из полного pinned CA-bundle извлечён только GTS Root R4 (DER 525 байт), SHA256
349dfa4058c5e263123b398ae795573c4e1313c83fe68f93556cd5e8031b3c7d.
scripts/extract_ra_tls_root.py проверяет bundle pin/DER/header. Узкий CA store
при смене цепочки откажет безопасно; это не browser trust/revocation support.
Новая compact configuration и anchor записаны в dependencies.lock.json;
upstream e0cd8a9b3ccf439532b24b4f1faf2cfc8c0198ab, rcheevos v12.5.0 commit
1433173220a7eaede6a9ed7a18e94117be1821e0, Mbed TLS 3.6.7 commit
068ff080b369adfac81509f9b57b2afabaf82dc5 и SDK digest не менялись.

Проверки выполнены 14 октября в существующих Ubuntu WSL / Docker SDK:

- До изменения воспроизведена 0.10 через scripts/build_minimal_client.ps1,
  сохранён её ZIP SHA256 6c0662b6aca9c20f5f2e7a447218a58a25b6ecec43d13a295a1d8382b09d543a.
- `wsl -d Ubuntu -- make -s -j4 -C /mnt/c/Users/itzsy/Documents/ChatGPT/PortableAchievementsSystem compact-tls-test ingame-test`
  прошёл: framing, fragmented body, bounds, каждая ошибка network cleanup/retry,
  actual bootstrap с отказом foreign network/KIRK и ранней очисткой.
  Loopback TLS: valid ECDSA/root P384, wrong name, expired, untrusted,
  clock/entropy errors, deadlines, chunked/close, усечение/overflow/gzip.
  На успешных тестах TLS heap peak около 24252 байт, remaining=0.
- `wsl -d Ubuntu -- make -s -j4 -C /mnt/c/Users/itzsy/Documents/ChatGPT/PortableAchievementsSystem/experiments/tls/compact BUILD=../../../build/compact-tls-sanitized 'CFLAGS=-O1 -g -std=c99 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -fsanitize=address,undefined -fno-omit-frame-pointer' 'LDFLAGS=-fsanitize=address,undefined' test`
  прошёл без диагностик ASan/UBSan; используется тот же formatter, что на PSP.
- `wsl -d Ubuntu -- make -s -C /mnt/c/Users/itzsy/Documents/ChatGPT/PortableAchievementsSystem tls-test`
  прошёл после изменения общего loopback-helper (добавлен optional close_notify).
- После volatile erase повторно прошёл
  `wsl -d Ubuntu -- make -s -C /mnt/c/Users/itzsy/Documents/ChatGPT/PortableAchievementsSystem ingame-test public-check`.
- `scripts/build_client_https.ps1` прошёл: pinned Docker --network none,
  PSP GCC 15.2.0, -Werror, documented vendor format warning exception для
  32-bit ABI. ELF bootstrap text/data/BSS 8273/16/28, user 146232/1396/13668;
  image user 161296 байт. PRX bootstrap/user 12202/175854 байта.
  Metadata/exports/heap и отсутствие запрещённых символов проверены сборщиком.
- ZIP build/client-https/PSP_RA_HTTPS_v0.11.zip: 118889 байт, SHA256
  2b2d11d3c8f89366e310bd49552c70cfdb0550f05daf97a94da95de6efd16339.
  Ровно 10 ожидаемых файлов, их hashes совпали с PRX/profile/инструкцией/
  install-entry/четырьмя лицензиями/CA notice. Синтаксис PowerShell проверен.
  Архивы 0.9/0.10 сохранены с прежними hashes. Файлы D: не менялись.

0.11 ещё не запущена на PSP. Дальше нужен один запуск с существующим Wi-Fi
profile.txt, проверка HTTPS_GAME_RESOLVED, памяти и cleanup. Набор/аккаунт
не включаются до подтверждения HTTPS на железе; полноценный set/TLS budget,
очередь транспорта с rc_client, токен lifecycle, game reader/frame source
и spectator achievement остаются отдельными невыполненными пунктами.

## 0.11 на PSP: guard RAM и отказ StopModule; диагностика 0.12

Журналы D:/SEPLUGINS/RAClientLite от 14 октября 15:31:40: оба PRX совпали
с 0.11 по SHA256. profile.txt = 1 LF, disc0-хеш сопоставлен успешно,
KIRK/network gate прошёл. User segment 161572 байта, attr=0; после load
user free 702464, largest 688384. После запуска потока свободная user RAM
составила 669696, largest 655616; остаток стека потока этим не измерен.

После двух load net module и успешного sceNetInit(65536,...) следующий check
зафиксировал USER_MEMORY_GUARD free=153600 largest=147712, result=-35305.
Между HTTPS_READY и guard разница free 516096 байт; она включает rc_api/newlib,
сетевые модули/инициализацию и возможные параллельные выделения игры. Нельзя
приписывать её целиком 64-КиБ пулу sceNetInit. APCTL/подключение/DNS/TLS/HTTPS
ещё не выполнялись. NetTerm и оба unload net module прошли; user log завершён
cleanup_result=0 log_error=0. На интервале 13 ready=1 done=1.

Затем StopModule и status вернули 0x800200D2, что SDK header pspkerror.h:62
называет SCE_KERNEL_ERROR_ILLEGAL_ARGUMENT. Unload user/free control не вызывались,
код/control удержаны. After cleanup user free/largest 669696/655616 вместо
исходных 864512/864512; итог bootstrap result=-35305, cleanup_result=-2147352366.
Из 0.11 неизвестно, вернул ли D2 WaitThreadEnd, DeleteThread или более ранний
путь: отдельной общей диагностики не было. FPS/ощущения игры не сообщены.

По запросу владельца подготовлена **диагностическая 0.12**:

- net_memory до/после rc_api request init/destroy, libc init, load COMMON/INET,
  net/inet/resolver/APCTL init, connect, DNS/socket/TLS и освобождения ресурсов.
  Частые BIO/poll/GetState циклы не засоряют лог одинаковыми замерами.
  После успешного acquisition ownership записывается даже при отказе post-call
  записи журнала; cleanup не прерывается из-за ошибки диагностического I/O.
- module_stop передаёт stage, owner/caller UID, отдельные wait/delete результаты
  и остаток timeout через control. После StopModule bootstrap пишет
  client_stop_diag, пока control ещё жив. User-журнал не открывается повторно.
  NOT_CALLED = 0x7FFFFFFF; NONE позволяет отличить отсутствие входа в helper.
- Свободная RAM вокруг libc deinit также передаётся через control, так как
  user log уже закрыт. Исходный worker_result сохраняется отдельно от cleanup.
- Реальный stop helper выделен в stop_owner.h для failure-injection теста.
  Ошибки wait/delete и done=0 удерживают UID/control; unsafe unload не добавлен.
  Недостаток RAM и аппаратный D2 не объявлены исправленными без нового запуска.

Heap 64 КиБ, owner stack 32 КиБ, entry stack 8 КиБ, pool 64 КиБ и guard
192 КиБ не изменены; 0.12 может повторно остановиться с -35305. TLS/core,
сертификаты, аккаунт/набор/frame/award и системные настройки не менялись.
Замеры не атомарны и не доказывают исключительное владение разницей RAM.

Проверки 14 октября 2026 года:

- До правок scripts/build_client_https.ps1 воспроизвёл 0.11 с прежним SHA256
  2b2d11d3c8f89366e310bd49552c70cfdb0550f05daf97a94da95de6efd16339.
  Исходники для сравнения сохранены в .work/https-0.11-before. Upstream
  e0cd8a9b3ccf439532b24b4f1faf2cfc8c0198ab, rcheevos v12.5.0 commit
  1433173220a7eaede6a9ed7a18e94117be1821e0, Mbed TLS 3.6.7 и SDK digest
  из dependencies.lock.json не менялись.
- `wsl -d Ubuntu -- make -s -C /mnt/c/Users/itzsy/Documents/ChatGPT/PortableAchievementsSystem ingame-test`
  прошёл. Новый actual stop helper test различает ILLEGAL_ARGUMENT в wait/delete,
  проверяет pending sentinel/timeout, done=0, повтор, отсутствие живого control
  и сохранение исходного worker error. Actual bootstrap test проверяет печать
  WAIT_ERROR/pending delete и удержание модуля/control после StopModule D2.
- `wsl -d Ubuntu -- make -s -C /mnt/c/Users/itzsy/Documents/ChatGPT/PortableAchievementsSystem/experiments/ingame BUILD=../../build/https-stop-sanitized 'CFLAGS=-O1 -g -std=c99 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer' 'LDFLAGS=-fsanitize=address,undefined' ../../build/https-stop-sanitized/test_https_stop`
  и `wsl -d Ubuntu -- /mnt/c/Users/itzsy/Documents/ChatGPT/PortableAchievementsSystem/build/https-stop-sanitized/test_https_stop`
  прошли без диагностик ASan/UBSan.
- scripts/build_client_https.ps1: pinned Docker/WSL --network none, PSP GCC
  15.2.0, -Werror. CA pin, syslib exports, privilege/version 0.12, heap 64 КиБ,
  размер и symbol checks прошли. ELF bootstrap text/data/BSS 8748/16/28;
  user 147772/1396/13668, image 162836 байт. PRX 12886/178774 байта.
- ZIP build/client-https/PSP_RA_HTTPS_v0.12.zip: 121539 байт, SHA256
  2dcef39ee7bda6f1ab155f0cbc2a9a10d906e9b8e1067306c389786329e79cf5.
  Все 10 записей побайтно соответствуют исходным PRX/profile/инструкции/
  install-entry/лицензиям/CA notice. Архив 0.11 сохранил прежний SHA256.
  Синтаксис PowerShell проверен; diff с сохранённым user source просмотрен.
- `wsl -d Ubuntu -- make -s -C /mnt/c/Users/itzsy/Documents/ChatGPT/PortableAchievementsSystem public-check`
  прошёл. Файлы на D: не менялись, сетевых обращений к RA/аккаунту не было.

Остался один запуск 0.12 на PSP с прежним profile.txt и двумя полными логами.
По net_memory и client_stop_diag нужно выбрать изменение размещения сети
и исправить конкретный путь остановки. Полный клиент/HTTPS/достижения на PSP
этим пакетом ещё не подтверждены.

## 0.12 на PSP и переход к ARK 5.1.0 +16MiB: 0.13

Владелец уточнил ARK 5.1.0. Логи 0.12 и SHA256 PRX совпали со сборкой:
COMMON 81920 байт, INET 360448, net init 73728; сумма 516096 (504 КиБ),
user free 668160 → 152064 → 668160 при очистке. До APCTL/DNS/TLS не дошло.
Stop diag: WAIT_ERROR(3), wait=800200D2, delete=PENDING, done=1,
worker_result=-35305; owner/caller различаются. Причина преобразования
отрицательного thread exit status — обоснованная исходниками PPSSPP гипотеза,
не доказанная версия поведения конкретной прошивки. Результат не объявлен
исправленным на PSP без нового запуска.

По запросу владельца подготовлена 0.13:

- HTTPS-bootstrap требует partition 2 start=08800000, size=02800000 (40 МиБ),
  user attributes. Это режим ARK Force +16MiB; 24 МиБ/MAX отклоняются до load.
  Проверки text/entry/segments/control охватывают весь выбранный раздел,
  исключают kernel alias и переход за конец. Никаких DDR/MMIO/CFW изменений.
- Настоящий rc_client создаётся перед HTTPS, spectator=1/hardcore=0 проверяются
  getter-ами; idle на том же worker, destroy до libc deinit. Нет игры/аккаунта,
  memory callback отказывает, server callback клиента отклоняет все запросы.
  Единственный сетевой вызов — прежний rc_api resolve_hash реального disc0.
- Worker/observer возвращают thread exit status=0, результат операции отдельно.
  Ошибка cleanup оставляет done=0. WaitThreadEnd проверяется вместе с
  ReferThreadStatus: для удаления нужны STOPPED и done=1; отрицательный wait
  допустим только при совпадении с ThreadMan exitStatus. Живой/убитый поток,
  неизвестное состояние/ошибки сохраняют ресурсы. Добавлены refer/status/exit.
- Сетевой pool, heap/стеки, guard RAM, CA/TLS/timeouts и хеш не ослаблены.
  rc_client vsnprintf перенаправлен на существующий integer formatter для
  ограниченной пробы. Полноценные форматы прогресса ещё не проверены.
  Library award internals могут остаться из-за зависимостей rc_client;
  символы входов login/load_game/do_frame отсутствуют, server callback закрыт.
- Инструкция docs/HTTPS_CLIENT.md: сначала игра с +16MiB без RA, затем один
  запуск 0.13. Карта памяти может менять размещение данных игры; совместимость
  реального набора и кадры по-прежнему требуют отдельного подтверждения.

Проверки 14 октября 2026:

- Перед правками scripts/build_client_https.ps1 воспроизвёл 0.12 с прежним
  SHA256 2dcef39ee7bda6f1ab155f0cbc2a9a10d906e9b8e1067306c389786329e79cf5.
  Копия затронутых исходников сохранена в .work/https-0.12-before, diff просмотрен.
- Upstream e0cd8a9b3ccf439532b24b4f1faf2cfc8c0198ab, rcheevos v12.5.0
  1433173220a7eaede6a9ed7a18e94117be1821e0, Mbed TLS 3.6.7 и PSPDEV digest
  из dependencies.lock.json не менялись; vendor и исходный plugin не менялись.
- `wsl -d Ubuntu -- make -s -C /mnt/c/Users/itzsy/Documents/ChatGPT/PortableAchievementsSystem online-test ingame-test public-check`
  прошёл: официальный хеш/карта памяти, настоящий rc_client/синтетическое
  spectator-достижение/асинхронная очередь, actual bootstrap и stop helper.
- `wsl -d Ubuntu -- make -s -C /mnt/c/Users/itzsy/Documents/ChatGPT/PortableAchievementsSystem/experiments/ingame BUILD=../../build/https-013-sanitized 'CFLAGS=-O1 -g -std=c99 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer' 'LDFLAGS=-fsanitize=address,undefined' ../../build/https-013-sanitized/test_https_stop ../../build/https-013-sanitized/test_https_bootstrap`
  и запуск обоих бинарников через `wsl -d Ubuntu --` прошли без диагностик.
  Включены control по 0A100000, text в расширенной области, выход за конец,
  ранний отказ 24/55 МиБ, отрицательный exit status, ошибки wait/refer/delete,
  done=0 и живой/убитый поток. Новые случаи проверяются реальным кодом.
- `./scripts/build_client_https.ps1` прошёл с pinned SDK, Docker --pull=never
  --network none, PSP GCC 15.2.0, -Werror. Exports/privilege/version/heap/size/
  required/forbidden symbols проверены. ELF kernel text/data/BSS 9112/16/28;
  user 206604/1768/13680, суммарно 222052 байта при лимите 294912.
- ZIP build/client-https/PSP_RA_HTTPS_v0.13.zip: 153001 байт, SHA256
  4c771acb644ba072705ef42c1f15649ef3bbe1a7bc16aef9c6fa40067742b70b.
  Kernel PRX SHA256 b231d2d8103853e5632452c9b075342720b5c4470e50365adcb2ce385f3d3fec;
  user PRX 54b22bcb3a550f5e4b3950fe6abcacf61077101d4336fccb562fbde9c8fc5734.
  PowerShell parser проверил сборщик. Архив 0.12 сохранён с прежним хешем.

0.13 на PSP ещё не проверена. Нет входа/набора/чтения памяти игры/frame/award.
Файлы на D: не менялись, SDK не устанавливался, обращения к аккаунту не было.
Следующее действие владельца — инструкция +16MiB и два журнала. Следующая
реализация после аппаратного успеха — реальный набор в spectator с очередью
сетевых ответов, вход/выход/удаление токена, проверенные game reader/frame.

## 0.13 на PSP: память занята к метке 3; ранняя инициализация 0.14

Владелец подтвердил нормальную работу игры с ARK 5.1.0 +16MiB и 0.13.
Оба PRX в D:/SEPLUGINS/RAClientLite совпали с 0.13. Partition 2 действительно
40 МиБ; free на метках 0–2=29379072, с метки 3=864512. Следовательно,
расширение раздела не сохранило свободный запас после старта игры. Вероятно,
игра увеличивает heap, но владельца конкретных выделений по этому логу
установить нельзя. Считать +16MiB достаточным решением было неверно.

0.13: rc_client create выделил libc heap 65536 байт, idle=1142.
COMMON 81920 и INET 360448 оставили 101120 байт; guard сработал до net init.
TLS/APCTL/DNS не запускались. Результат -35305, cleanup_result=0.
Исправление остановки подтверждено на PSP: FINISHED, wait/delete/refer/exit=0,
ThreadMan STOPPED(0x10), unload/control free успешны. User free вернулась
к 864512, largest тоже 864512. Освобождение libc вернуло её 64 КиБ heap.

По запросу владельца сделана 0.14:

- User PRX загружается сразу после обнаружения LocoRoco, на метке 0 вместо 10.
  Старт/after_load показывают фактическую метку вызова.
- После libc/rc_client и быстрых profile/RTC/power checks worker сначала
  загружает COMMON/INET, net pool, inet/resolver/APCTL и вызывает Connect.
  Дорогой disc0-хеш перенесён после приобретения этих ресурсов.
- Worker удерживает настоящие ресурсы, heap и стеки, пока bootstrap на метке
  10 не выставит request_allowed. В ожидании работают idle/cancel/guard.
  Освобождения фиктивного резерва с последующей попыткой забрать память нет.
- До load требуется free>=2 МиБ; перед каждым ранним приобретением также
  largest>=2 МиБ. Это консервативный preflight для отсечения поздних 844 КиБ,
  не резерв и не гарантия выигрыша гонки с выделениями игры. При пропущенном
  окне -35316/-35317, без поздней повторной попытки. Игра не приостанавливается,
  hooks/DDR/CFW/priority/game writes не добавлены.
- Инициализация/ожидание release ограничены 20 с. Хеш и HTTPS затем делят 45 с;
  файловый цикл теперь проверяет deadline, сохраняя параметры официального
  хеша. Ready означает успешный init/принятый Connect, не GOT_IP/TLS.
- Сетевая очистка перенесена в общий выход worker: выполняется и при отказе
  хеша, ожидания, логирования, частичного init. Владение фиксируется сразу
  после успешного native API до потенциально неуспешной записи журнала.
  Native cleanup error удерживает зависимости и done=0. Stop helper 0.13 сохранён.
- Heap 64 КиБ, owner stack 32 КиБ, net pool 64 КиБ, free floor 192 КиБ,
  CA/TLS/проверка часов и сертификатов не ослаблены. Аккаунт/набор/frame/award
  не добавлены. Успех ранней схемы и HTTPS на PSP пока не заявляется.

Проверки 14 октября 2026:

- До изменения `./scripts/build_client_https.ps1` воспроизвёл 0.13 с SHA256
  4c771acb644ba072705ef42c1f15649ef3bbe1a7bc16aef9c6fa40067742b70b.
  Затронутые исходники сохранены в .work/https-0.13-before, diff просмотрен.
- Pins не менялись: upstream e0cd8a9b3ccf439532b24b4f1faf2cfc8c0198ab;
  rcheevos v12.5.0 1433173220a7eaede6a9ed7a18e94117be1821e0;
  Mbed TLS 3.6.7 068ff080b369adfac81509f9b57b2afabaf82dc5;
  PSPDEV digest 511bd8c9eaff77c8a2e205631ae060b9cdf0e9b7e61a7a6ef19482fb4affcdfa,
  PSP GCC 15.2.0. Vendor/original plugin не менялись.
- `wsl -d Ubuntu -- make -s -C /mnt/c/Users/itzsy/Documents/ChatGPT/PortableAchievementsSystem ingame-test`
  прошёл. Actual bootstrap проверяет start на метке 0, release на 10,
  отсутствие load в позднем состоянии и прежнюю безопасную очистку.
  Actual early_network.h test проверяет каждый native отказ и каждую ошибку
  post-call log: точное число освобождений, успешное удержание при ожидании,
  cancel/deadline, low free/largest, cleanup error и retry.
- `wsl -d Ubuntu -- make -s -C /mnt/c/Users/itzsy/Documents/ChatGPT/PortableAchievementsSystem/experiments/ingame BUILD=../../build/https-014-sanitized 'CFLAGS=-O1 -g -std=c99 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer' 'LDFLAGS=-fsanitize=address,undefined' ../../build/https-014-sanitized/test_https_bootstrap ../../build/https-014-sanitized/test_early_network`
  и запуск обоих бинарников через `wsl -d Ubuntu --` прошли без диагностик.
  После уточнения точных release counts и fragmentation test_early_network
  пересобран и запущен с теми же sanitizers, PASS.
- `./scripts/build_client_https.ps1`: pinned Docker --pull=never --network none,
  -Werror, CA/exports/version 0.14/heap/size/symbol checks прошли.
  ELF kernel text/data/BSS 9420/16/28, user 207388/1768/13680 (222836 байт).
  PRX kernel 13782 байта, user 252074, лимиты 288/320 КиБ соблюдены.
- `wsl -d Ubuntu -- make -s -C /mnt/c/Users/itzsy/Documents/ChatGPT/PortableAchievementsSystem public-check`
  прошёл. PowerShell parser сборщика и whitespace проверены.
- ZIP build/client-https/PSP_RA_HTTPS_v0.14.zip: 154762 байта, SHA256
  dead6c9eb6217b7e5d9da61e8fd2227c216926dc54d1ea4e30244813e87e7552.
  Все 10 entries совпадают по SHA256 с исходными PRX/profile/docs/licenses.
  Kernel PRX SHA256 95feb381e73bb8bc6bc0373baa8d388261393183a7d38b1e667b2a148c46d779;
  user PRX 3c935b6602a8c091045e73368da357e36c0e2b016de9a205c06ae186d8dcaa6c.
  Архив 0.13 сохранён с прежним SHA256.

Файлы на D: не менялись, SDK не устанавливался, аккаунт не использовался.
Следующий шаг владельца: оставить +16MiB, заменить оба PRX на 0.14, один запуск
2–3 минуты с Wi-Fi и два лога. Повторять тест без плагина не требуется.
На PSP ещё нужно подтвердить раннее приобретение ресурсов, нормальную работу
игры, HTTPS_GAME_RESOLVED и очистку. Полноценный набор/память игры/кадры остаются
следующим этапом после подтверждения транспорта и бюджета.

## 0.14 на PSP: APCTL остаётся отключённым; отложенный Connect 0.15

Оба PRX владельца совпали с 0.14. User PRX исполнялся по адресу 0AFC2700
в расширенной partition 2. Ранние COMMON/INET/net/inet/resolver/APCTL init
прошли; guard памяти не сработал. Наблюдавшаяся free около 875008 байт,
минимум 580096. После очистки free=1719040. Наблюдение APCTL после раннего
Connect показало только DISCONNECTED(0), итог -35203 по deadline до DNS/TCP/TLS.
Обработчика событий в 0.14 не было, поэтому причина отказа не установлена.
Очистка и остановка успешны. Перенос Connect — проверяемая гипотеза,
а не доказанное исправление аппаратной причины.

В 0.15 ранние сетевые init и удержание ресурсов сохранены. Connect вызывается
только после request_allowed (метка 10) и проверки disc0-хеша. Перед ним:

- Проверка переключателя WLAN и наличия профиля по номеру, без чтения SSID,
  пароля или изменения настроек. Питание WLAN записывается в лог; 0 допустим
  до включения драйвера по требованию. Чужое активное состояние не захватывается.
- Регистрация обработчика APCTL до Connect. Callback кладёт время/состояния/
  событие/native error в статическую очередь на 16 записей под семафором;
  лог пишет worker. Первая ошибка сохраняется даже при переполнении очереди.
- Очистка Disconnect → DelHandler → ApctlTerm → остальные зависимости →
  unload → drain → DeleteSema. Handler ID=0 действителен. При ошибке очистки
  сохраняются зависимости/семафор/модуль. Владение фиксируется до записи лога.
- Коды -35318…-35322 различают WLAN switch, профиль, занятый APCTL,
  событие ERROR без native error и вызов Connect до request_allowed.

Heap/стеки/лимиты/сертификаты и stop helper сохранены. Pins не менялись:
upstream e0cd8a9b3ccf439532b24b4f1faf2cfc8c0198ab;
rcheevos v12.5.0 1433173220a7eaede6a9ed7a18e94117be1821e0;
Mbed TLS 3.6.7 068ff080b369adfac81509f9b57b2afabaf82dc5;
SDK digest 511bd8c9eaff77c8a2e205631ae060b9cdf0e9b7e61a7a6ef19482fb4affcdfa,
PSP GCC 15.2.0. Исходники до изменений — .work/https-0.14-before.

Проверки 14 октября 2026:

- До изменений `./scripts/build_client_https.ps1` воспроизвёл прежний SHA256
  пакета 0.14. Старый архив сохранён с тем же хешем.
- `wsl -d Ubuntu -- make -s -C /mnt/c/Users/itzsy/Documents/ChatGPT/PortableAchievementsSystem ingame-test`
  прошёл. Тестируются настоящие early_network/apctl_observer/connect_profile
  headers: Connect отсутствует при раннем init, профили/WLAN/состояния,
  handler ID=0, очередь/переполнение/native error, callback без записи лога,
  ошибки регистрации/лога/guard/очистки и сохранение владения.
- ASan/UBSan: сборка в experiments/ingame командой
  `make -s BUILD=../../build/https-015-sanitized 'CFLAGS=-O1 -g -std=c99 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer' 'LDFLAGS=-fsanitize=address,undefined' ../../build/https-015-sanitized/test_apctl_connect ../../build/https-015-sanitized/test_early_network`
  через `wsl -d Ubuntu --` и запуск обоих бинарников прошли без диагностик.
  Ресурсный тест собран в experiments/tls/compact с теми же флагами,
  `BUILD=../../../build/https-015-resources` и target
  `../../../build/https-015-resources/test_resources`; запуск прошёл.
- `./scripts/build_client_https.ps1`: закреплённый Docker SDK, --pull=never,
  --network none, -Werror, CA/exports/version/heap/size/symbol checks прошли.
  ELF kernel text/data/BSS 9420/16/28; user 209432/1776/14024 (225232 байта).
  PRX kernel 13782 байта, user 255066; лимиты 288/320 КиБ соблюдены.
- `wsl -d Ubuntu -- make -s -C /mnt/c/Users/itzsy/Documents/ChatGPT/PortableAchievementsSystem public-check`
  прошёл. PowerShell parser сборщика прошёл. Все 10 entries ZIP сверены
  по SHA256 с исходными PRX/profile/docs/licenses, совпадение полное.
- ZIP build/client-https/PSP_RA_HTTPS_v0.15.zip: 157352 байта, SHA256
  6692f566c2ff9dbcdd41f250b3fcf189bed4d72ebd1f11a97dff509e105a2d9c.
  Kernel PRX SHA256 2ed6bed3a2e74262a4337404e1b3ea865d29a32836a2e149c6a612fadfb4fe68;
  user PRX ec8ade9f8449f7e59eeabbf19ad819016fa1cb010e565cf7501797fbdcec7885.

0.15 на PSP не проверена. Аккаунт/набор/чтение памяти игры/frame/award
не добавлены; HTTPS_GAME_RESOLVED ещё требует подтверждения внутри игры.
Файлы на D: не менялись, SDK не устанавливался, запросов к аккаунту не было.
Владельцу: оставить +16MiB, сохранить логи 0.14, заменить оба PRX,
сохранить profile.txt и включить только bootstrap; Wi-Fi включить до запуска,
полностью перезапустить PSP, поиграть 2–3 минуты, выйти HOME и передать логи.

## 0.15 на PSP: HTTPS PASS; вход и spectator-набор 0.16

Свежие журналы владельца client_version=0.15; SHA256 обоих PRX на D:
совпали со сборкой. client-user.log:28 подтвердил ранний init, bootstrap:29 —
release second=10. User:29 — реальный disc0-хеш совпал с официальным PC.
WLAN switch/power=1, профиль существует, handler ID=0 действителен.
APCTL события user:43–50 прошли 0→2→6→3→4, native error=0.
DNS/TCP успешны. TCP connect=-1 — промежуточный неблокирующий результат:
код допустил продолжение, конечный HTTPS доказывает успешное соединение.
User:71 — stage=complete, code=0, verify_flags=0, HTTP 200, body=31,
TLS heap peak=25044, remaining=0. User:72 — Game ID=10523 из ответа сервера.
Оба финальных результата/cleanup/log_error=0. Bootstrap:48–54 — STOPPED,
wait/delete/exit=0, unload/control free успешны, итог user free=1722112.
Минимум наблюдавшейся free=564480, largest=561408 (second=16).
Это подтверждает транспорт внутри игры; кадры/набор/award в 0.15 отсутствуют.
Перенос Connect прошёл в этом запуске, точная причина отказа 0.14 не доказана.

По запросу владельца подготовлена 0.16:

- Настоящие token login и begin_load_game для вычисленного disc0-хеша,
  spectator=1/hardcore=0. Принимается только Game ID=10523, console=41,
  ненулевой набор. ID не подставляется. Память отклоняется, do_frame
  исключён из PSP ELF. Unsupported из-за отказа чтения ожидаем и логируется;
  работоспособность условий и открытия аккаунта здесь не заявляются.
- Использован существующий ra_client_session: отдельный HTTPS-поток берёт
  запросы, owner получает ответы через очередь и один вызывает rc_client.
  Под семафором 2 слота, POST 8 КиБ, response 64 КиБ, общий cap 72 КиБ;
  отдельный response-буфер 64 КиБ заранее выделен из heap. Транспорт допускает
  только login2/achievementsets на точном HTTPS endpoint. До 4 запросов,
  30 с на запрос, 100 с на хеш/Wi-Fi/сессию после release, early init 20 с.
- HTTPS/сертификаты/RTC сохранены. KIRK seed однократно питает родительский
  CTR-DRBG; каждый TLS handshake получает новую порцию из него. Это заменяет
  ограниченный одноразовый seed 0.15 для нескольких соединений. DNS 1 с/retry 1,
  poll <=50 мс. HTTPS-поток не пишет лог; numeric report передаётся owner.
- Heap увеличен до фиксированных 2 МиБ и приобретается ранним malloc.
  Оба рабочих стека по 32 КиБ. Для полного парсинга rcheevos используется
  настоящий newlib formatter; диагностический log оставлен ограниченным.
  Первая сборка дала ELF 305468 байт и была отклонена старым лимитом 288 КиБ.
  Новые проверяемые лимиты: ELF 320 КиБ, PRX 384 КиБ. Heap 2 МиБ — параметр
  эксперимента; фактическая достаточность/влияние на игру пока неизвестны.
  mallinfo arena/allocated/free_in_arena логируются до/после набора/клиента.
- При завершении сначала join + ThreadMan STOPPED + done сетевого потока,
  затем queue/client logout/destroy, сеть/APCTL, libc heap. Отказ join/close
  удерживает объекты/модуль с done=0. Native queue guard error завершает поток
  без подтверждения done: продолжение без блокировки исключено.
- Добавлены строгий парсер auth.txt и утилиты configure_psp_login.py/.ps1.
  Владелец вводит пароль скрыто на PC; официальный login2 по проверенному TLS
  возвращает token, сохраняется только username/token с маркером формата.
  Нет аргументов с паролем, вывода серверных ошибок/ответов, redirects,
  перезаписи существующего файла. Logout удаляет только распознанный файл.
  auth.txt не зашифрован, исключён из Git/public tree/ZIP. Для диагностики
  передаются только два .log, а не вся папка. Пароль/токен агенту не нужны.

Pins не менялись: upstream e0cd8a9b3ccf439532b24b4f1faf2cfc8c0198ab;
rcheevos v12.5.0 1433173220a7eaede6a9ed7a18e94117be1821e0;
Mbed TLS 3.6.7 068ff080b369adfac81509f9b57b2afabaf82dc5;
SDK digest 511bd8c9eaff77c8a2e205631ae060b9cdf0e9b7e61a7a6ef19482fb4affcdfa,
PSP GCC 15.2.0. Vendor/original plugin не менялись. До изменений сборка 0.15
воспроизведена с прежним SHA256; исходники сохранены в .work/https-0.15-before.

Проверки 14 октября 2026:

- `wsl -d Ubuntu -- make -s -C /mnt/c/Users/itzsy/Documents/ChatGPT/PortableAchievementsSystem online-test ingame-test public-check`
  прошёл: официальный hash/memory, очередь/тайм-аут/отмена/close, неуспешный login,
  настоящий rc_client с синтетическим набором, парсер токена, прежние игровые
  проверки и actual stop_transport.h с native отказами/ретенцией/retry.
- Дополненные тесты параллельного owner idle/transport complete и варианта
  RA_SESSION_NO_FRAMES прошли под ASan/UBSan. В последнем память отклоняется,
  набор загружается с unsupported=1; даже запросы кадра не вызывают do_frame,
  события/award отсутствуют, за 60 с spectator не делает startsession/ping.
- Точная sanitizer-сборка из experiments/host через `wsl -d Ubuntu --`:
  `make -s BUILD=../../build/https-016-sanitized 'CFLAGS=-O1 -g -std=c99 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer' 'LDFLAGS=-fsanitize=address,undefined' ../../build/https-016-sanitized/test_client_session ../../build/https-016-sanitized/test_client_session_noframes ../../build/https-016-sanitized/test_auth_file`.
  Все три бинарника запущены через `wsl -d Ubuntu --`, без диагностик.
  Аналогично experiments/ingame, BUILD=../../build/https-016-stop-sanitized,
  target ../../build/https-016-stop-sanitized/test_transport_stop: PASS.
- Четыре mocked HTTPS/file/CLI теста configure_psp_login прошли в WSL и
  Windows Python 3.12.14 из существующего runtime Codex. Проверены проверка
  сертификата/имени, отказ redirect/большого/повреждённого ответа, exclusive
  create, безопасный logout и отсутствие секретов/исключений в выводе.
  На Windows TEMP/TMP заданы в build/setup-test-temp для sandbox-совместимого
  удаления тестовых файлов; системные настройки не менялись.
- `./scripts/build_client_https.ps1`: pinned Docker --pull=never --network none,
  -Werror, CA/exports/0.16/heap/sizes/symbol checks прошли. В ELF есть
  token-login/load/queue/logout/transport, нет password login/do_frame/ExitGame/
  DDR/CFW hooks. ELF kernel text/data/BSS 9400/16/28; user 294568/1784/10328,
  суммарно 306680 байт. PRX kernel=13766, user=361562 байта.
- Оба PowerShell-скрипта прошли parser. ZIP: все 12 entries сверены по SHA256
  с исходными PRX/profile/docs/helpers/licenses, лишних/секретных entries нет.
- build/client-https/PSP_RA_HTTPS_v0.16.zip: 207344 байта, SHA256
  980cbd7e97071982daf83d2e7ddbb6e0f909a02c7bf0dfe2be7409e77358fb7d.
  Kernel PRX SHA256 133badb32200ac83754f6fb7df9a61a1c92eb92a00918b1e1ca70a062e5a7b4d;
  user PRX 2c4f5a96482e6537f03ae8fd11002aaf5e502c44fa8ea4138a6848a759ec38af.
  Архив 0.15 сохранён: 6692f566c2ff9dbcdd41f250b3fcf189bed4d72ebd1f11a97dff509e105a2d9c.

0.16 на PSP и с настоящим аккаунтом не проверена. Никаких запросов к аккаунту
агент не выполнял, D: не менялся, SDK не устанавливался. Нужно измерить размер
ответа/heap настоящего набора, поведение двух потоков, остановку и стабильность
игры с новым резервом. Ответ больше 64 КиБ отвергается, а не обрезается.
После этой аппаратной проверки — чтение памяти/источник кадров в spectator;
реальный award требует отдельного разрешения владельца.

## 0.16 на PSP: login PASS, HTTP-body отказ; приёмник 0.17

Оба PRX на карте совпали с 0.16. User log:50–51 — login HTTP 200,
verify_flags=0, body=244, logged_in=1/spectator=1/hardcore=0. Значения
auth.txt не читались. Второй запрос: user:52 HTTP_body/-35202, затем
SPECTATOR_SET result=-32 (RC_NO_RESPONSE), loaded=0. Это следствие отклонённого
транспортом ответа, не отказ токена. Старая диагностика не сохраняла status,
размер и конкретную причину декодера; превышение 64 КиБ не доказано.

Минимальная наблюдавшаяся user free=540160, largest=538880. TLS peak=25044,
remaining=0 у обоих запросов. Guard/OOM не отмечены; отказ возник в декодере.
Текущие mallinfo после отказа: arena=96512, allocated=66232 — не пик парсинга
набора, который не состоялся. Очистка успешна: free_before_deinit=1422848,
after=3520000 (возврат ровно 2097152), после unload=3860224. Bootstrap
FINISHED/STOPPED, wait/delete/exit=0, cleanup_result=0, итог -35406.

По запросу владельца сделана 0.17:

- Причины HTTP отказов раздельны: лимит тела/headers/строки/chunk metadata/
  trailers, syntax/framing/числовое переполнение, encoding, truncation,
  закрытие без close_notify и native TLS/BIO error. Статус и числовые
  счётчики сохраняются при ошибке. Частичный буфер очищается, body_size=0;
  успешно полученным ответом для rc_client он не становится.
- HTTP_DIAG выводит только фиксированные error/phase/encoding и числа:
  received/declared/has_length/overflow/wire/header/trailer/chunk counters.
  Заголовки, куки и тело не выводятся. Native TLS/BIO code сохраняется.
- Поддержаны field-name tchar из RFC 9110, extensions и trailers из RFC 9112.
  Неизвестные extensions/trailers валидируются и игнорируются; Content-Length,
  Transfer-Encoding/Content-Encoding в trailers запрещены. Неоднозначный
  framing, gzip/deflate/br, redirects и ошибки сертификатов по-прежнему
  отвергаются. Прочитаны первичные RFC 9112 7.1 и RFC 9110 5.6.2.
- Исправлен доказанный дефект кода: строки размеров чанков раньше тратили
  общий header budget=4096. Теперь header=4096, chunk metadata=16384,
  trailer=2048; строка до 1022 без CRLF. 1500 однобайтовых чанков проходят.
- Body cap=65536 остаётся прежним, теперь задан одним ra_response_limits.h
  для декодера/HTTPS/очереди. Heap 2 МиБ, стеки, early init/deferred Connect,
  spectator/no frames/no award и stop helpers не менялись. Токен сохраняется.
  Инструкция учитывает перенос файлов через карту без кабеля PSP.

Baseline 0.16 воспроизведён с прежним SHA256, исходники сохранены в
.work/https-0.16-before, diff просмотрен. Pins без изменений: upstream
e0cd8a9b3ccf439532b24b4f1faf2cfc8c0198ab; rcheevos v12.5.0
1433173220a7eaede6a9ed7a18e94117be1821e0; Mbed TLS 3.6.7
068ff080b369adfac81509f9b57b2afabaf82dc5; SDK digest
511bd8c9eaff77c8a2e205631ae060b9cdf0e9b7e61a7a6ef19482fb4affcdfa,
PSP GCC 15.2.0. Vendor/original plugin не менялись.

Проверки 14 октября 2026:

- `wsl -d Ubuntu -- make -s -C /mnt/c/Users/itzsy/Documents/ChatGPT/PortableAchievementsSystem compact-tls-test`
  PASS: byte-fragmented parser, точный предел 64 КиБ, числовое переполнение,
  syntax/extensions/trailers, отдельные бюджеты и 20 loopback HTTPS-сценариев.
  Проверены сохранение status/received после ошибки, кодировки/дубликаты,
  TLS verify/timeout/close/cleanup и отсутствие DIAG_SECRET в выводе.
- Полный тот же набор под ASan/UBSan прошёл. Точная команда:
  `wsl -d Ubuntu -- make -s -C /mnt/c/Users/itzsy/Documents/ChatGPT/PortableAchievementsSystem/experiments/tls/compact BUILD=../../../build/https-017-sanitized 'CFLAGS=-O1 -g -std=c99 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -fsanitize=address,undefined -fno-omit-frame-pointer' 'LDFLAGS=-fsanitize=address,undefined' test`.
  Повтор после уточнения BIO cancel/timeout также PASS, без диагностик.
- `wsl -d Ubuntu -- make -s -C /mnt/c/Users/itzsy/Documents/ChatGPT/PortableAchievementsSystem online-test ingame-test`
  PASS: настоящий rc_client/synthetic spectator, parallel queue, общий cap,
  auth parser/helper mocks, bootstrap и stop helpers. Внешнего аккаунта нет.
- `./scripts/build_client_https.ps1`: pinned SDK --pull=never --network none,
  -Werror, CA/exports/version 0.17/heap/sizes/required/forbidden symbols PASS.
  ELF kernel text/data/BSS=9400/16/28; user=297028/1784/10392,
  итого 309204 байта при лимите 327680. PRX kernel=13766, user=364770.
- PowerShell parser PASS. Все 12 entries ZIP сверены по SHA256 с исходными
  PRX/profile/docs/helpers/licenses, лишних/секретных entries нет.
- build/client-https/PSP_RA_HTTPS_v0.17.zip: 210934 байта, SHA256
  214f3836d99dce07adf0290b7f464000482c55a313ebc444857d3bdc2d7fbe79.
  Kernel PRX SHA256 eb081412450a4b144977c5bcf211cd92ef904da4dd02610be7d747ec59227ce9;
  user PRX 7dccd1e31ad40f13c13ee8e8a42cdce53b60d8ad823d477f34baffc2081f707b.
  Архив 0.16 сохранён: 980cbd7e97071982daf83d2e7ddbb6e0f909a02c7bf0dfe2be7409e77358fb7d.

На PSP 0.17 не проверена. Исправления совместимости доказаны синтетическими
сообщениями; причина старого аппаратного отказа по-прежнему неизвестна.
Нужен один запуск с обоими новыми PRX и прежними auth.txt/profile.txt/+16MiB,
затем только два журнала. При body_limit новый лог даст фактический размер
для следующего решения о бюджете. D: не менялся; к аккаунту агент не обращался,
SDK не устанавливался, реальные достижения не начислялись.

## 0.17 на PSP: доказан предел тела; исправление 0.18

Оба PRX на карте совпали с пакетом 0.17. Auth.txt не открывался.
User log:50–51 — login HTTP 200, verify=0, body=244, logged_in=1,
spectator=1/hardcore=0. User:52–53 — HTTP_body/-35202 и
`HTTP_DIAG ticket=2 error=body_limit phase=chunk_size received=0 declared=0
has_length=0 number_overflow=0 chunked=1 encoding=absent wire=1024
headers=884 trailers=0 chunk_meta=7 chunks=0 last_chunk=129891
line_bytes=5 limit=65536`. Первый объявленный чанк больше старого предела
64 КиБ; полный размер ответа неизвестен. User:56 SPECTATOR_SET result=-32,
loaded=0 — следствие отказа транспорта. Токен и проверка сертификата исправны.

Минимум user free/largest=540928. TLS peak=25044, remaining=0,
io_cleanup=0; FINISHED/STOPPED и wait/delete/exit=0. Heap до deinit=1423616,
после=3520768, возврат 2097152; итоговая free=3863296. Bootstrap result=-35406,
cleanup_result=0. Данных о расходе памяти разбора настоящего набора ещё нет.

По запросу владельца сделана 0.18:

- Общий предел HTTP/HTTPS/очереди поднят с 65536 до 262144 байт.
  Бюджет двух слотов очереди 270338 = POST 8192+NUL+response 262144+NUL.
  Проверка остатка защищена от вычитания при превышенном счётчике.
- Очередь считает peak буферов и отдельно различает отмену/транспорт,
  body cap/shared budget, malloc failure и NULL body. SESSION_QUEUE пишет
  только числа перед close. Это не пик всего heap и не подтверждение JSON.
- Heap 2 МиБ, размеры стеков, TLS cap 64 КиБ, early init/deferred Connect,
  запрет чтения памяти, spectator/no frames/no award не изменены.
  Рабочий response-буфер теперь 256 КиБ, одновременно возможна его копия;
  достаточность heap для настоящих условий проверяется следующим запуском.
- Синтетические наборы 129891/262144 байта проходят через настоящий rc_client.
  Проверены независимость копии, обнуление перед free, закрытие до обработки
  callback, cap+1, timeout и принудительный отказ malloc копии.
- В локальном TLS-стенде исправлено чтение POST перед отправкой ответа:
  прежде один recv мог прочитать только заголовки, а закрытие с непрочитанным
  телом обрывало большой ответ TCP reset. Это изменение только тестового сервера.

Baseline 0.17 воспроизведён с прежним SHA256 до изменений; снимок исходников
в .work/https-0.17-before, просмотренный diff в .work/https-018.diff.
Pins прежние: upstream e0cd8a9b3ccf439532b24b4f1faf2cfc8c0198ab;
rcheevos v12.5.0 1433173220a7eaede6a9ed7a18e94117be1821e0;
Mbed TLS 3.6.7 068ff080b369adfac81509f9b57b2afabaf82dc5;
SDK ghcr.io/pspdev/pspsdk@sha256:511bd8c9eaff77c8a2e205631ae060b9cdf0e9b7e61a7a6ef19482fb4affcdfa,
PSP GCC 15.2.0. Vendor и исходный плагин не менялись.

Проверки 14 октября 2026:

- `wsl -d Ubuntu -- make -s -C /mnt/c/Users/itzsy/Documents/ChatGPT/PortableAchievementsSystem compact-tls-test online-test ingame-test`
  PASS после исправления стенда. Parser: фрагментированный чанк 129891,
  точный предел 256 КиБ и суммарное превышение несколькими чанками.
  25 HTTPS loopback-сценариев: реальные TLS-передачи больших тел, cap+1,
  валидация сертификата, сроки, повреждённые/усечённые ответы, очистка,
  DIAG_SECRET отсутствует в выводе. Owner/callback/parallel queue/close,
  synthetic real rc_client, auth mocks, bootstrap/stop helpers также PASS.
- `wsl -d Ubuntu -- make -s -C /mnt/c/Users/itzsy/Documents/ChatGPT/PortableAchievementsSystem/experiments/tls/compact BUILD=../../../build/https-017-sanitized 'CFLAGS=-O1 -g -std=c99 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -fsanitize=address,undefined -fno-omit-frame-pointer' 'LDFLAGS=-fsanitize=address,undefined' test`
  PASS: тот же parser и все 25 HTTPS-сценариев под ASan/UBSan. Каталог
  sanitizer-библиотек предыдущей версии переиспользован; тесты пересобраны.
- `wsl -d Ubuntu -- make -s -C /mnt/c/Users/itzsy/Documents/ChatGPT/PortableAchievementsSystem/experiments/host BUILD=../../build/https-016-sanitized 'CFLAGS=-O1 -g -std=c99 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer' 'LDFLAGS=-fsanitize=address,undefined' ../../build/https-016-sanitized/test_large_session ../../build/https-016-sanitized/test_client_session ../../build/https-016-sanitized/test_client_session_noframes`
  Затем из корня в WSL выполнены `build/https-016-sanitized/test_large_session`,
  `build/https-016-sanitized/test_client_session` и
  `build/https-016-sanitized/test_client_session_noframes`: PASS, без диагностик.
- `wsl -d Ubuntu -- make -s -C /mnt/c/Users/itzsy/Documents/ChatGPT/PortableAchievementsSystem tls-test`
  PASS: RTC и 16 общих TLS-сценариев после изменения общего стенда.
- `./scripts/build_client_https.ps1`: закреплённый SDK, --pull=never,
  --network none, -Werror, CA/exports/version 0.18/heap/sizes/symbols PASS.
  ELF kernel text/data/BSS=9400/16/28; user=297280/1784/10392,
  итого 309456 при лимите 327680; BSS не вырос. PRX kernel=13766, user=365082.
- PowerShell parser и `scripts/check_public_tree.py .` PASS.
  Все 12 entries ZIP побайтно сверены с актуальными PRX/profile/docs/helpers/
  licenses; лишних entries, auth.txt и логов нет. Новых trailing whitespace нет.

Итоговый build/client-https/PSP_RA_HTTPS_v0.18.zip: 211659 байт, SHA256
f0e53ce31b81a40d533aa9d8b49a78f477f9d2fde0c8b2098e2af85f38084356.
Kernel PRX SHA256 9930d44c67e9473e42e262c5bda4352303b62fa169a26ecdc4f96d40bdf07672;
user PRX 6ad4be52b21d7162bba04a28269d8fea5414a64247a531717175567c10c31327.
Архив 0.17 сохранён с прежним SHA256
214f3836d99dce07adf0290b7f464000482c55a313ebc444857d3bdc2d7fbe79.

На PSP 0.18 ещё не проверена. Следующий шаг владельца: сохранить старые
логи, заменить оба PRX, оставить auth.txt/profile.txt/+16MiB и только
bootstrap в plugins; один запуск того же LocoRoco с Wi-Fi на 2–3 минуты,
выход HOME, передать только client-bootstrap.log и client-user.log.
Нужны успешный SPECTATOR_SET и замеры heap/SESSION_QUEUE, затем чтение
памяти и источник кадров в spectator. D: не менялся, запросов к аккаунту
агент не выполнял, реального зачёта достижений нет.

## 0.18 на PSP: набор PASS; чтение сегментов и наблюдение 0.19

Два запуска 0.18 проверены по обоим PRX (SHA256 совпали с пакетом).
Первый остановился до login на APCTL JOINING -> DISCONNECTED,
event=6/error=0x80410A08; submitted/completed=0. Внешняя причина разрыва
из журнала не устанавливается. Потоки завершились и heap возвращён.
Повторный запуск той же сборки прошёл:

- User:30 — реальный disc0 hash совпал с официальным PC-результатом.
- User:42–49 — APCTL 0 -> 2 -> 6 -> 3 -> 4, ошибок нет.
- User:50–52 — login HTTP 200, verify=0, body=244, logged_in=1,
  spectator=1/hardcore=0. Токен агент не читал.
- User:53–54 — HTTP 200, code=0, verify=0, полный body=222867,
  chunked=1/chunks=3, error=none/phase=done. TLS peak=25044/remaining=0,
  io_cleanup=0 у обоих запросов. Старый предел 64 КиБ был недостаточен;
  256 КиБ вместили полный ответ в этом запуске.
- User:55–57 — game_id=10523/console=41/achievements=101,
  unsupported=101, frames=0/earned=0. Последнее ожидаемо при выключенном
  чтении 0.18. Это загрузка набора, не выполнение достижений.
- User:56 — arena=1345792/allocated=861240/free_in_arena=484552.
  Эти текущие значения после разбора не являются пиком heap.
- User:59 — queue peak=222952/limit=270338, current=pending=0,
  last_copy_error=0, submitted=completed=2.
- User:88 и bootstrap:48–54 — result=0/cleanup_result=0/log_error=0,
  FINISHED/STOPPED, wait/delete/exit=0. До deinit free=1423872,
  после=3521024, возвращено 2097152; после unload free=3863808.
  Минимум по снимкам bootstrap free=541184/largest=540928.

По запросу владельца подготовлена 0.19. Новый адаптер ra_game_memory
использует официальный перевод RA -> PSP из закреплённого ra_psp_memory.
Kernel перед request release запрашивает metadata именно LocoRoco и
публикует проверенную карту: 1–4 непересекающихся сегмента в первичной
user RAM 0x08800000..0x09FFFFFF, text полностью покрыт. После публикации
карта неизменна, применены барьеры памяти. Kernel раз в секунду проверяет
UID/имя/атрибуты/адреса/размеры; изменение или ошибка запускают остановку.
Stop/join сохраняют прежнее владение control до завершения user-потока.

Callback owner читает только полностью разрешённый диапазон одного
сегмента. Kernel RAM, aliases, extra RAM и heap вне сегментов отвергаются
до разыменования; частичный read не становится успешным чтением нулей.
Это ограниченный эксперимент доступности, не полная карта всей памяти
игры. Значения игры не пишутся. GAME_READ логирует счётчики и первые
16 принятых/16 отклонённых RA-адресов с размерами, без значений данных.
Первые 16 успешных чтений до 4 байт повторяются раз в окно наблюдения;
выводится лишь количество изменившихся адресов. Проверены размер/число
диагностических образцов, чтобы они не могли переполнить локальный буфер.

После набора запускается 90 секунд наблюдения: poll 5 мс,
sceDisplayGetVcount/GetFrameBuf(IMMEDIATE), отчёт каждые 5 с и при метке.
L+R+SELECT переключает unmarked -> gameplay -> paused-manual ->
resumed-manual; пауза задаётся владельцем через START. Счётчик ошибок
контроллера выводится в конце. Ошибка/NULL framebuffer и gap >250 мс
сбрасывают базу сравнения; vcount допускает unsigned wrap.

Буферы могут смениться несколько раз между опросами, а логика может
обновляться без смены буфера. Эти наблюдения не доказывают игровой кадр.
Автоматического распознавания паузы/источника логических кадров нет.
RA_SESSION_NO_FRAMES и spectator сохранены; ELF не содержит do_frame,
условия не продвигаются даже в ручной фазе gameplay. В игровом потоке нет
новых хуков или сетевых операций. Сетевой owner обслуживает только idle.
Проверка корректных коротких событий и игровой синхронизации остаётся
следующим этапом после аппаратных данных; таймер не заменяет игровой кадр.

Ресурсы: heap прежний 2 МиБ, owner/HTTP стеки по 32 КиБ, entry 8 КиБ,
ответ 256 КиБ/shared queue 270338, TLS cap 64 КиБ. После загрузки набора
вводится отдельный deadline 90+5 с наблюдения, bootstrap <=180 с.
Bootstrap log ограничен 48 КиБ (для увеличенного времени), user — 16 КиБ.
Новые отказы -35409 (карта) и -35410 (контрольное чтение).

Аудит/сборка: исходная 0.18 воспроизведена до изменений с прежним SHA256.
Выбранные исходные файлы сохранены в .work/https-0.18-before, diff просмотрен.
Pins не менялись: upstream e0cd8a9b3ccf439532b24b4f1faf2cfc8c0198ab;
rcheevos v12.5.0 1433173220a7eaede6a9ed7a18e94117be1821e0;
Mbed TLS 3.6.7 068ff080b369adfac81509f9b57b2afabaf82dc5;
SDK digest 511bd8c9eaff77c8a2e205631ae060b9cdf0e9b7e61a7a6ef19482fb4affcdfa,
GCC 15.2.0. SDK source e2b2b313d4175823a263c2c6cf175311e824eb77
использован как справочник API, не объявляется версией SDK контейнера.
Vendor и исходный офлайн-плагин не изменялись, SDK не устанавливался.

Проверки 14 октября 2026:

- `wsl -d Ubuntu -- make -s -C /mnt/c/Users/itzsy/Documents/ChatGPT/PortableAchievementsSystem ingame-test online-test`
  PASS. Новый test_game_memory проверяет RA translation, точные границы,
  разрывы, пересечения/переполнение, kernel/alias/extra, partial failure,
  ограничение образцов, ошибки/разрывы/wrap счётчиков изображения.
  Реальный bootstrap протестирован при неудачной query, kernel/extra map,
  изменении сегмента, отказе повторной query и штатной публикации/очистке.
- test_game_session с настоящим rc_client и синтетическим условием:
  разрешённый сегмент принят (unsupported=0), отсутствующий адрес даёт
  unsupported=1. Даже истинное условие и вызов tick(verified=1) в точной
  конфигурации NO_FRAMES не дают кадров/событий/запросов award.
- ASan/UBSan: сборка `experiments/ingame` с
  `BUILD=../../build/https-019-sanitized`,
  `CFLAGS=-O1 -g -std=c99 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer`,
  `LDFLAGS=-fsanitize=address,undefined`, targets
  `../../build/https-019-sanitized/test_game_memory` и
  `../../build/https-019-sanitized/test_https_bootstrap`.
  Для `experiments/host` те же CFLAGS/LDFLAGS,
  `BUILD=../../build/https-016-sanitized`, target
  `../../build/https-016-sanitized/test_game_session` (прежняя библиотека
  rcheevos, новый тест/адаптер пересобраны). Все три бинарника выполнены
  из корня WSL, PASS без диагностик; финальный запуск после защиты образцов.
- `./scripts/build_client_https.ps1`: закреплённый SDK без сети,
  -Werror, CA/version 0.19/exports/heap/size/symbol checks PASS.
  ELF kernel text/data/BSS=11568/20/28, user=301160/1784/10784,
  user total=313728 при лимите 327680. PRX kernel=16322, user=369802
  при file guard 393216. do_frame отсутствует; reader/display/ctrl импорты
  проверены. Первый compile выявил signedness nsegment, исправлено явно.
- PowerShell parser, public-tree, UTF-8/локальные ссылки PASS. Все 12 entries
  архива побайтно сверены с актуальными PRX/profile/docs/helpers/licenses.
  Auth.txt, logs и другие дополнительные entries отсутствуют. Старый 0.18
  ZIP сохранён с f0e53ce31b81a40d533aa9d8b49a78f477f9d2fde0c8b2098e2af85f38084356.

build/client-https/PSP_RA_HTTPS_v0.19.zip: 217541 байт, SHA256
8be2cdcaf366a28ee368bb48842c20725cd3f8fc5043fcf786f1e22cf87724a8.
Kernel PRX SHA256 03f96d37a5764d98a814a0ec7409052930eb2f5ad78fa14504d59870730d5fda;
user PRX 0441ad49fbe8693252b45fe10d6f58ecd4d4b219ca893d8868d601356c85e64c.

0.19 на PSP ещё не запускалась. Нужен один запуск по HTTPS_CLIENT.md
с обоими PRX, прежними auth.txt/profile.txt/+16MiB, тремя ручными метками
и только двумя журналами. Нельзя считать новый callback или число
unsupported доказательством корректных ачивок. Дальнейшая привязка кадров
зависит от аппаратных наблюдений. Карту D: агент не менял, auth.txt не
открывал, запросов к аккаунту и реальных начислений агент не выполнял.

## 0.19 на PSP: память PASS; целевой анализ кода и блока 0.20

15 октября 2026 года. Оба PRX запуска 0.19 совпали с пакетом по SHA256.
Bootstrap:30 подтвердил сегмент [0x08804000,0x093FB580), size=12547456.
User:50–57: login и набор HTTP 200/verify=0, body=222867, Game ID 10523,
101 достижение, unsupported=1 вместо 101. User:58 — calls=607,
accepted=606/rejected=1/failed=0. Это успешная проверка адресов при загрузке,
не доказательство исполнения всех условий/косвенных указателей.
User:75 — единственный отказ RA 0x0179E4AC / PSP 0x0979E4AC, на 3813164
байта дальше конца сегмента. Принадлежность этой области игре не доказана.

Наблюдение 90 секунд: 21 окно, 3 ручные метки, 5289 наблюдавшихся смен
буферов, 4 разрыва >250 мс, controller_errors=0, frames=earned=0.
В отмеченном paused-manual окне только 1868 мс, vblank=112/смен=111;
это не подтверждает остановку игровой логики и не даёт кадр для do_frame.
Первый отслеживаемый байт изменился в первом окне; остальные 16 sample
байтов далее были стабильны. Они не являются доказанным счётчиком игры.
User:132 и bootstrap:138–144 — result/cleanup/log_error=0, STOPPED,
heap возвращён ровно 2097152, final free=3868416. Минимум free по снимкам
541440, largest=540928. Данных о FPS/корректности условий пока нет.

Исследование: original EBOOT magic ~PSP (reference.json), не готовый ELF.
Статически назвать адрес игрового цикла по имеющимся материалам нельзя.
В 0.20 добавлен целевой read-only анализ загруженного модуля, чтобы получить
код вокруг настоящих точек вызова, а не повторять неизменённый опрос экрана.

- game_code_audit: проверяемый разбор таблиц импортов внутри game map;
  exact names sceDisplay/sceCtrl/sceGe_user и 7 NID закреплённого PSPSDK.
  Поля читаются как little-endian с явными границами. Максимум text 4 МиБ,
  stub metadata 16 КиБ, 128 библиотек, 4096 функций, 8 нужных импортов.
- Поиск прямых J/JAL к найденным stub. До 24 записей GAME_CALL и 33 слов
  CODE вокруг каждой (не более 3168 байт кода, только text). Это статические
  кандидаты, не исполненные кадры; data в text/обёртки/непрямые вызовы
  требуют дополнительного анализа. total_calls учитывает потерю из-за cap.
- Никаких хуков, записи в игру/импорты/кеши, изменения DDR или do_frame.
  Ошибка сканера отражается в GAME_CODE_AUDIT, не открывает память и не
  подменяет обычный исход сессии. Kernel log cap увеличен до 64 КиБ.
- GAME_BLOCK на метках 10/60 только запрашивает sceKernelQueryMemoryInfo
  для 0x0979E4AC: native rc/partition UID/block UID и равенство mem_id
  LocoRoco/control. Не читает содержимое, имена и kernel pointers.
  rc=0 может вернуть -1 UID; partition UID не равен автоматически номеру 2.
  Даже same_game_block=1 не доказывает смысл адреса для достижения.
  Разрешённые сегменты прежние, адрес по-прежнему read=DENIED.
- scripts/analyze_game_code.py: локальный ограниченный декодер MIPS для
  CODE-записей из явно переданного client-bootstrap.log <=64 КиБ.
  Unknown words остаются .word, остальные строки не печатаются. Нет сети,
  аккаунта или записи файлов; скрипт не является полным дизассемблером.
  В ZIP нет ни фрагментов игры, ни логов, ни auth.txt; CODE появится лишь
  в журнале нового аппаратного запуска. Пакет предназначен для этой пробы.

Источник ABI: первичный uOFW sysmem/memory.c, commit
28306bb5bd4fcdcc0c1d469bf1f3bdb885dde3e9, строки 1096 и 1345.
SHA256 прочитанного файла b7a9a9187132598e7037616e8088fc60c5fa8507dc99a0c033eb4e79932bcb83;
он сохранён только в игнорируемой .work как справочник и не компилируется.
NID 0x2A3E5280 существует в закреплённом PSPSDK SysMemForKernel.S.
Это не утверждение ревизии firmware владельца; native import требует PSP.
Pins сборки прежние: upstream e0cd8a9b3ccf439532b24b4f1faf2cfc8c0198ab,
rcheevos v12.5.0 1433173220a7eaede6a9ed7a18e94117be1821e0,
Mbed TLS 3.6.7 068ff080b369adfac81509f9b57b2afabaf82dc5,
SDK digest 511bd8c9eaff77c8a2e205631ae060b9cdf0e9b7e61a7a6ef19482fb4affcdfa,
GCC 15.2.0. Baseline 0.19 воспроизведён с прежним SHA256; исходные файлы
в .work/https-0.19-before, просмотренный diff .work/https-020.diff.

Проверки 15 октября 2026:

- `wsl -d Ubuntu -- make -s -C /mnt/c/Users/itzsy/Documents/ChatGPT/PortableAchievementsSystem ingame-test`
  PASS: новый parser/J/JAL, повреждённые pointers/counts/len, отказ backend,
  unknown long libraries, call cap и остальные lifecycle/memory/stop tests.
  Настоящий bootstrap прошёл на mmap synthetic module в PSP-диапазоне:
  metadata -> import -> call -> CODE и сравнение block UID.
- ASan/UBSan: `make -s -C experiments/ingame BUILD=../../build/https-020-sanitized 'CFLAGS=-O1 -g -std=c99 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer' 'LDFLAGS=-fsanitize=address,undefined' ../../build/https-020-sanitized/test_game_code_audit ../../build/https-020-sanitized/test_https_bootstrap`
  в Ubuntu WSL из корня проекта. Оба бинарника выполнены, PASS без диагностик.
- `wsl -d Ubuntu -- python3 /mnt/c/Users/itzsy/Documents/ChatGPT/PortableAchievementsSystem/experiments/ingame/test_game_code_decoder.py`
  PASS: MIPS jump region/branch PC/sign extension/load/store и report bounds,
  игнорирование лишних строк, отказ читать auth.txt/oversize.
- `./scripts/build_client_https.ps1`: -Werror, pinned SDK --network none,
  CA/metadata 0.20/exports/heap/sizes/required-forbidden symbols PASS.
  Kernel ELF text/data/BSS=14376/20/28, user=301160/1784/10784,
  user total 313728 <=327680. PRX kernel=19730, user=369802 <=393216.
  do_frame отсутствует, QueryMemoryInfo и сканер присутствуют.
- Все 12 entries ZIP сверены побайтно с текущими PRX/profile/docs/helpers/
  licenses, лишних entries нет. PowerShell parser, public-tree и docs
  UTF-8/локальные ссылки PASS. SDK/vendor/original plugin не менялись.

build/client-https/PSP_RA_HTTPS_v0.20.zip: 220938 байт, SHA256
8bcfe25b406bcbcc716aaaeca9208547326fe0d9c87d8d5b1be745092d8999ab.
Kernel PRX SHA256 7f7d7456b66db482f04120813a5e4f0743a738be0afef4d686fe1f85401af21f;
user PRX a8b344527fe8c5fbffa45afbd739401e130239dd93f9955932eec9bdba59d3bd.
ZIP 0.19 сохранён: 8be2cdcaf366a28ee368bb48842c20725cd3f8fc5043fcf786f1e22cf87724a8.

0.20 на PSP ещё не проверена. Следующий конкретный шаг владельца: оба PRX,
прежние auth.txt/profile.txt/+16MiB, только bootstrap; обычная игра с Wi-Fi
2–3 минуты и только два журнала. Ручные метки повторять не требуется.
Точку обновления/счётчик и принадлежность отказавшего адреса ещё нельзя
считать установленными. D: агент не менял, auth.txt не открывал, обращений
к RetroAchievements и реального начисления достижений агент не выполнял.


## 0.20 на PSP и 0.21: ручная проверка состояния (15 октября 2026)

0.20: оба PRX совпали с пакетом 8bcfe25b406bcbcc716aaaeca9208547326fe0d9c87d8d5b1be745092d8999ab.
Profile=2 (сохранить пользовательский profile.txt). Login/verify=0,
набор 222870 байт / Game10523 / console41 / 101 / unsupported1.
Чтения 607: 606 приняты, 1 отказ, failed0. GAME_CODE_AUDIT result0:
6 импортов, 13 вызовов. ReadBufferPositive 0x0884092C, функция 0x08840920;
GeDrawSync 0x088428D8, функция 0x088428B0. Последняя увеличивает
0x08ADB21C перед веткой/poll GE, но не доказывает игровой кадр.
Оба GAME_BLOCK: query0, partitionUID002C4427, blockUIDFFFFFFFF,
same_game/control=0, DENIED. Это не доказательство свободной памяти.
90 секунд: 5394 наблюдавшихся смен буфера, gaps0, manual0, frames/earned0.
Queue current/pending0 peak222955, submitted/completed2. Cleanup0:
ThreadMan STOPPED, heap2MiB возвращён; финальная user free3868416.
Новая оценка FPS или подтверждение стабильности владельцем не получены.

0.21: добавлены online/include/ra_state_test.h и online/src/ra_state_test.c.
Одна private-копия официального условия ID397986, парсинг rcheevos,
строгий отказ при истории/неподдерживаемой форме. Исходное определение
пока не получено локально: пригодность реального условия неизвестна.
Кнопка L+R+SELECT проверяет два согласующихся прохода чтений и запускает
rc_test_trigger на frozen bytes после reset. Два прохода не атомарный
снимок игры. Успех — только false→true состояния, не событие/открытие.
Нет новых запросов, запись игры/хуки отсутствуют, do_frame исключён.
Буфер <=32КиБ, 16 refs, 128 условий / 16 альтернатив; 32 нажатия,
600 секунд после набора / bootstrap720. При отказе -35411, лимите нажатий
-35412, отсутствии перехода за отведённое время -35413; cleanup отдельно.
Точка игрового кадра/паузы ещё не доказана, автоматический клиент не готов.

Базовая 0.20 воспроизведена ./scripts/build_client_https.ps1 с прежним SHA.
Pins прежние (upstream/rcheevos/Mbed TLS/SDK — раздел 0.20 выше), GCC15.2.0.
Исходный снимок .work/https-0.20-before, просмотренный diff .work/https-021.diff.
Изменения ограничены диагностикой/её сборкой/тестами/документацией;
SDK, vendor, исходный plugin и D: не менялись. auth.txt не открывался,
обращений к аккаунту и реальных начислений агент не выполнял.

Проверки:

- Ubuntu WSL: из корня `make online-test ingame-test` — PASS.
- После добавления граничных случаев: `make -s -C experiments/host ../../build/host/test_target_session ../../build/host/test_game_session`, оба бинарника — PASS.
- ASan/UBSan: новые test_state_test/test_target_session собраны cc
  -O1 -g -std=c99 -Wall -Wextra -Werror -fsanitize=address,undefined
  -fno-omit-frame-pointer с RC_NO_THREADS; session также NO_FRAMES/STATE_TEST,
  библиотека build/https-016-sanitized/librcheevos.a. Оба выполнены — PASS,
  проверены статические integer/bit/24-bit, false-before-true, ошибка/изменение
  чтений, gate history, exact/missing/duplicate ID/game, отмена active transport.
- `make -s -C experiments/ingame BUILD=../../build/https-021-sanitized 'CFLAGS=-O1 -g -std=c99 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer' 'LDFLAGS=-fsanitize=address,undefined' ../../build/https-021-sanitized/test_https_bootstrap`, бинарник выполнен — PASS.
- `./scripts/build_client_https.ps1` — PASS: pinned Docker --network none,
  CA, -Werror, metadata0.21/exports/heap/размеры/символы.
  Kernel ELF14416/20/28, user303624/1784/10784 total316192 <=327680.
  PRX19778/372682 <=393216. do_frame отсутствует, state_test_check присутствует.
- 12 entries ZIP сверены побайтно с актуальными PRX/docs/helpers/profile/licenses;
  посторонних entries, auth, логов и определений нет. UTF-8/локальные ссылки
  обновлённых документов и public-tree checker — PASS.

build/client-https/PSP_RA_HTTPS_v0.21.zip 224325 байт, SHA256
7c49315bc3afa0fec101718be301c281a319b7db963a71f308687939db694fc7.
Kernel PRX82b215e98b1e93099237c4b3d1c6fe60717f098dbe1ca2ab57f5a901f88ce50d;
user3ed4d259ac85416e68afb6d030977c40984521438cc62ddb0f3e260bf1116b37.

На PSP 0.21 не проверена. Действия владельца и расшифровка исходов —
[HTTPS_CLIENT.md](HTTPS_CLIENT.md). Сохранить auth.txt/profile.txt/+16MiB,
заменить только оба PRX. Передать только client-bootstrap.log/client-user.log,
сообщить поведение игры. При eligible=0 не повторять прохождение вслепую.

## 0.21 на PSP: отказ условия; диагностика 0.22 (15 октября 2026)

Прочитаны только client-bootstrap.log/client-user.log с карты владельца.
0.21: HTTPS verify=0, login=0, набор Game10523/console41/101/unsupported1;
TARGET_STATE_READY id=397986 eligible=0 reason=history_dependent refs=5.
Ручные проверки не начались, результат -35411, cleanup=0, heap2MiB возвращён.
frames=0, spectator=1, hardcore=0. Поведение игры владелец пока не описал.
Повторное прохождение для 0.21 не требуется: общий reason скрывал точный отказ.

0.22 добавляет TARGET_STATE_REJECT с первым отказом, группой/условием/операндом,
enum и агрегатными счётчиками. Определение и секреты не журналируются.
Допуск и ручная оценка не изменены. Pins из dependencies.lock.json прежние:
upstream e0cd8a9b3ccf439532b24b4f1faf2cfc8c0198ab;
rcheevos v12.5.0 1433173220a7eaede6a9ed7a18e94117be1821e0;
SDK digest 511bd8c9eaff77c8a2e205631ae060b9cdf0e9b7e61a7a6ef19482fb4affcdfa,
GCC15.2.0; отдельная ревизия установленного PSPSDK не раскрывается.
На PSP 0.22 ещё не проверена. Карта, auth и профиль не изменялись.
Проверки 0.22:
- Ubuntu WSL: `make online-test ingame-test` — PASS. После уточнения
  отдельных причин операндов повторный `make -C experiments/host test` — PASS.
- Финальный сброс диагностических полей при groups_limit:
  `make -s -C experiments/host ../../build/host/test_state_test ../../build/host/test_target_session`
  и запуск обоих бинарников — PASS. Проверены точный контекст delta-операндов
  (в том числе в альтернативе и справа), hits/group flags, condition type,
  сброс после повторного prepare и прежние отказы/false-before-true.
- PowerShell `./scripts/build_client_https.ps1` — PASS, закреплённый SDK,
  Docker без сети, GCC15.2.0, CA/metadata0.22/exports/heap/symbol guards.
  Kernel ELF14416/20/28 total14464; user304356/1784/10784 total316924.
  PRX19778/373682, лимиты соблюдены, do_frame исключён.
- ZIP: 12 ожидаемых entries побайтно сверены с актуальными исходными
  PRX/docs/helpers/profile/licenses, auth/logs отсутствуют. UTF-8 и локальные
  ссылки трёх обновлённых документов — PASS.

build/client-https/PSP_RA_HTTPS_v0.22.zip 225303 байта, SHA256
9f640e833141f26c48b0d77efb9ab0bac4d5eb13fbe393cf3b22cbf426d05f51.
Kernel PRX51f720b3863df4980b374f8061052a696cbe6d95129f67533495db89f4e52da8;
user e3161d491fb8828a77792cfd1534ba657fc2c55371b3bb40189f5644e2a4c905.

Следующий шаг владельца: заменить оба PRX, сохранить auth.txt/profile.txt
(профиль2) и +16MiB. Запустить игру, подождать60 секунд, выйти HOME,
передать только client-bootstrap.log/client-user.log и поведение игры.
Проходить этап и нажимать сочетание для этой диагностики не требуется.
Ожидаемый результат — TARGET_STATE_REJECT, затем -35411/cleanup0.

## 0.22 на PSP и план пробы 0.23 (15 октября 2026)

0.22: TARGET_STATE_REJECT detail=operand_type group=0 condition=6 operand=1
operand_type=1 access=1, без hits/indirect/reset/measured. По закреплённому
rc_runtime_types.h это Delta; для реального движка нужна история кадров.
Результат -35411, cleanup0, heap2MiB возвращён. Логи прочитаны без auth.
Полное определение не получали; это первый отказ, не перечень всех зависимостей.

План 0.23: (1) bounded read-only поиск прямых J/JAL к 0x088428B0 и окна кода;
(2) 12-словная сигнатура перед опросом RA0x00ADB21C; (3) 4 ручные метки,
10-секундные окна в течение максимум240 секунд; (4) host/boundary tests,
PSP build/ELF/ZIP; (5) владелец проверяет игру/паузу/возобновление на PSP.
Опрос не становится вызовом do_frame. Вывод остаётся binding=UNPROVEN.
Сегменты/тайм-ауты/stop/suspend/cleanup прежние, хуков/записи игры нет.
Отклонённое условие сохраняется для диагноза, но ra_state_test_check
больше не вызывается и должно отсутствовать в ELF. При отказе signature
наблюдение не начинается, при коротком чтении counter прекращается.
Сборочная среда: pins dependencies.lock.json прежние — upstream
 e0cd8a9b3ccf439532b24b4f1faf2cfc8c0198ab; rcheevos v12.5.0
1433173220a7eaede6a9ed7a18e94117be1821e0; PSPDEV digest
511bd8c9eaff77c8a2e205631ae060b9cdf0e9b7e61a7a6ef19482fb4affcdfa,
GCC15.2.0, отдельный SHA установленного PSPSDK не раскрывается.
Базовая 0.22 уже собрана/проверена в предыдущей работе, без причины не повторялась.
Снимок затрагиваемых файлов .work/https-022-before. В git файлы исходного
импорта пока untracked; обычный git diff не отражает локальные правки.

Результаты локальной проверки 0.23:
- Ubuntu WSL, `make online-test ingame-test` — PASS. Новые тесты:
  точная/повреждённая/короткая signature, счётчик с wrap/покоем/gap/reset/
  overflow/read-error/rebaseline; direct J/JAL search с пределами сегмента,
  лимитом сохранённых точек и отказом чтения. SDK/vendor/plugin не менялись.
- ASan/UBSan: `make -s -C experiments/ingame BUILD=../../build/https-023-sanitized
  CFLAGS="-O1 -g -std=c99 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer"
  LDFLAGS="-fsanitize=address,undefined" ../../build/https-023-sanitized/test_frame_candidate
  ../../build/https-023-sanitized/test_game_code_audit`; оба бинарника — PASS.
- `./scripts/build_client_https.ps1` — PASS, pinned Docker --network none,
  CA/metadata0.23/exports/heap/symbols/size guards. Kernel ELF15684/20/28
  total15732; user304804/1784/10784 total317372<=327680. PRX21330/374354.
  do_frame, ra_state_test_check, ra_session_check_state отсутствуют.
  rc_test_trigger остаётся внутренней зависимостью официального richpresence;
  само наличие этого символа не означает вызова достижений из пробы.
- Отдельный candidate_reader использует тот же range adapter; для 4-байтового
  выровненного чтения native uint32_t. psp-objdump -d
  --disassemble=read_candidate_segment показал одну lw v0,0(s2) по offset0x618
  после проверки карты/размера/выравнивания. Это не снимок остальной памяти.
- Максимальная строка WINDOW даже с UINT32_MAX во всех полях359 байт<384.
  ZIP: 12 ожидаемых entries побайтно совпадают с актуальными PRX/docs/helpers/
  profile/licenses; auth/logs/определений нет. UTF-8/local links и
  `python3 scripts/check_public_tree.py .` — PASS.
- Снимок .work/https-022-before, локальный review diff .work/https-023.diff.
  Старые технические разделы инструкции сохранены как историческая справка.

build/client-https/PSP_RA_HTTPS_v0.23.zip 226692 байта, SHA256
133518ef0f18590d42c5c6d7e1bbe51e51e87a406f82ed8e9334a1066bee0668.
Kernel PRX7604261a334a9432b7840edb7dff0bf860fa756f340a453c6539ff63b47c964f;
user efbf6bdef64a39601ece95a722c4db6d7e787b02f8a2381de2c5e069707e17ab.
На PSP не проверено, связь с кадрами/паузой пока UNPROVEN.
Следующий шаг владельца — сценарий четырёх меток из HTTPS_CLIENT.md,
затем два лога и поведение игры/паузы. D: и auth не читались/не менялись
в ходе подготовки 0.23; предыдущие журналы0.22 были прочитаны раньше.

## 0.23 на PSP и план 0.24 (15 октября 2026)

0.23: signature1, четыре метки3,919/9,175/49,031/111,176 секунды после набора.
Фаза1 лишь5,256сек; фаза2≈39,856сек; фаза3≈62,145сек. Почти все increments0,
одно+1 и три discontinuities, два gaps; errors0. Это не полноценный тест
паузы. Ключевая новая улика: CODE08842944 AC40B21C обнуляет счётчик.
Прямых calls к088428B0=0. Completed1, result0/cleanup0/log_error0,
heap2MiB возвращён. Описания поведения игры/паузы владельцем пока нет.

План0.24: адресные LUI+ADDIU/ORI кандидаты, точные выровненные указатели
в подтверждённых сегментах и вторичный поиск ссылок на найденные слоты;
импорты GE callback/subinterrupt registration; соседнее окно08842960.
Затем host/boundary/ASan/UBSan, pinned PSP build/ELF/ZIP, запуск владельцем
без ручных меток. Удалён активный poll счётчика, старые исходники пробы
сохранены; адресные совпадения не считаются доказанным data flow/вызовом.
Pins dependencies.lock.json прежние: upstream e0cd8a9b3ccf439532b24b4f1faf2cfc8c0198ab,
rcheevos v12.5.0 1433173220a7eaede6a9ed7a18e94117be1821e0,
PSPDEV digest511bd8c9eaff77c8a2e205631ae060b9cdf0e9b7e61a7a6ef19482fb4affcdfa,
GCC15.2.0, standalone SHA установленного PSPSDK не раскрывается.
NID новых импортов сверены с локальным PSPSDK reference
 e2b2b313d4175823a263c2c6cf175311e824eb77 src/ge/sceGe_user.S и
src/user/InterruptManager.S; это справочник, не версия CFW владельца.
Базовая0.23 уже собрана и проверена; без причины повторно не собиралась.
Снимок до правок .work/https-023-before; git import остаётся untracked.

Проверки0.24:
- Ubuntu WSL: `make online-test ingame-test` — PASS. После добавления
  граничных случаев `make -s ingame-test` — PASS. Проверены signed ADDIU,
  ORI и разные регистры; пары через границу256-байтного блока и предел7
  инструкций; pointer slots/невыровненный старт сегмента; caps/размеры карты,
  неверный target, нулевое и короткое чтение; новые GE/InterruptManager NID.
  Python decoder: ORI/SLT,1712 CODE принимаются,1713 отклоняются.
- `make -s -C experiments/ingame BUILD=../../build/https-024-sanitized
  CFLAGS="-O1 -g -std=c99 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer"
  LDFLAGS="-fsanitize=address,undefined" ../../build/https-024-sanitized/test_game_code_audit`
  и ./build/https-024-sanitized/test_game_code_audit — PASS.
- `./scripts/build_client_https.ps1` — PASS: pinned SDK/Docker без сети,
  GCC15.2.0; CA/metadata0.24/exports/heap/symbols/sizes.
  Kernel ELF17820/20/32 total17872; user301868/1784/10784 total314436.
  PRX23786/370706. do_frame, ra_state_test_check, ra_session_check_state,
  ra_candidate_poll/signature отсутствуют; ra_game_address_refs присутствует.
- ZIP:12 entries побайтно сверены с актуальными PRX/docs/helpers/profile/
  licenses. auth/logs/определений нет. UTF-8/local links и
  `python3 scripts/check_public_tree.py .` — PASS. Diff .work/https-024.diff
  просмотрен по затронутым исходникам/интеграции; SDK/vendor/plugin не менялись.

build/client-https/PSP_RA_HTTPS_v0.24.zip 227874 байта, SHA256
1791d2dd09336e4eccd4b444ca114b4bc7cb85e6099dcd221167b00cf3aa1f7b.
Kernel PRXd8b52a805be1d90508bc373ed9c18f8d59a6b0f4444926fe9495ca887a646221;
user0edf134f75437a012cd09487371d8e40ec77bd82513b95cd8627c38829603e04.
На PSP не проверена, способ регистрации и связь с игровыми кадрами ещё
не установлены. Следующий шаг: обаPRX, сохранение auth/profile2/+16MiB,
один запуск, минута в меню без меток/этапа/паузы, два лога и поведение игры.
D: и auth в ходе разработки0.24 не читались/не менялись. Реальных
обращений к аккаунту/начислений агент не выполнял.

## 0.24 на PSP и план0.25 (15 октября2026)

0.24: pairs2/slots0 для088428B0. Пара08842C6C/08842C84 ложна как
доказательство data flow: между ней перезаписьa1 и вызов. Корректная пара
08842C7C/08842C84 формируетa1=088428B0, a0=15, a2=0; JAL08842C88
ведёт в08804DB8. Обёртка переставляет аргументы и зовёт RegisterSubIntrHandler
(NIDCA04A2B9, stub0895C6FC) сintno30, no15, handler=088428B0, arg0;
при rc>=0 зовёт EnableSubIntr. int30=VBlank по закреплённому pspintrman.h.
Это статическая регистрация, не трасса реального исполнения/успеха syscall.
Imports11/calls19, анализ0. HTTPS/login/набор0, cleanup/log0, heap2MiB возвращён.
Описания поведения игры от владельца пока нет. VBlank не принимается как кадр.

План0.25: (1) сигнатуры двух wrapper управления из прежних CODE;
(2) прямые J/JAL и консервативный prologue candidate; (3) BFS до8 узлов,
12 рёбер, depth3 и1536 CODE words; (4) bounded fallback refs для root без
прямых вызовов; (5) host/integration/ASan/UBSan/PSP/ZIP, один запуск владельцем.
Повторный анализ старого VBlank и poll не выполняются. Начальные точки:
08840920 ReadBufferPositive,0893B8EC PeekBufferPositive; source игрового
кадра/паузы остаётся UNPROVEN. Хуков/записи игры/зачёта нет.
Pins прежние из dependencies.lock.json: upstream e0cd8a9b3ccf439532b24b4f1faf2cfc8c0198ab;
rcheevos v12.5.0 1433173220a7eaede6a9ed7a18e94117be1821e0; PSPDEV digest
511bd8c9eaff77c8a2e205631ae060b9cdf0e9b7e61a7a6ef19482fb4affcdfa,
GCC15.2.0. Standalone revision установленного PSPSDK не раскрывается.
Базовая0.24 уже собрана/проверена; без причины не повторялась.
Снимок .work/https-024-before, git import остаётся untracked.

Результат подготовки 0.25:
- Эвристика начала функции добавлена в game_code_audit.c: окно 512 байт,
  рамка 16..4096, сохранение ra до ветвления/вызова/новой рамки; неизвестные
  границы не подменяются ближайшим адресом. Обход с дедупликацией и бюджетом.
- Fallback расширен с root на максимум два конечных узла без прямых вызовов:
  это даёт шанс найти регистрацию потока, но не доказывает её исполнение.
- `make -s ingame-test` (Ubuntu WSL) — PASS после обновления старого ожидания
  GAME_CALL в bootstrap-тесте: прежние окна VBlank больше не выдаются.
  Реальный bootstrap на синтетических mmap-сегментах проверяет несовпадение
  seed, цепочку из трёх функций, рекурсию без роста и pointer конечного узла.
- Последние проверки отказа перед условной веткой/JALR:
  `make -s -C experiments/ingame ../../build/ingame-host/test_game_code_audit
  ../../build/ingame-host/test_https_bootstrap` и оба бинарника — PASS.
- ASan/UBSan: те же цели с BUILD=../../build/https-025-sanitized,
  CFLAGS="-O1 -g -std=c99 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer",
  LDFLAGS="-fsanitize=address,undefined"; оба бинарника выполнены — PASS.
  Проверены границы окна/ra slot, return barrier, ветка/JALR, короткое чтение,
  интеграция очереди и bounded fallback; decoder сохраняет GAME_FLOW строки.
- `./scripts/build_client_https.ps1` — PASS: pinned Docker без сети, GCC15.2.0,
  CA/metadata0.25/exports/heap/symbols/sizes. Kernel ELF19104/20/32 total19156;
  user301860/1784/10784 total314428; PRX25250/370698.
  do_frame, ручная проверка, poll счётчика исключены из ELF.
- ZIP: 12 entries побайтно совпадают с актуальными PRX/docs/helpers/profile/
  licenses. auth/logs/определений нет. UTF-8/local links и
  `python3 scripts/check_public_tree.py .` — PASS. Просмотрены новый поиск
  границ и интеграция обхода; diff .work/https-025.diff.

build/client-https/PSP_RA_HTTPS_v0.25.zip — 230548 байт, SHA256:
049b9438521106b958a9fdf7a4784ab7b579f9b29a29af105f84e42bbed4564b.
Kernel PRX: 3d972e0b1ee179c9979e7974755f95c1e25afc7c27a19c0870e792f6221a006b;
user: 4232747c6bf3e81a0efdf846f041a2731e1d27f0f6b17835d83a96012eaba67a.

На PSP 0.25 ещё не проверена. Цикл логики/пауза не установлены, кадры и
достижения не продвигаются. Следующий шаг владельца: оба PRX при сохранении
своих auth/profile2/+16MiB, один запуск и минута в меню без действий,
два лога и поведение игры. D: и auth в ходе подготовки 0.25 не читались
и не менялись. SDK/vendor/plugin не менялись, внешних публикаций нет.

## 0.25 на PSP и план0.26 (15 октября2026)

0.25: обе signature1.08840920 вызывается по08842D98 из candidate08842D90.
У08842D90 calls0, refs: pairs0/slots1, slot089B4EE4. Второй root0893B8EC
вызывается по0893B40C; boundary0 из-за раннего JRra0893B35C. В коде
0893B45C..0893B474 — индексирование базы0897C21C и JALR; это не доказанный
кадр. Всего nodes3/edges2/448 CODE, caps не достигнуты. Result/cleanup/log0,
heap2MiB возвращён. Поведения игры владелец не описал; файлов не меняли.

План0.26:24-байтный кандидат дескриптора089B4ED0 с якорем+20=08842D90;
64-байтный кандидат dispatch0897C21C; только классификация, без сырых
unknown/data значений. Поиск LUI+ADDIU/ORI/LW/SW для базы/слота/dispatch,
окна08841480 (предполагаемый потребитель из0.24), его direct callers и
0893B2F0 для контекста второго вызова. До8 code-pointer окон/1536 CODE words.
Известный input callback не дампится повторно, обход0.25 отключён.
Далее host/integration/ASan/UBSan/PSP/ZIP, один запуск владельца без действий.
Pins зависимости прежние: upstream e0cd8a9b3ccf439532b24b4f1faf2cfc8c0198ab,
rcheevos v12.5.0 1433173220a7eaede6a9ed7a18e94117be1821e0;
PSPDEV digest511bd8c9eaff77c8a2e205631ae060b9cdf0e9b7e61a7a6ef19482fb4affcdfa,
GCC15.2.0; standalone SHA установленного SDK не раскрывается.
Базовая0.25 уже собрана/проверена на PSP, без причины повторно не собиралась.
Снимок .work/https-025-before, git import остаётся untracked.

Результат подготовки 0.26:
- Добавлена ra_game_table_probe: до16 фиксированных слов, проверка карты/
  text/выравнивания/якоря, без разыменования указателей. Значения zero,
  game_address и opaque не печатаются. code означает лишь адрес в text.
- Поиск адресных пар расширен LW/SW с signed offset; operation сохраняется
  отдельно. Лексические совпадения не считаются корректным data flow.
- Активный обход0.25 заменён целевой пробой: descriptor base/slot/dispatch,
  код предполагаемого потребителя08841480/два его caller, контекст0893B2F0,
  до8 уникальных code-окон при общем бюджете1536 слов. Известный input callback
  пропускается. Размеры24/64 байта — кандидатные участки, не доказанные ABI.
- Ubuntu WSL `make -s ingame-test` — PASS. Проверены классификация4 видов,
  guard mismatch, короткое чтение, пределы, LW/SW refs. Реальный bootstrap
  на трёх синтетических mmap-сегментах проверил связь со слотом и запрет
  вывода значений12345678/DEADBEEF и адреса данных; неверный якорь прекратил
  probe до печати полей. Decoder сохраняет GAME_TABLE строки.
- ASan/UBSan: `make -s -C experiments/ingame BUILD=../../build/https-026-sanitized
  CFLAGS="-O1 -g -std=c99 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer"
  LDFLAGS="-fsanitize=address,undefined" ../../build/https-026-sanitized/test_game_code_audit
  ../../build/https-026-sanitized/test_https_bootstrap`; оба бинарника — PASS.
- `./scripts/build_client_https.ps1` — PASS: pinned Docker без сети/GCC15.2.0,
  CA/metadata0.26/exports/heap/symbols/sizes. Kernel ELF19792/20/32 total19844;
  user301852/1784/10784 total314420; PRX26210/370690.
  do_frame/ручной тест/опрос счётчика отсутствуют в ELF.
- ZIP:12 entries побайтно сверены с текущими PRX/docs/helpers/profile/licenses;
  auth/logs/определений нет. UTF-8/local links и
  `python3 scripts/check_public_tree.py .` — PASS. Просмотрены классификатор,
  интеграция таблиц и host tests. Снимок .work/https-025-before,
  review diff .work/https-026.diff. SDK/vendor/plugin не менялись.

build/client-https/PSP_RA_HTTPS_v0.26.zip — 232547 байт. SHA256:
ce1e64bfde8487264e17791df396a4365130c2ec3c8b044c7a1e8fa0bd8ef9b2.
Kernel PRX:7aa749571cb341123f750d2049452179a25366b841ec6a95e855e85dd7144a62;
user:64597c924d959fee2da4161019d5320239072142675d4adcda5a073a3b9baa0d.

На PSP0.26 ещё не проверена, таблицы/роль потребителя/игровой цикл/пауза
пока UNPROVEN. Следующий шаг владельца — оба PRX, сохранение своих auth/
profile2/+16MiB, один запуск и минута в меню без меток/этапа/паузы, два лога
и поведение игры. В ходе подготовки D: и auth не читались и не менялись;
обращений к аккаунту и начислений агент не выполнял.
## 0.26 на PSP и подготовка 0.27 (15 октября 2026)

Оба аппаратных журнала0.26 прочитаны с D:/SEPLUGINS/RAClientLite.
Таблица089B4ED0:0/0/08842D20/08841530/08841608/08842D90. Базовый
инициализатор08841480 пишет089B4E58 в object+0x10, настройка08842984
заменяет её на089B4ED0 у объекта на стеке. Найдены косвенные методы+0x0C
по08841518 и+0x10 по088415F4. Операции двух связей+0/+4 и float-поля+0x0C
используют089CECA8/089CECAC/089CECB0; роль списка пока неизвестна.
В1024 CODE-записях703 уникальных слова, прямой цепочки к методу+0x14 нет.
Login0/logged_in1, game10523/console41/101/unsupported1, frames=earned0,
queue pending0, cleanup/log_error0, heap2097152 возвращён. Повтор0.26 не нужен.

В0.27 выполнены полный bounded scan text для LW+0x14/JALR (до8 инструкций),
поиск предшествующего LW+0x10 (ещё до8), ранжирование retained32 в пользу
цепочек, CODE-окна8 кандидатов, ссылки трёх списочных глобальных адресов,
два окна продолжения служебного кода и caller context до4 lexical owners.
Все кандидаты лексические: регистровый data flow, выполнение, кадры и пауза
не доказаны. Границы/ошибки/усечение отражаются в отчёте. Нет чтения самих
списочных указателей/объектов или новых диапазонов памяти. CODE<=3072,
окна<=40 по512байт, bootstrap log<=128КиБ. Повторный обход второй таблицы
убран. Сеть/user lifecycle/heap прежние; user main отличается от0.26 только
диагностической строкой. do_frame/state test/poll выключены.

Зависимости прежние: upstream e0cd8a9b3ccf439532b24b4f1faf2cfc8c0198ab;
rcheevos v12.5.0 1433173220a7eaede6a9ed7a18e94117be1821e0; PSPDEV
sha256:511bd8c9eaff77c8a2e205631ae060b9cdf0e9b7e61a7a6ef19482fb4affcdfa,
GCC15.2.0. Самостоятельная ревизия установленного PSPSDK не раскрывается.
Рабочая копия остаётся импортом без commit, исходники untracked.

Проверки этой работы:
- Ubuntu WSL GCC15.2.0: `make -s ingame-test` из корня — PASS.
  Сканер: граница256-байтных блоков, неверный регистр/другой метод, полный
  проход после насыщения retained, приоритет цепочки, недоступные границы
  и короткое чтение. Integration: настоящий bootstrap на mmap-памяти,
  guard mismatch и исключение непрозрачных значений из лога.
- ASan/UBSan: `make -s -C experiments/ingame BUILD=../../build/https-027-sanitized
  CFLAGS="-O1 -g -std=c99 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer"
  LDFLAGS="-fsanitize=address,undefined" ../../build/https-027-sanitized/test_game_code_audit
  ../../build/https-027-sanitized/test_https_bootstrap`; оба бинарника выполнены — PASS.
- `./scripts/build_client_https.ps1` — PASS, закреплённый Docker без сети,
  GCC15.2.0/-Werror, CA pin/ELF metadata0.27/exports/heap/symbol guards.
  Kernel text/data/bss20420/20/32 (20472), user301852/1784/10784 (314420).
  PRX26834/370690. Эти размеры не являются измерением расхода памяти на PSP.
- Исправлена собственная ошибка скрипта замены диагностической строки:
  user main точно восстановлен по снимку с проверкой полного соответствия
  известному преобразованию повреждённого файла. Итоговый diff user main
  содержит одну строку; повторная сборка успешна.
- ZIP:12 разрешённых entries побайтно сверены с текущими PRX/docs/helpers/
  profile/licenses, секретов/логов нет. `python3 scripts/check_public_tree.py .` — PASS.

На PSP0.27 ещё не проверена. Карта/auth.txt не менялись, auth.txt не читался,
обращений к аккаунту/начислений/публикаций агент не выполнял. Запуск пакета
на PSP сохраняет прежние login/load и требует разрешения владельца на аккаунт.
Дальнейший результат — логи единственного запуска0.27 и поведение игры;
привязку к игровым кадрам по статическому отчёту не объявлять доказанной.

Окончательный пакет build/client-https/PSP_RA_HTTPS_v0.27.zip: 233515 байт,
SHA256 49b18e9bab2ff1320e566cf82baabd377d24ad541c30dd7979088135ca8c57a2.
Kernel PRX SHA256 20f836cad8f83c26eb80d325b9e3c99e2b90d3a48902d1b4c845d6e230d660a2;
user PRX SHA256 2e9b1c2c9300c0f5c509dc776fadbe2eef5c1e0653cb577b8dd8eba4f217a0b4.
Окончательный ZIP после обновления инструкции повторно побайтно сверен.

## 0.27 на PSP и целевая проба 0.28 (15 октября 2026)

Перечитаны только журналы0.27 с D:/SEPLUGINS/RAClientLite, SHA256 двух
установленных PRX совпал с пакетом0.27. Guard0,354277 слов text,54 кандидата,
4 цепочки, retained32,15 окон/1920 CODE. Callers/list refs были усечены,
но путь к нужному обходу установлен:08841788 -> virtual+0x14 по08841800.
Условие: state[+8]==1 и(flags[+0x0A]&7)==0. Связь next+4, head089CECA8,
курсор089CECB0. Перед обходом бит0x01 очищается, после вызова устанавливается;
бит0x04 обрабатывается отложенным удалением.08841880 устанавливает/сбрасывает
бит0x02 для диапазона float-поля+0x0C; это ещё не доказанная игровая пауза.
Единственный прямой caller обхода08842CA0, затем08842CA8 ->08844440,
переход08842CB0 ->08842CA0. Для08844440 в0.27 получена лишь первая инструкция.

В user0.27 EARLY_NETWORK_READY, затем-35203=RA_HTTPS_TIMEOUT в
wait_request_release. Сканирование находится внутри publish_game_map до
выставления request_allowed; общий20-секундный дедлайн user уже отсчитывался.
submitted/completed/pending0, токен не читался, входа/набора в этом запуске
не было. Cleanup/log_error0, heap2097152 возвращён. Повтор0.27 не нужен.

0.28 подготовлена для одного целевого запуска:
- Старый полный поиск54 кандидатов и list refs заменён сигнатурами
 08844440/08842CA0/08842CA8/08841880, окнами трёх корней и ограниченным
 прямым J/JAL-обходом depth2, всего<=12 диапазонов/64 записанных переходов.
 Результаты lexical: фиксированные окна могут включать ранние возвраты,
 соседний код и данные. Не объявляются границами функций/исполнением.
- Полный поиск direct callers для08841880/08844440/08841788, retained32
 на цель/total, до8 окон caller. Точные детали в HTTPS_CLIENT.md.
- Статический режим после gate сразу идёт в cleanup: auth_read0, requests0.
  Пути read_auth/run_session/disc_hash/session_login/session_load исключены
  компилятором и запрещены проверкой ELF. Early network/2MiB heap/stop lifecycle
  сохранены, Wi-Fi Connect/вход/набор в0.28 не запускаются.
- Ожидание анализа получает собственный ограниченный срок180000мс и elapsed
  в логе; acquisition имеет прежние20с. Это инженерный timeout, не измеренный
  предел влияния на игру. Политика отмены/удержания ресурсов сохранена.
- Чтения только в разрешённых сегментах, guard code только внутри text;
  0979E4AC запрещён. До3072 CODE/40окон/128КиБ журнала. Отмена и неполные
  чтения узлов/поиска callers не выдаются за успешное завершение обхода.

Pins не менялись: upstream e0cd8a9b3ccf439532b24b4f1faf2cfc8c0198ab;
rcheevos v12.5.0 1433173220a7eaede6a9ed7a18e94117be1821e0;
PSPDEV sha256:511bd8c9eaff77c8a2e205631ae060b9cdf0e9b7e61a7a6ef19482fb4affcdfa,
GCC15.2.0. SDK установлен внутри образа, самостоятельный SHA не раскрывается.
Git по-прежнему без commit, импорт untracked; снимок .work/https-027-before.
Базовую0.27 без изменений повторно не собирали: PRX сверены с проверенными.

Проверки в Ubuntu WSL/GCC15.2.0:
- `make -s ingame-test` — PASS.
- `make -s -C experiments/ingame BUILD=../../build/https-028-sanitized
 CFLAGS="-O1 -g -std=c99 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer"
 LDFLAGS="-fsanitize=address,undefined" ../../build/https-028-sanitized/test_https_bootstrap
 ../../build/https-028-sanitized/test_early_network`; оба бинарника выполнены — PASS.
- Host integration: синтетические mmap-сегменты, root/child/цикл ссылок,
 несовпадение сигнатуры прекращает CODE, guard вне text отвергается,
 кап12 диапазонов/покрытые адреса/запрет внешних адресов, отмена узла,
 скрытие opaque данных. Gate: независимый срок после истечения прежних20с,
 timeout через180с по mock-clock, отмена и освобождение ресурсов.
- `./scripts/build_client_https.ps1`: PSPDEV без сети, -Werror,
 CA pin/ELF metadata0.28/exports/2MiB heap/лимиты/символы — PASS.
 0.28 пока не проверена на PSP. Карта и auth.txt не менялись, auth.txt
 не читался, обращения к аккаунту/начисления/публикация не выполнялись.

Итог0.28: kernel ELF21020/20/32=21072; user246768/1784/10776=259328.
PRX 27474/299194 байт. Это размеры секций, не измерение PSP heap/FPS.
ZIP12 entries побайтно сверены с текущими PRX/docs/helpers/profile/licenses;
секретов/логов/определений нет. Пакет build/client-https/PSP_RA_HTTPS_v0.28.zip:
205297 байт, SHA256 68e1f9091eaa5ae08a7137ce960ea8722989de53cc8d19d534cbec4dab28f7ee.
Kernel SHA256 6fd420c7cc5eeb3746afa5a63a18c968051496b7230a13db4568b82781db6ea3; user SHA256 1169e98a140f689ece5dffac0c433545e35e60d0ebc8072fce9c81da556bbd8f.
