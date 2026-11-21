<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.1" language="ru">
<context>
    <name>CanPage</name>
    <message>
        <source>Pause</source>
        <translation>Пауза</translation>
    </message>
    <message>
        <source>Freeze the table; frames are still collected</source>
        <translation>Заморозить таблицу; кадры продолжают собираться</translation>
    </message>
    <message>
        <source>Clear</source>
        <translation>Очистить</translation>
    </message>
    <message>
        <source>Autoscroll</source>
        <translation>Автопрокрутка</translation>
    </message>
    <message>
        <source>Stream</source>
        <translation>Поток</translation>
    </message>
    <message>
        <source>By ID</source>
        <translation>По ID</translation>
    </message>
    <message>
        <source>Stream: every frame. By ID: one row per ID with the last data and the period</source>
        <translation>Поток: каждый кадр. По ID: одна строка на ID с последними данными и периодом</translation>
    </message>
    <message>
        <source>Filter by ID (hex): 123, 100-1FF, 7E0:7F0, !7FF</source>
        <translation>Фильтр по ID (hex): 123, 100-1FF, 7E0:7F0, !7FF</translation>
    </message>
    <message>
        <source>Comma separated: ID, range A-B, ID:mask (id &amp; mask), a leading ! excludes. Empty = all</source>
        <translation>Через запятую: ID, диапазон A-B, ID:маска (id &amp; маска), ! в начале исключает. Пусто = все</translation>
    </message>
    <message>
        <source>Show sent</source>
        <translation>Показывать отправленные</translation>
    </message>
    <message>
        <source>Save trace…</source>
        <translation>Сохранить трассу…</translation>
    </message>
    <message>
        <source>Ctrl+C copies the selected frames as ID#DATA lines</source>
        <translation>Ctrl+C копирует выбранные кадры строками ID#ДАННЫЕ</translation>
    </message>
    <message>
        <source>Send</source>
        <translation>Отправить</translation>
    </message>
    <message>
        <source>Standard (11-bit)</source>
        <translation>Стандартный (11 бит)</translation>
    </message>
    <message>
        <source>Extended (29-bit)</source>
        <translation>Расширенный (29 бит)</translation>
    </message>
    <message>
        <source>ID, hex</source>
        <translation>ID, hex</translation>
    </message>
    <message>
        <source>FD</source>
        <translation>FD</translation>
    </message>
    <message>
        <source>BRS</source>
        <translation>BRS</translation>
    </message>
    <message>
        <source>Bit rate switch: the data phase runs at the FD data rate</source>
        <translation>Переключение скорости: фаза данных идёт на скорости данных FD</translation>
    </message>
    <message>
        <source>RTR</source>
        <translation>RTR</translation>
    </message>
    <message>
        <source>Remote frame (classic CAN only)</source>
        <translation>Удалённый кадр (только классический CAN)</translation>
    </message>
    <message>
        <source>DLC </source>
        <translation>DLC </translation>
    </message>
    <message>
        <source>Data, hex: 30 01 02 (Enter sends)</source>
        <translation>Данные, hex: 30 01 02 (Enter отправляет)</translation>
    </message>
    <message>
        <source>Repeat every</source>
        <translation>Повторять каждые</translation>
    </message>
    <message>
        <source> ms</source>
        <translation> мс</translation>
    </message>
    <message>
        <source>Sequence of frames, one per line (Ctrl+Enter sends):
  001#30            standard ID 001, data 30
  18FF0001#0102     extended ID (8 digits)
  123##1 00 11 22   CAN FD (the digit after ## : 1 = BRS)
  wait 100          pause, ms        # comment</source>
        <translation>Последовательность кадров, по одному в строке (Ctrl+Enter отправляет):
  001#30            стандартный ID 001, данные 30
  18FF0001#0102     расширенный ID (8 цифр)
  123##1 00 11 22   CAN FD (цифра после ##: 1 = BRS)
  wait 100          пауза, мс        # комментарий</translation>
    </message>
    <message>
        <source>Send sequence</source>
        <translation>Отправить последовательность</translation>
    </message>
    <message>
        <source>Stop</source>
        <translation>Стоп</translation>
    </message>
    <message>
        <source>Load file…</source>
        <translation>Загрузить файл…</translation>
    </message>
    <message>
        <source>Pause between frames (in addition to the explicit "wait" lines)</source>
        <translation>Пауза между кадрами (в дополнение к явным строкам «wait»)</translation>
    </message>
    <message>
        <source>Pause between frames:</source>
        <translation>Пауза между кадрами:</translation>
    </message>
    <message>
        <source>Resume</source>
        <translation>Продолжить</translation>
    </message>
    <message>
        <source>Sequence stopped after %1 of %2 frames</source>
        <translation>Последовательность остановлена после %1 из %2 кадров</translation>
    </message>
    <message>
        <source>Not connected</source>
        <translation>Нет подключения</translation>
    </message>
    <message>
        <source>   |   Rx %1   Tx %2   |   %3 frames/s   |   bus load ≈ %4%</source>
        <translation>   |   Rx %1   Tx %2   |   %3 кадр/с   |   нагрузка шины ≈ %4%</translation>
    </message>
    <message>
        <source>   |   %1 frames not shown (too fast)</source>
        <translation>   |   %1 кадров не показано (слишком быстро)</translation>
    </message>
    <message>
        <source>Save trace</source>
        <translation>Сохранить трассу</translation>
    </message>
    <message>
        <source>candump log (*.log);;CSV (*.csv)</source>
        <translation>Журнал candump (*.log);;CSV (*.csv)</translation>
    </message>
    <message>
        <source>Cannot write %1: %2</source>
        <translation>Не удалось записать %1: %2</translation>
    </message>
    <message>
        <source>Enter the ID in hex</source>
        <translation>Введите ID в hex</translation>
    </message>
    <message>
        <source>ID is larger than 1FFFFFFF</source>
        <translation>ID больше 1FFFFFFF</translation>
    </message>
    <message>
        <source>A standard ID is at most 7FF: choose Extended</source>
        <translation>Стандартный ID не больше 7FF: выберите «Расширенный»</translation>
    </message>
    <message>
        <source>FD length must be 0..8, 12, 16, 20, 24, 32, 48 or 64 bytes (now %1)</source>
        <translation>Длина FD: 0..8, 12, 16, 20, 24, 32, 48 или 64 байта (сейчас %1)</translation>
    </message>
    <message>
        <source>Classic CAN carries at most 8 bytes (now %1): tick FD for more</source>
        <translation>Классический CAN несёт не больше 8 байт (сейчас %1): для большего включите FD</translation>
    </message>
    <message>
        <source>DLC %1</source>
        <translation>DLC %1</translation>
    </message>
    <message>
        <source>DLC ?</source>
        <translation>DLC ?</translation>
    </message>
    <message>
        <source>Line %1: %2</source>
        <translation>Строка %1: %2</translation>
    </message>
    <message>
        <source>Nothing to send: the sequence has no frames</source>
        <translation>Нечего отправлять: в последовательности нет кадров</translation>
    </message>
    <message>
        <source>Load frames</source>
        <translation>Загрузить кадры</translation>
    </message>
    <message>
        <source>Text files (*.txt *.log *.can);;All files (*)</source>
        <translation>Текстовые файлы (*.txt *.log *.can);;Все файлы (*)</translation>
    </message>
    <message>
        <source>Cannot read %1: %2</source>
        <translation>Не удалось прочитать %1: %2</translation>
    </message>
    <message>
        <source>The file is too large for a sequence (over 1 MB)</source>
        <translation>Файл слишком велик для последовательности (больше 1 МБ)</translation>
    </message>
    <message>
        <source>Loaded %1</source>
        <translation>Загружено: %1</translation>
    </message>
    <message numerus="yes">
        <source>Sequence sent: %n frame(s)</source>
        <translation>
            <numerusform>Отправлен %n кадр</numerusform>
            <numerusform>Отправлено %n кадра</numerusform>
            <numerusform>Отправлено %n кадров</numerusform>
        </translation>
    </message>
    <message numerus="yes">
        <source>Saved %n frame(s) to %1</source>
        <translation>
            <numerusform>Сохранён %n кадр в %1</numerusform>
            <numerusform>Сохранено %n кадра в %1</numerusform>
            <numerusform>Сохранено %n кадров в %1</numerusform>
        </translation>
    </message>
</context>
<context>
    <name>CanSession</name>
    <message>
        <source>simulator</source>
        <translation>симулятор</translation>
    </message>
    <message>
        <source>, %1 kbit/s</source>
        <translation>, %1 кбит/с</translation>
    </message>
    <message>
        <source>, FD %1 Mbit/s</source>
        <translation>, FD %1 Мбит/с</translation>
    </message>
    <message>
        <source>, listen only</source>
        <translation>, только приём</translation>
    </message>
    <message>
        <source>No CAN adapter</source>
        <translation>Нет CAN-адаптера</translation>
    </message>
</context>
<context>
    <name>CanTraceModel</name>
    <message>
        <source>#</source>
        <translation>№</translation>
    </message>
    <message>
        <source>Time, s</source>
        <translation>Время, с</translation>
    </message>
    <message>
        <source>Dir</source>
        <translation>Напр.</translation>
    </message>
    <message>
        <source>Flags</source>
        <translation>Флаги</translation>
    </message>
    <message>
        <source>ID</source>
        <translation>ID</translation>
    </message>
    <message>
        <source>DLC</source>
        <translation>DLC</translation>
    </message>
    <message>
        <source>Data</source>
        <translation>Данные</translation>
    </message>
    <message>
        <source>ASCII</source>
        <translation>ASCII</translation>
    </message>
    <message>
        <source>Δ ms</source>
        <translation>Δ мс</translation>
    </message>
</context>
<context>
    <name>CanIdModel</name>
    <message>
        <source>ID</source>
        <translation>ID</translation>
    </message>
    <message>
        <source>Flags</source>
        <translation>Флаги</translation>
    </message>
    <message>
        <source>Dir</source>
        <translation>Напр.</translation>
    </message>
    <message>
        <source>DLC</source>
        <translation>DLC</translation>
    </message>
    <message>
        <source>Last data</source>
        <translation>Последние данные</translation>
    </message>
    <message>
        <source>Count</source>
        <translation>Кол-во</translation>
    </message>
    <message>
        <source>Period, ms</source>
        <translation>Период, мс</translation>
    </message>
    <message>
        <source>Last, s</source>
        <translation>Последний, с</translation>
    </message>
</context>
<context>
    <name>CanPlugin</name>
    <message>
        <source>CAN / CAN FD adapter</source>
        <translation>CAN / CAN FD адаптер</translation>
    </message>
    <message>
        <source>CAN bus</source>
        <translation>CAN-шина</translation>
    </message>
</context>
</TS>
