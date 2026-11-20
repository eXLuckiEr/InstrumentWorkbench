# Шпаргалка: новый endpoint (не байтовый канал)

Endpoint это канал «запрос -> ответ», который не сводится к потоку байт: HTTP/Redfish, SSH, NI-VISA (GPIB/USB/...).
Если достаточно байтового канала, нужен транспорт (`new-transport-plugin.md`).
Образцы: `plugins/endpoint/http` (токены, 401, пиннинг сертификата), `plugins/endpoint/ssh` (процесс, запросы пароля).

## 1. Шаги

1. `plugins/endpoint/<name>/`, цель `iwp_<name>`, `id` = `<name>`.
2. Решить, какой **kind** обслуживает плагин (`http`, `ssh`, `visa`...). Устройства объявляют роль с этим kind,
   хост сам предложит все плагины с таким kind в выпадающем списке.
3. Добавить `endpoint/<name>` в `products/*.cmake`, Run CMake.
4. Переводы `i18n/iwp_<name>_ru.ts`.
5. Проверить: недоступный хост, неверный пароль, обрыв посреди запроса, таймаут, повторное открытие.

## 2. Файлы

```cmake
find_package(Qt6 REQUIRED COMPONENTS Network)
iwp_add_plugin(iwp_<name> SOURCES <name>plugin.h <name>plugin.cpp <name>endpoint.h <name>endpoint.cpp LIBS Qt6::Network)
```
```json
{ "id": "<name>", "name": "My endpoint", "kind": "endpoint", "version": "0.1.0", "apiVersion": 4 }
```
```cpp
class MyPlugin : public QObject, public iwp::IEndpointFactory {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID IWP_ENDPOINT_FACTORY_IID FILE "<name>.json")
    Q_INTERFACES(iwp::IEndpointFactory)
public:
    QString kind() const override { return QStringLiteral("<kind>"); }     // как в EndpointRole::kind
    QString typeId() const override { return QStringLiteral("<name>"); }
    QString displayName() const override { return tr("My endpoint"); }
    QVariantMap defaultConfig() const override;                             // ключи для формы подключения
    iwp::IEndpoint *create() override { return new MyEndpoint; }
};
```

## 3. Класс endpoint

```cpp
class MyEndpoint : public iwp::IEndpoint {
    Q_OBJECT
public:
    using iwp::IEndpoint::IEndpoint;
    QString kind() const override { return QStringLiteral("<kind>"); }
protected:
    void doOpen(const QVariantMap &config) override;              // -> setState(State::Online) или fail(...)
    void doClose() override;
    void doSend(quint64 id, const iwp::Request &request) override; // выполнить; в конце finish(id, response)
    void doAbort(quint64 id) override {}                            // запрос истёк по таймауту: отпустить ресурсы
};
```

Что даёт базовый класс (`IEndpoint`): очередь с приоритетами, таймауты на каждый запрос, общий сброс при потере канала.
Плагин отвечает только за выполнение одного запроса.

- **Состояния:** `Connecting` -> (`Authenticating`) -> `Online`; `fail(text)` -> `Error`. Пока не `Online`, запросы ждут.
- **Параллелизм:** по умолчанию один запрос за раз; `setMaxConcurrent(n)` если канал умеет больше (HTTP).
- **Запрос:** `Request{payload, timeoutMs, priority, expectResponse}`. Что значит `payload`, определяет плагин:
  опишите формат в комментарии к классу (см. шапку `sshendpoint.h`, `httpendpoint.h`).
- **Ответ:** `Response{ok, data, error, status, latencyMs}`. `status` для кода протокола (HTTP-статус, код возврата);
  `ok` ставьте по смыслу. Сессия считает ответ признаком живого канала, если `ok == true` или `0 < status < 500`
  (HTTP 404 доказывает, что связь есть); иначе это неудача, три подряд помечают значения роли устаревшими.
  `finish()` на уже истёкший запрос безопасно игнорируется.
- **Асинхронность:** `doSend` не блокирует, завершение через сигналы Qt. QObject-ресурсы (`QProcess`, `QNetworkAccessManager`,
  таймеры) создавать лениво в `doOpen()` (рабочий поток), не в конструкторе.
- **Неожиданные данные:** `emit unsolicited(data)`.

## 4. Пароли, токены, подтверждения

Свои диалоги не делать и пароли не хранить: только через `credentials()` (сервис хоста; связка ключей ОС).

```cpp
iwp::CredentialRequest req;
req.key = "myproto|user@host:22";         // ключ записи в связке ключей, сделать уникальным
req.title = tr("Credentials for %1").arg(host);
req.message = tr("Enter the credentials for %1").arg(host);
req.username = configUser;  req.askUsername = configUser.isEmpty();
req.secretLabel = tr("Password");
req.allowRemember = true;                 // false для PIN и одноразовых кодов
req.forcePrompt = lastWasRejected;        // сохранённый секрет не подошёл: спросить заново
credentials()->get(req, this, [this](const iwp::Credentials &c) { if (!c.ok) fail(tr("Cancelled")); /* c.username, c.secret */ });

credentials()->confirm(tr("Trust host?"), message, this, [this](bool yes) { ... });   // да/нет
emit userActionRequired(tr("Touch your key"));                                       // подсказка пользователю
```
Ответ придёт в вашем потоке. Если провайдера нет (`credentials() == nullptr`), завершайте открытие понятной ошибкой.

## 5. Форма настроек (необязательно)

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

## 6. Подводные камни

- Любая потеря связи = `fail()`, иначе устройство не уйдёт в Offline.
- Не писать секреты в логи, сообщения об ошибках и конфиг.
- Самоподписанные сертификаты, `http://` и подобное: решает конфигурация (`trustedFingerprint`...), по умолчанию безопасно.
- Конфиг: имена ключей совпадают с `defaultConfig()`, все значения проверять, ошибки понятным текстом через `tr()`.
- Внешний процесс или SDK (`ssh`, NI-VISA): искать в `PATH`, при отсутствии давать подсказку, что установить.
  Библиотеки вроде VISA подключать во время работы (`QLibrary`), чтобы плагин собирался без SDK.
