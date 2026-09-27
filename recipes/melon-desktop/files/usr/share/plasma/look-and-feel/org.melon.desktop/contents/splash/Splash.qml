import QtQuick

Rectangle {
    id: root
    color: "#101512"
    property int stage

    onStageChanged: {
        if (stage == 2) { introAnimation.running = true }
        else if (stage == 5) { introAnimation.target = busyIndicator; introAnimation.from = 1; introAnimation.to = 0; introAnimation.running = true }
    }

    Item {
        id: content
        anchors.fill: parent
        opacity: 0
        Image {
            id: logo
            source: "images/logo.png"
            anchors.centerIn: parent
            anchors.verticalCenterOffset: -height * 0.1
            sourceSize.width: 220; sourceSize.height: 220
        }
        Text {
            anchors.top: logo.bottom; anchors.topMargin: 24
            anchors.horizontalCenter: parent.horizontalCenter
            text: "melon"; color: "#e9f5e1"; font.pointSize: 22; font.bold: true
        }
        Rectangle {
            id: busyIndicator
            anchors.bottom: parent.bottom; anchors.bottomMargin: parent.height / 8
            anchors.horizontalCenter: parent.horizontalCenter
            width: 180; height: 4; radius: 2; color: "#1e2a22"
            Rectangle {
                width: parent.width * Math.min(1, root.stage / 6); height: parent.height; radius: 2
                color: "#f0a35e"
                Behavior on width { NumberAnimation { duration: 300 } }
            }
        }
    }
    OpacityAnimator {
        id: introAnimation
        running: false; target: content; from: 0; to: 1; duration: 600; easing.type: Easing.InOutQuad
    }
}
