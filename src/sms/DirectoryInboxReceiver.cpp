#include "sms/DirectoryInboxReceiver.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>

namespace pf::sms {

namespace {
Q_LOGGING_CATEGORY(lcInbox, "pf.sms.inbox")
}

DirectoryInboxReceiver::DirectoryInboxReceiver(QObject *parent)
    : IBankTransactionReceiver(parent)
{
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(300);
    connect(&m_debounce, &QTimer::timeout, this, [this] { processPending(); });
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, [this] { scheduleProcessing(); });
    connect(&m_poll, &QTimer::timeout, this, [this] { processPending(); });
}

void DirectoryInboxReceiver::setInboxDirectory(const QString &path, const QString &unavailableReason)
{
    const bool wasRunning = m_running;
    if (wasRunning)
        stop();
    m_directory = path;
    m_unavailableReason = unavailableReason;
    if (wasRunning)
        start();
}

void DirectoryInboxReceiver::setWatching(bool watch, int pollMs)
{
    m_watch = watch;
    m_pollMs = pollMs;
}

bool DirectoryInboxReceiver::start()
{
    if (m_directory.isEmpty())
        return false;
    if (!QDir().mkpath(m_directory)) {
        m_unavailableReason = tr("Cannot create the inbox folder.");
        return false;
    }
    m_running = true;
    if (m_watch) {
        if (!m_watcher.directories().contains(m_directory))
            m_watcher.addPath(m_directory);
        if (m_pollMs > 0) {
            m_poll.setInterval(m_pollMs);
            m_poll.start();
        }
    }
    return true;
}

void DirectoryInboxReceiver::stop()
{
    m_running = false;
    m_poll.stop();
    m_debounce.stop();
    if (!m_watcher.directories().isEmpty())
        m_watcher.removePaths(m_watcher.directories());
}

void DirectoryInboxReceiver::scheduleProcessing()
{
    if (m_running)
        m_debounce.start(); // files arrive in bursts; coalesce
}

IncomingSms DirectoryInboxReceiver::readEnvelope(const QString &filePath, bool *ok)
{
    if (ok)
        *ok = false;
    IncomingSms sms;
    sms.channel = channel::ShortcutIntent;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
        return sms;
    if (file.size() > kMaxFileBytes) {
        qCWarning(lcInbox) << "envelope too large, rejected:" << QFileInfo(filePath).fileName();
        return sms;
    }
    QJsonParseError error;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject())
        return sms;
    const QJsonObject o = doc.object();
    if (o.value(QStringLiteral("v")).toInt(0) != kEnvelopeVersion)
        return sms;
    if (!o.value(QStringLiteral("body")).isString())
        return sms;

    sms.body = o.value(QStringLiteral("body")).toString();
    sms.sender = o.value(QStringLiteral("sender")).toString();
    sms.externalId = o.value(QStringLiteral("id")).toString();
    sms.bankHint = o.value(QStringLiteral("bank")).toString();
    const QString ch = o.value(QStringLiteral("channel")).toString();
    if (!ch.isEmpty())
        sms.channel = ch;
    const QString when = o.value(QStringLiteral("receivedAt")).toString();
    if (!when.isEmpty())
        sms.receivedAt = QDateTime::fromString(when, Qt::ISODate).toLocalTime();
    if (ok)
        *ok = true;
    return sms;
}

int DirectoryInboxReceiver::processPending()
{
    if (m_directory.isEmpty() || !m_sink || m_processing)
        return 0;
    m_processing = true; // the sink can take a while; never re-enter

    const QDir dir(m_directory);
    QFileInfoList files = dir.entryInfoList({QStringLiteral("*.json")}, QDir::Files | QDir::Readable, QDir::Time | QDir::Reversed);
    int accepted = 0;
    for (const QFileInfo &info : std::as_const(files)) {
        bool ok = false;
        IncomingSms sms = readEnvelope(info.absoluteFilePath(), &ok);
        if (!ok)
            sms.externalId.clear(); // a broken envelope has no trustworthy id
        const bool handled = m_sink(sms);
        if (handled) {
            if (!QFile::remove(info.absoluteFilePath()))
                qCWarning(lcInbox) << "could not remove processed envelope" << info.fileName();
            ++accepted;
        } else {
            qCWarning(lcInbox) << "sink could not store" << info.fileName() << "- keeping it for the next attempt";
        }
    }
    m_processing = false;
    if (accepted > 0)
        emit messagesProcessed(accepted);
    return accepted;
}

ReceiverStatus DirectoryInboxReceiver::status() const
{
    ReceiverStatus s;
    s.location = m_directory;
    if (m_directory.isEmpty()) {
        s.available = false;
        s.detail = m_unavailableReason.isEmpty() ? tr("No inbox location is configured.") : m_unavailableReason;
        return s;
    }
    s.available = QFileInfo(m_directory).isDir() || m_running;
    if (!s.available)
        s.detail = m_unavailableReason;
    s.pending = int(QDir(m_directory).entryList({QStringLiteral("*.json")}, QDir::Files).size());
    return s;
}

} // namespace pf::sms
