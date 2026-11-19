pragma Singleton
import QtQuick
import PersonalFinance.Core

// Design tokens shared by every component. Colors follow the system or the
// user-selected light/dark theme (App.settings.effectiveDark).
QtObject {
    readonly property bool dark: App.settings.effectiveDark

    // Surfaces
    readonly property color background: dark ? "#0A0C11" : "#F2F3F7"
    readonly property color surface: dark ? "#14171F" : "#FFFFFF"
    readonly property color card: dark ? "#1A1E28" : "#FFFFFF"
    readonly property color field: dark ? "#232835" : "#EFF0F5"
    readonly property color border: dark ? "#272C3A" : "#E4E6EC"
    readonly property color segmentThumb: dark ? "#363C4D" : "#FFFFFF"
    readonly property color pressed: dark ? "#FFFFFF10" : "#00000008"

    // Text
    readonly property color text: dark ? "#F4F6FA" : "#0D1017"
    readonly property color textSecondary: dark ? "#9BA3B4" : "#677083"
    readonly property color textTertiary: dark ? "#6B7385" : "#9AA1B0"

    // Brand and semantic colors
    readonly property color accent: dark ? "#7B83FF" : "#4F46E5"
    readonly property color accentSoft: dark ? "#2A2F57" : "#E8E7FC"
    readonly property color textOnAccent: "#FFFFFF"
    readonly property color income: dark ? "#34D399" : "#0F9F4F"
    readonly property color expense: dark ? "#FF6B7A" : "#E5484D"
    readonly property color warning: "#F5A524"
    readonly property color heroTop: dark ? "#4A47D4" : "#5B54F0"
    readonly property color heroBottom: dark ? "#2B2A8E" : "#3A33C6"

    // Metrics
    readonly property int pagePadding: 20
    readonly property int radiusCard: 22
    readonly property int radiusField: 14
    readonly property int tabBarHeight: 62

    // Typography (pixel sizes; the platform font is SF Pro on iOS)
    readonly property int fontLargeTitle: 32
    readonly property int fontTitle: 22
    readonly property int fontBody: 16
    readonly property int fontCaption: 13

    // Accepts a color or a color string such as "#E57373" (model roles are strings).
    function alpha(c, a) {
        const col = Qt.darker(c, 1.0)
        return Qt.rgba(col.r, col.g, col.b, a)
    }

    // 24x24 stroke icons (own drawings; SVG path syntax)
    readonly property var iconPaths: ({
        "home": "M3 11.5 L12 4 L21 11.5 M5.5 10 V20 H18.5 V10 M10 20 V14 H14 V20",
        "transactions": "M7 20 V5 M3.5 8.5 L7 5 L10.5 8.5 M17 4 V19 M13.5 15.5 L17 19 L20.5 15.5",
        "accounts": "M3 7 a2 2 0 0 1 2 -2 H19 a2 2 0 0 1 2 2 V17 a2 2 0 0 1 -2 2 H5 a2 2 0 0 1 -2 -2 Z M3 10 H21 M7 15 H10",
        "budgets": "M12 3 a9 9 0 1 0 9 9 H12 Z M15 3.4 a9 9 0 0 1 5.6 5.6 H15 Z",
        "analytics": "M5 20 V12 M12 20 V5 M19 20 V9",
        "settings": "M4 7 H6.5 M11.5 7 H20 M4 17 H12.5 M17.5 17 H20 M6.5 7 a2.5 2.5 0 1 0 5 0 a2.5 2.5 0 1 0 -5 0 M12.5 17 a2.5 2.5 0 1 0 5 0 a2.5 2.5 0 1 0 -5 0",
        "plus": "M12 5 V19 M5 12 H19",
        "search": "M4 11 a7 7 0 1 0 14 0 a7 7 0 1 0 -14 0 M16.2 16.2 L20.5 20.5",
        "chevron-right": "M9 6 L15 12 L9 18",
        "chevron-left": "M15 6 L9 12 L15 18",
        "close": "M6 6 L18 18 M18 6 L6 18",
        "check": "M5 12.5 L10 17.5 L19 7.5",
        "trash": "M4 7 H20 M9 7 V4.5 H15 V7 M6.5 7 L7.5 20 H16.5 L17.5 7 M10 11 V16 M14 11 V16",
        "calendar": "M4 6 H20 V20 H4 Z M4 10 H20 M8 3.5 V7.5 M16 3.5 V7.5",
        "arrow-up-right": "M7 17 L17 7 M9 7 H17 V15",
        "arrow-down-left": "M17 7 L7 17 M7 9 V17 H15",
        "lock": "M6 11 H18 V20 H6 Z M8.5 11 V8 a3.5 3.5 0 0 1 7 0 V11",
        "archive": "M4 6 H20 V10 H4 Z M5.5 10 V19 H18.5 V10 M10 14 H14"
    })
}
