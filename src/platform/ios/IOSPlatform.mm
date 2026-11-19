// iOS-only translation unit. NOT compiled or tested in the Linux development
// sandbox (no Xcode / UIKit there) - verify on a Mac before relying on it.

#include "platform/ios/IOSPlatform.h"

#import <UIKit/UIKit.h>

namespace pf {

namespace {

UIWindow *activeKeyWindow()
{
    UIWindow *fallback = nil;
    for (UIScene *scene in UIApplication.sharedApplication.connectedScenes) {
        if (![scene isKindOfClass:[UIWindowScene class]])
            continue;
        UIWindowScene *windowScene = (UIWindowScene *)scene;
        for (UIWindow *window in windowScene.windows) {
            if (window.isKeyWindow)
                return window;
            if (!fallback)
                fallback = window;
        }
    }
    return fallback;
}

} // namespace

IOSPlatform::IOSPlatform(QObject *parent)
    : PlatformServices(parent)
{
    refreshSafeArea();
}

void IOSPlatform::refreshSafeArea()
{
    UIWindow *window = activeKeyWindow();
    if (!window)
        return;
    // UIKit reports insets in points, the same unit QML uses.
    const UIEdgeInsets insets = window.safeAreaInsets;
    setSafeArea(insets.top, insets.left, insets.bottom, insets.right);
}

} // namespace pf
