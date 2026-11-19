#pragma once

#include "sms/SmsTypes.h"

#include <QObject>
#include <QString>

#include <functional>

namespace pf::sms {

struct ReceiverStatus
{
    bool available = false; // can messages reach the app through this receiver right now?
    QString location; // where it listens (a path); shown in the setup screen
    int pending = 0; // files waiting to be processed
    QString detail; // human-readable explanation when !available
};

/// Entry point for bank SMS into the C++ layer. Implementations decide *how*
/// messages arrive (a watched folder on the desktop, the App Group inbox on
/// iOS); everything after that - parsing, categorizing, storing - is shared.
///
/// Delivery contract: a message is handed to the sink; the receiver removes
/// its source (the envelope file) only if the sink returns true ("durably
/// handled"). A crash or a storage error therefore never loses an SMS.
class IBankTransactionReceiver : public QObject
{
    Q_OBJECT

public:
    using Sink = std::function<bool(const IncomingSms &)>;

    explicit IBankTransactionReceiver(QObject *parent = nullptr) : QObject(parent) {}

    virtual QString id() const = 0; // "desktop-folder", "ios-app-group"
    virtual QString displayName() const = 0;

    virtual void setSink(Sink sink) = 0;
    /// Starts listening (creates the inbox if needed). Safe to call twice.
    virtual bool start() = 0;
    virtual void stop() = 0;
    /// Ingests everything that is waiting right now. Returns how many messages
    /// the sink accepted.
    virtual int processPending() = 0;
    virtual ReceiverStatus status() const = 0;

signals:
    /// Emitted after processPending() delivered at least one message.
    void messagesProcessed(int count);
};

} // namespace pf::sms
