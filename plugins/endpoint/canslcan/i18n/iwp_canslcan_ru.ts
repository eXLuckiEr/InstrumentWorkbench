<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.1" language="ru">
<context>
    <name>SlcanEndpoint</name>
    <message>
        <source>The adapter does not answer. Is it in SLCAN mode (not gs_usb/candlelight)?</source>
        <translation>Адаптер не отвечает. Он в режиме SLCAN (а не gs_usb/candlelight)?</translation>
    </message>
    <message>
        <source>The adapter did not answer the command %1</source>
        <translation>Адаптер не ответил на команду: %1</translation>
    </message>
    <message>
        <source>No port specified. Available: %1</source>
        <translation>Порт не указан. Доступные: %1</translation>
    </message>
    <message>
        <source>none found</source>
        <translation>не найдено</translation>
    </message>
    <message>
        <source>Bit rate %1 is not supported by the adapter presets (10k, 20k, 50k, 100k, 125k, 250k, 500k, 1M); use "initCommands" for anything else</source>
        <translation>Скорость %1 не входит в пресеты адаптера (10k, 20k, 50k, 100k, 125k, 250k, 500k, 1M); для другой используйте «initCommands»</translation>
    </message>
    <message>
        <source>FD data bit rate must be 1, 2, 4 or 5 Mbit/s</source>
        <translation>Скорость данных FD должна быть 1, 2, 4 или 5 Мбит/с</translation>
    </message>
    <message>
        <source>Cannot open %1: %2</source>
        <translation>Не удалось открыть %1: %2</translation>
    </message>
    <message>
        <source>bit rate</source>
        <translation>скорость</translation>
    </message>
    <message>
        <source>FD data bit rate</source>
        <translation>скорость данных FD</translation>
    </message>
    <message>
        <source>listen-only mode</source>
        <translation>режим только приём</translation>
    </message>
    <message>
        <source>automatic retransmission</source>
        <translation>автоповтор отправки</translation>
    </message>
    <message>
        <source>opening the channel</source>
        <translation>открытие канала</translation>
    </message>
    <message>
        <source>The adapter is not open</source>
        <translation>Адаптер не открыт</translation>
    </message>
    <message>
        <source>CAN FD frame, but FD is off in the connection settings</source>
        <translation>Кадр CAN FD, но FD выключен в настройках подключения</translation>
    </message>
    <message>
        <source>This frame cannot be sent (length does not fit the frame type)</source>
        <translation>Этот кадр нельзя отправить (длина не подходит типу кадра)</translation>
    </message>
    <message>
        <source>The adapter rejected a command or frame (BEL)</source>
        <translation>Адаптер отклонил команду или кадр (BEL)</translation>
    </message>
    <message>
        <source>The adapter rejected: %1</source>
        <translation>Адаптер отклонил: %1</translation>
    </message>
    <message>
        <source>The adapter rejected the optional command: %1</source>
        <translation>Адаптер отклонил необязательную команду: %1</translation>
    </message>
</context>
<context>
    <name>SlcanPlugin</name>
    <message>
        <source>CAN adapter (SLCAN, serial port)</source>
        <translation>CAN-адаптер (SLCAN, последовательный порт)</translation>
    </message>
</context>
<context>
    <name>SlcanConfigEditor</name>
    <message>
        <source>FD data rate</source>
        <translation>Скорость данных FD</translation>
    </message>
    <message>
        <source>Advanced ▼</source>
        <translation>Дополнительно ▼</translation>
    </message>
    <message>
        <source>Advanced ▲</source>
        <translation>Дополнительно ▲</translation>
    </message>
    <message>
        <source>adapter commands before opening, separated by ;</source>
        <translation>команды адаптеру перед открытием, через ;</translation>
    </message>
    <message>
        <source>no ports found</source>
        <translation>портов не найдено</translation>
    </message>
    <message>
        <source>Refresh</source>
        <translation>Обновить</translation>
    </message>
    <message>
        <source>Port</source>
        <translation>Порт</translation>
    </message>
    <message>
        <source>Bit rate</source>
        <translation>Скорость</translation>
    </message>
    <message>
        <source>10 kbit/s</source>
        <translation>10 кбит/с</translation>
    </message>
    <message>
        <source>20 kbit/s</source>
        <translation>20 кбит/с</translation>
    </message>
    <message>
        <source>50 kbit/s</source>
        <translation>50 кбит/с</translation>
    </message>
    <message>
        <source>100 kbit/s</source>
        <translation>100 кбит/с</translation>
    </message>
    <message>
        <source>125 kbit/s</source>
        <translation>125 кбит/с</translation>
    </message>
    <message>
        <source>250 kbit/s</source>
        <translation>250 кбит/с</translation>
    </message>
    <message>
        <source>500 kbit/s</source>
        <translation>500 кбит/с</translation>
    </message>
    <message>
        <source>1 Mbit/s</source>
        <translation>1 Мбит/с</translation>
    </message>
    <message>
        <source>CAN FD</source>
        <translation>CAN FD</translation>
    </message>
    <message>
        <source>FD data bit rate</source>
        <translation>Скорость данных FD</translation>
    </message>
    <message>
        <source>2 Mbit/s</source>
        <translation>2 Мбит/с</translation>
    </message>
    <message>
        <source>4 Mbit/s</source>
        <translation>4 Мбит/с</translation>
    </message>
    <message>
        <source>5 Mbit/s</source>
        <translation>5 Мбит/с</translation>
    </message>
    <message>
        <source>Listen only (no acknowledge, no transmit)</source>
        <translation>Только приём (без подтверждения, без отправки)</translation>
    </message>
    <message>
        <source>Automatic retransmission</source>
        <translation>Автоповтор отправки</translation>
    </message>
    <message>
        <source>Extra commands</source>
        <translation>Доп. команды</translation>
    </message>
    <message>
        <source>Select the adapter's serial port</source>
        <translation>Выберите последовательный порт адаптера</translation>
    </message>
</context>
</TS>
