#pragma once

#include "sms/DirectoryInboxReceiver.h"

namespace pf::sms {

/// iOS receiver: reads the SMS envelopes that the "Import Bank SMS" App Intent
/// (ios/IntentsExtension) writes into the shared App Group container.
///
/// Why a folder and not a callback: when a bank SMS arrives this app is usually not
/// running, and the Shortcuts action runs in a separate extension process that has no
/// access to this app's memory or to Qt. The extension drops one JSON file per SMS into
/// `<App Group container>/inbox/`; this class drains that folder whenever the app runs.
///
/// Objective-C++ (IOSBankTransactionReceiver.mm). NOT compiled or tested in the Linux
/// sandbox: build and verify on a Mac and an iPhone.
class IOSBankTransactionReceiver final : public DirectoryInboxReceiver
{
    Q_OBJECT

public:
    explicit IOSBankTransactionReceiver(QObject *parent = nullptr);

    QString id() const override { return QStringLiteral("ios-app-group"); }
    QString displayName() const override { return tr("Shortcuts (App Group inbox)"); }
};

} // namespace pf::sms
