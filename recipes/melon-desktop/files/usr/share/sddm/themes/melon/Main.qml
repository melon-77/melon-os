// melon's login screen (SDDM, Qt 6): the melon planet, a clock, the user, a password field.
// The survivor edition (melon-gold) is the same file with another theme.conf (background, accent).
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Rectangle {
    id: root
    width: 1920; height: 1080
    color: "#0a0e0c"

    readonly property color accent: config.accent || "#6fbf4a"
    readonly property color text: "#eef6e8"
    property string message: ""

    Connections {
        target: sddm
        function onLoginFailed() { root.message = "That password didn't work. Try again."; password.text = ""; password.forceActiveFocus() }
        function onLoginSucceeded() { root.message = "" }
    }

    Image {
        anchors.fill: parent
        source: config.background
        fillMode: Image.PreserveAspectCrop
        asynchronous: false
    }
    Rectangle {   // a soft shade behind the text, lighter over the picture's bright corner
        anchors.fill: parent
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: "#b0070a08" }
            GradientStop { position: 0.55; color: "#60070a08" }
            GradientStop { position: 1.0; color: "#10070a08" }
        }
    }

    ColumnLayout {
        anchors.left: parent.left
        anchors.leftMargin: parent.width * 0.09
        anchors.verticalCenter: parent.verticalCenter
        spacing: 0
        width: Math.min(parent.width * 0.36, 560)

        Text {
            id: clock
            text: Qt.formatTime(new Date(), "hh:mm")
            color: root.text; font.pixelSize: root.height * 0.11; font.weight: Font.Light
            Timer { interval: 1000; running: true; repeat: true; onTriggered: { clock.text = Qt.formatTime(new Date(), "hh:mm"); date.text = Qt.formatDate(new Date(), "dddd, d MMMM") } }
        }
        Text {
            id: date
            text: Qt.formatDate(new Date(), "dddd, d MMMM")
            color: root.text; opacity: 0.8; font.pixelSize: root.height * 0.026
            Layout.bottomMargin: root.height * 0.06
        }

        // the user: a name, or a choice when there are several
        ComboBox {
            id: user
            Layout.fillWidth: true
            model: userModel
            textRole: "realName"
            valueRole: "name"
            currentIndex: userModel.lastIndex >= 0 ? userModel.lastIndex : 0
            enabled: count > 1
            font.pixelSize: root.height * 0.024
            indicator.visible: count > 1
            background: Item {}
            contentItem: Text {
                text: user.displayText !== "" ? user.displayText : user.currentValue
                color: root.text; font.pixelSize: user.font.pixelSize; font.weight: Font.DemiBold
                verticalAlignment: Text.AlignVCenter
            }
        }

        TextField {
            id: password
            Layout.fillWidth: true
            Layout.topMargin: 12
            echoMode: TextInput.Password
            placeholderText: "Password"
            placeholderTextColor: "#99b0a0"
            color: root.text
            font.pixelSize: root.height * 0.022
            padding: root.height * 0.014
            focus: true
            background: Rectangle {
                radius: 12
                color: "#99101612"
                border.color: password.activeFocus ? root.accent : "#40ffffff"
                border.width: 2
            }
            Keys.onReturnPressed: root.login()
            Keys.onEnterPressed: root.login()
            Component.onCompleted: forceActiveFocus()
        }

        Text {
            text: root.message
            visible: root.message !== ""
            color: "#ff9b8b"; font.pixelSize: root.height * 0.018
            Layout.topMargin: 10
        }

        Button {
            id: go
            Layout.topMargin: 16
            text: "Log in"
            font.pixelSize: root.height * 0.02
            onClicked: root.login()
            contentItem: Text { text: go.text; font: go.font; color: "#0b150c"; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
            background: Rectangle { implicitWidth: root.height * 0.14; implicitHeight: root.height * 0.05; radius: 12; color: go.down ? Qt.darker(root.accent, 1.2) : root.accent }
        }
    }

    // the session, and power, along the bottom
    RowLayout {
        anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
        anchors.margins: root.height * 0.03
        spacing: 18
        ComboBox {
            id: session
            model: sessionModel
            textRole: "name"
            currentIndex: sessionModel.lastIndex >= 0 ? sessionModel.lastIndex : 0
            font.pixelSize: root.height * 0.016
            implicitWidth: root.height * 0.3
            background: Rectangle { radius: 8; color: "#66101612"; border.color: "#30ffffff" }
            contentItem: Text { text: session.displayText; color: root.text; font: session.font; leftPadding: 10; verticalAlignment: Text.AlignVCenter }
        }
        Item { Layout.fillWidth: true }
        Repeater {
            model: [ { label: "Sleep", can: sddm.canSuspend, act: function () { sddm.suspend() } },
                     { label: "Restart", can: sddm.canReboot, act: function () { sddm.reboot() } },
                     { label: "Shut down", can: sddm.canPowerOff, act: function () { sddm.powerOff() } } ]
            delegate: Button {
                required property var modelData
                visible: modelData.can
                text: modelData.label
                font.pixelSize: root.height * 0.016
                onClicked: modelData.act()
                contentItem: Text { text: parent.text; font: parent.font; color: root.text; horizontalAlignment: Text.AlignHCenter }
                background: Rectangle { radius: 8; color: parent.hovered ? "#33ffffff" : "transparent"; implicitWidth: root.height * 0.1; implicitHeight: root.height * 0.038 }
            }
        }
    }

    function login() {
        root.message = ""
        sddm.login(user.currentValue, password.text, session.currentIndex)
    }
}
