#include "core/ApplicationManager.h"
#include "core/DemoData.h"
#include "sms/UrlImportHandler.h"

#include <QFileOpenEvent>
#include <QGuiApplication>
#include <QLoggingCategory>
#include <QPalette>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QStyleHints>
#include <QtQml>

#ifdef PF_DEV_TOOLS
#include <QImage>
#include <QTimer>
#include <functional>
#include <memory>
#endif

namespace {

QString databasePath()
{
    // PF_DB_PATH lets developers (and the screenshot tooling) use a throw-away database.
    const QByteArray override = qgetenv("PF_DB_PATH");
    if (!override.isEmpty())
        return QString::fromLocal8Bit(override);
    // iOS: <app sandbox>/Library/Application Support/<app>/ - private to the app and
    // covered by iOS data protection. Nothing ever leaves the device.
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/personalfinance.sqlite");
}

/// Keeps SettingsManager informed about the OS light/dark appearance.
void followSystemAppearance(QGuiApplication &app, pf::SettingsManager *settings)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    const auto update = [&app, settings] {
        settings->setSystemDark(app.styleHints()->colorScheme() == Qt::ColorScheme::Dark);
    };
    update();
    QObject::connect(app.styleHints(), &QStyleHints::colorSchemeChanged, settings, update);
#else
    // Qt < 6.5 has no colorScheme API: infer it from the platform palette.
    struct PaletteWatcher : QObject
    {
        pf::SettingsManager *settings;
        explicit PaletteWatcher(pf::SettingsManager *s, QObject *parent) : QObject(parent), settings(s) { refresh(); }
        void refresh() { settings->setSystemDark(QGuiApplication::palette().color(QPalette::Window).lightness() < 128); }
        bool eventFilter(QObject *, QEvent *e) override
        {
            if (e->type() == QEvent::ApplicationPaletteChange)
                refresh();
            return false;
        }
    };
    auto *watcher = new PaletteWatcher(settings, &app);
    app.installEventFilter(watcher);
#endif
}

/// iOS hands `personalfinance://...` URLs to the app as QFileOpenEvent (also while it is
/// already running). Only the import URL of the Shortcuts fallback is recognised; anything
/// else is ignored. UNVERIFIED on a device - see docs/IOS_SMS_FEASIBILITY.md.
class UrlEventFilter : public QObject
{
public:
    explicit UrlEventFilter(pf::ApplicationManager *manager, QObject *parent) : QObject(parent), m_manager(manager) {}

    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (event->type() == QEvent::FileOpen) {
            const QUrl url = static_cast<QFileOpenEvent *>(event)->url();
            if (url.scheme() == QLatin1String(pf::sms::UrlImportHandler::kScheme))
                return m_manager->handleIncomingUrl(url);
        }
        return QObject::eventFilter(watched, event);
    }

private:
    pf::ApplicationManager *m_manager;
};

#ifdef PF_DEV_TOOLS
/// Developer tool: PF_SCREENSHOT=<prefix> PF_STEPS="tab:0,tab:1,newTransaction,..."
/// drives the UI and saves one PNG per step, then quits. Used to verify the
/// QML interface headlessly (QT_QPA_PLATFORM=offscreen).
bool applyStep(QQuickWindow *window, pf::ApplicationManager &manager, const QString &step)
{
    const QString name = step.section(QLatin1Char(':'), 0, 0);
    const QString arg = step.section(QLatin1Char(':'), 1);
    if (name == QLatin1String("tab"))
        return window->setProperty("currentTab", arg.toInt());
    if (name == QLatin1String("dark") || name == QLatin1String("light") || name == QLatin1String("system")) {
        manager.settings()->setTheme(name);
        return true;
    }
    if (name == QLatin1String("search")) {
        manager.transactionModel()->setSearchText(arg);
        return true;
    }
    if (name == QLatin1String("filter")) {
        manager.transactionModel()->setTypeFilter(arg);
        return true;
    }
    if (name == QLatin1String("editTransaction")) {
        return QMetaObject::invokeMethod(window, "editTransaction",
                                         Q_ARG(QVariant, manager.transactionModel()->get(arg.toInt())));
    }
    if (name == QLatin1String("editAccount")) {
        return QMetaObject::invokeMethod(window, "editAccount",
                                         Q_ARG(QVariant, manager.accountModel()->get(arg.toInt())));
    }
    if (name == QLatin1String("size")) { // size:390x1800 - a tall window shows a whole scrolling page
        const QStringList wh = arg.split(QLatin1Char('x'));
        if (wh.size() != 2)
            return false;
        window->resize(wh.at(0).toInt(), wh.at(1).toInt());
        return true;
    }
    if (name == QLatin1String("wait"))
        return true;
    // Anything else is a zero-argument QML function on the window:
    // newTransaction, newAccount, closeSheets, ...
    return QMetaObject::invokeMethod(window, name.toLatin1().constData());
}

void runScreenshotSteps(QQmlApplicationEngine &engine, pf::ApplicationManager &manager, QGuiApplication &app)
{
    const QString prefix = qEnvironmentVariable("PF_SCREENSHOT");
    const QStringList steps = qEnvironmentVariable("PF_STEPS", QStringLiteral("tab:0")).split(QLatin1Char(','), Qt::SkipEmptyParts);
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().value(0));
    if (!window) {
        qCritical() << "PF_SCREENSHOT: no root window";
        QCoreApplication::exit(2);
        return;
    }

    struct State
    {
        int index = 0;
    };
    auto state = std::make_shared<State>();
    auto next = std::make_shared<std::function<void()>>();
    *next = [=, &engine, &manager, &app]() {
        Q_UNUSED(engine)
        if (state->index >= steps.size()) {
            app.quit();
            return;
        }
        const QString step = steps.at(state->index);
        if (!applyStep(window, manager, step))
            qWarning() << "PF_STEPS: could not apply step" << step;
        QTimer::singleShot(900, &app, [=, &app]() {
            const QString file = QStringLiteral("%1_%2.png").arg(prefix).arg(state->index, 2, 10, QLatin1Char('0'));
            if (!window->grabWindow().save(file))
                qWarning() << "could not save" << file;
            else
                qInfo() << "saved" << file << "after step" << step;
            ++state->index;
            (*next)();
        });
    };
    QTimer::singleShot(1200, &app, [next] { (*next)(); });
}
#endif

} // namespace

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setOrganizationName(QStringLiteral("PersonalFinance"));
    QGuiApplication::setApplicationName(QStringLiteral("PersonalFinance"));
    QGuiApplication::setApplicationDisplayName(QStringLiteral("Personal Finance"));
    QGuiApplication::setApplicationVersion(QStringLiteral(PF_VERSION));

    // The Basic style is the fully customizable one; every control in the UI is
    // restyled in QML, so the result does not depend on a desktop look and feel.
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    pf::ApplicationManager manager(databasePath());
    const pf::Status started = manager.initialize();
    if (!started)
        qCritical().noquote() << "Startup failed:" << started.message();
    followSystemAppearance(app, manager.settings());

    // Bank SMS captured by the Shortcuts action are waiting in the inbox whenever the app
    // comes to the foreground (iOS cannot run this app when the SMS arrives).
    QObject::connect(&app, &QGuiApplication::applicationStateChanged, &manager, [&manager](Qt::ApplicationState state) {
        if (state == Qt::ApplicationActive)
            manager.onAppActivated();
    });
    app.installEventFilter(new UrlEventFilter(&manager, &app));

#ifdef PF_DEV_TOOLS
    if (started && qEnvironmentVariableIntValue("PF_SEED_DEMO") != 0) {
        if (pf::DemoData::seed(manager))
            pf::DemoData::seedSms(manager);
    }
#endif

    qmlRegisterSingletonInstance("PersonalFinance.Core", 1, 0, "App", &manager);

    QQmlApplicationEngine engine;
    engine.addImportPath(QStringLiteral("qrc:/qt/qml"));
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app, [] { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);
    engine.load(QUrl(QStringLiteral("qrc:/qt/qml/PersonalFinance/qml/Main.qml")));
    if (engine.rootObjects().isEmpty())
        return -1;

#ifdef PF_DEV_TOOLS
    if (!qEnvironmentVariable("PF_SCREENSHOT").isEmpty())
        runScreenshotSteps(engine, manager, app);
#endif

    return app.exec();
}
