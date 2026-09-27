import QtQuick 2.0
import calamares.slideshow 1.0

Presentation {
    id: presentation
    function nextSlide() { presentation.goToNextSlide(); }
    Timer { interval: 6000; running: presentation.activatedInCalamares; repeat: true; onTriggered: nextSlide() }
    Slide {
        Image { source: "welcome.png"; width: 260; height: 260; fillMode: Image.PreserveAspectFit; anchors.centerIn: parent; anchors.verticalCenterOffset: -40 }
        Text { anchors.horizontalCenter: parent.horizontalCenter; y: parent.height - 90; text: "melon is being installed. You earned it."; font.pixelSize: 20; color: "#1d3b22" }
    }
    Slide {
        Text { anchors.centerIn: parent; width: parent.width * 0.8; wrapMode: Text.WordWrap; horizontalAlignment: Text.AlignHCenter
               text: "musl, BusyBox, runit and apk underneath.\nKDE Plasma on Wayland on top.\nFlatpak for games."; font.pixelSize: 22; color: "#1d3b22" }
    }
    function onActivate() { }
    function onLeave() { }
}
