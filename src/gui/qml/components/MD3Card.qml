import QtQuick
import WebClip

Rectangle {
    id: root

    property string variant: "outlined"

    radius: MD3Theme.cornerL

    color: {
        if (variant === "elevated") return MD3Theme.surfaceContainerLow
        if (variant === "filled") return MD3Theme.surfaceContainerHighest
        if (variant === "outlined") return MD3Theme.surfaceContainerLowest
        return MD3Theme.surfaceContainer
    }

    border.color: variant === "outlined" ? MD3Theme.outlineVariant : "transparent"
    border.width: variant === "outlined" ? 1 : 0
}
