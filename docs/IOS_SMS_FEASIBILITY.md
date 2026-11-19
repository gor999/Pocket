# iOS 26 feasibility report: automatic bank-SMS import

> **Read this first.** This report was produced in a Linux sandbox with no Mac,
> Xcode, iOS SDK or iPhone. Everything below about iOS is therefore **research
> plus engineering inference, not a measurement**. Each claim carries an
> evidence grade, and nothing is described as "working" unless it was run:
>
> | Grade | Meaning |
> |---|---|
> | **A** | Stated on an Apple support or developer page |
> | **B** | Stated by an Apple engineer (DTS) on the Apple Developer Forums |
> | **C** | Reported by third-party guides or user posts; plausible, not independently verified |
> | **D** | My engineering inference; **no source found, must be tested on a device** |
>
> The device test list is in section 7. Until you have run it on your iPhone 14 Pro,
> treat the iOS side as **unverified**.

## 1. Verdict

* **A fully automatic path is possible, with one structural limitation.**
  After a one-time setup, an incoming bank SMS can reach the app without the
  user copying, pasting or tapping anything, using only documented APIs:
  *Message automation (Run Immediately) → App Intent in an App Intents
  extension → file in an App Group container.*
* **The limitation:** the Qt app is **not running** when the SMS arrives and iOS
  gives it no way to run on SMS arrival. So the SMS is *captured* instantly and
  silently, but *parsed, categorized and added to statistics the next time the
  app is opened or comes to the foreground*. Nothing is lost in between; the
  inbox waits on disk. This is by design of iOS, not of this app.
* **Not possible, and not attempted:** reading the Messages database, any
  background SMS API, private APIs, jailbreak tricks. The app never asks for
  access to Messages.
* **What the user must still do by hand, once:** create the Message automation
  in the Shortcuts app. iOS has no API for an app to create or inspect a user's
  personal automations (grade D for "no API"; Apple documents creating them in
  the Shortcuts app only, grade A).

## 2. Findings per question in the brief

### 2.1 Shortcuts Message automation and its trigger filters

| Finding | Grade | Source |
|---|---|---|
| The Message trigger has exactly two options: **Sender** and **Message Contains**. When several criteria are set, **all** must match. | A | [Apple: Communication triggers](https://support.apple.com/guide/shortcuts/apdd711f9dff/ios) |
| Message Contains is a plain phrase match, **no regular expressions**. Leaving it as a single space matches almost any text. | C | [tinyapps.org](https://tinyapps.org/blog/ios-autoreply-text.html) |
| The Message trigger exists in iOS 26 and behaves the same as in iOS 18. | C | [MacMost (iOS 26 walkthrough)](https://macmost.com/how-to-set-up-your-iphone-to-automatically-respond-to-text-messages.html) |
| Alphanumeric bank sender IDs (for example a bank name shown instead of a number) **cannot be reliably chosen in the Sender field**, which is contact-based. Workaround: leave Sender empty and filter with Message Contains. | C / D | Third-party guides describe the field as contact/number based; verify on your phone |

**Consequence for the design:** the user creates **one automation per bank**,
each filtered by a phrase that appears in that bank's SMS (for example the
bank's name). Sender is left empty. The app cannot tell Shortcuts to be more
selective than that, so the app itself must discard anything that is not a
transaction (OTP codes, promotions). It does, see 5.3.

### 2.2 "Run Immediately" (no confirmation)

| Finding | Grade | Source |
|---|---|---|
| Apple's current page lists **Message** among the personal automations that *can* run automatically, and documents the **Allow Running When Locked** privacy switch. | A | [Apple: Add automations](https://support.apple.com/guide/shortcuts/apdfbdbd7123/) |
| An older (iOS 15-era) revision of Apple's page listed Message among automations that *could not* run automatically. The restriction was lifted from iOS 17 on, when "Run Immediately" was added for almost all triggers. | A / C | [older Apple page](https://support.apple.com/en-kg/guide/shortcuts/apd602971e63/5.0/ios), [Cassinelli](https://matthewcassinelli.com/automations-run-immediately-shortcuts-notifications/), [Finny](https://getfinny.app/blog/shortcut-automation-confirm-send-message-2026) |
| Worked example on iOS 26 of a Message automation that runs without the phone being unlocked (it sends a reply). | C | [MacMost](https://macmost.com/how-to-set-up-your-iphone-to-automatically-respond-to-text-messages.html) |
| In the iOS 17 betas, Apple required automations that run immediately to show a notification ("Notify When Run" could not be switched off). Whether iOS 26 still shows a banner for the Message trigger is **not confirmed**. | C | [Cassinelli](https://matthewcassinelli.com/automations-run-immediately-shortcuts-notifications/) |
| Actions that need to open an app or touch files can fail while the phone is locked. | C | [MacMost (iOS 14)](https://macmost.com/run-personal-automation-shortcuts-automatically-in-ios-14.html) |

**Consequence:** "runs without asking" is documented and widely reported, so the
design relies on it, but a banner such as "Running your automation" may remain
visible. That is iOS policy and cannot be removed by the app. Because file and
open-app actions are the ones that can fail on a locked phone, the design uses
**neither**: the single action in the automation is our App Intent, which does
the file write itself inside the extension.

### 2.3 Does the Shortcut receive the complete SMS body?

* The automation passes a *Message* object as **Shortcut Input**. People use its
  **Content** and **Sender** properties in later actions. (grade C: the same
  guides that build auto-replies and expense trackers from SMS.)
* **Unknown, test on device:** whether Content is ever truncated for long or
  multi-part SMS; whether a received-date property exists (the design does not
  depend on one: the extension stamps the time it ran). (grade D)

### 2.4 Can a Shortcut call a third-party app without interaction?

* **Yes, through App Intents.** An intent whose `openAppWhenRun` is `false`
  runs its `perform()` in the background. (grade C: several developer guides.)
* **In an App Intents *extension*, `openAppWhenRun` cannot be `true`**, because
  the extension point exists for intents that complete without a visible app
  launch. (grade B: Apple DTS engineer, [forum thread 770421](https://developer.apple.com/forums/thread/770421).)
* **An intent compiled into the *app itself* launches the app process in the
  background without a UI scene**; Apple's guidance is that the app must cope
  with a background launch with no scene. (grade B: DTS,
  [forum thread 769924](https://developer.apple.com/forums/thread/769924).)
  Qt's iOS startup assumes a normal UI launch. Whether `QGuiApplication` and a
  QML engine survive a scene-less background launch is **unknown** (grade D),
  and a crash there would happen on every SMS. **So the design does not put the
  intent in the Qt app.**
* The silent alternatives that do *not* exist: opening the app's UI silently
  (an "Open URL"/"Open App" action brings the app to the foreground and is
  visible; reported to fail on a locked phone, grade C).

### 2.5 App Intents and Qt

* App Intents are declared in **Swift** (protocol conformances and macros) and
  Xcode extracts their metadata at build time. They cannot be expressed in
  C++, Objective-C++ or QML, and Qt has no module for them (grade D: I found no
  Qt documentation for App Intents). **App Intents do not work with a Qt
  application "automatically".**
* So the iOS integration layer is a **small pure-Swift App Intents extension**
  that contains no Qt. It only writes a JSON file. All logic stays in C++.
* The extension and the app share files through an **App Group** container.
* **Build risk (grade D, the largest single risk in this report):** the
  extension is added to the CMake-generated Xcode project as an ExtensionKit
  target (`XCODE_PRODUCT_TYPE com.apple.product-type.extensionkit-extension`,
  embedded with `XCODE_EMBED_EXTENSIONKIT_EXTENSIONS`, CMake 3.26+). I found
  the CMake properties documented, but **I could not verify that Xcode runs its
  App Intents metadata-extraction step for a CMake-generated target.** If it does
  not, the Shortcuts action will not appear. The CMake option
  `PF_BUILD_INTENTS_EXTENSION` is therefore **OFF by default**, and
  `docs/BUILD_IOS.md` gives the fallback: add the extension as a normal Xcode
  target and drop the same Swift files into it.
* **App Groups and a free Apple ID:** whether a free "Personal Team" may use
  App Groups is reported inconsistently (grade C/D). A paid developer account
  removes the doubt. If App Groups are unavailable, Tier B (below) still works.

### 2.6 Background execution limits

* The extension has a short, system-controlled time budget (commonly cited as
  about 30 seconds, grade C; I found no number on an Apple page). The extension
  therefore does **one small thing**: validate, write one file, return.
* The Qt app gets **no** background time from an SMS. It processes the inbox when
  it launches or returns to the foreground.
* After a reboot and before the first unlock, protected files may be
  unwritable. The inbox uses `completeUntilFirstUserAuthentication`. A message
  arriving in that window may fail to be captured (grade D).

### 2.7 URL schemes / Universal Links

* A custom URL scheme (`personalfinance://import?...`) invoked with Shortcuts'
  **Open URL** action works, but it **brings the app to the foreground**: visible
  and not silent; reported to fail while locked. (grade C)
* Any other app or web page can also open that URL, so URL-delivered SMS are
  **untrusted** and always routed to "needs review" (section 5.4).
* Universal Links need a web domain; the app has none and never contacts a
  server, so they are not used.

### 2.8 Other official mechanisms considered and rejected

| Mechanism | Why it does not fit |
|---|---|
| **SMS/MMS Message Filter extension** (IdentityLookup `ILMessageFilterExtension`) | Receives sender and body, but **only for SMS/MMS from unknown senders, never iMessage**, is meant for spam filtering, **cannot write to shared containers and cannot use the network**. A.  [Apple](https://developer.apple.com/documentation/identitylookup/sms-and-mms-message-filtering), [WWDC17-249](https://developer.apple.com/videos/play/wwdc2017/249) |
| **Reading `sms.db`, Messages notifications, accessibility scraping** | Outside the sandbox; private or undocumented. Never to be used. |
| **Wallet "Transaction" automation trigger** (iOS 17+) | Officially passes card, merchant and amount as structured input, *without* any parsing. But it fires only for **Apple Pay** taps, not transfers, salary, ATM or online payments, and I cannot confirm that Armenian cards are available in Wallet. Complementary at best; not built here. C |
| **Bank APIs / open banking** | Out of scope and conflicts with the offline-only requirement. |

## 3. Classification

| Capability | Officially supported | Needs user interaction | Technically impossible / not provided |
|---|:--:|:--:|:--:|
| Trigger on incoming SMS by phrase / sender | ✔ (A) | | |
| Create the automation | | ✔ once, in Shortcuts (A) | App cannot create or inspect it (D) |
| Run without confirmation | ✔ documented (A); on-device check pending | | |
| Hand the SMS text to a third-party action | ✔ App Intent (B/C) | | |
| Do it without opening the app | ✔ extension, `openAppWhenRun=false` (B) | | |
| Intent inside the Qt app process | ? background launch of Qt untested (D) | | |
| Parse / categorize at the instant the SMS arrives | | | ✘ Qt app not running; extension is Swift-only by design |
| Parse / categorize on next app open | ✔ | | |
| Silent *foreground* hand-off (URL scheme) | | ✔ app becomes visible (C) | |
| Read the Messages database | | | ✘ sandbox; never attempted |
| Choose an alphanumeric sender in the trigger | | | ✘ reported unsupported (C); use Message Contains |
| Know whether the automation exists | | | ✘ no API (D); the app infers it from received messages |

## 4. Chosen design (three tiers, best first)

```
Bank SMS ─► Message automation ─► "Import Bank SMS" action ─► JSON file ─► Qt app ─► SQLite
 (iOS)      Run Immediately         App Intents extension      App Group     on open     stats update
            Message Contains        (Swift, no Qt, <1 s)       inbox/        parse C++
                                                                              categorize
                                                                              dedupe
```

* **Tier A, silent (primary).** Exactly the diagram above. Needs
  `PF_BUILD_INTENTS_EXTENSION=ON`, an App Group, and the one-time automation.
* **Tier B, visible fallback.** The automation uses **Open URL**
  `personalfinance://import?…`. The app opens, ingests, shows the result. Works
  without an extension or App Group. Not silent; marked untrusted.
* **Tier C, documentation only.** Shortcuts saves the text to a file in Files and
  the app imports it through a user-granted folder. Needs more than one tap
  and is not implemented.

The diagram of the whole system, including the desktop receiver used for
development, is `docs/sms_architecture.svg` (source: `docs/sms_architecture.dot`).

## 5. How the app handles what arrives

### 5.1 Receiver abstraction
`IBankTransactionReceiver` delivers *incoming SMS envelopes*. One portable class
(`DirectoryInboxReceiver`) reads envelope files from a directory and is fully
unit-tested on Linux. `DesktopBankTransactionReceiver` points it at a folder
and watches it; `IOSBankTransactionReceiver` (Objective-C++, iOS only) points it
at the App Group container.

### 5.2 Envelope (extension → app)
A small JSON file, version 1: `v`, `id` (UUID, makes processing idempotent),
`channel`, `receivedAt` (ISO 8601, set when the extension ran), `sender`, `body`.
Written atomically with file protection. Deleted by the app **only after** the
database write succeeded, so a crash never loses a message.

### 5.3 Non-transaction and secret-bearing messages are never stored
Because the trigger is only a phrase, one-time passcodes and promotions will
arrive too. Messages that look like OTP / 3-D Secure / PIN / CVV / password
messages are **discarded before anything is written to the database**; only an
"ignored" counter row without the text remains. Card-number-like digit runs and
CVV/PIN values are **masked in anything that is stored** (last four digits only).

### 5.4 Review instead of silent guessing
A transaction is stored but flagged **needs review** when: the category
confidence is below the threshold (the suggestion is kept, not applied), the
parse confidence is low, the amount format is ambiguous, a near-duplicate exists,
or the message came through the untrusted URL channel. Review is resolved in
the UI; "always categorize this merchant like this" creates a user rule.

### 5.5 Duplicates
Four layers, checked in this order:

1. **Envelope id.** The same envelope delivered twice (a crash before the file was
   removed, a re-ingest) is recognised by its UUID and changes nothing.
2. **Same text within 120 seconds** (two automations matched, or a double trigger):
   recorded as a duplicate, no second transaction.
3. **Exact transaction fingerprint** against *all* existing transactions, including
   manual ones: same account, minute, amount, type and normalized merchant. Recorded
   as a duplicate and not added, so the same purchase described by two different SMS
   texts, or typed in by hand first, is never counted twice.
4. **Near match:** same account, type and amount within 10 minutes but a different
   minute or merchant spelling. The transaction **is** added but flagged
   "may duplicate another transaction" for one-tap review (keep or remove).

Known trade-off: two genuinely separate purchases of the same amount at the same
merchant in the same minute look identical and layer 3 skips the second one. It stays
visible in the import history as "duplicate, not added".

### 5.6 Privacy
All processing and storage are on the device. No SMS text or financial data is
sent anywhere; there is no network code in the import path and no external AI.
Only the last four card digits are stored. The extension logs nothing.

## 6. Remaining limitations (honest list)

1. **Deferred processing.** The transaction appears in the app the next time it is
   opened, not the instant the SMS arrives. (Structural.)
2. **One-time manual setup per bank**, done in Shortcuts. No API to automate or verify it.
3. **A banner may be shown** by iOS when the automation runs (unconfirmed for iOS 26).
4. **Filtering is by phrase**, so non-transaction SMS reach the app; they are discarded there.
5. **Extension build under CMake is unverified**, App Intents metadata extraction included; a documented Xcode-target fallback exists.
6. **App Groups may need a paid developer account** (unclear for a free Personal Team).
7. **Bank SMS templates are synthetic.** I found no real IDBank, Ameriabank or ACBA message formats. The parsers have bank-specific templates *and* a bank-agnostic extractor, and the tests use invented examples that are labelled as such. Real messages from your phone must be added to the test data before the parsers can be called reliable. See `tests/sms_samples/README.md`.
8. **Armenian-language SMS depend on the same caveat**; hy/ru/en keyword lists are generic, not captured from banks.
9. **Messages before the first unlock after a reboot** may not be captured (D).
10. **Currency balances and "available balance" semantics** vary by bank (it may include credit limits or holds). It is shown as reported, not reconciled.
11. **Multi-part / long SMS** behaviour of Shortcuts is untested.
12. **Notifications on import** (a local notification from the extension) are not implemented; they belong with the Phase 4 notification work.
13. **Budgets.** There is no budget service yet (Phase 3, schema only). Imported transactions already feed the monthly totals, which are computed from the transactions table, so budgets will include them automatically when they exist; nothing about budgets was tested.
14. **Noisy errors list.** A message that contains an operation word and a number but no amount in a supported currency (for example a payment reminder with a phone number) is listed under "Could not import" instead of being silently dropped. That is deliberate: a real transaction in an unknown format must be visible. You can dismiss entries.
15. **Same-minute identical purchases** are collapsed by the exact-fingerprint rule (section 5.5).
16. **URL import (Tier B)** relies on Qt delivering `personalfinance://` links as `QFileOpenEvent` on iOS 26. Untested.
17. **Swift code** has never been compiled (no Swift toolchain in the sandbox). Expect small compile fixes on the first build.

## 7. What was tested, and what you must test on the iPhone

**Tested here (Linux, desktop build):** the parsers, sanitizer, merchant
normalizer, categorizer, duplicate detection, import pipeline, inbox reader,
database migration, and the QML screens with synthetic data.

**Not tested (cannot be tested without a Mac and an iPhone):** the Swift
extension, its CMake/Xcode integration, App Group access, the Obj-C++ receiver,
the URL handler on iOS, and the Shortcuts automation itself.

Device checklist (iPhone 14 Pro, iOS 26.x), in order. Stop at the first failure
and use the fallback in `docs/SHORTCUTS_SETUP.md`:

1. App builds and launches with `PF_BUILD_INTENTS_EXTENSION=OFF` (baseline).
2. Rebuild with it ON. In Shortcuts, search the actions list for *Import Bank SMS*. **Not visible → metadata extraction did not run; use the Xcode-target fallback.**
3. Run the action manually with the text `PF-TEST` (see `docs/SHORTCUTS_SETUP.md`, section 3). Open the app: **Settings → Bank SMS import** should show "Connection works" and a last-test time.
4. Create the Message automation (guide), choose *Run Immediately*. Send yourself a real or test SMS containing the filter phrase. Lock the phone first; then check the app.
5. Note whether iOS shows a banner, and whether the action ran with the phone locked.
6. Send a long (2-part) SMS and compare the stored text with the original.
7. Reboot, do not unlock, receive an SMS, then unlock and check.
8. Enable Tier B once, to confirm the URL handler.

Report the results and the parsers/limitations table above can be updated from facts.

## 8. Sources

* Apple, [Communication triggers in Shortcuts](https://support.apple.com/guide/shortcuts/apdd711f9dff/ios)
* Apple, [Add automations to Shortcuts](https://support.apple.com/guide/shortcuts/apdfbdbd7123/)
* Apple, [Enable or disable a personal automation](https://support.apple.com/en-kg/guide/shortcuts/apd602971e63/5.0/ios) (older revision)
* Apple, [SMS and MMS message filtering](https://developer.apple.com/documentation/identitylookup/sms-and-mms-message-filtering); WWDC17 session [249](https://developer.apple.com/videos/play/wwdc2017/249)
* Apple Developer Forums (DTS), threads [770421](https://developer.apple.com/forums/thread/770421) and [769924](https://developer.apple.com/forums/thread/769924)
* [MacMost: automatic replies to text messages (iOS 26)](https://macmost.com/how-to-set-up-your-iphone-to-automatically-respond-to-text-messages.html); [MacMost: automations without asking](https://macmost.com/run-personal-automation-shortcuts-automatically-in-ios-14.html)
* [tinyapps.org: auto-reply via Shortcuts](https://tinyapps.org/blog/ios-autoreply-text.html)
* [Matthew Cassinelli: Automations run immediately](https://matthewcassinelli.com/automations-run-immediately-shortcuts-notifications/)
* [Finny: why a message automation can still ask](https://getfinny.app/blog/shortcut-automation-confirm-send-message-2026)
* [Cassinelli: iOS 17 Transaction trigger](https://matthewcassinelli.com/?p=26581)
