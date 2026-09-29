// The melon gauntlet page, in two parts.
// 1. The install gauntlet: 200 easy questions, shuffled every run (order and answers). Pick an answer, wait for the
//    countdown, press "Next question". A wrong answer sends you back `config.setback` questions. After the last one
//    config.passed becomes true and Calamares' own Next button unlocks: melon installs.
// 2. The final trial (optional, for the survivor rewards): 50 real questions, shuffled, 90 seconds each. A wrong answer
//    or a timeout sends you back 10; in the last 20 any mistake restarts the finale. Answers are salted md5 hashes.
//    Walking away (Calamares' Next) still installs melon, without the rewards (config.trialPassed stays false).
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "questions.js" as Q
import "trial.js" as T

Rectangle {
    id: page
    color: "#16301c"
    anchors.fill: parent

    readonly property int trialSeconds: 90
    readonly property int finale: 20
    // "install", "choice" (install gauntlet done), "trial", "done" (trial passed)
    property string phase: config.trialPassed ? "done" : config.passed ? "choice" : "install"
    property var order: []           // shuffled question order for this run
    property var torder: []
    property int tindex: 0
    property var shown: []           // the current question's answers, shuffled
    property string chosen: ""
    property int countdown: 0
    property string flash: ""

    readonly property int total: Q.questions.length
    readonly property int ttotal: T.questions.length
    property int index: config.progress
    property var current: phase === "install" && index < total ? Q.questions[order[index]]
                        : phase === "trial" && tindex < ttotal ? T.questions[torder[tindex]] : null

    function shuffled(n) {
        var a = []; for (var i = 0; i < n; i++) a.push(i)
        for (var j = n - 1; j > 0; j--) { var k = Math.floor(Math.random() * (j + 1)); var t = a[j]; a[j] = a[k]; a[k] = t }
        return a
    }
    function shuffle(list) { var o = shuffled(list.length); return o.map(function (i) { return list[i] }) }

    // attention checks ask about an earlier question by number: after shuffling, each comes after the question it
    // names, and its text names that question's new number
    function refOf(q) { var m = /^Attention check: question \d+ asked "(.*)"/.exec(q); return m ? m[1] : "" }
    function placeChecks(o) {
        for (var i = 0; i < o.length; i++) {
            var ref = refOf(Q.questions[o[i]].q); if (ref === "") continue
            for (var j = 0; j < o.length; j++) if (Q.questions[o[j]].q === ref) break
            if (j > i) { var t = o[i]; o[i] = o[j]; o[j] = t; i = -1 }
        }
        return o
    }
    function shownText(c) {
        if (!c || page.phase !== "install") return c ? c.q : ""
        var ref = refOf(c.q); if (ref === "") return c.q
        for (var j = 0; j < order.length; j++) if (Q.questions[order[j]].q === ref) break
        return c.q.replace(/question \d+ asked/, "question " + (j + 1) + " asked")
    }

    function startQuestion() {
        chosen = ""
        shown = current ? shuffle(current.o) : []
        countdown = phase === "trial" ? trialSeconds : config.delay
        tick.restart()
    }

    function submitInstall() {
        if (chosen === current.a) {
            flash = "right"
            if (index + 1 >= total) { config.progress = total; config.passed = true; phase = "choice" }
            else config.progress = index + 1
        } else {
            flash = "wrong"; config.recordMistake()
            config.progress = Math.max(0, index - config.setback)
        }
        flashTimer.restart(); startQuestion()
    }

    function trialMistake(why) {
        config.recordMistake()
        var back = tindex >= ttotal - finale ? ttotal - finale : Math.max(0, tindex - 10)
        flash = tindex >= ttotal - finale ? why + " The finale starts over." : why + " Back " + (tindex - back) + " questions."
        tindex = back
        flashTimer.restart(); startQuestion()
    }

    function submitTrial() {
        if (Qt.md5(current.s + chosen) === current.h) {
            flash = "right"
            if (tindex + 1 >= ttotal) { config.trialPassed = true; phase = "done"; tick.stop(); return }
            tindex = tindex + 1
            flashTimer.restart(); startQuestion()
        } else trialMistake("Wrong.")
    }

    function startTrial() { torder = shuffled(ttotal); tindex = 0; phase = "trial"; startQuestion() }

    Component.onCompleted: { order = placeChecks(shuffled(total)); if (phase === "install") startQuestion() }

    Timer {
        id: tick
        interval: 1000; repeat: true
        onTriggered: {
            if (page.countdown > 0) page.countdown--
            else { stop(); if (page.phase === "trial") page.trialMistake("Time's up.") }
        }
    }
    Timer { id: flashTimer; interval: 1800; onTriggered: page.flash = "" }

    // ---------------------------------------------------------------- questions (both parts)
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 40
        spacing: 18
        visible: page.phase === "install" || page.phase === "trial"

        RowLayout {
            Layout.fillWidth: true
            Label {
                text: page.phase === "trial" ? "The final trial" : "Gauntlet"
                color: page.phase === "trial" ? "#f0d27a" : "#f0a35e"; font.pixelSize: 26; font.bold: true
            }
            Item { Layout.fillWidth: true }
            Label {
                text: page.phase === "trial"
                      ? "question " + (page.tindex + 1) + " of " + page.ttotal + (page.tindex >= page.ttotal - page.finale ? "  (finale: no mistakes)" : "")
                      : "question " + Math.min(page.index + 1, page.total) + " of " + page.total
                color: "#e9f5e1"; font.pixelSize: 16
            }
        }

        ProgressBar {
            Layout.fillWidth: true
            from: 0; to: page.phase === "trial" ? page.ttotal : page.total
            value: page.phase === "trial" ? page.tindex : page.index
        }

        Label {
            Layout.fillWidth: true
            text: page.flash === "right" ? "Correct." : page.flash === "wrong" ? "Wrong. Back " + config.setback + " questions."
                : page.flash !== "" ? page.flash : " "
            color: page.flash === "right" ? "#9fe3a0" : "#ff7b7b"
            font.pixelSize: 16; font.bold: true
        }

        Label {
            Layout.fillWidth: true
            Layout.topMargin: 10
            text: page.shownText(page.current)
            color: "#e9f5e1"; font.pixelSize: 24
            wrapMode: Text.WordWrap
        }

        Repeater {
            model: page.shown
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
                text: page.phase === "trial"
                      ? "time left: " + page.countdown + " s" + (config.mistakes > 0 ? "   ·   mistakes so far: " + config.mistakes : "")
                      : config.mistakes > 0 ? "mistakes so far: " + config.mistakes : ""
                color: page.phase === "trial" && page.countdown <= 15 ? "#ff9b7b" : "#9fb8a0"; font.pixelSize: 14
            }
            Item { Layout.fillWidth: true }
            Button {
                visible: page.phase === "install"
                text: page.countdown > 0 ? "Next question (" + page.countdown + ")" : "Next question"
                enabled: page.countdown === 0 && page.chosen !== ""
                font.pixelSize: 16
                onClicked: page.submitInstall()
            }
            Button {
                visible: page.phase === "trial"
                text: "Answer"
                enabled: page.chosen !== ""
                font.pixelSize: 16
                onClicked: page.submitTrial()
            }
        }
    }

    // ---------------------------------------------------------------- the install gauntlet survived: the choice
    ColumnLayout {
        anchors.centerIn: parent
        width: parent.width * 0.7
        spacing: 16
        visible: page.phase === "choice"
        Label {
            Layout.alignment: Qt.AlignHCenter
            text: "You made it through the gauntlet."
            color: "#f0a35e"; font.pixelSize: 32; font.bold: true
        }
        Label {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap
            text: "melon will install now: press Next.\n\nOr take the final trial first: 50 real questions about Linux, computers and melon, "
                + "90 seconds each, shuffled every time. A wrong answer or running out of time sends you back 10, and the last 20 "
                + "must be flawless. Get through it and you earn the survivor rewards: melon in gold (colours, login screen, "
                + "boot menu), three wallpapers, a certificate and a badge. You can walk away at any point; melon installs either way."
            color: "#e9f5e1"; font.pixelSize: 17
        }
        Button {
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: 10
            text: "Take the final trial"
            font.pixelSize: 18
            onClicked: page.startTrial()
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
        CheckBox {
            Layout.alignment: Qt.AlignHCenter
            checked: config.noRust
            onToggled: config.noRust = checked
            text: "Remove everything built with Rust, and whatever depends on it (no safeguards; earns a wallpaper)"
            font.pixelSize: 16
            contentItem: Text {
                text: parent.text; font: parent.font; color: "#e9f5e1"
                leftPadding: parent.indicator.width + parent.spacing
                verticalAlignment: Text.AlignVCenter
            }
        }
        // the desktop: Plasma from this ISO, or niri + Noctalia from melon's online repository (config.checkNiri)
        onVisibleChanged: if (visible) config.checkNiri()
        Label {
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: 20
            text: "Desktop"
            color: "#f0a35e"; font.pixelSize: 18; font.bold: true
        }
        ButtonGroup { id: desktopChoice }
        RadioButton {
            Layout.alignment: Qt.AlignHCenter
            ButtonGroup.group: desktopChoice
            checked: !config.niri
            onToggled: if (checked) config.niri = false
            text: "KDE Plasma (on this ISO)"
            font.pixelSize: 16
            contentItem: Text {
                text: parent.text; font: parent.font; color: "#e9f5e1"
                leftPadding: parent.indicator.width + parent.spacing
                verticalAlignment: Text.AlignVCenter
            }
        }
        RadioButton {
            Layout.alignment: Qt.AlignHCenter
            ButtonGroup.group: desktopChoice
            enabled: config.niriState === 1
            checked: config.niri
            onToggled: if (checked) config.niri = true
            text: "niri with the Noctalia shell: scrollable tiling (downloaded while melon installs)"
            font.pixelSize: 16
            contentItem: Text {
                text: parent.text; font: parent.font; color: parent.enabled ? "#e9f5e1" : "#7d8c78"
                leftPadding: parent.indicator.width + parent.spacing
                verticalAlignment: Text.AlignVCenter
            }
        }
        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            visible: config.niriState !== 1
            Label {
                text: config.niriState === 0 ? "Checking melon's online repository for niri..."
                    : config.niriState === 2 ? "niri needs a network connection: it comes from melon's online repository."
                    : "This ISO doesn't offer niri."
                color: "#b9c7b3"; font.pixelSize: 14
            }
            Button {
                visible: config.niriState === 2
                text: "Check again"
                onClicked: config.checkNiri()
            }
        }
        Label {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap
            visible: config.niri && config.noRust
            text: "niri and Noctalia are built with Rust: removing everything built with Rust leaves no desktop at all."
            color: "#f0a35e"; font.pixelSize: 15
        }
    }

    // ---------------------------------------------------------------- the trial survived
    ColumnLayout {
        anchors.centerIn: parent
        spacing: 16
        visible: page.phase === "done"
        Label {
            Layout.alignment: Qt.AlignHCenter
            text: "You went through the gauntlet."
            color: "#f0d27a"; font.pixelSize: 34; font.bold: true
        }
        Label {
            Layout.alignment: Qt.AlignHCenter
            text: "This is the other side."
            color: "#f0d27a"; font.pixelSize: 22; font.italic: true
        }
        Label {
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: 10
            horizontalAlignment: Text.AlignHCenter
            text: "Your rewards come with the install: melon in gold (colours, login screen and boot menu),\nthree survivor wallpapers, a certificate in your Pictures folder, and a badge in melonfetch.\n\nPress Next to continue."
            color: "#e9f5e1"; font.pixelSize: 18
        }
    }
}
