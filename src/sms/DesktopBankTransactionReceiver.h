#pragma once

#include "sms/DirectoryInboxReceiver.h"

namespace pf::sms {

/// Development / test receiver: watches a folder on the computer. Drop an
/// envelope file in it (see DirectoryInboxReceiver) to simulate what the
/// iPhone's Shortcuts action writes. Compiled on every platform.
class DesktopBankTransactionReceiver : public DirectoryInboxReceiver
{
    Q_OBJECT

public:
    /// `defaultDirectory` is used unless the PF_SMS_INBOX environment variable names another folder.
    explicit DesktopBankTransactionReceiver(const QString &defaultDirectory, QObject *parent = nullptr);

    QString id() const override { return QStringLiteral("desktop-folder"); }
    QString displayName() const override { return tr("Folder on this computer (development)"); }
};

} // namespace pf::sms
