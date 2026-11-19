// iOS-only translation unit. NOT compiled or tested in the Linux development sandbox
// (no Xcode / Foundation there) - verify on a Mac before relying on it.

#include "platform/ios/IOSBankTransactionReceiver.h"

#import <Foundation/Foundation.h>

namespace pf::sms {

namespace {

/// The App Group id comes from Info.plist (`PFAppGroupIdentifier`, normally
/// "group.<bundle id>") so that the app, the extension and the entitlements agree.
NSString *appGroupIdentifier()
{
    id value = [[NSBundle mainBundle] objectForInfoDictionaryKey:@"PFAppGroupIdentifier"];
    return [value isKindOfClass:[NSString class]] ? (NSString *)value : nil;
}

} // namespace

IOSBankTransactionReceiver::IOSBankTransactionReceiver(QObject *parent)
    : DirectoryInboxReceiver(parent)
{
    // The extension writes from another process; kqueue notifications only work while we run, so
    // a poll timer is kept as well. The main trigger is "app became active" (ApplicationManager).
    setWatching(true, 15000);

    @autoreleasepool {
        NSString *group = appGroupIdentifier();
        if (group.length == 0) {
            setInboxDirectory(QString(), tr("App Group is not configured (PFAppGroupIdentifier missing in Info.plist)."));
            return;
        }
        NSURL *container = [[NSFileManager defaultManager] containerURLForSecurityApplicationGroupIdentifier:group];
        if (!container) {
            // Typical cause: the App Group capability/entitlement is missing or was not granted by the
            // provisioning profile (it may be unavailable with a free Apple ID - see docs/BUILD_IOS.md).
            setInboxDirectory(QString(), tr("The App Group container is not available. Check the App Groups "
                                            "capability and the signing team."));
            return;
        }
        NSURL *inbox = [container URLByAppendingPathComponent:@"inbox" isDirectory:YES];
        NSError *error = nil;
        NSDictionary *attributes = @{NSFileProtectionKey : NSFileProtectionCompleteUntilFirstUserAuthentication};
        if (![[NSFileManager defaultManager] createDirectoryAtURL:inbox
                                      withIntermediateDirectories:YES
                                                       attributes:attributes
                                                            error:&error]) {
            setInboxDirectory(QString(), tr("Cannot create the inbox folder: %1")
                                             .arg(QString::fromNSString(error.localizedDescription)));
            return;
        }
        // Captured SMS text must not end up in iCloud / iTunes backups.
        [inbox setResourceValue:@YES forKey:NSURLIsExcludedFromBackupKey error:nil];
        setInboxDirectory(QString::fromNSString(inbox.path));
    }
}

} // namespace pf::sms
