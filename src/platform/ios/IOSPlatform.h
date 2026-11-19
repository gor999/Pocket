#pragma once

#include "platform/PlatformServices.h"

namespace pf {

/// iOS implementation of PlatformServices (UIKit via Objective-C++).
/// Compiled only when targeting iOS - see CMakeLists.txt.
class IOSPlatform final : public PlatformServices
{
    Q_OBJECT

public:
    explicit IOSPlatform(QObject *parent = nullptr);

    QString name() const override { return QStringLiteral("ios"); }
    void refreshSafeArea() override;
};

} // namespace pf
