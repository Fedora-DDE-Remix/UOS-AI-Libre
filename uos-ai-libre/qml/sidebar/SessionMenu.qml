import UosAiLibre

MenuPopup {
    id: root
    menuWidth: 158
    showIcons: true
    items: [
        { label: "重命名", icon: "edit" },
        { label: "删除", icon: "trash" }
    ]

    signal renameRequested()
    signal deleteRequested()

    onItemClicked: (index) => {
        close()
        if (index === 0)
            root.renameRequested()
        else
            root.deleteRequested()
    }
}
