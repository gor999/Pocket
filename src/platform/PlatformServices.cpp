#include "platform/PlatformServices.h"

#include <QtGlobal>

#ifdef Q_OS_IOS
#include "platform/ios/IOSPlatform.h"
#endif

namespace pf {

PlatformServices::PlatformServices(QObject *parent)
    : QObject(parent)
{
}

void PlatformServices::setSafeArea(qreal top, qreal left, qreal bottom, qreal right)
{
    if (qFuzzyCompare(1 + m_top, 1 + top) && qFuzzyCompare(1 + m_left, 1 + left)
        && qFuzzyCompare(1 + m_bottom, 1 + bottom) && qFuzzyCompare(1 + m_right, 1 + right)) {
        return;
    }
    m_top = top;
    m_left = left;
    m_bottom = bottom;
    m_right = right;
    emit safeAreaChanged();
}

#ifndef Q_OS_IOS
namespace {

/// Stand-in used on desktop/CI. Set PF_SIMULATE_SAFE_AREA=1 to emulate an
/// iPhone with Dynamic Island (59pt top, 34pt bottom) while developing.
class DesktopPlatform final : public PlatformServices
{
public:
    explicit DesktopPlatform(QObject *parent = nullptr)
        : PlatformServices(parent)
    {
        refreshSafeArea();
    }

    QString name() const override { return QStringLiteral("desktop"); }

    void refreshSafeArea() override
    {
        if (qEnvironmentVariableIntValue("PF_SIMULATE_SAFE_AREA") != 0)
            setSafeArea(59, 0, 34, 0);
        else
            setSafeArea(0, 0, 0, 0);
    }
};

} // namespace
#endif

PlatformServices *PlatformServices::create(QObject *parent)
{
#ifdef Q_OS_IOS
    return new IOSPlatform(parent);
#else
    return new DesktopPlatform(parent);
#endif
}

} // namespace pf
