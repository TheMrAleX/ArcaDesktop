import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import QtQuick.Dialogs
import Arca.Native 1.0

Rectangle {
    id: view
    color: "#060809"
    required property var media
    property bool controlsVisible: true
    property bool locked: false
    property bool initialized: false
    property bool defaultsApplied: false
    property bool pip: false
    property rect savedGeometry
    property real brightness: 1
    property string feedback: ""
    property int skipMs: Number(library.setting("skipSeconds",10))*1000
    property var audioOptions: player.audioTracks
    property var subtitleOptions: player.subtitleTracks
    property var next: library.nextEpisode(media.path)
    signal closed()
    signal playNext(var file)

    function persist() { if(initialized)library.saveProgress(media.path,player.position,player.duration) }
    function closePlayer(){persist();if(pip)togglePip();player.stop();view.Window.window.showNormal();closed()}
    function seek(delta){player.setPosition(Math.max(0,Math.min(player.duration,player.position+delta)));feedback=(delta<0?"−":"+")+Math.abs(delta/1000)+" s";feedbackTimer.restart()}
    function fullscreen(){if(pip)togglePip();let win=view.Window.window;if(win.visibility===Window.FullScreen)win.showNormal();else win.showFullScreen()}
    function togglePip(){
        let win=view.Window.window
        if(!pip){savedGeometry=Qt.rect(win.x,win.y,win.width,win.height);win.showNormal();win.flags=win.flags|Qt.WindowStaysOnTopHint;pip=true;win.width=480;win.height=300}
        else{win.flags=win.flags&~Qt.WindowStaysOnTopHint;win.x=savedGeometry.x;win.y=savedGeometry.y;win.width=savedGeometry.width;win.height=savedGeometry.height;pip=false}
        win.show();controlsVisible=true
    }
    function trackLabel(track){return library.trackLanguageName(track.lang)+(track.channels?" · "+(track.channels===6?"5.1":track.channels===8?"7.1":track.channels+".0"):"")+(track.forced?" · Forzados":"")+(track.title?" · "+track.title:"")+(track.external?" · externo":"")}
    function chooseAudio(index,save){if(index<0||index>=audioOptions.length)return;player.activeAudioTrack=index;if(save)library.setPlaybackSetting("audioChoice",media.path,library.encodeTrack(audioOptions[index]))}
    function chooseSubtitle(index,save){
        player.activeSubtitleTrack=index
        if(save){library.setPlaybackSetting("subtitleChoice",media.path,index<0?"off":library.encodeTrack(subtitleOptions[index]));library.setPlaybackSetting("externalSubtitleUrl",media.path,index>=0&&subtitleOptions[index].external?subtitleOptions[index].url.toString():"")}
    }
    function applyDefaults(){
        if(!initialized||defaultsApplied)return;defaultsApplied=true
        let audioIndex=library.pickTrack(audioOptions,String(library.playbackSetting("audioChoice",media.path,"")))
        if(audioIndex<0)audioIndex=library.preferredTrack(audioOptions,false)
        if(audioIndex<0)audioIndex=player.activeAudioTrack
        chooseAudio(audioIndex,false)
        let saved=String(library.playbackSetting("subtitleChoice",media.path,""))
        if(saved==="off"){chooseSubtitle(-1,false);return}
        let subIndex=library.pickTrack(subtitleOptions,saved)
        let external=String(library.playbackSetting("externalSubtitleUrl",media.path,""))
        if(subIndex<0&&external){player.addSubtitle(external);return}
        if(subIndex<0){let preferred=String(library.setting("languages","es,en")).split(',')[0];let lang=audioOptions[audioIndex]?audioOptions[audioIndex].lang:"";subIndex=library.preferredTrack(subtitleOptions,!!lang&&lang.split('-')[0]===preferred.split('-')[0])}
        chooseSubtitle(subIndex,false)
    }
    function time(ms){let s=Math.max(0,Math.floor(ms/1000));return (s>=3600?Math.floor(s/3600)+":":"")+String(Math.floor(s/60)%60).padStart(2,"0")+":"+String(s%60).padStart(2,"0")}
    function start(){initialized=false;defaultsApplied=false;player.source=media.url;player.play()}
    onMediaChanged:start()
    Component.onCompleted:{start();forceActiveFocus()}
    Component.onDestruction:persist()
    focus:true
    Keys.onSpacePressed:if(!locked)player.playbackState===NativeVideo.PlayingState?player.pause():player.play()
    Keys.onLeftPressed:if(!locked)seek(-skipMs)
    Keys.onRightPressed:if(!locked)seek(skipMs)
    Keys.onEscapePressed:if(pip)togglePip();else closePlayer()
    Keys.onPressed:function(event){if(event.key===Qt.Key_F&&!locked){fullscreen();event.accepted=true}}

    NativeVideo {
        id:player;objectName:"mediaPlayer";anchors.fill:parent
        mediaTitle:view.media.title||"";hasNext:!!view.next.path
        onNextRequested:{view.persist();view.playNext(view.next)}
        hardware:library.setting("decoder","HARDWARE")==="HARDWARE"
        subtitleDelay:Number(library.playbackSetting("subDelay",view.media.path,0))
        subtitleScale:Number(library.setting("subtitleScale",1))
        volume:Number(library.setting("volume",0.8))
        onMediaStatusChanged:{
            if(mediaStatus===NativeVideo.LoadedMedia&&!view.initialized){view.initialized=true;if(view.media.startPosition>0)setPosition(view.media.startPosition);chooseDefaults.restart()}
            if(mediaStatus===NativeVideo.EndOfMedia){view.persist();if(library.setting("autoNext",true)&&view.next.path)view.playNext(view.next)}
        }
        onTracksChanged:if(view.initialized&&!view.defaultsApplied)chooseDefaults.restart()
    }
    Rectangle {anchors.fill:parent;color:"black";opacity:(1-view.brightness)*0.85}
    Timer {id:chooseDefaults;interval:100;onTriggered:view.applyDefaults()}
    Timer {id:feedbackTimer;interval:900;onTriggered:view.feedback=""}
    Timer {id:hideControls;interval:3500;running:true;onTriggered:if(player.playbackState===NativeVideo.PlayingState&&!tracks.opened)view.controlsVisible=false}
    Timer {interval:3000;repeat:true;running:player.playbackState===NativeVideo.PlayingState;onTriggered:view.persist()}
    MouseArea {
        anchors.fill:parent;hoverEnabled:true
        property point origin
        property real initialVolume
        property real initialBrightness
        property real initialPosition
        property bool dragging:false
        property int direction:0
        onPressed:function(mouse){origin=Qt.point(mouse.x,mouse.y);initialVolume=player.volume;initialBrightness=view.brightness;initialPosition=player.position;dragging=false;direction=0}
        onPositionChanged:function(mouse){
            view.controlsVisible=true;hideControls.restart()
            if(!pressed||view.locked)return
            let dx=mouse.x-origin.x,dy=mouse.y-origin.y
            if(!direction&&Math.max(Math.abs(dx),Math.abs(dy))>14){direction=Math.abs(dx)>Math.abs(dy)?1:2;dragging=true}
            if(direction===1)view.feedback=view.time(Math.max(0,Math.min(player.duration,initialPosition+dx/width*120000)))
            else if(direction===2){let level=Math.max(0,Math.min(1,(origin.x<width/2?initialBrightness:initialVolume)-dy/height));if(origin.x<width/2){view.brightness=level;view.feedback="Brillo "+Math.round(level*100)+" %"}else{player.volume=level;view.feedback="Volumen "+Math.round(level*100)+" %"}}
        }
        onReleased:function(mouse){if(dragging&&!view.locked){if(direction===1)player.setPosition(Math.max(0,Math.min(player.duration,initialPosition+(mouse.x-origin.x)/width*120000)));else library.setSetting("volume",player.volume);feedbackTimer.restart()}}
        onClicked:if(!view.locked&&!dragging)view.controlsVisible=!view.controlsVisible
        onDoubleClicked:function(mouse){if(!view.locked){if(mouse.x<width*0.35)view.seek(-view.skipMs);else if(mouse.x>width*0.65)view.seek(view.skipMs);else view.fullscreen()}}
        onWheel:function(wheel){if(!view.locked){player.volume=Math.max(0,Math.min(1,player.volume+wheel.angleDelta.y/1200));library.setSetting("volume",player.volume)}}
    }
    Rectangle {anchors.centerIn:parent;visible:view.feedback.length>0;width:feedbackText.implicitWidth+36;height:54;radius:27;color:"#CF15181D";Text {id:feedbackText;anchors.centerIn:parent;text:view.feedback;color:"white";font.pixelSize:20;font.bold:true}}
    Rectangle {anchors {left:parent.left;right:parent.right;bottom:parent.bottom}height:3;color:"#222831";visible:!view.controlsVisible&&library.setting("playerProgressBar",true);Rectangle {height:3;width:parent.width*(player.position/Math.max(1,player.duration));color:"#FF9E7A"}}
    Rectangle {anchors {top:parent.top;left:parent.left;right:parent.right}height:150;visible:view.controlsVisible;gradient:Gradient {GradientStop {position:0;color:"#B0000000"}GradientStop {position:1;color:"transparent"}}}
    Rectangle {anchors {bottom:parent.bottom;left:parent.left;right:parent.right}height:210;visible:view.controlsVisible;gradient:Gradient {GradientStop {position:0;color:"transparent"}GradientStop {position:1;color:"#E0000000"}}}
    ColumnLayout {
        anchors.fill:parent;anchors.margins:view.pip?10:24;visible:view.controlsVisible;spacing:12
        RowLayout {Layout.fillWidth:true
            Pill {text:view.pip?"Ampliar":"Volver";onClicked:view.pip?view.togglePip():view.closePlayer()}
            ColumnLayout {Layout.fillWidth:true;Text {text:view.media.title||"";color:"white";font.family:"Bricolage Grotesque";font.pixelSize:view.pip?15:24;Layout.fillWidth:true;elide:Text.ElideRight} Text {visible:!view.pip;text:view.media.kind==="EPISODE"?"Temporada "+view.media.season+" · Episodio "+view.media.episode:"Película";color:"#C2C8D0"} }
            Pill {visible:!view.pip;text:view.locked?"Desbloquear":"Bloquear";onClicked:view.locked=!view.locked}
        }
        Item {Layout.fillHeight:true}
        RowLayout {Layout.alignment:Qt.AlignHCenter;spacing:18;enabled:!view.locked
            Pill {text:"−"+(view.skipMs/1000)+" s";onClicked:view.seek(-view.skipMs)}
            Pill {text:player.playbackState===NativeVideo.PlayingState?"Pausar":"Reproducir";primary:true;onClicked:player.playbackState===NativeVideo.PlayingState?player.pause():player.play()}
            Pill {text:"+"+(view.skipMs/1000)+" s";onClicked:view.seek(view.skipMs)}
        }
        Item {Layout.fillHeight:true}
        Text {visible:!view.pip&&library.setting("autoNext",true)&&player.duration-player.position<20000&&!!view.next.path;Layout.alignment:Qt.AlignHCenter;text:"Siguiente episodio en "+Math.ceil((player.duration-player.position)/1000)+" s";color:"#FF9E7A"}
        RowLayout {Layout.fillWidth:true;enabled:!view.locked
            Text {text:view.time(player.position);color:"white"}
            Slider {Layout.fillWidth:true;from:0;to:Math.max(1,player.duration);value:player.position;onMoved:player.setPosition(value)}
            Text {text:"−"+view.time(player.duration-player.position);color:"#C2C8D0"}
        }
        Flow {visible:!view.pip;Layout.fillWidth:true;spacing:8;enabled:!view.locked
            Pill {text:"Pistas";onClicked:tracks.open()}
            ComboBox {model:["0.5×","0.75×","1×","1.25×","1.5×","2×"];currentIndex:2;onActivated:player.playbackRate=[0.5,0.75,1,1.25,1.5,2][currentIndex]}
            Slider {width:110;from:0;to:1;value:player.volume;onMoved:{player.volume=value;library.setSetting("volume",value)}}
            Pill {text:["Ajustar","Llenar","Estirar"][player.fitMode];onClicked:player.fitMode=(player.fitMode+1)%3}
            Pill {text:"Ventana flotante";onClicked:view.togglePip()}
            Pill {visible:!!view.next.path;text:"Siguiente";onClicked:{view.persist();view.playNext(view.next)}}
            Pill {text:"Pantalla completa";onClicked:view.fullscreen()}
            ComboBox {visible:player.screens().length>1;model:player.screens();onActivated:player.moveToScreen(currentIndex)}
        }
    }
    Rectangle {anchors.centerIn:parent;width:Math.min(parent.width-60,640);height:310;radius:18;color:"#15181D";border.color:"#FF9E7A";visible:player.error!==NativeVideo.NoError
        ColumnLayout {anchors.fill:parent;anchors.margins:24;spacing:12
            Text {text:"No puedo con este vídeo";color:"white";font.pixelSize:24;font.bold:true}
            Text {text:player.errorString;color:"#C2C8D0";wrapMode:Text.Wrap;maximumLineCount:6;elide:Text.ElideRight;Layout.fillWidth:true}
            Text {text:view.media.name||"";color:"#8A919B";font.family:"monospace";elide:Text.ElideMiddle;Layout.fillWidth:true}
            Flow {Layout.fillWidth:true;spacing:8;Pill {text:"Probar por software";onClicked:player.retrySoftware()} Pill {text:"Volver";onClicked:view.closePlayer()}}
        }
    }
    Drawer {
        id:tracks;edge:Qt.RightEdge;width:Math.min(410,parent.width);height:parent.height;modal:true
        background:Rectangle {color:"#15181D"}
        ScrollView {anchors.fill:parent;contentWidth:availableWidth;leftPadding:20;rightPadding:20;topPadding:24
            ColumnLayout {width:parent.width;spacing:14
                Text {text:"Audio y subtítulos";color:"white";font.family:"Bricolage Grotesque";font.pixelSize:23;font.bold:true}
                Text {text:"AUDIO";color:"#8B9CFF";font.letterSpacing:2;font.pixelSize:11}
                Repeater {model:view.audioOptions;Pill {required property var modelData;required property int index;text:view.trackLabel(modelData);selected:player.activeAudioTrack===index;Layout.fillWidth:true;onClicked:view.chooseAudio(index,true)}}
                Text {text:"SUBTÍTULOS";color:"#8B9CFF";font.letterSpacing:2;font.pixelSize:11}
                Pill {text:"Desactivados";selected:player.activeSubtitleTrack<0;Layout.fillWidth:true;onClicked:view.chooseSubtitle(-1,true)}
                Repeater {model:view.subtitleOptions;Pill {required property var modelData;required property int index;text:view.trackLabel(modelData);selected:player.activeSubtitleTrack===index;Layout.fillWidth:true;onClicked:view.chooseSubtitle(index,true)}}
                Pill {text:"Abrir .srt o .ass";Layout.fillWidth:true;onClicked:subtitlePicker.open()}
                Text {text:"RETARDO";color:"#8A919B";font.pixelSize:11}
                RowLayout {Pill {text:"−0,1 s";onClicked:{player.subtitleDelay-=0.1;library.setPlaybackSetting("subDelay",view.media.path,player.subtitleDelay)}} Text {text:player.subtitleDelay.toFixed(1)+" s";color:"white"} Pill {text:"+0,1 s";onClicked:{player.subtitleDelay+=0.1;library.setPlaybackSetting("subDelay",view.media.path,player.subtitleDelay)}}}
                Text {text:"Si van adelantados, réstale.";color:"#8A919B";font.pixelSize:11}
                RowLayout {Repeater {model:["S","M","L"];Pill {required property string modelData;required property int index;text:modelData;selected:Math.abs(player.subtitleScale-[0.8,1,1.3][index])<0.01;onClicked:{player.subtitleScale=[0.8,1,1.3][index];library.setSetting("subtitleScale",player.subtitleScale)}}}}
                Pill {text:player.hardware?"Decodificación: automática":"Decodificación: software";Layout.fillWidth:true;onClicked:{player.hardware=!player.hardware;library.setSetting("decoder",player.hardware?"HARDWARE":"SOFTWARE")}}
                Pill {text:"Cerrar";onClicked:tracks.close()}
                Item {Layout.preferredHeight:24}
            }
        }
    }
    FileDialog {id:subtitlePicker;title:"Abrir subtítulos";nameFilters:["Subtítulos (*.srt *.ass *.ssa *.vtt)"];onAccepted:{player.addSubtitle(selectedFile);library.setPlaybackSetting("externalSubtitleUrl",view.media.path,selectedFile.toString());library.setPlaybackSetting("subtitleChoice",view.media.path,"external")}}
}
