# Third-party material

The project itself is under the Mozilla Public License 2.0 (`LICENSE` in the repository root). The files listed
below are not covered by it, they keep their own licences.

Everything under `host/resources/` and `plugins/*/*/resources/` (icons) is drawn for this project.

If you add icons, fonts or other assets from elsewhere, keep their licence text here
(`THIRD_PARTY/<name>-LICENSE.txt`) and list them below. It has to ship with the product.
Good sources with permissive licences: Lucide (ISC), Phosphor (MIT), Material Symbols (Apache-2.0).

| Component | Where | Source | Licence |
|-----------|-------|--------|---------|
| xterm.js (`xterm.js`, `xterm.css`) and its fit addon (`addon-fit.js`), npm `@xterm/xterm` / `@xterm/addon-fit`, fetched 2026-11-20 | `plugins/device/console/web/` | https://github.com/xtermjs/xterm.js | MIT, `THIRD_PARTY/xterm.js-LICENSE.txt` (a copy also sits next to the files) |
| Qt (incl. Qt WebEngine / Chromium, used by the console terminal) | linked, not vendored | https://www.qt.io | LGPLv3 / GPL, see Qt's own notices; ship Qt as shared libraries |

Vendored code (copied into the repository, not downloaded at build time) is listed in the table with its
licence file. The binary packages must contain this directory.
