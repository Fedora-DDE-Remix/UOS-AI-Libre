pragma Singleton
import QtQuick

QtObject {
    property bool isDark: false

    readonly property string fontFamily: app.fontFamily.length > 0
                                           ? app.fontFamily : "Noto Sans CJK SC"

    readonly property int windowRadius: 12
    readonly property int sidebarWidth: 204
    readonly property int titleBarHeight: 48

    readonly property color accent: isDark ? "#4C8DFF" : "#1F6EE7"
    readonly property color accentHover: isDark ? "#6AA3FF" : "#3D82F0"
    readonly property color accentSoft: isDark ? "#22314D" : "#E6EFFF"
    readonly property color accentDisabled: isDark ? "#39496A" : "#AFC8F2"

    readonly property color windowBg: isDark ? "#19191C" : "#F6F6F7"
    readonly property color chatBg: isDark ? "#1D1D20" : "#FBFBFC"
    readonly property color sidebarTop: isDark ? "#2A2230" : "#E9DEE9"
    readonly property color sidebarMid: isDark ? "#202026" : "#F3EFF5"
    readonly property color sidebarBottom: isDark ? "#191B1E" : "#E2E7EA"
    readonly property color sidebarPink: isDark ? "#3A2636" : "#F6DCE8"
    readonly property color sidebarBlue: isDark ? "#1E2B38" : "#E2EFF8"

    readonly property color card: isDark ? "#232326" : "#FFFFFF"
    readonly property color cardHover: isDark ? "#28282C" : "#FCFCFD"
    readonly property color cardBorder: isDark ? "#303035" : "#E8E8EA"
    readonly property color border: isDark ? "#2C2C31" : "#E8E8EA"
    readonly property color borderStrong: isDark ? "#3A3A40" : "#D9D9DD"

    // Menus are slightly darker than cards, like the UOS AI popups.
    readonly property color menuBackground: isDark ? "#2A2A2C" : "#F0F0F1"
    readonly property color menuBorder: isDark ? "#3B3B40" : "#E3E3E5"
    readonly property color menuHighlightText: "#FFFFFF"

    readonly property color textPrimary: isDark ? "#F1F1F3" : "#1A1A1A"
    readonly property color textSecondary: isDark ? "#A8A8AF" : "#66666B"
    readonly property color textTertiary: isDark ? "#7C7C84" : "#99999F"
    readonly property color textInverse: "#FFFFFF"

    readonly property color userBubble: isDark ? "#2B2B30" : "#E9E9EA"
    readonly property color userText: isDark ? "#F1F1F3" : "#1A1A1A"
    readonly property color reasonText: isDark ? "#A0A0A8" : "#76767D"

    readonly property color hover: isDark ? "#0DFFFFFF" : "#08000000"
    readonly property color pressed: isDark ? "#16FFFFFF" : "#10000000"
    readonly property color selected: isDark ? "#2D2D33" : "#E5E5E7"
    readonly property color selectedPressed: isDark ? "#37373E" : "#D8D8DB"

    // Sidebar rows need a stronger hover/pressed contrast than ordinary
    // controls because they sit on a coloured aurora gradient.
    readonly property color sidebarHover: isDark ? "#1AFFFFFF" : "#1A000000"
    readonly property color sidebarPressed: isDark ? "#30FFFFFF" : "#30000000"

    readonly property color codeBg: isDark ? "#1A1A1E" : "#F7F7F8"
    readonly property color codeHeader: isDark ? "#242429" : "#F1F1F3"
    readonly property color tableHeader: isDark ? "#26262B" : "#F7F7F8"
    readonly property color tableBorder: isDark ? "#33333A" : "#E5E5E8"
    readonly property color inputPlaceholder: isDark ? "#6E6E76" : "#B8B8BE"

    readonly property color icon: isDark ? "#C8C8CE" : "#4A4A4F"
    readonly property color iconSecondary: isDark ? "#8A8A92" : "#7A7A80"
    readonly property color iconDisabled: isDark ? "#55555C" : "#BEBEC3"

    readonly property color danger: isDark ? "#FF6B6E" : "#E5484D"
    readonly property color dangerHover: isDark ? "#FF8588" : "#F06A6E"
    readonly property color warning: isDark ? "#FFB84D" : "#D97706"
    readonly property color success: isDark ? "#4CD08A" : "#2BA471"

    readonly property color shadowColor: isDark ? "#66000000" : "#33000000"
    readonly property color overlay: "#66000000"

    readonly property int radiusSmall: 6
    readonly property int radiusMedium: 8
    readonly property int radiusLarge: 12

    readonly property int fontSizeTiny: 11
    readonly property int fontSizeSmall: 12
    readonly property int fontSizeBase: 13
    readonly property int fontSizeBody: 14
    readonly property int fontSizeChat: 15
    readonly property int fontSizeTitle: 24

    readonly property int durationFast: 120
    readonly property int durationNormal: 180
}
