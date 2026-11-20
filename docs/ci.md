# CI и релизы (GitHub Actions)

Файл: `.github/workflows/ci.yml`.

| Когда | Что запускается |
|-------|-----------------|
| push в `main`, pull request, вручную (Actions → CI → Run workflow) | тесты логики (`plotcore_test`, `historycore_test`) и сборка на Linux, macOS, Windows; на macOS и Windows ещё упаковка |
| тег `v*` (например `v0.1.0`) | то же, затем **черновик** релиза с пакетами (zip) |

Пакеты каждой сборки лежат во вкладке Actions → запуск → Artifacts (14 дней).

## Выпуск релиза

```
git tag v0.1.0
git push origin v0.1.0
```

Через несколько минут в Releases появится черновик с `IWP-macos-v0.1.0.zip` и `IWP-win64-v0.1.0.zip`. Проверьте
пакеты (хотя бы запуск на чистой машине), допишите заметки и нажмите Publish.

## Как устроена сборка

- **Qt** ставит `jurplel/install-qt-action` (aqtinstall), версия в переменной `QT_VERSION` в начале файла
  (сейчас 6.8.*; проект требует 6.5+). Модули: `qtserialport`, `qtshadertools`. Когда нужная вам версия
  (например 6.12) появится у aqtinstall, поменяйте одну строку.
- **QtKeychain** собирается на раннере один раз на версию Qt и кэшируется.
- **Qt WebEngine** на раннерах пока нет, поэтому CI собирает с `-DIWP_CONSOLE_TERMINAL=OFF`: в релизных сборках
  консоль без вкладки «Терминал». Ставить расширение WebEngine можно через `aqt install-qt-official` с учётной
  записью Qt (хранится в секретах репозитория) или собрать из исходников, но это не сделано.
- **Упаковка:** `tools/deploy_macos.sh` и `tools/deploy_windows.ps1`; их можно запускать и локально после сборки Release:

  ```
  tools/deploy_macos.sh build Release dist 0.1.0        # dist/IWP-macos-0.1.0.zip
  pwsh tools/deploy_windows.ps1 -Version 0.1.0          # dist\IWP-win64-0.1.0.zip
  ```

  Что в них важно: `windeployqt`/`macdeployqt` получают не только хост, но и SDK, `iwp_askpass` и **каждый плагин**
  (плагины грузятся на лету, иначе их модули Qt не будут скопированы); на macOS из обычного исполняемого файла с
  `plugins/` собирается `.app` (плагины в `Contents/PlugIns`); в пакет кладутся `LICENSE` и `THIRD_PARTY/`; на
  Windows рядом должен лежать `qt6keychain.dll`, если сборка с QtKeychain.

## Подпись

Сейчас пакеты **не подписаны** по-настоящему: macOS получает ad-hoc подпись (запускается на этом Mac, на чужих
Gatekeeper будет ругаться: «Открыть» через правую кнопку или `xattr -cr IWP.app`), Windows - без подписи
(SmartScreen предупредит). Для настоящей подписи:

- **macOS:** сертификат Developer ID (программа Apple Developer) в секретах репозитория, импорт в временную
  keychain на раннере, `CODESIGN_IDENTITY` для `deploy_macos.sh`, затем нотаризация (`xcrun notarytool`).
- **Windows:** сертификат подписи кода (или Azure Trusted Signing), `signtool sign` над `iwp_host.exe`.

Этих шагов в workflow пока нет.

## Если что-то упало

Первые запуски почти всегда требуют правок: смотрите лог упавшего шага. Типичное: нет модуля Qt (добавить в
`modules:`), `windeployqt`/`macdeployqt` не нашёл Qt (`QT_ROOT_DIR`), нужной версии Qt нет у aqtinstall
(`QT_VERSION`), на macOS плагин не грузится из-за путей библиотек (`otool -L`).
