# Маленький user PRX в partition 2: проба 0.8

Проба прошла на PSP, владелец подтвердил хорошую работу игры. Текущий этап —
[запуск настоящего rc_client, 0.10](MINIMAL_CLIENT.md). Ниже сохранена инструкция 0.8.

## Задача и исходные результаты

PSP владельца: PSP-3000, 6.60 + ARK5 по его сообщению, LocoRoco USA
UCUS-98662. Точная ревизия установленной ARK пока не зафиксирована.
В 0.6 загрузка user PRX в extra partition 9 отвергнута с
`0x80020132 PARTITION_MISMATCH`. В 0.7 на PSP подтверждён ранний отказ:
`PLACEMENT_UNSUPPORTED ... attr=0x0C reason=USER_PARTITION_MISMATCH
load=NOT_ATTEMPTED`, успешные check/free 2 МиБ и `log_error=0`.
Это ограничение размещения user-модуля, а не ошибка владельца при установке.

0.8 проверяет следующий узкий вариант: маленький user PRX загружается **только
в partition 2** (`mpidtext=2`, `mpiddata=2`), а kernel scratch 2 МиБ остаётся
в partition 9. Это явный новый эксперимент, не автоматический fallback.
На PSP 0.8 проверена: 121 замер, загрузка/выгрузка и check/free успешны,
result=0 log_error=0; разница user free 2560 байт восстановилась при unload.
Минимум free 567040, largest 566784 байта. Старые архивы и журналы сохраняются.

## Что выполняется

После обнаружения LocoRoco kernel observer измеряет свободную память раз в
секунду до 120 интервалов. На метке 10 резервирует и проверяет прежний блок
2 МиБ в partition 9, затем пытается загрузить `RAPlacementUser.prx` в раздел 2.
User PRX без CRT, newlib, heap, импортов, callbacks или внутренних потоков:
есть по 1 КиБ data/BSS. **StartModule/StopModule не вызываются**, entrypoints
не исполняются, user-стек не выделяется этой пробой. Поток observer имеет
прежние kernel stack 8 КиБ и priority 0x60.

Перед загрузкой проверяются user-атрибуты раздела 2, его начало `0x08800000`
и размер не более 24 МиБ. Сегменты PRX должны целиком лежать в этом разделе;
kernel aliases, extra RAM, пустой text и некорректные data/BSS отклоняются.
Сумма text/data/BSS и отдельно сумма размеров непустых сегментов ограничены
32 КиБ. Проверяются метаданные загрузчика, PRX напрямую не разыменовывается.

Временные guards эксперимента: до LoadModule свободно хотя бы 512 + 32 КиБ,
крупнейший блок хотя бы 32 КиБ. После загрузки и при каждом очередном замере
должно оставаться хотя бы 512 КиБ free и крупнейший блок хотя бы 32 КиБ.
Если после LoadModule free уменьшился более чем на 32 КиБ, наблюдение также
завершается с `USER_MEMORY_GUARD`, модуль выгружается и scratch освобождается.
32 КиБ — предел проверяемых метаданных и порог разницы замеров, а не жёсткий
лимит внутренних временных выделений загрузчика. Эти guards выбраны для
маленькой пробы, достаточный бюджет игры ими не доказан. Между замерами игра
может выделить память; секундный опрос не гарантирует предотвращения OOM.

`placement_memory` записывается до и после LoadModule, до и после UnloadModule:
free и largest обоих разделов 2/9. В `placement_memory_delta` положительное
`user_consumed` означает уменьшение свободного места, отрицательное — рост.
Замеры не атомарны; игра может параллельно выделять/освобождать RAM.
`attribution=UNPROVEN` запрещает считать всю разницу расходом PRX или любую
невосстановленную разницу утечкой. Ошибка замера или журнала не отменяет cleanup.

На обычном завершении и любом обработанном отказе observer выгружает
незапущенный PRX, проверяет и освобождает свой scratch. При отказе Unload/Free
UID сохраняется, module_stop повторяет очистку после join и отказывается
выгрузить bootstrap до успеха. За живым observer ресурсы не освобождаются.
Строка `cleanup` содержит первоначальный результат и отдельные результаты
выгрузки/освобождения; итог содержит hex-код, имя, этап и outcome.

Сеть, достижения, power callbacks, чтение disc0/памяти игры, изменения защиты
DDR/MMIO и пользовательских настроек отсутствуют. Успешная загрузка в partition 2
не доказывает user-доступ к буферам partition 9, user-исполнение или работу TLS.

## Один запуск на PSP

1. Сохрани старые логи. Из `PSP_RA_UserPlacement_v0.8.zip` скопируй **оба PRX**
   в `SEPLUGINS/RAInGameLite` на Memory Stick с заменой.
2. `RAInGameProbe` оставь выключенным. Включи только запись этой пробы:

   ```text
   UCUS98662, ms0:/seplugins/RAInGameLite/RAInGameLite.prx, on
   ```

   `RAPlacementUser` отдельно в плагины не добавляй.
3. Полностью перезапусти PSP. Запусти тот же ISO, пропусти ролик и поиграй
   2–3 минуты на первом уровне. Wi-Fi и комбинации кнопок не нужны;
   sleep/resume и сеть игры в этом запуске не проверяются.
4. Выйди HOME, подключи USB и передай
   `SEPLUGINS/RAInGameLite/placement-user2.log`. Сообщи, нормально ли работала
   игра. Затем отключи запись пробы.

Ожидаемый положительный результат:

- `probe_version=0.8-placement-user2` и `placement_begin text_partition=2 data_partition=2 start=NEVER`;
- замеры `before_load`/`after_load`, метаданные и `PLACEMENT_USER2_LOAD_OK ... execution=UNTESTED stack=UNTESTED`;
- дальнейшие sample/owned_block_check до конца наблюдения;
- неотрицательный результат `unload unstarted placement user PRX`, замеры `before_unload`/`after_unload` (на PSP возвращён UID 0x042BA355);
- успешные финальная проверка scratch/free и `lite_finished ... result=0 outcome=COMPLETE log_error=0x00000000`.

`USER_MEMORY_GUARD` даёт `result=3 outcome=MEMORY_GUARD`; `PLACEMENT_UNSUPPORTED`
даёт `result=2 outcome=UNSUPPORTED`. Это полезные отрицательные результаты,
не подтверждение размещения. Отказ LoadModule/metadata тоже нужно сохранить.
При выключении/зависании отключи пробу, сохрани имеющийся журнал и не повторяй
сбойный запуск. Старые placement/placement-preflight/reserve.log не перезаписываются;
повторный запуск 0.8 перезапишет только placement-user2.log.

## Сборка и проверка

`scripts/build_ingame_lite.ps1`, закреплённый PSPSDK из `dependencies.lock.json`,
GCC 15.2.0, Docker/WSL без сети. Сборщик исключает StartModule/StopModule,
network/DDR/runtime и проверяет отсутствие импортов у user PRX. В ZIP ровно
два PRX, эта инструкция, INSTALL_ENTRY.txt и BSD-лицензия PSPSDK.

`make ingame-test` проверяет диапазоны, aliases, overflow, размеры и guards
памяти. Host-тест реального load/unload пути bootstrap с подменёнными PSP API
проверяет выбор раздела 2, замеры, ранний отказ по памяти, ошибки загрузки,
metadata/журнала, выгрузку и повтор после её отказа. Он не эмулирует PSP loader
или DDR. Результаты сборки/проверок фиксируются в `INGAME_PROBE_STATUS.md`.
