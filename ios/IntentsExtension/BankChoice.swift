// NOT compiled or tested in the Linux development sandbox (no Xcode there).
// Build on a Mac with Xcode 26 and verify on a real iPhone - see docs/BUILD_IOS.md.

import AppIntents

/// The bank picker shown in the Shortcuts action. The raw value is the parser id of the C++
/// layer (src/sms/*SmsParser.cpp): `BankSmsParser::id()`.
///
/// Adding a bank = a C++ parser + one case here. "Detect" and "Other" send no hint, so the app
/// identifies the bank from the message text instead.
enum BankChoice: String, AppEnum {
    case automatic
    case idbank
    case ameriabank
    case acba
    case other

    static var typeDisplayRepresentation: TypeDisplayRepresentation = "Bank"

    static var caseDisplayRepresentations: [BankChoice: DisplayRepresentation] = [
        .automatic: "Detect from the message",
        .idbank: "IDBank",
        .ameriabank: "Ameriabank",
        .acba: "ACBA Bank",
        .other: "Other bank",
    ]

    /// Value of the envelope field "bank" ("" = no hint).
    var hint: String {
        switch self {
        case .automatic, .other: return ""
        default: return rawValue
        }
    }
}
