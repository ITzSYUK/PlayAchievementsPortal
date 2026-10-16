# PPSSPP: кадровая интеграция RA и ограничения переноса

Аудит16 октября2026. Источники просмотрены в upstream master как справочный
материал, не добавлены в сборку и не являются новой плавающей зависимостью.
Ни исходники, ни GPL-код PPSSPP в плагин не копировались.

Путь: `sceDisplaySetFramebuf(IMMEDIATE)` либо `hleEnterVblank()` с применением
latched framebuffer/fallback → `__DisplayFlip` → проверка needFlip →
`DisplayFireFlip` → зарегистрированный `__DisplayListenFlip` callback →
`EmuScreen::HandleFlip` → `Achievements::FrameUpdate` → `rc_client_do_frame`.

`needFlip` включает dirty framebuffer, fallback после10VBlank и влияние
настроек вывода. Поэтому это внутреннее составное событие PPSSPP, а не любой
VBlank, таймер или syscall SetFramebuf. В upstream есть предупреждение, что
некоторые игры не вызывают SetFramebuf во время gameplay. `DisplayFireFlip`
и `DisplayFireActualFlip` — разные события; последнее отдельно считает
фактические flips. Переносить семантику одного на другое нельзя.

Адаптер RA в PPSSPP прибавляет08000000 и проверяет Memory::IsValidRange;
у нас тот же displacement, но доступ ограничен явно разрешённым диапазоном.
Прямое копирование эмуляторного доступа ко всей RAM непригодно на железе.

START-пауза сохраняет выполнение PSP и обработку меню/ввода. Остановка
эмулятора — другое состояние, для неё есть idle. Это уточнение отменяет
предположение, будто наш метод ввода обязан остановиться на START-паузе.
Требование проекта о непродвижении условий при остановке сохраняется;
автоматически блокировать do_frame по факту меню START пока не обосновано.

Данные0.29:7161 вход за120с; метод продолжал вызываться на подтверждённой
START-паузе. Это полезная точка для корреляции, не доказанный кадровый адаптер.
0.30 сравнивает VCount/метаданные framebuffer непосредственно до/после метода,
считает пропуски/повторы и длительность. Она не перехватывает SetFramebuf,
не обнаруживает dirty framebuffer и не воссоздаёт needFlip.

Следующее решение принимается по логу0.30: если есть стабильная связь с
display, оценить пригодность точки и порядка чтения; если связь отсутствует,
исследовать подачу/GE. Прямое включение do_frame не разрешается результатом
одного счётчика. Очередь отметок времени не позволяет восстановить прошлую
память: нельзя позднее «догнать» пропущенные кадры чтением текущего состояния.

Источники:

- [sceDisplay.cpp](https://github.com/hrydgard/ppsspp/blob/master/Core/HLE/sceDisplay.cpp)
- [Display.cpp](https://github.com/hrydgard/ppsspp/blob/master/Core/HW/Display.cpp)
- [EmuScreen.cpp](https://github.com/hrydgard/ppsspp/blob/master/UI/EmuScreen.cpp)
- [RetroAchievements.cpp](https://github.com/hrydgard/ppsspp/blob/master/Core/RetroAchievements.cpp)

Сборочные зависимости проекта прежние: upstream e0cd8a9b3ccf439532b24b4f1faf2cfc8c0198ab,
rcheevos12.5.0/1433173220a7eaede6a9ed7a18e94117be1821e0,
PSPDEV sha256:511bd8c9eaff77c8a2e205631ae060b9cdf0e9b7e61a7a6ef19482fb4affcdfa,
PSP GCC15.2.0. Сетевые/аккаунтные действия и копирование на карту не выполнялись.
