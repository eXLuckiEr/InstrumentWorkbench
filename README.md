# Instrument Workbench Platform (IWP)

Модульная платформа для приборных приложений на Qt 6: ядро (host) + плагины
транспортов (TCP, UART, CAN, VISA...) и плагины устройств (протокол + UI конкретной железки).

## Архитектура

```
Transport (байты: TCP, UART, sim)  -> StreamEndpoint (SCPI-строки / raw)  \
Endpoint-плагины (VISA, HTTP, SSH)  ---------------------------------------> SessionBase -> DeviceModel -> Page (UI)
                                                                              (оркестратор)   (состояние)
```

- **Transport** (`ITransport`) - канал байтов. Плагины: `tcp`, `sim` (симулятор SCPI-прибора).
- **Framer** (`IFramer`) - режет поток байт на кадры и оборачивает исходящие данные. Готовые: `LineFramer`
  (строки с терминатором, SCPI), `RawFramer`, `FixedFrameFramer` (бинарный поток фиксированных кадров:
  заголовок, длина, CRC16-CCITT-FALSE, ресинхронизация после потери байт). Выбирается в конфиге роли
  (`"framing": {...}`) или своим классом через `IDeviceFactory::createFramer()`.
- **Endpoint** (`IEndpoint`) - место, куда отправляют запрос и получают ответ. Базовый класс сам держит
  очередь: приоритеты, таймауты, ограничение параллелизма, отказ всех запросов при потере связи.
  `StreamEndpoint` делает endpoint из любого транспорта (kind `scpi` - строки с терминатором, `stream` - сырые байты).
  Не-байтовые каналы (NI-VISA, HTTP/Redfish, SSH) - отдельные плагины `IEndpointFactory`.
- **Session** (`SessionBase`) - оркестратор устройства: жизненный цикл (ждёт все endpoint-ы, `onStart` -
  идентификация/настройка, потом опрос), `PollScheduler` (циклические задачи, без накопления запросов),
  помощники `query()/write()/poll()`. Живёт в рабочем потоке вместе со своими endpoint-ами.
- **DeviceModel** - единственное, с чем разговаривает UI: значения по ключам с пометкой stale,
  события (`postEvent`), статус, команды UI -> сессия (`sendCommand`). Живёт в UI-потоке,
  методы сессии потокобезопасны.
- **Device-плагин** = сессия + страница + список ролей endpoint-ов (`{"scpi","scpi"}`, `{"api","http"}`...).
  Хост строит по ролям форму подключения (профиль), профиль запоминается.

## Структура

```
sdk/        интерфейсы и базовые классы (iwp::sdk) - единственная зависимость плагинов
host/       приложение: плагины, DeviceRuntime (поток на устройство), окно с профилем подключения
plugins/
  transport/tcp, transport/serial, transport/sim   (serial: QSerialPort; sim: SCPI-прибор и поток телеметрии 400 Гц)
  device/console
  device/fakepsu                   (SCPI: опрос, ручка, индикаторы)
  device/telemetry                 (push-поток 32-байтных кадров: фреймер, статистика канала, watchdog)
  endpoint/http                    (QNetworkAccessManager: Redfish-сессии, Basic, Bearer, pinning сертификата)
  device/redfish                   (монитор BMC: шасси, Thermal, Power)
  endpoint/ssh                     (системный OpenSSH: один `ssh host sh`, askpass-помощник iwp_askpass)
  device/linuxhost                 (датчики Linux-узла по SSH одной командой за цикл)
products/   профили поставки: список плагинов, которые собираются и отдаются клиенту
cmake/      iwp_add_plugin() и прочие функции
```

Зависимости строго вниз: `plugin -> sdk`, `host -> sdk`. Плагины не знают друг о друге.

## Сборка

Нужны Qt >= 6.5 (с модулем Qt Serial Port для `transport/serial`; без него плагин пропускается) и CMake >= 3.21.

**Qt Creator (рекомендуется):** File -> Open File or Project -> `CMakeLists.txt`, выбрать Qt-кит,
запускать таргет `iwp_host`. Профиль поставки: Projects -> Build -> Initial CMake Parameters,
например `-DIWP_PRODUCT=customer_example`.

**Командная строка:**

```
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/Qt/6.x.x/<kit> -DCMAKE_BUILD_TYPE=Debug
cmake --build build
./build/bin/Debug/iwp_host
```

Пример пресетов для CLI лежит в `tools/CMakePresets.example.json` (скопировать в корень как
`CMakePresets.json`; Qt Creator с ними конфликтует, поэтому по умолчанию они отключены).

## QtKeychain (пароли в системной связке ключей)

Пароли никогда не пишутся в профили и файлы. Хост спрашивает их в диалоге и по желанию сохраняет в
Keychain (macOS) / Credential Manager (Windows) / libsecret (Linux) через QtKeychain.
Без QtKeychain всё работает, но пароль спрашивается каждый раз.

Один раз собрать QtKeychain под ту же версию Qt и установить, например в `~/Qt/qtkeychain`:

```
git clone https://github.com/frankosterfeld/qtkeychain.git
cmake -S qtkeychain -B qtkeychain-build -DBUILD_WITH_QT6=ON -DBUILD_TRANSLATIONS=OFF \
      -DBUILD_TEST_APPLICATION=OFF -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_PREFIX_PATH=$HOME/Qt/6.12.0/macos -DCMAKE_INSTALL_PREFIX=$HOME/Qt/qtkeychain
cmake --build qtkeychain-build
cmake --install qtkeychain-build
```

Проект ищет его в `~/Qt/qtkeychain` или в `-DQTKEYCHAIN_ROOT=<prefix>`. Отключить: `-DIWP_USE_KEYCHAIN=OFF`.
(Windows: тот же набор команд, Linux: нужен пакет libsecret-1-dev.)

## HTTP / Redfish

Роль типа `http` настраивается JSON-ом (`baseUrl`, `auth`, `username`, `trustedFingerprint`, ...,
см. комментарий в `plugins/endpoint/http/httpendpoint.h`). Для самоподписанного сертификата BMC:
подключиться, в ошибке будет SHA-256 отпечаток, вписать его в `trustedFingerprint`.
HTTP без TLS поддерживается, но тогда логин и пароль идут по сети открытым текстом.

## SSH

Плагин `endpoint/ssh` запускает системный `ssh host sh` (нужен OpenSSH-клиент в PATH; на Windows 10/11 он
входит в систему) и гоняет через него команды. Вся авторизация остаётся за OpenSSH: ключи, ssh-agent,
FIDO2/YubiKey (`*-sk` ключи), сертификаты, `ProxyJump`, `~/.ssh/config`. Всё, что он спрашивает
(пароль, passphrase, PIN, подтверждение ключа хоста, «коснитесь ключа»), перехватывается через
`SSH_ASKPASS` (помощник `iwp_askpass` рядом с приложением) и показывается диалогами хоста.
Пароли и passphrase можно сохранить в связке ключей, PIN и одноразовые коды не сохраняются никогда.
Нужен OpenSSH >= 8.4 (`SSH_ASKPASS_REQUIRE`). Каждая команда выполняется в отдельной subshell
(`cd` и переменные между командами не сохраняются).

## Локализация

Исходный язык всех строк - английский: в коде везде `tr("...")`, по умолчанию приложение английское.
Каждый модуль (host, sdk, каждый плагин) держит свои переводы в `<модуль>/i18n/<цель>_<язык>.ts`
и вшивает их в себя (`:/i18n/<цель>_<язык>.qm`), поэтому клиент получает переводы только тех плагинов, которые ему отданы.
Нужен Qt Linguist Tools (компонент Qt, CMake-пакет `Qt6LinguistTools`); без него сборка идёт без переводов.

Язык: `--lang ru`, переменная окружения `IWP_LANG=ru` или ключ `ui/language` в QSettings
(`en` по умолчанию, `system` - язык ОС). Применяется при запуске.

Правила: не склеивать строки (`tr("Port %1").arg(n)`), для статических таблиц `QT_TR_NOOP`,
для множественного числа `tr("%n channel(s)", "", n)`, двусмысленные слова размечать `//: комментарий`.
Протоколы, ключи модели и имена ролей не переводятся.

Рабочий цикл: после правки строк собрать цель `update_translations` (lupdate пополняет все `.ts`),
перевести в Qt Linguist, обычная сборка сама делает `.qm`. Новый язык: положить в `i18n/` модулей
`<цель>_<язык>.ts` (скопировать русский и сменить `language=`) и заново запустить CMake.

## Настройки

Всё хранится в одном INI-файле через `AppSettings` (`host/src/appsettings.*`), ключи нигде больше не пишутся.
Расположение: `--settings <файл>`, иначе `iwp.ini` рядом с exe (переносимый режим), иначе
`AppConfigLocation` (Windows `%APPDATA%\IWP\...`, Linux `~/.config/IWP/...`, macOS `~/Library/Preferences/...`);
полный путь виден в подсказке строки состояния. Сохраняются геометрия окна, размер разделителя, последнее устройство,
язык и профили подключений (выбранный способ и настройки каждого способа отдельно, JSON). Пароли только в связке ключей.
Файл версионируется (`meta/version`): при несовместимом изменении ключей поднять `AppSettings::kVersion`
и добавить шаг в `migrate()`. Записи старой сборки (системное хранилище) подхватываются при первом запуске.

## Ресурсы (иконки, картинки)

Файлы из `<модуль>/resources/` (svg, png, jpg, шрифты) вшиваются в сам модуль и доступны как
`:/iwp/<модуль>/<файл>` (`<модуль>` - цель CMake без `iwp_`): `host/resources/connect.svg` -> `:/iwp/host/connect.svg`,
`plugins/device/console/resources/icon.svg` -> `:/iwp/console/icon.svg`. Для плагинов это делает `iwp_add_plugin()`
само, клиент получает только ресурсы своих плагинов.

- Иконки - SVG. Монохромные глифы рисовать чёрным: `iwp::themedIcon(":/iwp/host/connect.svg")` перекрасит
  в цвет текста палитры (светлая/тёмная тема). Цветные картинки: `iwp::svgIcon(...)`.
- Иконка устройства в списке: `IDeviceFactory::icon()` (API версии 3).
- Значок приложения: окно и панель задач - `main.cpp`; файл exe - `host/platform/app.rc` + `app.ico` (Windows),
  `app.icns` (macOS), `app.png` (Linux). Эти файлы генерирует `tools/make_app_icon.py` (нужен Pillow),
  рисунок должен совпадать с `host/resources/app.svg`.
- Чужие материалы только с совместимой лицензией, тексты лицензий в `THIRD_PARTY/`.
- Очень большие данные лучше не вшивать, а собирать в отдельный `.rcc` (`qt_add_binary_resources`).

## Поставка клиенту

Профиль в `products/<name>.cmake` задаёт `IWP_PLUGINS`. Лишние плагины не компилируются вообще:

```
cmake -S . -B build -DIWP_PRODUCT=customer_example ...
```

## Новый плагин

1. Папка `plugins/<kind>/<name>/` c `CMakeLists.txt`, `<name>.json` и классом-фабрикой.
2. В `CMakeLists.txt`: `iwp_add_plugin(iwp_<name> SOURCES ... LIBS ...)`.
3. В JSON обязательны `id`, `name`, `kind`, `version`, `apiVersion` (= `IWP_API_VERSION`).
4. Добавить `<kind>/<name>` в нужные `products/*.cmake`.

Подробные шпаргалки: [docs/](docs/README.md) (устройство, транспорт, endpoint).
Форма настроек подключения у транспорта/endpoint необязательна (`createConfigEditor()`, см. шпаргалки);
без неё показывается строка JSON. Кнопка `{ }` рядом с формой открывает тот же конфиг как JSON.

Устройство: наследоваться от `iwp::SessionBase`, в конструкторе описать `poll(...)`,
в `onStart()` идентифицировать прибор и вызвать `setRunning()`, в `onCommand()` обрабатывать команды UI.
Результаты - только через `model()->update(key, value, role)`. Страница работает только с `DeviceModel`.

## Правила

- Сессия, endpoint-ы и транспорты живут в одном рабочем потоке. Не создавать QObject-детей
  (сокеты, таймеры) в конструкторе транспорта - только в `doOpen()`.
- Виджеты трогает только страница; сессия никогда.
- Быстрые потоки (сотни кадров в секунду): сессия копит последние значения и публикует снимок
  ~30 раз в секунду через `DeviceModel::updateMany()`, а не по событию на каждый кадр.
- Устройства, которые только шлют данные: `watchdog()` + `kick()` вместо проверки ошибок запросов.
- Менять интерфейсы SDK несовместимо = увеличить `IWP_API_VERSION`.
- Плагины и хост собираются одним компилятором и одной версией Qt.
- Пароли не хранятся в профилях и в плагинах: только через `ICredentialProvider` хоста (QtKeychain).

## Дальше

- `ScpiSessionBase`: `*IDN?`, очередь ошибок `SYST:ERR?`, `*OPC?`, типизированные query.
- `transport/serial` (QSerialPort), `transport/udp`.
- `endpoint/visa_*` (gpib, usb, serial, tcpip) на общей библиотеке `visa_core`, NI-VISA подгружается в рантайме.
- `endpoint/ssh`: потоковые команды (`tail -f`, цикл на устройстве) как push-данные, автопереподключение,
  мультиплексирование `ControlMaster` на mac/Linux.
- Страницы устройств на QML, скрипт `tools/new_plugin.py`.
