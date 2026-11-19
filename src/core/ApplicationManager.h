#pragma once

#include "categorization/CategorizationService.h"
#include "core/Result.h"
#include "core/SettingsManager.h"
#include "database/AccountRepository.h"
#include "database/CategorizationRuleRepository.h"
#include "database/CategoryRepository.h"
#include "database/DatabaseManager.h"
#include "database/SettingsRepository.h"
#include "database/SmsImportRepository.h"
#include "database/TransactionRepository.h"
#include "models/AccountListModel.h"
#include "models/CategoryListModel.h"
#include "models/TransactionListModel.h"
#include "platform/PlatformServices.h"
#include "services/AccountService.h"
#include "services/AnalyticsService.h"
#include "services/SmsImportService.h"
#include "services/TransactionService.h"
#include "sms/IBankTransactionReceiver.h"

#include <QObject>
#include <QUrl>

#include <memory>

namespace pf {

/// Composition root. Owns the database, repositories, services and QML models,
/// wires their change notifications together and is the single object exposed
/// to QML (as the `App` singleton).
class ApplicationManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool ready READ ready NOTIFY readyChanged)
    Q_PROPERTY(QString startupError READ startupError NOTIFY readyChanged)
    /// Incremented after every data change. Bind to it to refresh derived
    /// values: `readonly property var x: { App.revision; return App.analytics.monthSummary(...) }`
    Q_PROPERTY(int revision READ revision NOTIFY revisionChanged)

    Q_PROPERTY(SettingsManager *settings READ settings CONSTANT)
    Q_PROPERTY(PlatformServices *platform READ platform CONSTANT)
    Q_PROPERTY(AccountService *accounts READ accountService CONSTANT)
    Q_PROPERTY(TransactionService *transactions READ transactionService CONSTANT)
    Q_PROPERTY(AnalyticsService *analytics READ analyticsService CONSTANT)
    Q_PROPERTY(SmsImportService *smsImport READ smsImportService CONSTANT)
    Q_PROPERTY(CategorizationService *categorization READ categorizationService CONSTANT)
    Q_PROPERTY(AccountListModel *accountModel READ accountModel CONSTANT)
    Q_PROPERTY(CategoryListModel *categoryModel READ categoryModel CONSTANT)
    Q_PROPERTY(TransactionListModel *transactionModel READ transactionModel CONSTANT)
    Q_PROPERTY(TransactionListModel *recentModel READ recentModel CONSTANT)

public:
    explicit ApplicationManager(const QString &databasePath, QObject *parent = nullptr);

    /// Opens/migrates the database and loads settings and models.
    /// On failure the app still starts and QML shows `startupError`.
    Status initialize();

    bool ready() const { return m_ready; }
    QString startupError() const { return m_startupError; }
    int revision() const { return m_revision; }

    SettingsManager *settings() { return &m_settings; }
    PlatformServices *platform() { return m_platform; }
    AccountService *accountService() { return &m_accountService; }
    TransactionService *transactionService() { return &m_transactionService; }
    AnalyticsService *analyticsService() { return &m_analyticsService; }
    SmsImportService *smsImportService() { return &m_smsImportService; }
    CategorizationService *categorizationService() { return &m_categorizationService; }
    AccountListModel *accountModel() { return &m_accountModel; }
    CategoryListModel *categoryModel() { return &m_categoryModel; }
    TransactionListModel *transactionModel() { return &m_transactionModel; }
    TransactionListModel *recentModel() { return &m_recentModel; }

    CategoryRepository &categoryRepository() { return m_categoryRepository; }
    AccountRepository &accountRepository() { return m_accountRepository; }
    TransactionRepository &transactionRepository() { return m_transactionRepository; }
    DatabaseManager &database() { return m_database; }

    /// Call whenever the app (re)gains the foreground: iOS cannot run our code when an SMS
    /// arrives, so messages captured by the Shortcuts action are ingested here.
    Q_INVOKABLE int onAppActivated();
    /// Handles `personalfinance://import?...` (the fallback hand-over). Returns false when the
    /// URL is not an import URL or the app is not ready.
    Q_INVOKABLE bool handleIncomingUrl(const QUrl &url);

signals:
    void readyChanged();
    void revisionChanged();

private:
    void onDataChanged();
    void createSmsReceiver(const QString &databasePath);

    // Order matters: later members use earlier ones.
    DatabaseManager m_database;
    AccountRepository m_accountRepository;
    TransactionRepository m_transactionRepository;
    CategoryRepository m_categoryRepository;
    SettingsRepository m_settingsRepository;
    CategorizationRuleRepository m_ruleRepository;
    SmsImportRepository m_smsImportRepository;

    PlatformServices *m_platform = nullptr; // QObject child
    SettingsManager m_settings;
    AccountService m_accountService;
    TransactionService m_transactionService;
    AnalyticsService m_analyticsService;
    CategorizationService m_categorizationService;
    SmsImportService m_smsImportService;
    sms::IBankTransactionReceiver *m_smsReceiver = nullptr; // QObject child
    AccountListModel m_accountModel;
    CategoryListModel m_categoryModel;
    TransactionListModel m_transactionModel;
    TransactionListModel m_recentModel;

    bool m_ready = false;
    QString m_startupError;
    int m_revision = 0;
};

} // namespace pf
