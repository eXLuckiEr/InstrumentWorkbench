<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.1" language="ru">
<context>
    <name>ConsolePage</name>
    <message>
        <source>No ending</source>
        <translation>Без окончания</translation>
    </message>
    <message>
        <source>Send</source>
        <translation>Отправить</translation>
    </message>
    <message>
        <source>Clear</source>
        <translation>Очистить</translation>
    </message>
    <message>
        <source>Pin: keep this command (sequence) at the top of the history</source>
        <translation>Закрепить: держать эту команду (последовательность) вверху истории</translation>
    </message>
    <message>
        <source>Remove this command (sequence) from the history</source>
        <translation>Удалить эту команду (последовательность) из истории</translation>
    </message>
    <message>
        <source>Clear history</source>
        <translation>Очистить историю</translation>
    </message>
    <message>
        <source>Forget the history of this connection (pinned entries stay)</source>
        <translation>Забыть историю этого подключения (закреплённое остаётся)</translation>
    </message>
    <message>
        <source>Command or several lines (a sequence). Enter sends, Shift+Enter starts a new line, Up/Down walk through the history</source>
        <translation>Команда или несколько строк (последовательность). Enter — отправить, Shift+Enter — новая строка, Вверх/Вниз — история</translation>
    </message>
    <message>
        <source>Added to every line</source>
        <translation>Добавляется к каждой строке</translation>
    </message>
    <message>
        <source>No pause</source>
        <translation>Без паузы</translation>
    </message>
    <message>
        <source>%1 ms</source>
        <translation>%1 мс</translation>
    </message>
    <message>
        <source>Pause between the lines of a sequence</source>
        <translation>Пауза между строками последовательности</translation>
    </message>
    <message>
        <source>Load file...</source>
        <translation>Загрузить файл...</translation>
    </message>
    <message>
        <source>Put the lines of a text file into the input (nothing is sent yet)</source>
        <translation>Поместить строки текстового файла в поле ввода (ничего не отправляется)</translation>
    </message>
    <message>
        <source>History (%1)</source>
        <translation>История (%1)</translation>
    </message>
    <message>
        <source>   (+%1 lines)</source>
        <translation>   (+%1 строк)</translation>
    </message>
    <message>
        <source>Forget the commands sent to this connection? Pinned entries stay.</source>
        <translation>Забыть команды, отправленные в это подключение? Закреплённые останутся.</translation>
    </message>
    <message>
        <source>Load commands</source>
        <translation>Загрузить команды</translation>
    </message>
    <message>
        <source>Text files (*.txt *.scpi *.cmd *.cfg);;All files (*)</source>
        <translation>Текстовые файлы (*.txt *.scpi *.cmd *.cfg);;Все файлы (*)</translation>
    </message>
    <message>
        <source>The file is too large (over 1 MiB).</source>
        <translation>Файл слишком большой (больше 1 МиБ).</translation>
    </message>
    <message>
        <source>Stop (%1)</source>
        <translation>Стоп (%1)</translation>
    </message>
    <message>
        <source>Line</source>
        <translation>Строка</translation>
    </message>
    <message>
        <source>Terminal</source>
        <translation>Терминал</translation>
    </message>
    <message>
        <source>Hex</source>
        <translation>Hex</translation>
    </message>
</context>
<context>
    <name>HexView</name>
    <message>
        <source>Show sent data</source>
        <translation>Показывать отправленное</translation>
    </message>
    <message>
        <source>Clear</source>
        <translation>Очистить</translation>
    </message>
    <message>
        <source>Send</source>
        <translation>Отправить</translation>
    </message>
    <message>
        <source>Hex bytes, e.g. 48 65 6C 6C 6F or 0x48, 0x65</source>
        <translation>Hex-байты, например 48 65 6C 6C 6F или 0x48, 0x65</translation>
    </message>
</context>
<context>
    <name>TerminalView</name>
    <message>
        <source>As received</source>
        <translation>Как есть</translation>
    </message>
    <message>
        <source>LF → CR LF</source>
        <translation>LF → CR LF</translation>
    </message>
    <message>
        <source>CR → CR LF</source>
        <translation>CR → CR LF</translation>
    </message>
    <message>
        <source>Newlines:</source>
        <translation>Новая строка:</translation>
    </message>
    <message>
        <source>Line ends of the device output. Use &quot;LF → CR LF&quot; if lines form a staircase, &quot;CR → CR LF&quot; if lines overwrite each other.</source>
        <translation>Окончания строк в выводе устройства. Если строки идут лесенкой, выберите «LF → CR LF», если перезаписывают друг друга — «CR → CR LF».</translation>
    </message>
    <message>
        <source>Clear</source>
        <translation>Очистить</translation>
    </message>
    <message>
        <source>Reset</source>
        <translation>Сброс</translation>
    </message>
    <message>
        <source>Copy</source>
        <translation>Копировать</translation>
    </message>
    <message>
        <source>Paste</source>
        <translation>Вставить</translation>
    </message>
    <message>
        <source>Send size (stty)</source>
        <translation>Передать размер (stty)</translation>
    </message>
    <message>
        <source>Tell the remote shell the window size: sends &quot;stty rows R cols C&quot;</source>
        <translation>Сообщить удалённой оболочке размер окна: отправляет «stty rows R cols C»</translation>
    </message>
</context>
<context>
    <name>hexutil</name>
    <message>
        <source>Not a hex byte sequence: &quot;%1&quot;</source>
        <translation>Не hex-последовательность: «%1»</translation>
    </message>
    <message>
        <source>Nothing to send</source>
        <translation>Нечего отправлять</translation>
    </message>
</context>
<context>
    <name>ConsolePlugin</name>
    <message>
        <source>Raw byte stream</source>
        <translation>Сырой поток байт</translation>
    </message>
    <message>
        <source>Console</source>
        <translation>Консоль</translation>
    </message>
</context>
</TS>
