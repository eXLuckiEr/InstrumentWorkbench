# Архитектура

```
Transport (байты: TCP, UART, sim)  -> StreamEndpoint (SCPI-строки / raw)  \
Endpoint-плагины (HTTP, SSH)        ---------------------------------------> SessionBase -> DeviceModel -> Page (UI)
                                                                              (оркестратор)   (состояние)
```

## Слои

- **Transport** (`ITransport`) - канал байтов. Плагины: `tcp`, `serial` (QSerialPort), `sim` (симулятор
  SCPI-прибора и потока телеметрии 400 Гц).
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

## Структура репозитория

```
sdk/        интерфейсы и базовые классы (iwp::sdk) - единственная зависимость плагинов
host/       приложение: плагины, DeviceRuntime (поток на устройство), окно с профилем подключения
plugins/
  transport/tcp, transport/serial, transport/sim
  device/console                   (сырой поток: строки/SCPI, терминал, hex)
  device/fakepsu                   (SCPI: опрос, ручка, индикаторы)
  device/telemetry                 (push-поток 32-байтных кадров: фреймер, статистика канала, watchdog)
  device/nge103b                   (SCPI-блок питания с графиками)
  endpoint/http                    (QNetworkAccessManager: Redfish-сессии, Basic, Bearer, pinning сертификата)
  device/redfish                   (монитор BMC: шасси, Thermal, Power)
  endpoint/ssh                     (системный OpenSSH: один `ssh host sh`, askpass-помощник iwp_askpass)
  endpoint/sshshell                (интерактивная оболочка `ssh -tt host` как поток байт для консоли)
  device/linuxhost                 (датчики Linux-узла по SSH одной командой за цикл)
products/   профили поставки: список плагинов, которые собираются и отдаются клиенту
cmake/      iwp_add_plugin() и прочие функции
```

Зависимости строго вниз: `plugin -> sdk`, `host -> sdk`. Плагины не знают друг о друге.

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

## Новый плагин

1. Папка `plugins/<kind>/<name>/` c `CMakeLists.txt`, `<name>.json` и классом-фабрикой.
2. В `CMakeLists.txt`: `iwp_add_plugin(iwp_<name> SOURCES ... LIBS ...)`.
3. В JSON обязательны `id`, `name`, `kind`, `version`, `apiVersion` (= `IWP_API_VERSION`).
   Необязательное `order` (по умолчанию 100) задаёт место в списках устройств и способов подключения: меньше = выше.
   Необязательное `group` (`instruments`, `servers`, `tools`, `simulators` или своё) - группа в списке устройств.
4. Добавить `<kind>/<name>` в нужные `products/*.cmake`.

Устройство: наследоваться от `iwp::SessionBase`, в конструкторе описать `poll(...)`,
в `onStart()` идентифицировать прибор и вызвать `setRunning()`, в `onCommand()` обрабатывать команды UI.
Результаты - только через `model()->update(key, value, role)`. Страница работает только с `DeviceModel`.
Форма настроек подключения у транспорта/endpoint необязательна (`createConfigEditor()`); без неё показывается
строка JSON. Кнопка `{ }` рядом с формой открывает тот же конфиг как JSON. Подробности: шпаргалки
[устройство](new-device-plugin.md), [транспорт](new-transport-plugin.md), [endpoint](new-endpoint-plugin.md).
