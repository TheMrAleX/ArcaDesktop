import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Qt5Compat.GraphicalEffects

ItemDelegate {
    id: card
    required property var media
    width: 160; height: 292
    padding: 0
    background: Rectangle { color: "transparent"; radius: 12; border.color: card.activeFocus ? "#8B9CFF" : "transparent"; border.width: 3 }
    contentItem: ColumnLayout {
        spacing: 8
        Rectangle {
            id:cover
            layer.enabled:true;layer.effect:OpacityMask {maskSource:Rectangle {width:cover.width;height:cover.height;radius:12}}
            Layout.fillWidth: true; Layout.preferredHeight: 236
            radius: 12; clip: true
            color: Qt.hsla((card.media.title.length * 0.071) % 1,0.24,0.17,1)
            Image { anchors.fill: parent; source: card.media.poster || card.media.thumb || ""; fillMode: Image.PreserveAspectCrop; asynchronous: true; sourceSize.width: 320; cache: true }
            Rectangle { anchors.fill: parent; color: "transparent"; gradient: Gradient { GradientStop { position: 0.35; color: "transparent" } GradientStop { position: 1; color: "#D90A0B0E" } } }
            Text { anchors { left: parent.left; right: parent.right; bottom: parent.bottom; margins: 14 } text: card.media.title; font.family: "Bricolage Grotesque"; font.pixelSize: 23; font.bold: true; color: "white"; wrapMode: Text.Wrap; maximumLineCount: 4; elide: Text.ElideRight; visible: !card.media.poster }
            Rectangle { visible: !!card.media.watched; anchors { top: parent.top; right: parent.right; margins: 8 } width: 48; height: 26; radius: 13; color: "#173D32"; Text { anchors.centerIn: parent; text: "Visto"; color: "#6FE3C4"; font.pixelSize: 11 } }
            Rectangle { anchors { left: parent.left; bottom: parent.bottom } height: 3; width: parent.width * Math.min(1,(card.media.position || 0) / Math.max(1,card.media.progressDuration || card.media.duration || 1)); color: "#FF9E7A" }
        }
        Text { Layout.fillWidth: true; text: card.media.title; color: "#F4F5F7"; font.pixelSize: 13; font.bold: true; elide: Text.ElideRight }
        Text { Layout.fillWidth: true; text: card.media.kind === "EPISODE" ? "Serie · " + (card.media.episodeCount || 1) + " episodios" : (card.media.year || "Local") + " · " + (card.media.width >= 3200 ? "4K" : card.media.width >= 1800 ? "1080p" : "Vídeo"); color: "#A8AEB8"; font.pixelSize: 11; elide: Text.ElideRight }
    }
}
