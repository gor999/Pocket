# Setting up automatic bank SMS import (Shortcuts)

> **Status: not verified on a device.** The app side was tested on Linux with
> synthetic messages. The steps below describe Apple's Shortcuts app from Apple's
> documentation and third-party guides; **menu names can differ on iOS 26**, and I
> could not click through them. Section 6 lists what to confirm on your iPhone.

You do this **once per bank**. Afterwards you never copy, paste or import anything.

## What happens

```
Bank SMS arrives -> Shortcuts "Message" automation (Run Immediately)
   -> action "Import Bank SMS" (runs in an App Intents extension, the app is NOT opened)
   -> the text is saved into the app's private shared folder on this iPhone
   -> next time you open Personal Finance, it parses, categorizes and adds the transaction
```

Two facts you should know up front:

* **iOS cannot start the app when an SMS arrives.** The SMS is captured immediately,
  but it appears in your transactions (and in the monthly numbers) when you next open
  the app. Nothing is lost while the app is closed.
* **Nothing leaves your iPhone.** No server, no cloud, no AI service.

## 1. Before you start

1. Install Personal Finance built with the **App Intents extension** (`PF_BUILD_INTENTS_EXTENSION=ON`,
   see `docs/BUILD_IOS.md` section 8). Without it, only the visible fallback (section 5) is available.
2. Open the app once, then **Settings → Bank SMS import**. The "Receiver" line must not show an
   error. (An error about the *App Group* means the extension and the app cannot share storage yet.)
3. Look at one real SMS from your bank and note a word that is always in it (usually the bank
   name). The table gives my starting point; **these words are assumptions, not captured from real
   messages**: check them against yours.

| Bank | "Message Contains" (to check) | Bank to choose in the action |
|---|---|---|
| IDBank | `IDBank` | IDBank |
| Ameriabank | `Ameriabank` | Ameriabank |
| ACBA | `ACBA` | ACBA Bank |
| Another bank | a phrase that is in all its transaction SMS | Detect from the message |

Why the *text* and not the sender: Shortcuts can filter on sender and text, but reports say it cannot
select alphanumeric sender names such as `IDBank` (you can pick contacts and numbers). Leave
**Sender empty** and filter on the text. (Not verified for iOS 26.)

## 2. Create the automation (repeat per bank)

1. Open **Shortcuts → Automation** → **+** (New Automation) → **Message**.
2. **Sender:** leave empty. **Message Contains:** the phrase from the table.
3. Choose **Run Immediately** (not "Run After Confirmation"). If a *Notify When Run* switch is
   shown, turn it off.
4. Tap **Next**, then **New Blank Automation** → **Add Action** and search **Import Bank SMS**
   (it is listed under *Personal Finance*).
5. Fill in the action:
   * **Message** → tap, choose the variable **Shortcut Input** (the message content).
   * **Sender** (optional) → the sender of the Shortcut Input, if offered.
   * **Bank** → the bank from the table (or *Detect from the message*).
6. Tap **Done**. If a setting like *Allow Running When Locked* is offered for the automation,
   leave it on, so that it also works while the phone is locked.

**If "Import Bank SMS" is not in the list of actions:** the extension did not register. Rebuild the
app, reinstall, and if it is still missing use the Xcode fallback in `docs/BUILD_IOS.md` section 8,
or the visible fallback below.

## 3. Test it (without waiting for a real SMS)

1. In Shortcuts create a normal shortcut with two actions: **Text** with the content
   `PF-TEST connection check`, then **Import Bank SMS** (Message = the Text, Bank = Detect).
2. Run it, then open **Personal Finance → Settings → Bank SMS import**.
3. The status should change to **Connection works** and "Last test message" shows the time.
   (A test message never creates a transaction.)
4. Then wait for a real bank SMS, or ask someone to send you a message that contains your
   filter phrase. (Sending yourself an SMS may not trigger the automation; not verified.)
5. After a real SMS: open the app. The status becomes **Active** and the transaction is in the
   list. If it needs a decision it is under **Needs your review**.

## 4. What you will see

* A transaction appears with the amount, merchant, card, date and category. The category is applied
  only when the app is confident; otherwise it is suggested and waits in **Needs your review**.
  "Always use this category for this merchant" teaches the app your own rule.
* iOS may show a banner or ask once whether the automation may run. Whether Message
  automations run without any banner on iOS 26 is **unconfirmed** (section 6).
* Messages that are not transactions (offers, one-time codes) are discarded. One-time codes
  and similar secrets are never stored.
* Messages the app cannot read appear under **Could not import** with the reason. Send me a
  sample (with card numbers removed) and I can add a parser template.

## 5. Visible fallback (works without the extension)

If the silent route does not work for you, make the automation open the app instead:

1. Same automation as above, but the action is **Open URLs**.
2. URL: `personalfinance://import?bank=idbank&body=` followed by the Shortcut Input. Characters
   such as `&`, `#` or `%` in an SMS break a URL unless the text is URL-encoded first; use a
   URL-encode action on the message if your Shortcuts app offers one (action names vary; unverified).
3. The app comes to the foreground and imports the message. **Any app or web page can open such a
   link**, so messages received this way are marked *untrusted* and wait in **Needs your review**
   until you confirm them (Settings → *Trust links from other apps* changes that; leave it off).

## 6. What to confirm on your iPhone 14 Pro (iOS 26.x)

The full checklist is in `docs/IOS_SMS_FEASIBILITY.md` section 7. The short version:

| Question | If the answer is no |
|---|---|
| Is "Import Bank SMS" listed in Shortcuts? | Xcode-target fallback, `docs/BUILD_IOS.md` §8 |
| Does the automation run with no confirmation tap? | Re-check *Run Immediately*; otherwise use section 5 |
| Does it run while the phone is locked? | The SMS is then captured on the next unlock, or not at all; tell me what you see |
| Is a banner shown? | Informational only |
| Does a 2-part (long) SMS arrive complete? | Report it; the parser may need joining logic |
| Is an SMS received before the first unlock after a reboot captured? | Expected to be unreliable |

## 7. Another bank

1. Create the automation (section 2) with that bank's phrase and **Detect from the message**.
2. Messages from an unknown bank that look like transactions show up under **Could not import**.
3. For a permanent fix, a developer adds one C++ parser (`src/sms/<Bank>SmsParser.cpp`,
   about 40 lines: its name, filter words and one layout per language), registers it in
   `ParserRegistry::withBuiltIns()`, and adds one case to `ios/IntentsExtension/BankChoice.swift`.
   The generic reader already handles many layouts at lower confidence in the meantime.

## 8. Privacy

* The text is stored on the iPhone only, in the app's database; card numbers are masked.
* The extension writes a file to the app's shared folder; it logs nothing and has no network code.
* The shared folder is excluded from backups and protected until first unlock.
* You can turn off any bank (Settings → Bank SMS import → Banks) and clear the log.
