# Building and deploying to a physical iPhone

> **Status of this document.** The project was developed in a Linux sandbox
> with Qt 6.4.2. There is no macOS, Xcode or iOS SDK there, so **nothing in the
> iOS build path has been executed yet**: not the Xcode generation, not
> `IOSPlatform.mm`, not `cmake/ios/Info.plist.in`, not signing. The desktop
> build, the unit tests and the QML UI *were* run (see README). Treat the steps
> below as a verified-by-reading checklist and expect to fix small things the
> first time you run them on a Mac.

## 1. What you need

| Requirement | Why |
|---|---|
| A Mac with a recent **Xcode** (and its command-line tools) | iOS apps can only be built and signed with Apple's toolchain |
| **Qt 6.8 LTS or newer**, installed with the **iOS** kit (Qt Online Installer or `aqtinstall`) plus the *Qt Quick*, *Qt Quick Controls*, *Qt Shader Tools*, *Qt SQL* modules | The apt Qt 6.4.2 used in the sandbox has no iOS target. `CMakeLists.txt` accepts 6.4+ for desktop, but iOS should use 6.8+ (iOS 16 deployment target) |
| **CMake 3.21+** | iOS + Xcode generator support in Qt's toolchain files |
| An **Apple ID** (free) or an **Apple Developer Program** membership | Needed to sign. A free account works for installing on your own phone, with a 7-day profile expiry and limits on capabilities |
| An iPhone running **iOS 16+**, a USB cable (or Wi‑Fi pairing) | Deployment target |

You do **not** need the iOS Simulator; it is simply not required for the
physical-device flow below. (If you want it, it works with the same Xcode
project, but the Keychain / Face ID behavior differs there.)

## 2. Generate the Xcode project

Qt ships `qt-cmake`, a wrapper that sets the iOS toolchain for you:

```bash
export QT_IOS=~/Qt/6.8.0/ios          # adjust to your install

$QT_IOS/bin/qt-cmake \
    -S . -B build-ios -G Xcode \
    -DPF_DEVELOPMENT_TEAM=ABCDE12345 \
    -DPF_BUNDLE_IDENTIFIER=com.yourname.personalfinance
```

* `PF_DEVELOPMENT_TEAM` is your 10-character **Team ID**
  (developer.apple.com → Membership, or Xcode → Settings → Accounts → select the
  team). Omit it to pick the team inside Xcode instead.
* `PF_BUNDLE_IDENTIFIER` must be globally unique to your team. The default
  (`com.example.personalfinance`) will not sign.
* The iOS configuration automatically disables tests and developer tooling
  (`PF_BUILD_TESTS`, `PF_DEV_TOOLS` are forced off), so the demo-data seeder and
  screenshot driver never ship in a device build.

Then open the project:

```bash
open build-ios/PersonalFinance.xcodeproj
```

## 3. Signing and provisioning

The CMake target requests **automatic signing** (`CODE_SIGN_STYLE=Automatic`).

1. In Xcode select the `PersonalFinance` target → **Signing & Capabilities**.
2. Tick *Automatically manage signing* and choose your **Team** (if you did not
   pass `PF_DEVELOPMENT_TEAM`).
3. Xcode creates a development certificate and provisioning profile the first
   time it can reach your device. No special capabilities are required for the
   base app: it uses the app-private Keychain and Face ID, neither of which
   needs an entitlement for a single-app setup. The optional **Bank SMS import**
   (section 8) adds one: the *App Groups* capability.
4. If Xcode reports *"Failed to register bundle identifier"*, change
   `PF_BUNDLE_IDENTIFIER` to something unique and re-run `qt-cmake`.

## 4. Prepare the iPhone

1. Plug in the phone and tap **Trust This Computer**.
2. **Settings → Privacy & Security → Developer Mode → On** (iOS 16+; the phone
   restarts). The switch only appears after Xcode has tried to talk to the
   device once.
3. After the first install of a *free-account* build: **Settings → General →
   VPN & Device Management →** your Apple ID → **Trust**.

## 5. Build and run on the device

From Xcode: pick your iPhone in the scheme's destination menu and press **Run**.

Or from the command line:

```bash
# list connected devices and note the identifier
xcrun devicectl list devices

cmake --build build-ios --config Release --target PersonalFinance -- \
    -destination 'generic/platform=iOS' -allowProvisioningUpdates

# install and launch (path to the built .app depends on the configuration)
APP=$(find build-ios -name 'PersonalFinance.app' -path '*Release-iphoneos*' | head -1)
xcrun devicectl device install app --device <DEVICE-ID> "$APP"
xcrun devicectl device process launch --device <DEVICE-ID> com.yourname.personalfinance
```

Use `--config Debug` while developing. For App Store Connect / TestFlight you
additionally need *Product → Archive* and an app record; that is out of scope
for the current phase.

## 6. What to check first on the device

These are the places most likely to need a fix because they could not be run in
the sandbox:

* **App starts and the QML loads.** The QML is AOT-compiled (`qmlcachegen`);
  a missing import usually shows up as a blank window. Look at the Xcode
  console.
* **SQLite driver.** `CMakeLists.txt` links `Qt6::QSQLiteDriverPlugin` for
  static builds. If the log shows *"QSqlDatabase: QSQLITE driver not loaded"*,
  the `qt_import_plugins` line needs adjusting for your Qt version.
* **Safe areas** (`src/platform/ios/IOSPlatform.mm`). Notch/home-indicator
  spacing on Home and in the bottom sheets, in both orientations of the keyboard.
* **Keyboard behavior in the bottom sheets** (`Qt.inputMethod.keyboardRectangle`
  is used to lift the sheet). Not testable on desktop.
* **Icons** are drawn with `QtQuick.Shapes`; check they are crisp on your
  device. They are rasterized at 3× through a layer.
* **Category icons are emoji.** They render in color on iOS; in the
  sandbox only a monochrome fallback font was available, so the colored look has
  not been seen.
* **`Info.plist`.** Run `plutil -lint` on the generated plist inside the built
  `.app`. `NSFaceIDUsageDescription` must be present before Face ID is added in
  Phase 4.

## 7. Qt version note

Everything compiled and ran on **Qt 6.4.2**. A few APIs behave differently
between 6.4 and 6.8 (for example `Qt.styleHints.colorScheme` exists from 6.5;
the code falls back to a palette event filter below that). If you hit a
6.8-specific compile error, please report the message; the code is written to
the 6.4 API surface and should be forward compatible, but that is exactly what
has not been exercised.

## 8. Bank SMS import: App Intents extension and App Group

Background and every limitation: [IOS_SMS_FEASIBILITY.md](IOS_SMS_FEASIBILITY.md). User setup in the
Shortcuts app: [SHORTCUTS_SETUP.md](SHORTCUTS_SETUP.md). **None of this could be built or run in the
sandbox.** Treat each step as unverified.

### Option 1: through CMake (preferred)

```bash
qt-cmake -S . -B build-ios -G Xcode \
    -DPF_BUILD_INTENTS_EXTENSION=ON \
    -DPF_BUNDLE_IDENTIFIER=com.yourname.personalfinance \
    -DPF_DEVELOPMENT_TEAM=YOURTEAMID
```

This needs CMake 3.26+ (`XCODE_EMBED_EXTENSIONKIT_EXTENSIONS`) and creates a second target,
`PFIntentsExtension` (Swift, `ios/IntentsExtension/*.swift`), embedded in the app, with the App Group
`group.<bundle identifier>` in both targets' entitlements (`ios/PersonalFinance.entitlements`) and the
`PFAppGroupIdentifier` key in both Info.plists. The app also registers the `personalfinance://` URL scheme.

What can go wrong and what to check:

* The extension target does not build (product type, Info.plist keys): compare with a fresh
  *App Intents Extension* target created in Xcode and copy any difference into `CMakeLists.txt`.
* The action **Import Bank SMS** is missing in Shortcuts even though the build succeeded: Xcode's App
  Intents *metadata extraction* may not have run for a CMake-generated target. Use Option 2.
* *"Provisioning profile doesn't support the App Groups capability"*: see "Apple ID" below.

### Option 2: Xcode target by hand (fallback)

1. Build the app **without** `PF_BUILD_INTENTS_EXTENSION` and open the generated Xcode project.
2. *File → New → Target → App Intents Extension* (iOS). Name it `PFIntentsExtension`; delete the template
   Swift files and add the four files from `ios/IntentsExtension/`. Use `ios/IntentsExtension/Info.plist`
   (or merge its `PFAppGroupIdentifier` key into the generated one).
3. On **both** targets: *Signing & Capabilities → + Capability → App Groups* and add the same group,
   `group.<your bundle identifier>`. In *Build Settings* add the user-defined setting `PF_APP_GROUP_ID`
   with that value for both targets (Info.plist reads it).
4. If you edit the CMake-generated project by hand, remember a CMake re-run regenerates it and drops the
   manual target; keep the manual target in a separate `.xcodeproj` or script it.

### Apple ID, App Groups, and a free Personal Team

App Groups are an Apple Developer capability. Whether a **free** Apple ID (Personal Team) can sign an app
and an extension with a shared App Group is **unclear to me and not verified**; reports differ. If Xcode
refuses, you need a paid Apple Developer Program membership for the silent route. Without an App Group the
app still works: the receiver shows "App Group container not available", and the visible fallback (the
Shortcuts action *Open URLs* → `personalfinance://import?...`, SHORTCUTS_SETUP.md section 5) needs no
App Group and no extension.

### Check on the device

1. Baseline: the app launches with the extension OFF; Settings → *Bank SMS import* opens (status "Not set up yet").
2. With the extension ON: *Import Bank SMS* appears in Shortcuts. Run the test shortcut from
   SHORTCUTS_SETUP.md section 3; the status must become "Connection works".
3. Create a Message automation, receive a real SMS, open the app: the transaction must appear.
4. Then work through the list in IOS_SMS_FEASIBILITY.md section 7 (locked phone, banner, long SMS, reboot).
