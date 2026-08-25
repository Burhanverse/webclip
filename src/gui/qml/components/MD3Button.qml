import QtQuick
import WebClip

Item {
    id: control

    property string text: ""
    property string iconName: ""
    property string variant: "filled"
    signal clicked()

    implicitWidth: contentRow.implicitWidth + (variant === "text" ? 24 : 32)
    implicitHeight: 40

    readonly property color contentColor: {
        if (!control.enabled) return MD3Theme.colorWithAlpha(MD3Theme.onSurface, 0.38)
        if (variant === "filled") return MD3Theme.onPrimary
        if (variant === "tonal") return MD3Theme.onSecondaryContainer
        if (variant === "outlined" || variant === "text") return MD3Theme.primary
        return MD3Theme.primary
    }

    readonly property color buttonBgColor: {
        if (!control.enabled) return variant === "text" || variant === "outlined" ? "transparent" : MD3Theme.colorWithAlpha(MD3Theme.onSurface, 0.12)
        if (variant === "filled") return MD3Theme.primary
        if (variant === "tonal") return MD3Theme.secondaryContainer
        if (variant === "outlined" || variant === "text") return "transparent"
        return MD3Theme.primary
    }

    Rectangle {
        id: bg
        anchors.fill: parent
        radius: 20
        color: control.buttonBgColor

        border.color: (control.variant === "outlined")
            ? (control.enabled ? (mouseArea.containsPress ? MD3Theme.primary : MD3Theme.outline) : MD3Theme.colorWithAlpha(MD3Theme.onSurface, 0.12))
            : "transparent"
        border.width: control.variant === "outlined" ? 1 : 0

        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            color: control.contentColor
            opacity: mouseArea.pressed ? 0.14 : (mouseArea.containsMouse ? 0.08 : 0.0)
            Behavior on opacity { NumberAnimation { duration: 150 } }
        }
    }

    Row {
        id: contentRow
        anchors.centerIn: parent
        spacing: 8

        MD3Icon {
            visible: control.iconName !== ""
            name: control.iconName
            color: control.contentColor
            size: 18
            anchors.verticalCenter: parent.verticalCenter
        }

        Text {
            text: control.text
            font: MD3Theme.labelLarge
            color: control.contentColor
            anchors.verticalCenter: parent.verticalCenter
        }
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        enabled: control.enabled
        hoverEnabled: control.enabled
        cursorShape: control.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
        onClicked: control.clicked()
    }
}
