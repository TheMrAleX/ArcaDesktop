import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import QtQuick.Dialogs
import QtMultimedia

Rectangle {
    id: view
    color:"#060809"
    required property var media
    property bool controlsVisible:true
    property bool locked:false
    property bool initialized:false
    property var captions:[]
    property string externalSubtitle:""
    property real subtitleDelay:0
    property real subtitleScale:1
    property var externalTracks:library.externalSubtitles(media.path)
    signal closed()
    signal playNext(var file)
    function persist() { if(initialized)library.saveProgress(media.path,player.position,player.duration) }
    function closePlayer(){persist();player.stop();view.Window.window.showNormal();closed()}
    function seek(delta){player.setPosition(Math.max(0,Math.min(player.duration,player.position+delta)))}
    function fullscreen(){let win=view.Window.window;if(win.visibility===Window.FullScreen)win.showNormal();else win.showFullScreen()}
    onMediaChanged:{initialized=false;captions=[];externalSubtitle="";subtitleDelay=Number(library.setting("subDelay/"+media.path,0));player.source=media.url;player.play()}
    Component.onCompleted:{player.source=media.url;player.play();forceActiveFocus()}
    Component.onDestruction:persist()
    focus:true
    Keys.onSpacePressed: player.playbackState===MediaPlayer.PlayingState?player.pause():player.play()
    Keys.onLeftPressed:seek(-10000)
    Keys.onRightPressed:seek(10000)
    Keys.onEscapePressed:closePlayer()
    Keys.onPressed:function(event){if(event.key===Qt.Key_F){fullscreen();event.accepted=true}}
    MediaPlayer {
        id:player
        objectName:"mediaPlayer"
        videoOutput:video
        audioOutput:AudioOutput { id:audio;volume:0.8 }
        onMediaStatusChanged:{
            if(mediaStatus===MediaPlayer.LoadedMedia&&!view.initialized){
                view.initialized=true
                if(view.media.startPosition>0)setPosition(view.media.startPosition)
                let audioIndex=Number(library.setting("audioTrack/"+view.media.path,-2))
                let subIndex=Number(library.setting("subtitleTrack/"+view.media.path,-2))
                if(audioIndex>=0&&audioIndex<audioTracks.length)activeAudioTrack=audioIndex
                if(subIndex>=-1&&subIndex<subtitleTracks.length)activeSubtitleTrack=subIndex
            }
            if(mediaStatus===MediaPlayer.EndOfMedia){view.persist();let next=library.nextEpisode(view.media.path);if(library.setting("autoNext",true)&&next.path){view.persist();view.playNext(next)}}
        }
        onActiveAudioTrackChanged:if(view.initialized)library.setSetting("audioTrack/"+view.media.path,activeAudioTrack)
        onActiveSubtitleTrackChanged:if(view.initialized)library.setSetting("subtitleTrack/"+view.media.path,activeSubtitleTrack)
    }
    VideoOutput {id:video;anchors.fill:parent;fillMode:VideoOutput.PreserveAspectFit}
    Text {anchors {left:parent.left;right:parent.right;bottom:parent.bottom;leftMargin:60;rightMargin:60;bottomMargin:view.controlsVisible?180:40} horizontalAlignment:Text.AlignHCenter;wrapMode:Text.Wrap;text:subtitleText();color:"white";font.pixelSize:26*view.subtitleScale;style:Text.Outline;styleColor:"black";textFormat:Text.PlainText;visible:view.captions.length>0}
    MouseArea {anchors.fill:parent;hoverEnabled:true;onPositionChanged:{view.controlsVisible=true;hideControls.restart()} onClicked:{if(!view.locked)view.controlsVisible=!view.controlsVisible} onDoubleClicked:view.fullscreen()}
    Timer {id:hideControls;interval:3500;running:true;onTriggered:if(player.playbackState===MediaPlayer.PlayingState&&!tracks.opened)view.controlsVisible=false}
    Timer {interval:3000;repeat:true;running:player.playbackState===MediaPlayer.PlayingState;onTriggered:view.persist()}
    Rectangle {anchors {left:parent.left;right:parent.right;bottom:parent.bottom}height:3;color:"#222831";visible:!view.controlsVisible;Rectangle {height:3;width:parent.width*(player.position/Math.max(1,player.duration));color:"#FF9E7A"}}
    ColumnLayout {
        anchors.fill:parent;anchors.margins:24;visible:view.controlsVisible
        RowLayout {Layout.fillWidth:true
            Pill {text:"Volver";onClicked:view.closePlayer()}
            ColumnLayout {Layout.fillWidth:true;Text {text:view.media.title||"";color:"white";font.family:"Bricolage Grotesque";font.pixelSize:24;Layout.fillWidth:true;elide:Text.ElideRight} Text {text:view.media.kind==="EPISODE"?"Temporada "+view.media.season+" · Episodio "+view.media.episode:"Película";color:"#C2C8D0"} }
            Pill {text:view.locked?"Desbloquear":"Bloquear";onClicked:view.locked=!view.locked}
        }
        Item {Layout.fillHeight:true}
        RowLayout {Layout.alignment:Qt.AlignHCenter;spacing:18;enabled:!view.locked
            Pill {text:"−10 s";onClicked:view.seek(-10000)}
            Pill {text:player.playbackState===MediaPlayer.PlayingState?"Pausar":"Reproducir";primary:true;onClicked:player.playbackState===MediaPlayer.PlayingState?player.pause():player.play()}
            Pill {text:"+10 s";onClicked:view.seek(10000)}
        }
        Item {Layout.fillHeight:true}
        Text {Layout.alignment:Qt.AlignHCenter;visible:library.setting("autoNext",true)&&player.duration-player.position<20000&&!!library.nextEpisode(view.media.path).path;text:"Siguiente episodio en "+Math.ceil((player.duration-player.position)/1000)+" s";color:"#FF9E7A"}
        RowLayout {Layout.fillWidth:true;enabled:!view.locked
            Text {text:formatTime(player.position);color:"white"}
            Slider {id:timeline;Layout.fillWidth:true;from:0;to:Math.max(1,player.duration);value:player.position;onMoved:player.setPosition(value)}
            Text {text:"−"+formatTime(player.duration-player.position);color:"#C2C8D0"}
        }
        RowLayout {enabled:!view.locked
            Pill {text:"Pistas";onClicked:tracks.open()}
            ComboBox {model:["0.5×","0.75×","1×","1.25×","1.5×","2×"];currentIndex:2;onActivated:player.playbackRate=[0.5,0.75,1,1.25,1.5,2][currentIndex]}
            Text {text:"Volumen";color:"#C2C8D0"}
            Slider {from:0;to:1;value:audio.volume;onMoved:audio.volume=value;Layout.preferredWidth:120}
            Item {Layout.fillWidth:true}
            Pill {visible:!!library.nextEpisode(view.media.path).path;text:"Siguiente";onClicked:{view.persist();view.playNext(library.nextEpisode(view.media.path))}}
            Pill {text:"Pantalla completa";onClicked:view.fullscreen()}
        }
    }
    Rectangle {anchors.centerIn:parent;width:Math.min(parent.width-80,600);height:230;radius:18;color:"#15181D";border.color:"#FF9E7A";visible:player.error!==MediaPlayer.NoError
        ColumnLayout {anchors.fill:parent;anchors.margins:24;spacing:12
            Text {text:"No puedo con este vídeo";color:"white";font.pixelSize:24;font.bold:true}
            Text {text:player.errorString+"\nComprueba que el archivo siga disponible y que FFmpeg esté instalado.";color:"#C2C8D0";wrapMode:Text.Wrap;Layout.fillWidth:true}
            Text {text:view.media.name||"";color:"#8A919B";font.family:"monospace";elide:Text.ElideMiddle;Layout.fillWidth:true}
            Pill {text:"Volver";onClicked:view.closePlayer()}
        }
    }
    Drawer {
        id:tracks;edge:Qt.RightEdge;width:360;height:parent.height;modal:true
        background:Rectangle {color:"#15181D"}
        ColumnLayout {anchors.fill:parent;anchors.margins:24;spacing:14
            Text {text:"Audio y subtítulos";color:"white";font.pixelSize:23;font.bold:true}
            Text {text:"Audio";color:"#8B9CFF"}
            Repeater {model:player.audioTracks;Pill {required property var modelData;required property int index;text:(modelData.Language||modelData.language||("Pista "+(index+1)))+"";selected:player.activeAudioTrack===index;Layout.fillWidth:true;onClicked:player.activeAudioTrack=index}}
            Text {text:"Subtítulos";color:"#8B9CFF"}
            Pill {text:"Desactivados";selected:player.activeSubtitleTrack===-1&&!view.captions.length;Layout.fillWidth:true;onClicked:{player.activeSubtitleTrack=-1;view.captions=[];view.externalSubtitle=""}}
            Repeater {model:player.subtitleTracks;Pill {required property var modelData;required property int index;text:(modelData.Language||modelData.language||("Pista "+(index+1)))+"";selected:player.activeSubtitleTrack===index;Layout.fillWidth:true;onClicked:{view.captions=[];view.externalSubtitle="";player.activeSubtitleTrack=index}}}
            Repeater {model:view.externalTracks;Pill {required property var modelData;text:modelData.name;selected:view.externalSubtitle===modelData.url.toString();Layout.fillWidth:true;onClicked:view.loadSubtitle(modelData.url)}}
            Pill {text:"Abrir .srt o .ass";Layout.fillWidth:true;onClicked:subtitlePicker.open()}
            RowLayout {visible:view.captions.length>0;Pill {text:"−0,2 s";onClicked:view.changeDelay(-0.2)} Text {text:view.subtitleDelay.toFixed(1)+" s";color:"white"} Pill {text:"+0,2 s";onClicked:view.changeDelay(0.2)}}
            RowLayout {visible:view.captions.length>0;Repeater {model:["S","M","L"];Pill {required property string modelData;required property int index;text:modelData;onClicked:view.subtitleScale=[0.8,1,1.3][index]}}}
            Item {Layout.fillHeight:true}
            Pill {text:"Cerrar";onClicked:tracks.close()}
        }
    }
    FileDialog {id:subtitlePicker;title:"Abrir subtítulos";nameFilters:["Subtítulos (*.srt *.ass *.ssa *.vtt)"];onAccepted:view.loadSubtitle(selectedFile)}
    function loadSubtitle(url){captions=library.readSubtitles(url);externalSubtitle=url.toString();player.activeSubtitleTrack=-1}
    function changeDelay(delta){subtitleDelay+=delta;library.setSetting("subDelay/"+media.path,subtitleDelay)}
    function subtitleText(){let position=player.position-view.subtitleDelay*1000;let output=[];for(let c of captions){if(c.start>position)break;if(c.end>position)output.push(c.text)}return output.join("\n")}
    function formatTime(ms){let s=Math.max(0,Math.floor(ms/1000));return Math.floor(s/3600)+":"+String(Math.floor(s/60)%60).padStart(2,"0")+":"+String(s%60).padStart(2,"0")}
}
