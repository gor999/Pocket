#pragma once

#include <QObject>
#include <QString>

namespace pf {

/// Boundary between portable C++/QML code and native platform APIs.
///
/// QML and the services only ever see this interface. The iOS implementation
/// (src/platform/ios/IOSPlatform.mm, Objective-C++) is compiled only for iOS;
/// every other platform gets a desktop stand-in, so the whole app can be
/// developed and tested without an iPhone.
///
/// Phase 4 adds Face ID, Keychain, local notifications and the document
/// picker to this interface.
class PlatformServices : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString name READ name CONSTANT)
    /// Insets (in points) occupied by the notch / Dynamic Island / home indicator.
    Q_PROPERTY(qreal safeAreaTop READ safeAreaTop NOTIFY safeAreaChanged)
    Q_PROPERTY(qreal safeAreaBottom READ safeAreaBottom NOTIFY safeAreaChanged)
    Q_PROPERTY(qreal safeAreaLeft READ safeAreaLeft NOTIFY safeAreaChanged)
    Q_PROPERTY(qreal safeAreaRight READ safeAreaRight NOTIFY safeAreaChanged)

public:
    explicit PlatformServices(QObject *parent = nullptr);

    /// Creates the implementation for the current platform.
    static PlatformServices *create(QObject *parent = nullptr);

    virtual QString name() const = 0;

    /// Re-reads the safe-area insets from the OS. QML calls this when the
    /// window size or orientation changes.
    Q_INVOKABLE virtual void refreshSafeArea() = 0;

    qreal safeAreaTop() const { return m_top; }
    qreal safeAreaBottom() const { return m_bottom; }
    qreal safeAreaLeft() const { return m_left; }
    qreal safeAreaRight() const { return m_right; }

signals:
    void safeAreaChanged();

protected:
    void setSafeArea(qreal top, qreal left, qreal bottom, qreal right);

private:
    qreal m_top = 0;
    qreal m_left = 0;
    qreal m_bottom = 0;
    qreal m_right = 0;
};

} // namespace pf
