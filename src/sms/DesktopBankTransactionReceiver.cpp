#include "sms/DesktopBankTransactionReceiver.h"

namespace pf::sms {

DesktopBankTransactionReceiver::DesktopBankTransactionReceiver(const QString &defaultDirectory, QObject *parent)
    : DirectoryInboxReceiver(parent)
{
    const QByteArray override = qgetenv("PF_SMS_INBOX");
    setInboxDirectory(override.isEmpty() ? defaultDirectory : QString::fromLocal8Bit(override));
    setWatching(true);
}

} // namespace pf::sms
