import QtQuick 2.0
import calamares.slideshow 1.0

// melon's install slideshow: dark, quiet, one small logo; the accent green only on the first line
Presentation {
    id: presentation
    function nextSlide() { presentation.goToNextSlide(); }
    Timer { interval: 6000; running: presentation.activatedInCalamares; repeat: true; onTriggered: nextSlide() }
    Slide {
        Rectangle { anchors.fill: parent; color: "#141916" }
        Image { source: "logo.png"; width: 120; height: 120; fillMode: Image.PreserveAspectFit; anchors.centerIn: parent; anchors.verticalCenterOffset: -50 }
        Text { anchors.horizontalCenter: parent.horizontalCenter; y: parent.height / 2 + 40; text: "melon is being installed. You earned it."; font.pixelSize: 20; color: "#e6ece3" }
    }
    Slide {
        Rectangle { anchors.fill: parent; color: "#141916" }
        Text { anchors.centerIn: parent; width: parent.width * 0.8; wrapMode: Text.WordWrap; horizontalAlignment: Text.AlignHCenter; lineHeight: 1.3
               text: "musl, BusyBox, runit and apk underneath.\nWayfire with LXQt's panel on top.\nFlatpak for games."; font.pixelSize: 22; color: "#e6ece3" }
    }
    function onActivate() { }
    function onLeave() { }
}
