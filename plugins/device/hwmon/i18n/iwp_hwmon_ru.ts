<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.1" language="ru">
<context>
    <name>HwmonPage</name>
    <message>
        <source>Device</source>
        <translation>Устройство</translation>
    </message>
    <message>
        <source>Time span:</source>
        <translation>Интервал:</translation>
    </message>
    <message>
        <source>10 s</source>
        <translation>10 с</translation>
    </message>
    <message>
        <source>30 s</source>
        <translation>30 с</translation>
    </message>
    <message>
        <source>1 min</source>
        <translation>1 мин</translation>
    </message>
    <message>
        <source>5 min</source>
        <translation>5 мин</translation>
    </message>
    <message>
        <source>15 min</source>
        <translation>15 мин</translation>
    </message>
    <message>
        <source>Pause graphs</source>
        <translation>Пауза графиков</translation>
    </message>
    <message>
        <source>Settings</source>
        <translation>Настройки</translation>
    </message>
    <message>
        <source>Precise timing ($EPOCHREALTIME, bash 5)</source>
        <translation>Точное время ($EPOCHREALTIME, bash 5)</translation>
    </message>
    <message>
        <source>On: the device stamps every sample and keeps the period without drift. Off: simple mode, needs only bash 4; the samples are stamped when they arrive.</source>
        <translation>Вкл.: устройство ставит метку каждому отсчёту и держит период без дрейфа. Выкл.: простой режим, достаточно bash 4; метки ставятся при получении.</translation>
    </message>
    <message>
        <source>Sampling period:</source>
        <translation>Период опроса:</translation>
    </message>
    <message>
        <source> ms</source>
        <translation> мс</translation>
    </message>
    <message>
        <source>Sensors: name | file | divisor | unit | options</source>
        <translation>Датчики: имя | файл | делитель | единицы | опции</translation>
    </message>
    <message>
        <source>Commands after connecting: name | command</source>
        <translation>Команды после подключения: имя | команда</translation>
    </message>
    <message>
        <source>Apply</source>
        <translation>Применить</translation>
    </message>
    <message>
        <source>Defaults</source>
        <translation>По умолчанию</translation>
    </message>
    <message>
        <source>The divisor turns the raw number into the unit (hwmon gives milliamperes: 1; amperes: 1000). A * in the path is fine: the hwmon numbers change between boots. Options: min=N max=N (the tile turns red outside), text (a state, shown as it is) with expect=VALUE (red when it differs); {} in the name makes a tile for every file the * matches. Lines starting with # are comments. Nothing is installed on the device.</source>
        <translation>Делитель переводит исходное число в единицы (hwmon отдаёт миллиамперы: 1; амперы: 1000). * в пути допустима: номера hwmon меняются между загрузками. Опции: min=N max=N (плитка краснеет за пределами), text (состояние, показывается как есть) с expect=ЗНАЧЕНИЕ (красная, если отличается); {} в имени создаёт плитку для каждого файла, подошедшего под *. Строки с # — комментарии. На устройство ничего не устанавливается.</translation>
    </message>
    <message>
        <source>The versions are read after connecting.</source>
        <translation>Версии читаются после подключения.</translation>
    </message>
    <message>
        <source>%1 sensors, every %2 ms</source>
        <translation>Датчиков: %1, период %2 мс</translation>
    </message>
    <message>
        <source>Preset:</source>
        <translation>Пресет:</translation>
    </message>
    <message>
        <source>Save as...</source>
        <translation>Сохранить как...</translation>
    </message>
    <message>
        <source>Open...</source>
        <translation>Открыть...</translation>
    </message>
    <message>
        <source>Choose a preset...</source>
        <translation>Выберите пресет...</translation>
    </message>
    <message>
        <source>%1 (saved)</source>
        <translation>%1 (сохранённый)</translation>
    </message>
    <message>
        <source>Loaded &quot;%1&quot;. Press Apply to use it.</source>
        <translation>Загружен «%1». Нажмите «Применить», чтобы использовать.</translation>
    </message>
    <message>
        <source>Save preset</source>
        <translation>Сохранить пресет</translation>
    </message>
    <message>
        <source>my preset</source>
        <translation>мой пресет</translation>
    </message>
    <message>
        <source>hwmon presets (*.json)</source>
        <translation>Пресеты hwmon (*.json)</translation>
    </message>
    <message>
        <source>Cannot write %1</source>
        <translation>Не удалось записать %1</translation>
    </message>
    <message>
        <source>Saved: %1</source>
        <translation>Сохранено: %1</translation>
    </message>
    <message>
        <source>Open preset</source>
        <translation>Открыть пресет</translation>
    </message>
    <message>
        <source>Cannot read %1</source>
        <translation>Не удалось прочитать %1</translation>
    </message>
    <message>
        <source>%1: %2</source>
        <translation>%1: %2</translation>
    </message>
</context>
<context>
    <name>HwmonPlugin</name>
    <message>
        <source>SSH shell of the device (choose “SSH shell”)</source>
        <translation>SSH-оболочка устройства (выберите «SSH shell»)</translation>
    </message>
</context>
<context>
    <name>HwmonSession</name>
    <message>
        <source>The device did not start the receiver. Is bash available there?</source>
        <translation>Устройство не запустило приёмник. Есть ли там bash?</translation>
    </message>
</context>
<context>
    <name>SensorTile</name>
    <message>
        <source>Double click to open a larger graph</source>
        <translation>Двойной щелчок открывает большой график</translation>
    </message>
    <message>
        <source>Expected: %1</source>
        <translation>Ожидается: %1</translation>
    </message>
    <message>
        <source>min %1</source>
        <translation>мин %1</translation>
    </message>
    <message>
        <source>max %1</source>
        <translation>макс %1</translation>
    </message>
</context>
<context>
    <name>HwmonSettings</name>
    <message>
        <source>Sensors: %1</source>
        <translation>Датчики: %1</translation>
    </message>
    <message>
        <source>Info commands: %1</source>
        <translation>Команды: %1</translation>
    </message>
</context>
<context>
    <name>HwmonPreset</name>
    <message>
        <source>not a JSON file: %1</source>
        <translation>не JSON-файл: %1</translation>
    </message>
    <message>
        <source>this is not an hwmon preset</source>
        <translation>это не пресет hwmon</translation>
    </message>
    <message>
        <source>unsupported preset version</source>
        <translation>неподдерживаемая версия пресета</translation>
    </message>
    <message>
        <source>sensors: %1</source>
        <translation>датчики: %1</translation>
    </message>
    <message>
        <source>commands: %1</source>
        <translation>команды: %1</translation>
    </message>
</context>
</TS>
