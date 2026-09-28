import QtQuick

// melon's loading screen: the planet from the default wallpaper eases up into place while Plasma starts,
// so the desktop takes over from the same picture; a thin progress bar follows KSplash's stages (1-6)
Rectangle {
    id: root
    color: "#0a0e0c"
    property int stage

    Image {
        id: planet
        anchors.fill: parent
        source: "images/splash.jpg"
        fillMode: Image.PreserveAspectCrop
        opacity: 0
        scale: 1.08
        transformOrigin: Item.BottomRight
        Component.onCompleted: rise.start()
        ParallelAnimation {
            id: rise
            OpacityAnimator { target: planet; from: 0; to: 1; duration: 900; easing.type: Easing.OutQuad }
            ScaleAnimator { target: planet; from: 1.08; to: 1.0; duration: 3200; easing.type: Easing.OutCubic }
        }
    }

    Column {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: parent.height * 0.1
        spacing: 16
        opacity: 0
        OpacityAnimator on opacity { from: 0; to: 1; duration: 1200; easing.type: Easing.InOutQuad }

        Text {
            readonly property int px: Math.round(root.height * 0.022)
            anchors.horizontalCenter: parent.horizontalCenter
            text: "melon"
            color: "#d6ecc8"
            opacity: 0.85
            font.pixelSize: px
            font.letterSpacing: px * 0.45
            font.weight: Font.Light
        }
        Rectangle {
            width: Math.round(root.width * 0.14); height: 3; radius: 1.5
            color: "#33ffffff"
            Rectangle {
                width: parent.width * Math.min(1, root.stage / 6); height: parent.height; radius: 1.5
                color: "#8fd46a"
                Behavior on width { NumberAnimation { duration: 450; easing.type: Easing.OutCubic } }
            }
        }
    }
}
