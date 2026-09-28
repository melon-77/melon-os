// The melon gauntlet page. One easy question per screen. Pick an answer, wait for the countdown,
// press "Next question". A wrong answer sends you back `config.setback` questions. After the last one,
// config.passed becomes true and Calamares' own Next button unlocks.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "questions.js" as Q

Rectangle {
    id: page
    color: "#16301c"
    anchors.fill: parent

    readonly property int total: Q.questions.length
    property int index: config.progress
    property var current: index < total ? Q.questions[index] : null
    property string chosen: ""
    property int countdown: config.delay
    property string flash: ""          // "right" | "wrong" | ""

    function startQuestion() {
        chosen = ""
        countdown = config.delay
        tick.restart()
    }

    function submit() {
        if (chosen === current.a) {
            flash = "right"
            if (index + 1 >= total) {
                config.progress = total
                config.passed = true
            } else {
                config.progress = index + 1
            }
        } else {
            flash = "wrong"
            config.recordMistake()
            config.progress = Math.max(0, index - config.setback)
        }
        flashTimer.restart()
        startQuestion()
    }

    Component.onCompleted: startQuestion()

    Timer {
        id: tick
        interval: 1000; repeat: true
        onTriggered: { if (page.countdown > 0) page.countdown--; else stop() }
    }
    Timer { id: flashTimer; interval: 1600; onTriggered: page.flash = "" }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 40
        spacing: 18
        visible: !config.passed

        RowLayout {
            Layout.fillWidth: true
            Label {
                text: "Gauntlet"
                color: "#f0a35e"; font.pixelSize: 26; font.bold: true
            }
            Item { Layout.fillWidth: true }
            Label {
                text: "question " + Math.min(page.index + 1, page.total) + " of " + page.total
                color: "#e9f5e1"; font.pixelSize: 16
            }
        }

        ProgressBar {
            Layout.fillWidth: true
            from: 0; to: page.total; value: page.index
        }

        Label {
            Layout.fillWidth: true
            text: page.flash === "wrong" ? "Wrong. Back " + config.setback + " questions."
                : page.flash === "right" ? "Correct." : " "
            color: page.flash === "wrong" ? "#ff7b7b" : "#9fe3a0"
            font.pixelSize: 16; font.bold: true
        }

        Label {
            Layout.fillWidth: true
            Layout.topMargin: 10
            text: page.current ? page.current.q : ""
            color: "#e9f5e1"; font.pixelSize: 24
            wrapMode: Text.WordWrap
        }

        Repeater {
            model: page.current ? page.current.o : []
            delegate: RadioButton {
                required property string modelData
                text: modelData
                checked: page.chosen === modelData
                onClicked: page.chosen = modelData
                font.pixelSize: 18
                contentItem: Text {
                    text: parent.text; font: parent.font; color: "#e9f5e1"
                    leftPadding: parent.indicator.width + parent.spacing
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }

        Item { Layout.fillHeight: true }

        RowLayout {
            Layout.fillWidth: true
            Label {
                text: config.mistakes > 0 ? "mistakes so far: " + config.mistakes : ""
                color: "#9fb8a0"; font.pixelSize: 14
            }
            Item { Layout.fillWidth: true }
            Button {
                text: page.countdown > 0 ? "Next question (" + page.countdown + ")" : "Next question"
                enabled: page.countdown === 0 && page.chosen !== ""
                font.pixelSize: 16
                onClicked: page.submit()
            }
        }
    }

    // the end
    ColumnLayout {
        anchors.centerIn: parent
        spacing: 16
        visible: config.passed
        Label {
            Layout.alignment: Qt.AlignHCenter
            text: "You survived the gauntlet."
            color: "#f0a35e"; font.pixelSize: 32; font.bold: true
        }
        Label {
            Layout.alignment: Qt.AlignHCenter
            horizontalAlignment: Text.AlignHCenter
            text: "Your rewards come with the install:\nthe survivor wallpaper, a certificate in your Pictures folder,\nand a badge in melonfetch."
            color: "#e9f5e1"; font.pixelSize: 18
        }
        CheckBox {
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: 20
            checked: config.alpine
            onToggled: config.alpine = checked
            text: "Also offer Alpine's packages (opt-in, as apk add name@alpine)"
            font.pixelSize: 16
            contentItem: Text {
                text: parent.text; font: parent.font; color: "#e9f5e1"
                leftPadding: parent.indicator.width + parent.spacing
                verticalAlignment: Text.AlignVCenter
            }
        }
        Label {
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: 10
            text: "Press Next to continue."
            color: "#e9f5e1"; font.pixelSize: 18
        }
    }
}
