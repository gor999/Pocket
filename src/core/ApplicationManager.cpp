#include "core/ApplicationManager.h"

#include "sms/DesktopBankTransactionReceiver.h"
#include "sms/UrlImportHandler.h"
#ifdef Q_OS_IOS
#include "platform/ios/IOSBankTransactionReceiver.h"
#endif

#include <QFileInfo>

namespace pf {

ApplicationManager::ApplicationManager(const QString &databasePath, QObject *parent)
    : QObject(parent)
    , m_database(databasePath)
    , m_accountRepository(m_database)
    , m_transactionRepository(m_database)
    , m_categoryRepository(m_database)
    , m_settingsRepository(m_database)
    , m_ruleRepository(m_database)
    , m_smsImportRepository(m_database)
    , m_platform(PlatformServices::create(this))
    , m_settings(m_settingsRepository)
    , m_accountService(m_accountRepository, m_settings)
    , m_transactionService(m_transactionRepository, m_accountRepository)
    , m_analyticsService(m_transactionRepository, m_settings)
    , m_categorizationService(m_categoryRepository, m_ruleRepository)
    , m_smsImportService(m_database, m_smsImportRepository, m_transactionService, m_transactionRepository,
                         m_accountRepository, m_accountService, m_categoryRepository, m_categorizationService,
                         m_settingsRepository, m_settings, sms::ParserRegistry::withBuiltIns())
    , m_accountModel(m_accountRepository, m_settings)
    , m_categoryModel(m_categoryRepository, m_settings)
    , m_transactionModel(m_transactionRepository, m_accountRepository, m_categoryRepository, m_settings)
    , m_recentModel(m_transactionRepository, m_accountRepository, m_categoryRepository, m_settings)
{
    m_recentModel.setMaxRows(5);
    m_recentModel.setPageSize(5);

    connect(&m_accountService, &AccountService::changed, this, &ApplicationManager::onDataChanged);
    connect(&m_transactionService, &TransactionService::changed, this, &ApplicationManager::onDataChanged);
    // The SMS screens bind to `App.revision`. Transaction/account models are already reloaded by
    // the services' own signals, so only the revision is bumped here.
    connect(&m_smsImportService, &SmsImportService::changed, this, [this] {
        ++m_revision;
        emit revisionChanged();
    });
    createSmsReceiver(databasePath);
    // Formatting depends on the locale.
    connect(&m_settings, &SettingsManager::languageChanged, this, [this] {
        ++m_revision;
        emit revisionChanged();
    });
}

void ApplicationManager::createSmsReceiver(const QString &databasePath)
{
#ifdef Q_OS_IOS
    m_smsReceiver = new sms::IOSBankTransactionReceiver(this);
#else
    // Development / tests: a folder next to the database.
    const QString dir = QFileInfo(databasePath).absolutePath() + QStringLiteral("/sms_inbox");
    m_smsReceiver = new sms::DesktopBankTransactionReceiver(dir, this);
#endif
    // The receiver removes its source file only when this returns true (durably handled).
    m_smsReceiver->setSink([this](const sms::IncomingSms &message) { return m_smsImportService.process(message).handled; });
    m_smsImportService.setReceiver(m_smsReceiver);
    connect(m_smsReceiver, &sms::IBankTransactionReceiver::messagesProcessed, this, [this](int) {
        ++m_revision;
        emit revisionChanged();
    });
}

int ApplicationManager::onAppActivated()
{
    if (!m_ready)
        return 0;
    return m_smsImportService.ingestPending();
}

bool ApplicationManager::handleIncomingUrl(const QUrl &url)
{
    if (!m_ready)
        return false;
    const auto message = sms::UrlImportHandler::parse(url);
    if (!message)
        return false;
    m_smsImportService.process(*message);
    return true;
}

Status ApplicationManager::initialize()
{
    const Status opened = m_database.open();
    if (!opened) {
        m_startupError = opened.message();
        m_ready = false;
        emit readyChanged();
        return opened;
    }

    m_settings.load();
    m_categorizationService.reload();
    m_categoryModel.reload();
    m_accountModel.reload();
    m_transactionModel.reload();
    m_recentModel.reload();

    m_startupError.clear();
    m_ready = true;
    emit readyChanged();

    // Start listening for captured SMS, and pick up anything that arrived while the app was closed.
    if (m_smsReceiver && m_smsReceiver->start())
        m_smsReceiver->processPending();
    return Status::ok();
}

void ApplicationManager::onDataChanged()
{
    m_accountModel.reload();
    m_transactionModel.reload();
    m_recentModel.reload();
    ++m_revision;
    emit revisionChanged();
}

} // namespace pf
