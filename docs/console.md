# Console plugin

One byte stream (any `stream` endpoint: UART, TCP, ...), three views, tabs on the device page:

| Tab | What it is |
|-----|------------|
| **Line** | For SCPI-style work: log of received/sent text with time stamps; the input takes one command or a whole block (paste a sequence); history of sent blocks. See below. |
| **Terminal** | A real terminal: every key goes to the device immediately, escape sequences (colours, cursor movement, vim, htop) work. Meant for a Linux console on a serial port. |
| **Hex** | Hex dump of everything received and sent (16 bytes per row + ASCII), and a line to send raw bytes: `48 65 6C 6C 6F`, `0x48, 0x65`, `48:65`. |

All views see the same data; switching tabs loses nothing (the terminal buffers output until it is shown).

## Line view: command blocks and history

* The input is multi-line: paste a sequence of commands, **Enter** sends, **Shift+Enter** starts a new line.
  Blank lines are skipped, every other line goes out with the chosen ending (LF / CR / CRLF), with a pause
  between lines (*No pause* ... 1 s; instruments lose commands that come too fast). *Stop (n)* cancels the rest.
  The sequence is also cancelled when the connection drops.
* *Load file...* puts a text file (up to 1 MiB) into the input; nothing is sent until you press Send.
* **History**: what you send is remembered as one entry per Send (a block stays a block), newest first, the
  last 100 per connection (`prefs.limit` in the JSON file below, 10..10000). The drop-down puts an entry into the
  input (it is not sent), Up/Down in the input walk through the entries like in a shell. `★` pins the entry
  in the input (pinned entries are kept at the top of the drop-down and are never pushed out), `✕` removes it.
  Sending the same text again moves it to the front.
* The history is **per connection** (`host:port`, `user@host:port` or the serial port name; the session
  publishes it as the value `connection`) and survives restarts. It is stored as plain text in
  `<AppConfigLocation>/console_history.json` (macOS `~/Library/Preferences/IWP/...`, Windows
  `%APPDATA%/IWP/...`), together with the ending and the pause: do not put secrets into commands.
  The logic is in `historycore.h` (no Qt), test: `tools/historycore_test.cpp`.

## SSH terminal

Besides UART and TCP the console can use the interface **SSH terminal (system OpenSSH)** (plugin
`endpoint/sshshell`, kind `stream`). It runs `ssh -tt host`, so the remote side gets a pty: shells, `htop`,
`vim`, `tmux` work in the *Terminal* tab. Settings are the same as for the SSH command channel (host, port, user,
key, jump host...) plus `remoteCommand` in JSON (run it instead of the login shell). Passwords, passphrases,
PIN, host key questions and "touch your key" are handled like everywhere else (askpass, OS keychain).

* The window size is not forwarded (there is no local pty): press *Send size (stty)* after resizing.
* `TERM` is `xterm-256color`.
* When you leave the shell (`exit`) the device goes Offline.
* The interface also shows up for other devices with a `stream` role (the telemetry source, for example).

## Terminal

The emulator is [xterm.js](https://github.com/xtermjs/xterm.js) (the one VS Code uses) running in Qt WebEngine;
`plugins/device/console/web/terminal.js` connects it to C++ through QWebChannel (`TerminalBridge`).
Everything is embedded in the plugin as Qt resources, nothing is loaded from the network, the web view is
off-the-record and may only show `qrc:` pages.

* **Size.** A serial line cannot tell the remote shell the window size. After resizing the window press
  *Send size (stty)*: it types `stty rows R cols C` into the shell.
* **Copy / paste.** Buttons in the toolbar; Cmd+C / Cmd+V work on macOS. Ctrl+C always goes to the device
  (that is what a terminal does), so use *Copy* on Windows/Linux.
* A serial port has no flow of "terminal type": set `TERM=xterm-256color` on the device if colours look wrong.

### Building

Terminal mode needs Qt WebEngine and Qt WebChannel. Since Qt 6.12 WebEngine is not part of the Qt release but a
separate **Extension** (version 6.140.x) in the Maintenance Tool (top level "Extensions"; on Windows only the MSVC
kits). If CMake does not find it, add the extension's install prefix (the folder that contains
`lib/cmake/Qt6WebEngineWidgets`) to `CMAKE_PREFIX_PATH`.

Without WebEngine the plugin still builds and has the Line and Hex tabs: CMake prints
`Console: Qt WebEngine not found, terminal mode disabled`. To switch the terminal off on purpose use
`-DIWP_CONSOLE_TERMINAL=OFF`.

The host sets `Qt::AA_ShareOpenGLContexts` at start-up (WebEngine requires it when it is loaded from a plugin).

### Shipping

The binary package must contain Qt WebEngine (`QtWebEngineProcess`, resources, `libexec`): use `macdeployqt` /
`windeployqt` on the host and check that the console plugin's dependencies are copied too. The licences of
xterm.js and Qt are listed in `THIRD_PARTY/`.

### Updating xterm.js

Replace `xterm.js`, `xterm.css`, `addon-fit.js` and `LICENSE` in `plugins/device/console/web/` with the files of
the new `@xterm/xterm` / `@xterm/addon-fit` release (`lib/xterm.js`, `css/xterm.css`, `lib/addon-fit.js`) and
update the date in `THIRD_PARTY/README.md`.
