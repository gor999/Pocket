cmake_minimum_required(VERSION 3.21)

# iOS deployment target must be chosen before project(). Qt 6.8 requires iOS 16.
if(NOT CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "16.0" CACHE STRING "Minimum iOS version")
endif()

project(PersonalFinance
    VERSION 0.1.0
    DESCRIPTION "Personal finance tracker (Qt 6 / C++ / QML)"
    LANGUAGES CXX)

if(APPLE)
    enable_language(OBJCXX)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------
option(PF_BUILD_APP "Build the QML application" ON)
option(PF_BUILD_TESTS "Build unit tests (desktop only)" ON)
option(PF_DEV_TOOLS "Developer helpers: demo data and screenshot mode (desktop only)" ON)
set(PF_DEVELOPMENT_TEAM "" CACHE STRING "Apple Developer Team ID used for iOS code signing")
set(PF_BUNDLE_IDENTIFIER "com.example.personalfinance" CACHE STRING "iOS bundle identifier (must be unique to your team)")

if(IOS)
    # Tests and developer tooling never ship in the iOS app.
    set(PF_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(PF_DEV_TOOLS OFF CACHE BOOL "" FORCE)
endif()

# ---------------------------------------------------------------------------
# Qt
# ---------------------------------------------------------------------------
# Qt 6.4 is the oldest version that builds this project on the desktop. For the
# iOS build use Qt 6.8 LTS or newer (see docs/BUILD_IOS.md).
set(PF_QT_COMPONENTS Core Gui Qml Quick QuickControls2 Sql)
if(PF_BUILD_TESTS)
    list(APPEND PF_QT_COMPONENTS Test Concurrent)
endif()
find_package(Qt6 6.4 REQUIRED COMPONENTS ${PF_QT_COMPONENTS})
qt_standard_project_setup()

# ---------------------------------------------------------------------------
# pf_core: all C++ logic (no QML, no GUI) - linked by the app and the tests
# ---------------------------------------------------------------------------
set(PF_CORE_SOURCES
    src/core/Currency.cpp
    src/core/Money.cpp
    src/core/SettingsManager.cpp
    src/core/ApplicationManager.cpp
    src/core/DemoData.cpp
    src/models/Enums.cpp
    src/models/Transaction.cpp
    src/models/Category.cpp
    src/models/AccountListModel.cpp
    src/models/CategoryListModel.cpp
    src/models/TransactionListModel.cpp
    src/database/DatabaseManager.cpp
    src/database/Migrations.cpp
    src/database/AccountRepository.cpp
    src/database/TransactionRepository.cpp
    src/database/CategoryRepository.cpp
    src/database/SettingsRepository.cpp
    src/database/CategorizationRuleRepository.cpp
    src/database/SmsImportRepository.cpp
    src/categorization/StarterRules.cpp
    src/categorization/MerchantNormalizer.cpp
    src/categorization/CategorizationService.cpp
    src/services/AccountService.cpp
    src/services/TransactionService.cpp
    src/services/AnalyticsService.cpp
    src/platform/PlatformServices.cpp
    # --- bank-SMS import (parsing layer) ---
    src/sms/SmsTypes.cpp
    src/sms/SmsSanitizer.cpp
    src/sms/SmsText.cpp
    src/sms/TemplateSmsParser.cpp
    src/sms/IDBankSmsParser.cpp
    src/sms/AmeriabankSmsParser.cpp
    src/sms/ACBASmsParser.cpp
    src/sms/ParserRegistry.cpp
    src/sms/IBankTransactionReceiver.cpp
    src/sms/DirectoryInboxReceiver.cpp
    src/sms/DesktopBankTransactionReceiver.cpp
    src/sms/UrlImportHandler.cpp
    src/services/SmsImportService.cpp
)


add_library(pf_core STATIC ${PF_CORE_SOURCES})
target_include_directories(pf_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_features(pf_core PUBLIC cxx_std_20)
target_link_libraries(pf_core PUBLIC Qt6::Core Qt6::Sql)
if(IOS)
    # Objective-C++ platform code: UIKit now; LocalAuthentication, Security
    # (Keychain) and UserNotifications are added in Phase 4.
    target_sources(pf_core PRIVATE src/platform/ios/IOSPlatform.mm src/platform/ios/IOSBankTransactionReceiver.mm)
    target_link_libraries(pf_core PUBLIC "-framework UIKit" "-framework Foundation")
endif()
if(MSVC)
    target_compile_options(pf_core PRIVATE /W4)
else()
    target_compile_options(pf_core PRIVATE -Wall -Wextra -Wpedantic)
endif()


# ---------------------------------------------------------------------------
# Application (QML)
# ---------------------------------------------------------------------------
if(PF_BUILD_APP)
    qt_add_executable(PersonalFinance main.cpp)

    # Theme.qml is a QML singleton (design tokens shared by all components).
    set_source_files_properties(qml/styles/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

    qt_add_qml_module(PersonalFinance
        URI PersonalFinance
        VERSION 1.0
        RESOURCE_PREFIX /qt/qml
        # Keep generated qmldir/qmltypes out of build/PersonalFinance, which would
        # collide with the executable of the same name.
        OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/qmlmodule/PersonalFinance
        QML_FILES
            qml/Main.qml
            qml/styles/Theme.qml
            qml/components/Icon.qml
            qml/components/Card.qml
            qml/components/PrimaryButton.qml
            qml/components/SegmentedControl.qml
            qml/components/ChipRow.qml
            qml/components/FormField.qml
            qml/components/SheetPopup.qml
            qml/components/BottomBar.qml
            qml/components/EmptyState.qml
            qml/components/TransactionDelegate.qml
            qml/components/AccountCard.qml
            qml/components/DateTimePicker.qml
            qml/components/TransactionSheet.qml
            qml/components/AccountSheet.qml
            qml/components/ToggleSwitch.qml
            qml/components/SmsReviewCard.qml
            qml/pages/DashboardPage.qml
            qml/pages/TransactionsPage.qml
            qml/pages/AccountsPage.qml
            qml/pages/BudgetsPage.qml
            qml/pages/AnalyticsPage.qml
            qml/pages/SettingsPage.qml
            qml/pages/SmsImportPage.qml
    )

    target_link_libraries(PersonalFinance PRIVATE
        pf_core Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2)
    target_compile_definitions(PersonalFinance PRIVATE
        PF_VERSION="${PROJECT_VERSION}"
        $<$<BOOL:${PF_DEV_TOOLS}>:PF_DEV_TOOLS>)

    if(TARGET Qt6::QSQLiteDriverPlugin)
        # Static (iOS) builds must link the SQLite driver plugin explicitly.
        qt_import_plugins(PersonalFinance INCLUDE Qt6::QSQLiteDriverPlugin)
    endif()

    if(IOS)
        set_target_properties(PersonalFinance PROPERTIES
            MACOSX_BUNDLE_GUI_IDENTIFIER "${PF_BUNDLE_IDENTIFIER}"
            MACOSX_BUNDLE_BUNDLE_NAME "Personal Finance"
            MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
            MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
            MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/cmake/ios/Info.plist.in"
            XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "${PF_BUNDLE_IDENTIFIER}"
            XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1" # iPhone only
            XCODE_ATTRIBUTE_CODE_SIGN_STYLE "Automatic"
        )
        if(PF_DEVELOPMENT_TEAM)
            set_target_properties(PersonalFinance PROPERTIES
                XCODE_ATTRIBUTE_DEVELOPMENT_TEAM "${PF_DEVELOPMENT_TEAM}")
        endif()
    endif()
endif()

# ---------------------------------------------------------------------------
# iOS: App Intents extension for the "Import Bank SMS" Shortcuts action
# ---------------------------------------------------------------------------
# OFF by default and iOS-only. NOT verified: the Linux sandbox has no Xcode, so this target has never
# been generated or built. If CMake/Xcode cannot build it, add an "App Intents Extension" target in
# Xcode by hand and add ios/IntentsExtension/*.swift to it (docs/BUILD_IOS.md, "Fallback").
option(PF_BUILD_INTENTS_EXTENSION "Build the App Intents extension (iOS, needs App Groups)" OFF)

if(IOS AND PF_BUILD_APP)
    # Both targets read the group id from this build setting (Info.plist + entitlements).
    set_target_properties(PersonalFinance PROPERTIES
        XCODE_ATTRIBUTE_PF_APP_GROUP_ID "group.${PF_BUNDLE_IDENTIFIER}")
endif()

if(IOS AND PF_BUILD_APP AND PF_BUILD_INTENTS_EXTENSION)
    if(CMAKE_VERSION VERSION_LESS 3.26)
        message(FATAL_ERROR "PF_BUILD_INTENTS_EXTENSION needs CMake >= 3.26 (XCODE_EMBED_EXTENSIONKIT_EXTENSIONS).")
    endif()
    enable_language(Swift)

    add_executable(PFIntentsExtension MACOSX_BUNDLE
        ios/IntentsExtension/PFIntentsExtension.swift
        ios/IntentsExtension/ImportBankSMSIntent.swift
        ios/IntentsExtension/BankChoice.swift
        ios/IntentsExtension/InboxWriter.swift)

    set_target_properties(PFIntentsExtension PROPERTIES
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/ios/IntentsExtension/Info.plist"
        XCODE_PRODUCT_TYPE "com.apple.product-type.extensionkit-extension"
        XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "${PF_BUNDLE_IDENTIFIER}.intents"
        XCODE_ATTRIBUTE_PF_APP_GROUP_ID "group.${PF_BUNDLE_IDENTIFIER}"
        XCODE_ATTRIBUTE_CODE_SIGN_ENTITLEMENTS "${CMAKE_CURRENT_SOURCE_DIR}/ios/PersonalFinance.entitlements"
        XCODE_ATTRIBUTE_CODE_SIGN_STYLE "Automatic"
        XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1"
        XCODE_ATTRIBUTE_SWIFT_VERSION "5.0"
        XCODE_ATTRIBUTE_IPHONEOS_DEPLOYMENT_TARGET "17.0"
        XCODE_ATTRIBUTE_SKIP_INSTALL "YES"
        XCODE_ATTRIBUTE_LD_RUNPATH_SEARCH_PATHS "$(inherited) @executable_path/Frameworks @executable_path/../../Frameworks"
        XCODE_ATTRIBUTE_APPLICATION_EXTENSION_API_ONLY "YES")
    if(PF_DEVELOPMENT_TEAM)
        set_target_properties(PFIntentsExtension PROPERTIES XCODE_ATTRIBUTE_DEVELOPMENT_TEAM "${PF_DEVELOPMENT_TEAM}")
    endif()

    # The app carries the extension and the same App Group entitlement.
    set_target_properties(PersonalFinance PROPERTIES
        XCODE_EMBED_EXTENSIONKIT_EXTENSIONS PFIntentsExtension
        XCODE_EMBED_EXTENSIONKIT_EXTENSIONS_CODE_SIGN_ON_COPY ON
        XCODE_ATTRIBUTE_CODE_SIGN_ENTITLEMENTS "${CMAKE_CURRENT_SOURCE_DIR}/ios/PersonalFinance.entitlements")
    add_dependencies(PersonalFinance PFIntentsExtension)
endif()

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
if(PF_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
