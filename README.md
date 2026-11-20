# Instrument Workbench Platform (IWP)

Модульное настольное приложение на Qt 6 для работы с приборами и устройствами: ядро (host) и плагины.
Плагины дают каналы связи (UART, TCP, HTTP, SSH...) и сами устройства (протокол и страница интерфейса
конкретной «железки»). Пароли хранятся только в системной связке ключей.

Что есть сейчас:

| Группа | Плагин | Что делает |
|--------|--------|------------|
| Приборы | `nge103b` | Rohde & Schwarz NGE103B по SCPI: каналы, уставки, графики напряжения/тока/мощности |
| Серверы | `redfish` | монитор BMC по Redfish: шасси, температуры, питание |
| Серверы | `linuxhost` | датчики Linux-узла по SSH |
| Инструменты | `console` | сырой поток байт: строки/SCPI-блоки с историей, полноценный терминал (xterm.js), hex |
| Симуляторы | `fakepsu`, `telemetry` | SCPI-источник питания; поток 32-байтных кадров 400 Гц |

Каналы: транспорты `tcp`, `serial`, `sim`; endpoint-ы `http`, `ssh` (команды), `sshshell` (интерактивная оболочка).

Дополнительно: быстрые графики (`PlotView`), профили подключения, переводы (английский и русский),
профили поставки (в бинарник попадают только нужные плагины).

## Требования

| Компонент | Версия | Нужен | Без него |
|-----------|--------|-------|----------|
| CMake | 3.21+ | обязательно | не соберётся |
| Компилятор с C++17 | Apple Clang (Xcode Command Line Tools), MSVC 2022, GCC / Clang | обязательно | |
| Qt | 6.5+ (Core, Gui, Widgets, Svg, Network) | обязательно | |
| Qt Serial Port | как у Qt | нет | плагин `transport/serial` пропускается |
| Qt Linguist Tools | Qt 6.7+ | нет | приложение только на английском |
| Qt Shader Tools и Gui (private) | Qt 6.7+ | нет | нет GPU-рендера графиков, остаётся CPU (он и так по умолчанию) |
| Qt WebEngine и Qt WebChannel | как у Qt | нет | в консоли нет вкладки «Терминал», остаются «Строка» и «Hex» |
| QtKeychain | с поддержкой Qt 6 | нет | пароль нельзя сохранить, он спрашивается каждый раз |
| Клиент OpenSSH | 8.4+ | только при работе | плагины `ssh`, `sshshell`, `linuxhost` не подключатся |

Разработка и проверка идут на **Qt 6.12.0, macOS (Apple Silicon)**. Windows 10/11 и Linux - целевые платформы,
код кроссплатформенный, но сборка на них пока не проверялась.

Qt 6.12: Qt WebEngine больше не входит в релиз Qt, а ставится отдельно в Maintenance Tool в разделе верхнего
уровня **Extensions** (версии 6.140.x). Для Windows нужен MSVC-комплект (не MinGW). Путь установки расширения
может понадобиться добавить в `CMAKE_PREFIX_PATH`.

## Сборка

```
git clone https://github.com/eXLuckiEr/InstrumentWorkbench.git
cd InstrumentWorkbench
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_PREFIX_PATH=/path/to/Qt/6.12.0/macos
cmake --build build
```

`CMAKE_PREFIX_PATH` - каталог Qt вашего комплекта: `~/Qt/6.12.0/macos`, `C:/Qt/6.12.0/msvc2022_64`,
`~/Qt/6.12.0/gcc_64`. Всё (приложение, SDK, плагины, помощник `iwp_askpass`) собирается в `build/bin/<Config>/`.
Запуск: `build/bin/Release/iwp_host` (на macOS это обычный исполняемый файл, не `.app`; приложение-бандл для
раздачи собирается отдельно, см. «Раздача бинарников»). Ninja необязателен: подойдёт любой генератор CMake.

**Qt Creator (рекомендуется):** File → Open File or Project → `CMakeLists.txt`, выбрать комплект Qt, в Run
Configuration выбрать `iwp_host`. Параметры CMake задаются в Projects → Build → Initial CMake Parameters.

### Параметры CMake

| Параметр | По умолчанию | Смысл |
|----------|--------------|-------|
| `IWP_PRODUCT` | `full` | профиль поставки (`products/<имя>.cmake`): какие плагины собирать |
| `IWP_USE_KEYCHAIN` | `ON` | использовать QtKeychain, если найден |
| `QTKEYCHAIN_ROOT` | пусто | куда установлен QtKeychain (ещё ищется `~/Qt/qtkeychain`) |
| `IWP_CONSOLE_TERMINAL` | `ON` | вкладка «Терминал» в консоли (нужен Qt WebEngine; если его нет, пропускается сама) |

В конце настройки CMake печатает, что нашлось: `Console: terminal mode enabled`, `PlotView: GPU renderer ...`,
`QtKeychain found`. Если чего-то не хватает, это видно там же, сборка при этом не прерывается.

### Зависимости по платформам

**macOS:** Xcode Command Line Tools (`xcode-select --install`), Qt из Qt Installer (модули Qt Serial Port и
Extensions → Qt WebEngine отмечаются в Maintenance Tool), CMake (из Qt Installer, Homebrew или cmake.org).

**Windows:** Visual Studio 2022 (Build Tools с компонентом C++), Qt для `MSVC 2022 64-bit` из Qt Installer,
CMake. Клиент OpenSSH: Параметры → Приложения → Дополнительные компоненты → «Клиент OpenSSH» (на Windows 10/11
есть в системе). Сборку удобнее вести из Qt Creator или из «x64 Native Tools Command Prompt».

**Linux:** компилятор и CMake (`build-essential cmake ninja-build`), Qt 6.5+ из Qt Installer или из пакетов
дистрибутива (на Ubuntu примерно так: `qt6-base-dev libqt6svg6-dev libqt6serialport6-dev qt6-tools-dev`), заголовки OpenGL
(`libgl1-dev`), `libsecret-1-dev` для QtKeychain. Пакеты WebEngine называются у разных дистрибутивов по-разному,
или ставится Extension из Qt Installer.

### QtKeychain (пароли в связке ключей)

Пароли никогда не пишутся в профили и файлы. Хост спрашивает их в диалоге и по желанию сохраняет в Keychain
(macOS), Credential Manager (Windows) или libsecret (Linux) через
[QtKeychain](https://github.com/frankosterfeld/qtkeychain). Его нужно один раз собрать под ту же версию Qt:

```
git clone https://github.com/frankosterfeld/qtkeychain.git
cmake -S qtkeychain -B qtkeychain-build -DBUILD_WITH_QT6=ON -DBUILD_TRANSLATIONS=OFF \
      -DBUILD_TEST_APPLICATION=OFF -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_PREFIX_PATH=/path/to/Qt/6.12.0/macos -DCMAKE_INSTALL_PREFIX=$HOME/Qt/qtkeychain
cmake --build qtkeychain-build
cmake --install qtkeychain-build
```

Проект найдёт его в `~/Qt/qtkeychain` или по `-DQTKEYCHAIN_ROOT=<prefix>`. Без него всё работает, пароль
спрашивается каждый раз (при настройке CMake будет предупреждение).

### Профили поставки

Профиль `products/<имя>.cmake` задаёт список плагинов (`IWP_PLUGINS`); остальные не компилируются вообще:

```
cmake -S . -B build -DIWP_PRODUCT=customer_example ...
```

## Запуск

```
iwp_host [--lang ru|en|system] [--settings <файл.ini>]
```

Язык также задаёт переменная `IWP_LANG` или ключ `ui/language` в настройках (по умолчанию английский).
Файл настроек: `--settings`, иначе `iwp.ini` рядом с программой (переносимый режим), иначе каталог конфигурации ОС.
Параметры графиков: `docs/plot.md`. Настройки, SSH, HTTP, пароли: [docs/configuration.md](docs/configuration.md).

## Тесты

Логика без Qt проверяется отдельными программами (запускать из корня репозитория):

```
g++ -std=c++17 -O2 -Isdk/src tools/plotcore_test.cpp -o /tmp/plotcore_test && /tmp/plotcore_test
g++ -std=c++17 -Wall -Iplugins/device/console tools/historycore_test.cpp -o /tmp/historycore_test && /tmp/historycore_test
```

Те же тесты и сборка на Linux, macOS и Windows запускаются в CI (`.github/workflows/ci.yml`), по тегу `v*` готовится
черновик релиза с пакетами: см. [docs/ci.md](docs/ci.md).

## Архитектура

```
Transport (байты: TCP, UART, sim)  -> StreamEndpoint (SCPI-строки / raw)  \
Endpoint-плагины (HTTP, SSH)        ---------------------------------------> SessionBase -> DeviceModel -> Page (UI)
                                                                              (оркестратор)   (состояние)
```

Зависимости строго вниз: `плагин → sdk`, `host → sdk`; плагины не знают друг о друге. Устройство = сессия
(рабочий поток) + страница (UI) + список ролей endpoint-ов; хост по ролям строит форму подключения.

```
sdk/        интерфейсы и базовые классы (iwp::sdk): единственная зависимость плагинов
host/       приложение: загрузка плагинов, поток на устройство, окно, профили подключения
plugins/    transport/, endpoint/, device/
products/   профили поставки
cmake/      iwp_add_plugin() и другие функции
docs/       документация
tools/      тесты логики, генератор значка, пример CMakePresets
```

Подробно: [docs/architecture.md](docs/architecture.md).

## Документация

- [Архитектура](docs/architecture.md): слои, потоки, правила
- [Новый плагин устройства](docs/new-device-plugin.md), [транспорт](docs/new-transport-plugin.md), [endpoint](docs/new-endpoint-plugin.md)
- [Консоль](docs/console.md): строка/SCPI-блоки и история, терминал, hex, SSH-терминал
- [Графики](docs/plot.md)
- [Настройки, пароли, HTTP и SSH](docs/configuration.md)
- [Разработка](docs/development.md): переводы, ресурсы, значок приложения
- [CI и релизы](docs/ci.md): GitHub Actions, упаковка, подпись

## Лицензия

[Mozilla Public License 2.0](LICENSE) (MPL-2.0): код можно свободно использовать, менять и включать в свои
продукты, в том числе закрытые, без гарантий. Условие одно, на уровне файлов: если вы меняете файлы этого
репозитория и распространяете результат (исходники или бинарники), изменённые файлы остаются под MPL-2.0 и их
исходный код нужно предоставить получателям. Свои собственные файлы и плагины рядом с ним можно выпускать под
любой лицензией, в том числе закрытой.

Сторонние компоненты (xterm.js, Qt и другие) остаются под своими лицензиями, см. [THIRD_PARTY/](THIRD_PARTY/README.md).
Это краткое пояснение, не юридическая консультация: действует текст лицензии.
