// NOT compiled or tested in the Linux development sandbox (no Xcode there).

import AppIntents

/// The "Import Bank SMS" action for a Shortcuts *Message* automation.
///
/// `openAppWhenRun = false` is the point of this design: the action runs in this extension without
/// launching the app (the Qt app is not running when an SMS arrives, and Qt cannot host App Intents).
/// The action only drops the text into the App Group inbox; parsing, categorizing and saving
/// happen in the app (C++) the next time it is opened.
struct ImportBankSMSIntent: AppIntent {
    static var title: LocalizedStringResource = "Import Bank SMS"
    static var description = IntentDescription(
        "Hands a bank SMS to Personal Finance on this iPhone. Nothing leaves the device.",
        categoryName: "Personal Finance")

    /// Must stay false: otherwise iOS would bring the app to the foreground for every SMS.
    static var openAppWhenRun: Bool = false

    @Parameter(title: "Message", description: "The text of the SMS (use Shortcut Input)")
    var message: String

    @Parameter(title: "Sender", description: "Optional. Used only to help identify the bank")
    var sender: String?

    @Parameter(title: "Bank", default: .automatic)
    var bank: BankChoice

    static var parameterSummary: some ParameterSummary {
        Summary("Import \(\.$message) from \(\.$bank)") {
            \.$sender
        }
    }

    func perform() async throws -> some IntentResult {
        try InboxWriter.write(body: message, sender: sender, bankHint: bank.hint)
        return .result()
    }
}
