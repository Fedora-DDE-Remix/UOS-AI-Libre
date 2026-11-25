import QtQuick
import QtQuick.Controls
import UosAiLibre

Item {
    id: root
    property Item anchorItem
    property bool closingSubmenuForHover: false

    signal updatesRequested()
    signal settingsRequested()
    signal helpRequested()
    signal aboutRequested()

    function openMenu() {
        if (themeMenu.opened)
            themeMenu.close()
        mainMenu.highlightedIndex = -1
        mainMenu.closePolicy = Popup.CloseOnEscape | Popup.CloseOnPressOutside
        if (!anchorItem)
            return
        const point = anchorItem.mapToItem(Overlay.overlay, 0, 0)
        mainMenu.x = Math.max(8, Math.min(Overlay.overlay.width - mainMenu.width - 8,
                                         point.x + anchorItem.width - mainMenu.width))
        mainMenu.y = point.y + anchorItem.height + 6
        mainMenu.open()
    }

    function openThemeSubmenu() {
        if (!mainMenu.opened || themeMenu.opened)
            return
        // Keep the parent menu open while the pointer is over the submenu.
        // Dismissing the submenu dismisses the whole menu tree.
        mainMenu.closePolicy = Popup.CloseOnEscape
        mainMenu.highlightedIndex = 2
        themeMenu.x = Math.max(8, mainMenu.x - themeMenu.width + 1)
        const menuHeight = themeMenu.height > 0 ? themeMenu.height : 110
        themeMenu.y = Math.max(8, Math.min(Overlay.overlay.height - menuHeight - 8,
                                            mainMenu.y + 6 + 34 * 2 - 8))
        themeMenu.open()
    }

    MenuPopup {
        id: mainMenu
        parent: Overlay.overlay
        menuWidth: 160
        showIcons: false
        items: [
            { label: "更新记录" },
            { label: "设置" },
            { label: "主题", submenu: true },
            { separator: true },
            { label: "帮助" },
            { label: "关于" }
        ]
        onItemClicked: (index) => {
            if (index === 0) {
                mainMenu.close()
                root.updatesRequested()
            } else if (index === 1) {
                mainMenu.close()
                root.settingsRequested()
            } else if (index === 2) {
                root.openThemeSubmenu()
            } else if (index === 4) {
                mainMenu.close()
                root.helpRequested()
            } else if (index === 5) {
                mainMenu.close()
                root.aboutRequested()
            }
        }
        onItemHovered: (index) => {
            if (index === 2) {
                root.openThemeSubmenu()
            } else if (themeMenu.opened) {
                root.closingSubmenuForHover = true
                themeMenu.close()
            }
        }
        onClosed: {
            if (themeMenu.opened)
                themeMenu.close()
            highlightedIndex = -1
        }
    }

    MenuPopup {
        id: themeMenu
        parent: Overlay.overlay
        menuWidth: 150
        showIcons: false
        checkLeft: true
        items: [
            { label: "浅色", checked: app.themeMode === 0 },
            { label: "深色", checked: app.themeMode === 1 },
            { label: "跟随系统", checked: app.themeMode === 2 }
        ]
        onItemClicked: (index) => {
            root.closingSubmenuForHover = false
            app.themeMode = index
            themeMenu.close()
        }
        onClosed: {
            mainMenu.closePolicy = Popup.CloseOnEscape | Popup.CloseOnPressOutside
            mainMenu.highlightedIndex = -1
            const hoverClose = root.closingSubmenuForHover
            root.closingSubmenuForHover = false
            if (!hoverClose && mainMenu.opened)
                mainMenu.close()
        }
    }
}
