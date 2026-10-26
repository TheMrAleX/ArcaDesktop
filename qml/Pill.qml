import QtQuick
import QtQuick.Controls

Button {
    id: control
    property bool primary: false
    property bool selected: false
    implicitHeight: 46
    leftPadding: 22; rightPadding: 22
    font.family: "Manrope"; font.pixelSize: 13; font.bold: true
    contentItem: Text {
        text: control.text; font: control.font
        color: control.primary ? "#0A0B0E" : control.selected ? "#8B9CFF" : "#F4F5F7"
        horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
    background: Rectangle {
        radius: height / 2
        color: control.primary ? (control.down ? "#AFBBFF" : "#8B9CFF") : control.selected ? "#242B49" : control.hovered ? "#292D36" : "#1A1D24"
        border.color: control.activeFocus ? "#8B9CFF" : control.selected ? "#5966A8" : "#2A2D35"
        border.width: control.activeFocus ? 3 : 1
        Behavior on color { ColorAnimation { duration: 120 } }
    }
    opacity: enabled ? 1 : 0.45
}
