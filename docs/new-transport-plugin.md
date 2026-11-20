# Шпаргалка: новый транспорт (физический канал)

Транспорт только двигает **байты** (TCP, UART, UDP, CAN...). О протоколах и приборах он ничего не знает.
Подходит для ролей `scpi` и `stream`: хост сам оборачивает его в `StreamEndpoint` (разбор кадров, очередь, таймауты).
Образцы: `plugins/transport/tcp` (простой), `serial` (необязательная зависимость), `sim` (симулятор).

## 1. Шаги

1. Папка `plugins/transport/<name>/`, цель CMake `iwp_<name>`, `id` в json = `<name>`.
2. Добавить `transport/<name>` в `products/*.cmake`, Run CMake.
3. Переводы: `i18n/iwp_<name>_ru.ts` (скопировать другой, `update_translations`).
4. Проверить: открыть, закрыть, обрыв связи, повторное открытие, неверный конфиг, быстрый поток данных.

## 2. Файлы

`CMakeLists.txt`
```cmake
find_package(Qt6 REQUIRED COMPONENTS Network)        # нужные модули Qt
iwp_add_plugin(iwp_<name>
    SOURCES <name>plugin.h <name>plugin.cpp <name>transport.h <name>transport.cpp
    LIBS Qt6::Network)
```
Если зависимость необязательна (как Qt Serial), плагин не должен ронять сборку: см. `plugins/transport/serial/CMakeLists.txt`
(`find_package(... QUIET)`, при неудаче `message(WARNING ...)` + `return()`; кэшированный `...-NOTFOUND` нужно сбрасывать через `unset(... CACHE)`).

`<name>.json`
```json
{ "id": "<name>", "name": "My transport", "kind": "transport", "version": "0.1.0", "apiVersion": 4 }
```

Фабрика
```cpp
class MyPlugin : public QObject, public iwp::ITransportFactory {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID IWP_TRANSPORT_FACTORY_IID FILE "<name>.json")
    Q_INTERFACES(iwp::ITransportFactory)
public:
    QString typeId() const override { return QStringLiteral("<name>"); }
    QString displayName() const override { return tr("My transport"); }
    QVariantMap defaultConfig() const override { return {{"host", "127.0.0.1"}, {"port", 5000}}; }  // видно в форме
    iwp::ITransport *create() override { return new MyTransport; }
};
```

## 3. Класс транспорта

```cpp
class MyTransport : public iwp::ITransport {
    Q_OBJECT
public:
    QString typeId() const override { return QStringLiteral("<name>"); }
protected:
    void doOpen(const QVariantMap &config) override;   // начать открытие
    void doClose() override;
    qint64 doWrite(const QByteArray &data) override;   // вернуть число записанных байт или -1
private:
    QTcpSocket *m_socket = nullptr;                    // создаётся лениво!
};
```

```cpp
void MyTransport::doOpen(const QVariantMap &config)
{
    if (!m_socket) {                                   // QObject-дети только здесь, не в конструкторе
        m_socket = new QTcpSocket(this);
        connect(m_socket, &QTcpSocket::readyRead, this, [this] { emit dataReceived(m_socket->readAll()); });
        connect(m_socket, &QTcpSocket::connected, this, [this] { setState(State::Open); });
        connect(m_socket, &QTcpSocket::errorOccurred, this, [this] { fail(m_socket->errorString()); });
    }
    setState(State::Opening);
    m_socket->connectToHost(config.value("host").toString(), config.value("port").toInt());
}
```

Правила:
- Конструктор лёгкий: транспорт создаётся в главном потоке, потом переезжает в рабочий. Сокеты, порты, таймеры создавать в `doOpen()`.
- Состояния: `Opening` -> `Open` (когда реально готов, можно асинхронно) -> `Closed`; ошибка = `fail(text)` (выставляет `Error`).
- Входящие байты: `emit dataReceived(chunk)`. Границ кадров транспорт не знает, куски приходят как придётся.
- Любая потеря связи должна дойти до хоста (`fail()` или `setState(Closed)`), иначе устройство не перейдёт в Offline.
- Параметры только из `config` (ключи совпадают с `defaultConfig`), значения проверять и давать понятные сообщения (`tr("...")`).
- Блокирующих вызовов нет. Запись больших объёмов: учитывать, что `doWrite` может принять не всё.
- Строки для пользователя через `tr()`, статические функции тоже (`tr()` статический).

## 4. Форма настроек (необязательно)

Пока формы нет, хост показывает настройки строкой JSON из `defaultConfig()`. Чтобы пользователь выбирал, а не помнил,
сделайте форму рядом с остальными исходниками плагина (`<name>configeditor.h/.cpp`, добавить в `SOURCES`):

```cpp
class MyConfigEditor : public iwp::FormConfigEditor {
    Q_OBJECT
public:
    explicit MyConfigEditor(QWidget *parent = nullptr) : iwp::FormConfigEditor(parent) {
        addText("host", tr("Host"), "192.168.0.10");          // пустое поле = ключа в конфиге нет
        addInt("port", tr("Port"), 1, 65535);
        addChoice("parity", tr("Parity"),
                  {{tr("None"), QStringLiteral("none")}, {tr("Even"), QStringLiteral("even")}});
        addFile("identityFile", tr("Identity file"));          // поле + кнопка выбора файла
        addFlag("keepAlive", tr("Keep alive"));
    }
    QString validate() const override {                        // "" = всё хорошо, иначе текст ошибки пользователю
        return config().value("host").toString().isEmpty() ? tr("Enter the host") : QString();
    }
};
```
В фабрике: `iwp::ConfigEditor *createConfigEditor(QWidget *parent) override { return new MyConfigEditor(parent); }`.

- Ключи формы = ключи `defaultConfig()`. Ключи, для которых поля нет (`extraArgs`, `terminator` от устройства), форма не трогает и отдаёт обратно.
- Своё поле: `bind(key, getter, setter)` или `bindCombo(key, comboBox)` (пример: список портов с кнопкой «Обновить» в `serialconfigeditor.cpp`).
- Хост сам добавляет кнопку `{ }` для правки того же конфига как JSON, поэтому редкие параметры не нужно выносить в форму.
- Форма работает только в UI-потоке. Транспорт и сессия получают готовый `QVariantMap`.
- Своя форма не обязана быть `FormConfigEditor`: достаточно наследника `iwp::ConfigEditor` (`setConfig`, `config`, `validate`, сигнал `changed`).
