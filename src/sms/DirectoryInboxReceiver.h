#pragma once

#include "sms/IBankTransactionReceiver.h"

#include <QFileSystemWatcher>
#include <QTimer>

#include <optional>

namespace pf::sms {

/// Reads SMS "envelopes" from a directory. This is the one implementation of the
/// inbox protocol; the desktop and iOS receivers only choose the directory.
///
/// Envelope (UTF-8 JSON, one file per SMS, name `*.json`):
///   { "v": 1, "id": "<uuid>", "channel": "shortcut_intent",
///     "receivedAt": "2026-11-15T10:32:07Z", "sender": "...", "body": "...", "bank": "idbank" }
/// Files being written use a `.tmp` name and are renamed when complete, so a
/// half-written file is never read. Oversized, malformed or unsupported
/// envelopes are handed to the sink with an empty body so they are recorded as
/// rejected instead of silently piling up.
class DirectoryInboxReceiver : public IBankTransactionReceiver
{
    Q_OBJECT

public:
    static constexpr qint64 kMaxFileBytes = 64 * 1024;
    static constexpr int kEnvelopeVersion = 1;

    explicit DirectoryInboxReceiver(QObject *parent = nullptr);

    /// Sets the directory. May be called before start(); an empty path means "unavailable".
    void setInboxDirectory(const QString &path, const QString &unavailableReason = QString());
    QString inboxDirectory() const { return m_directory; }
    /// Watch the directory and poll every `pollMs` (0 disables the timer).
    void setWatching(bool watch, int pollMs = 15000);

    void setSink(Sink sink) override { m_sink = std::move(sink); }
    bool start() override;
    void stop() override;
    int processPending() override;
    ReceiverStatus status() const override;

    /// Parses one envelope file. Exposed for tests. `ok` is false for a bad envelope;
    /// the returned message then has an empty body.
    static IncomingSms readEnvelope(const QString &filePath, bool *ok = nullptr);

private:
    void scheduleProcessing();

    QString m_directory;
    QString m_unavailableReason;
    Sink m_sink;
    bool m_watch = false;
    int m_pollMs = 0;
    bool m_running = false;
    bool m_processing = false;
    QFileSystemWatcher m_watcher;
    QTimer m_debounce;
    QTimer m_poll;
};

} // namespace pf::sms
