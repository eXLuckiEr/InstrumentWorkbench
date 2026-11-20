# Шпаргалка: новый плагин устройства

Устройство = **сессия** (что и в каком порядке спрашивать) + **страница** (как выглядит) + **роли** (какие каналы нужны).
Образцы: `plugins/device/fakepsu` (SCPI, опрос), `telemetry` (поток с push), `linuxhost` (SSH), `redfish` (HTTP), `console`.

## 1. Шаги

1. Скопировать папку похожего плагина в `plugins/device/<name>/` и переименовать: файлы, классы, `id` в json, цель CMake `iwp_<name>`.
   Имя цели важно: от него зависят `i18n/iwp_<name>_ru.ts` и префикс ресурсов `:/iwp/<name>/`.
2. Добавить `device/<name>` в `products/full.cmake` (и в профили клиентов, которым он нужен).
3. Run CMake. Собрать.
4. Иконка: `resources/icon.svg` (24x24, чёрный контур, `stroke-width=2`), `icon()` возвращает `iwp::themedIcon(":/iwp/<name>/icon.svg")`.
5. Переводы: скопировать `i18n/iwp_<other>_ru.ts` в `i18n/iwp_<name>_ru.ts`, собрать цель `update_translations`, перевести в Linguist.
6. Проверить: отключение кабеля, молчащий прибор, неверный ответ, повторное подключение.

## 2. Файлы

```
plugins/device/<name>/
  CMakeLists.txt  <name>.json  <name>plugin.h/.cpp  <name>session.h/.cpp  <name>page.h/.cpp
  resources/icon.svg   i18n/iwp_<name>_ru.ts
```

`CMakeLists.txt`
```cmake
iwp_add_plugin(iwp_<name>
    SOURCES <name>plugin.h <name>plugin.cpp <name>session.h <name>session.cpp <name>page.h <name>page.cpp)
```

`<name>.json` (`apiVersion` = `IWP_API_VERSION`, сейчас 6, иначе хост отклонит плагин)
```json
{ "id": "<name>", "name": "Human readable name", "kind": "device", "version": "0.1.0", "apiVersion": 4 }
```

Необязательное поле `"order"` (число, по умолчанию 100) задаёт место в списках: меньше = выше, при равенстве по названию.
Принято: 0-9 служебное (консоль), 10-99 ваши избранные, 100 обычные.

Фабрика (`<name>plugin.h`)
```cpp
class MyPlugin : public QObject, public iwp::IDeviceFactory {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID IWP_DEVICE_FACTORY_IID FILE "<name>.json")
    Q_INTERFACES(iwp::IDeviceFactory)
public:
    QString deviceId() const override { return QStringLiteral("<name>"); }
    QString displayName() const override { return tr("My device"); }
    QIcon icon() const override;
    QList<iwp::EndpointRole> endpointRoles() const override;
    iwp::SessionBase *createSession(const QHash<QString, iwp::IEndpoint *> &endpoints,
                                    iwp::DeviceModel *model) override;
    QWidget *createPage(iwp::DeviceModel *model, QWidget *parent) override;
    // по желанию: std::unique_ptr<iwp::IFramer> createFramer(const QString &role) override;
};
```

## 3. Роли (каналы)

```cpp
iwp::EndpointRole scpi;
scpi.role = "scpi";            // имя, под которым сессия обращается к каналу
scpi.kind = "scpi";            // см. таблицу
scpi.description = tr("SCPI command channel");
scpi.defaults = {{"terminator", "LF"}};   // подсказка для строки конфигурации
return {scpi};                 // optional = true: канал можно не подключать
```

| kind | кто обслуживает | как говорить |
|------|-----------------|--------------|
| `scpi` | любой транспорт (UART, TCP, sim...) | `query(role, "*IDN?", ...)`, ответ = одна строка |
| `stream` | любой транспорт | поток кадров, `framing`/`terminator` в конфиге или `createFramer()` |
| `http` | плагин `endpoint/http` | payload `"GET /path"` или `"POST /path\n<json>"` |
| `ssh` | плагин `endpoint/ssh` | payload = команда оболочки, `Response.status` = код возврата |
| свой | свой `IEndpointFactory` | см. `new-endpoint-plugin.md` |

Несколько ролей разрешены (например `scpi` + `ssh`), хост построит строку подключения для каждой.

## 4. Сессия (оркестратор)

Живёт в рабочем потоке вместе со своими каналами. **Виджетов не трогает, только `model()`.**

```cpp
MySession::MySession(const Endpoints &endpoints, iwp::DeviceModel *model, QObject *parent)
    : SessionBase(endpoints, model, parent)
{
    // циклический опрос; запускается из setRunning(); пока ответ не пришёл, задача не стартует повторно
    poll("volt", 200, "scpi", "MEAS:VOLT?", [this](const iwp::Response &r) {
        if (r.ok) this->model()->update("volt", r.toDouble(), "scpi");   // 3-й аргумент = роль-источник
    });
    // push-прибор: watchdog("data", 500) и kick("data") на каждый кадр
}

void MySession::onStart()                // каналы готовы: опознать прибор, настроить, затем setRunning()
{
    query("scpi", "*IDN?", [this](const iwp::Response &r) {
        if (!r.ok) { fail(tr("No answer to *IDN?: %1").arg(r.error)); return; }
        model()->update("idn", r.text(), "scpi");
        setRunning();
    }, 0, 2000);                         // приоритет 0, таймаут 2000 мс
}

void MySession::onCommand(const QString &name, const QVariant &arg)   // команды из UI
{
    if (name == "setpoint")
        write("scpi", "SOUR:VOLT " + QByteArray::number(arg.toDouble(), 'f', 3), 10);   // 10 = раньше опроса
}
```

Состояния: Idle -> Starting (`onStart`) -> Running -> при потере канала Offline (значения помечаются устаревшими) -> снова Starting.
Три подряд неудачных запроса по роли = значения роли `stale`.

## 5. Модель

| Вызов | Для чего |
|-------|----------|
| `update(key, value, role)` | одно значение (любой поток) |
| `updateMany(map, role)` | быстрые потоки: публиковать снимок ~30 раз/с, а не каждый кадр |
| `markStale(role)` | вручную пометить значения роли устаревшими |
| `postEvent(text)` | событие в журнал |
| `setStatus(...)` | сессия делает сама через `setRunning()`/`fail()` |
| `sendCommand(name, arg)` | UI -> сессия, приходит в `onCommand` |

Имена ключей: латиница, `snake_case`, единицы в документации к сессии (комментарий в `.h`: ключи и команды).

## 6. Страница

Только `DeviceModel`, никакого доступа к сессии и каналам.

- Минимум: `return new iwp::KeyValuePage(model, parent);` (таблица всех ключей, серые = устаревшие).
- Своя: на старте заполнить из `model->keys()` / `value()`, дальше слушать `valueChanged`, `staleChanged`, `statusChanged`;
  кнопки и поля вызывают `model->sendCommand("name", arg)`.
- Тексты только через `tr()`; склейка строк запрещена (`tr("Port %1").arg(n)`).

### Страница в Qt Designer (.ui)

Раскладку удобнее рисовать мышью. Образец: `plugins/device/fakepsu` (`fakepsupage.ui`, `fakepsupage.h/.cpp`).

1. Qt Creator: Add New... -> Qt -> Qt Designer Form (Widget), назвать `<name>page`; файл `.ui` лежит рядом с `.cpp` страницы.
2. В Designer задать `objectName` виджетам осмысленно (`voltLcd`, `setpointSpin`): так они будут называться в коде (`ui->voltLcd`).
3. Добавить `<name>page.ui` в `SOURCES` в `CMakeLists.txt`. Остальное сделает AUTOUIC (`ui_<name>page.h`).
4. Страница наследует `QWidget`, держит `std::unique_ptr<Ui::MyPage> ui`, в конструкторе `ui->setupUi(this)`,
   дальше только подключения к `DeviceModel` (`valueChanged`, `sendCommand`...). Деструктор объявить в `.h`, определить в `.cpp`.
5. Имя класса в `.ui` (`<class>`) должно совпадать с именем класса страницы: оно же контекст переводов.
   Тексты из Designer попадают в `.ts` через `update_translations` сами.
6. В `.ui` только раскладка и свойства. Поведение (сигналы, слоты, формат значений) пишется в коде страницы.
7. Нужный нестандартный виджет: в Designer «Promote to...» и указать свой класс и заголовок.

## 7. Подводные камни

- Не блокировать рабочий поток (никаких `waitFor...` и `sleep` в сессии).
- Числа в SCPI форматировать без локали: `QByteArray::number(x, 'f', 3)`. `%L1` только для показа пользователю.
- Своего QObject-канала в конструкторе транспорта/endpoint не создавать, только в `doOpen()`.
- Пароли только через сервис учётных данных, в конфиг и файлы их не писать.
- Новые ключи настроек страницы не писать в `QSettings` напрямую, использовать `AppSettings` хоста.
- Свой бинарный протокол: `createFramer(role)` или `"framing"` в конфигурации роли (`fixed`: header + length + CRC).
  Образец: `plugins/device/telemetry`.

## Что сделать при отключении (`onStopping`)

Когда пользователь нажимает «Отключить» или закрывает приложение, сессия вызывает
`onStopping(done)` **пока порт ещё открыт**. Здесь отправляют «прощальные» команды:
вернуть прибор в локальный режим, выключить выходы и т. п. Вызовите `done()`, когда прибор
ответил: надёжнее всего послать запрос `*OPC?` и вызвать `done()` в его обработчике. Ответ
доказывает, что предыдущие байты ушли по UART и прибор их обработал. Если прибор молчит,
SDK закроет порт сам через 1,5 с. По умолчанию `done()` вызывается сразу.

```cpp
void MySession::onStopping(std::function<void()> done)
{
    write(kScpi, "SYST:LOC", 20);
    query(kScpi, "*OPC?", [done](const iwp::Response &) { done(); }, 20, 1000);
}
```
