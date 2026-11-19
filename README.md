# Personal Finance Tracker

Offline personal finance app for iPhone: Qt 6, C++20, QML, SQLite, CMake.
All data stays on the device. See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)
for design and [docs/BUILD_IOS.md](docs/BUILD_IOS.md) for the iPhone build.

## Status

**Phase 1 done and the bank-SMS import implemented, both desktop-verified only.**
Not yet built for iOS; the iOS half of the SMS import (Swift extension, App Group,
Shortcuts automation) is **written but untested** - it needs a Mac, Xcode and your iPhone.
All bank SMS layouts used in tests are **synthetic** (no real IDBank/Ameriabank/ACBA samples were available).

Working: accounts (multi-currency, card network, last four only), transaction
create/edit/delete with duplicate warning, search and filters (Armenian, Russian
and English text), Dashboard with hero balance and monthly income/expenses,
light/dark, bottom sheets, wheel date picker, settings (theme, language
formatting, default currency).

Bank SMS import (see [docs/IOS_SMS_FEASIBILITY.md](docs/IOS_SMS_FEASIBILITY.md) for what iOS
allows and [docs/SHORTCUTS_SETUP.md](docs/SHORTCUTS_SETUP.md) for the one-time setup): parsers
for IDBank, Ameriabank and ACBA (Armenian, Russian, English) with a generic fallback,
on-device auto-categorization with merchant rules, duplicate detection, a review list, import
history and error log, and a receiver that reads messages handed over by a Shortcuts automation.

Not yet: CSV import; budgets and charts (Phase 3); Face ID, Keychain, notifications,
document picker, full translations (Phase 4).

## Build and run on desktop (Linux/macOS)

Requires Qt 6.4+ with Quick, Quick Controls 2, Sql, and CMake 3.21+.

```bash
cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure   # 5 suites, all passing
./build/PersonalFinance
```

Developer options (desktop only, forced off for iOS):

| Variable | Effect |
|---|---|
| `PF_SEED_DEMO=1` | seed an empty database with demo accounts, ~75 days of transactions and a few synthetic bank SMS |
| `PF_SMS_INBOX=/path` | folder watched for SMS envelope files (default: `sms_inbox/` next to the database) |
| `PF_DB_PATH=/path/x.sqlite` | use a specific database file |
| `PF_SIMULATE_SAFE_AREA=1` | emulate an iPhone notch/home indicator |
| `PF_SCREENSHOT=/tmp/shots/run PF_STEPS="tab:1,newTransaction,..."` | headless screenshot driver |

Headless screenshots with a real GPU-style render path (the Qt Quick *software*
backend does not draw `Shape` icons):

```bash
QT_QPA_PLATFORM=xcb LIBGL_ALWAYS_SOFTWARE=1 QSG_RHI_BACKEND=opengl QT_SCALE_FACTOR=2 \
PF_DB_PATH=/tmp/demo.sqlite PF_SEED_DEMO=1 PF_SIMULATE_SAFE_AREA=1 \
PF_SCREENSHOT=/tmp/shots/run PF_STEPS="light,tab:0,tab:1,newTransaction,closeSheets,dark,tab:2" \
xvfb-run -a -s "-screen 0 1400x1800x24" ./build/PersonalFinance
```

Steps: `size:390x2400` (taller window), `tab:N` (5 = Settings, 6 = Bank SMS import), `light|dark|system`, `search:text`, `filter:all|income|expense`,
`editTransaction:i`, `editAccount:i`, `wait`, or any zero‑argument function on
the main window (`newTransaction`, `newAccount`, `openDatePicker`,
`closeSheets`, …).

## Project layout

```
CMakeLists.txt  main.cpp
src/core        Money, Currency, Result, settings, composition root
src/models      value types and QAbstractListModel subclasses
src/database    connections, migrations, repositories
src/services    validation and use-cases (incl. SmsImportService)
src/sms         bank-SMS receivers, sanitizer, parsers, registry
src/categorization  merchant normalizer, starter rules, categorizer
ios/            Swift App Intents extension, entitlements
src/platform    PlatformServices (desktop stub, iOS Objective-C++)
qml/            Main, pages, components, Theme singleton
tests/          QtTest suites
cmake/ios       Info.plist template
docs/           architecture, iOS build guide, SMS feasibility report and Shortcuts guide
```

## Privacy

No network access, no accounts, no analytics. Full card numbers, CVV codes, PINs
and banking passwords are never stored; only the last four digits of a card may
be saved, and the schema enforces that.

## Trying the SMS import on the desktop

Drop an envelope file into the inbox folder (shown in Settings → Bank SMS import) and the app
imports it, exactly as the iPhone extension would:

```bash
mkdir -p ~/.local/share/PersonalFinance/PersonalFinance/sms_inbox
cat > /tmp/x.json <<'JSON'
{"v":1,"id":"demo-1","channel":"shortcut_intent","receivedAt":"2026-11-15T10:32:07Z",
 "sender":"IDBank","bank":"idbank",
 "body":"IDBank: Purchase 12,500.00 AMD, SAS SUPERMARKET YEREVAN, Card *1234, 15.11.2026 14:32"}
JSON
mv /tmp/x.json ~/.local/share/PersonalFinance/PersonalFinance/sms_inbox/x.json   # rename = atomic hand-over
```

(That is the default folder on Linux, next to the database; set `PF_SMS_INBOX` to use another. Verified by running the app and dropping the file while it was open.)
