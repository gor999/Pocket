// NOT compiled or tested in the Linux development sandbox (no Xcode there).

import Foundation

/// Writes one SMS "envelope" into the shared App Group container where the Qt app picks it up.
/// The format is defined by src/sms/DirectoryInboxReceiver.h (envelope version 1).
///
/// This runs inside the extension process, possibly while the iPhone is locked and the app is not
/// running. Rules:
///  * the message text is never logged, printed or sent anywhere;
///  * the file is written as `<uuid>.json.tmp` and renamed, so the app never reads half a file;
///  * file protection is "complete until first user authentication" so that the write also works
///    while the phone is locked (after the first unlock since boot).
enum InboxWriter {
    enum Failure: Error, CustomLocalizedStringResourceConvertible {
        case appGroupMissing
        case inboxFull
        case writeFailed

        var localizedStringResource: LocalizedStringResource {
            switch self {
            case .appGroupMissing:
                return "Personal Finance has no shared storage yet. Open the app once and check the App Groups setting."
            case .inboxFull:
                return "Too many messages are waiting. Open Personal Finance to add them."
            case .writeFailed:
                return "The message could not be handed over to Personal Finance."
            }
        }
    }

    static let envelopeVersion = 1
    static let maxPendingFiles = 500
    /// The app rejects bodies over 2000 characters; this only protects the disk from absurd input.
    static let maxBodyBytes = 32 * 1024

    static func appGroupIdentifier() -> String? {
        Bundle.main.object(forInfoDictionaryKey: "PFAppGroupIdentifier") as? String
    }

    static func inboxDirectory() throws -> URL {
        guard let group = appGroupIdentifier(), !group.isEmpty,
              let container = FileManager.default.containerURL(forSecurityApplicationGroupIdentifier: group)
        else { throw Failure.appGroupMissing }

        let inbox = container.appendingPathComponent("inbox", isDirectory: true)
        try? FileManager.default.createDirectory(
            at: inbox, withIntermediateDirectories: true,
            attributes: [.protectionKey: FileProtectionType.completeUntilFirstUserAuthentication])
        return inbox
    }

    static func write(body: String, sender: String?, bankHint: String) throws {
        let inbox = try inboxDirectory()

        let pending = (try? FileManager.default.contentsOfDirectory(atPath: inbox.path)) ?? []
        if pending.filter({ $0.hasSuffix(".json") }).count >= maxPendingFiles { throw Failure.inboxFull }

        var text = body
        while text.utf8.count > maxBodyBytes { text.removeLast(max(1, text.count / 8)) }

        let id = UUID().uuidString.lowercased()
        let envelope: [String: Any] = [
            "v": envelopeVersion,
            "id": id,
            "channel": "shortcut_intent",
            "receivedAt": ISO8601DateFormatter().string(from: Date()), // UTC, e.g. 2026-11-15T10:32:07Z
            "sender": sender ?? "",
            "body": text,
            "bank": bankHint,
        ]
        guard let data = try? JSONSerialization.data(withJSONObject: envelope, options: []) else {
            throw Failure.writeFailed
        }

        let temporary = inbox.appendingPathComponent("\(id).json.tmp")
        let final = inbox.appendingPathComponent("\(id).json")
        do {
            try data.write(to: temporary, options: [.atomic, .completeFileProtectionUntilFirstUserAuthentication])
            try FileManager.default.moveItem(at: temporary, to: final)
        } catch {
            try? FileManager.default.removeItem(at: temporary)
            throw Failure.writeFailed
        }
    }
}
