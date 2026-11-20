// Glue between xterm.js and the Qt side (TerminalBridge, registered as "bridge" on the web channel).
// Device output arrives as base64 bytes, key presses go back through sendText/sendBinary.
(function () {
    'use strict';

    const term = new Terminal({
        cursorBlink: true,
        scrollback: 10000,
        fontFamily: 'Menlo, Consolas, "DejaVu Sans Mono", monospace',
        fontSize: 13,
        convertEol: false,          // a real terminal: the device sends CR LF itself
        theme: { background: '#1e1e1e' }
    });
    const fit = new FitAddon.FitAddon();
    term.loadAddon(fit);
    term.open(document.getElementById('terminal'));

    function doFit() {
        try { fit.fit(); } catch (e) { /* container not laid out yet */ }
    }
    doFit();
    window.addEventListener('resize', doFit);
    // The container size or the font metrics may change after start-up (layout, font loading):
    // refit whenever that happens, otherwise the column count is wrong and long lines are clipped.
    if (window.ResizeObserver)
        new ResizeObserver(doFit).observe(document.getElementById('terminal'));
    if (document.fonts && document.fonts.ready)
        document.fonts.ready.then(doFit);
    setTimeout(doFit, 100);
    setTimeout(doFit, 500);

    // Called by the Qt toolbar.
    window.iwp = {
        focus: function () { term.focus(); },
        clear: function () { term.clear(); },
        reset: function () { term.reset(); },
        selection: function () { return term.getSelection(); },
        paste: function (text) { term.paste(text); }
    };

    new QWebChannel(qt.webChannelTransport, function (channel) {
        const bridge = channel.objects.bridge;

        bridge.output.connect(function (b64) {
            const bin = atob(b64);
            const bytes = new Uint8Array(bin.length);
            for (let i = 0; i < bin.length; i++) bytes[i] = bin.charCodeAt(i);
            term.write(bytes);
        });
        term.onData(function (data) { bridge.sendText(data); });
        term.onBinary(function (data) { bridge.sendBinary(btoa(data)); });
        term.onResize(function (size) { bridge.resized(size.cols, size.rows); });

        doFit();
        bridge.resized(term.cols, term.rows);
        bridge.ready();
        term.focus();
    });
})();
