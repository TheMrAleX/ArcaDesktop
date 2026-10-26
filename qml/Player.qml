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
    property bool defaultsApplied:false
    property var captions:[]
    property string externalSubtitle:""
    property real subtitleDelay:0
    property real subtitleScale:1
    property var externalTracks:library.externalSubtitles(media.path)
    property var audioOptions:trackOptions(true)
    property var subtitleOptions:trackOptions(false).concat(externalTracks.map(f=>Object.assign({},f,{external:true})))
    property int skipMs:Number(library.setting("skipSeconds",10))*1000
    signal closed()
    signal playNext(var file)
    function persist() { if(initialized)library.saveProgress(media.path,player.position,player.duration) }
    function closePlayer(){persist();player.stop();view.Window.window.showNormal();closed()}
    function seek(delta){player.setPosition(Math.max(0,Math.min(player.duration,player.position+delta)))}
    function fullscreen(){let win=view.Window.window;if(win.visibility===Window.FullScreen)win.showNormal();else win.showFullScreen()}
    onMediaChanged:{initialized=false;defaultsApplied=false;captions=[];externalSubtitle="";subtitleDelay=Number(library.playbackSetting("subDelay",media.path,0));player.source=media.url;player.play()}
    Component.onCompleted:{subtitleDelay=Number(library.playbackSetting("subDelay",media.path,0));subtitleScale=Number(library.setting("subtitleScale",1));player.source=media.url;player.play();forceActiveFocus()}
    Component.onDestruction:persist()
    focus:true
    Keys.onSpacePressed:if(!locked)player.playbackState===MediaPlayer.PlayingState?player.pause():player.play()
    Keys.onLeftPressed:if(!locked)seek(-skipMs)
    Keys.onRightPressed:if(!locked)seek(skipMs)
    Keys.onEscapePressed:closePlayer()
    Keys.onPressed:function(event){if(event.key===Qt.Key_F&&!locked){fullscreen();event.accepted=true}}
    MediaPlayer {
        id:player
        objectName:"mediaPlayer"
        videoOutput:video
        audioOutput:AudioOutput { id:audio;volume:0.8 }
        onMediaStatusChanged:{
            if(mediaStatus===MediaPlayer.LoadedMedia&&!view.initialized){
                view.initialized=true
                if(view.media.startPosition>0)setPosition(view.media.startPosition)
                chooseDefaults.restart()
            }
            if(mediaStatus===MediaPlayer.EndOfMedia){view.persist();let next=library.nextEpisode(view.media.path);if(library.setting("autoNext",true)&&next.path){view.persist();view.playNext(next)}}
        }
        onTracksChanged:if(view.initialized&&!view.defaultsApplied)chooseDefaults.restart()
    }
    Timer {id:chooseDefaults;interval:100;onTriggered:view.applyDefaults()}
    VideoOutput {id:video;anchors.fill:parent;fillMode:VideoOutput.PreserveAspectFit}
    Text {anchors {left:parent.left;right:parent.right;bottom:parent.bottom;leftMargin:60;rightMargin:60;bottomMargin:view.controlsVisible?180:40} horizontalAlignment:Text.AlignHCenter;wrapMode:Text.Wrap;text:subtitleText();color:"white";font.pixelSize:26*view.subtitleScale;style:Text.Outline;styleColor:"black";textFormat:Text.PlainText;visible:view.captions.length>0}
    MouseArea {anchors.fill:parent;hoverEnabled:true;onPositionChanged:{view.controlsVisible=true;hideControls.restart()} onClicked:{if(!view.locked)view.controlsVisible=!view.controlsVisible} onDoubleClicked:function(mouse){if(!view.locked){if(mouse.x<width*0.35)view.seek(-view.skipMs);else if(mouse.x>width*0.65)view.seek(view.skipMs);else view.fullscreen()}}}
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
            Pill {text:"−"+(view.skipMs/1000)+" s";onClicked:view.seek(-view.skipMs)}
            Pill {text:player.playbackState===MediaPlayer.PlayingState?"Pausar":"Reproducir";primary:true;onClicked:player.playbackState===MediaPlayer.PlayingState?player.pause():player.play()}
            Pill {text:"+"+(view.skipMs/1000)+" s";onClicked:view.seek(view.skipMs)}
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
            Repeater {model:view.audioOptions;Pill {required property var modelData;required property int index;text:view.trackLabel(modelData);selected:player.activeAudioTrack===index;Layout.fillWidth:true;onClicked:view.chooseAudio(index,true)}}
            Text {text:"Subtítulos";color:"#8B9CFF"}
            Pill {text:"Desactivados";selected:player.activeSubtitleTrack===-1&&!view.captions.length;Layout.fillWidth:true;onClicked:view.chooseSubtitle(-1,true)}
            Repeater {model:view.subtitleOptions;Pill {required property var modelData;required property int index;text:view.trackLabel(modelData);selected:modelData.external?view.externalSubtitle===modelData.url.toString():!view.externalSubtitle&&player.activeSubtitleTrack===modelData.index;Layout.fillWidth:true;onClicked:view.chooseSubtitle(index,true)}}
            Pill {text:"Abrir .srt o .ass";Layout.fillWidth:true;onClicked:subtitlePicker.open()}
            RowLayout {visible:view.captions.length>0;Pill {text:"−0,2 s";onClicked:view.changeDelay(-0.2)} Text {text:view.subtitleDelay.toFixed(1)+" s";color:"white"} Pill {text:"+0,2 s";onClicked:view.changeDelay(0.2)}}
            RowLayout {visible:view.captions.length>0;Repeater {model:["S","M","L"];Pill {required property string modelData;required property int index;text:modelData;onClicked:{view.subtitleScale=[0.8,1,1.3][index];library.setSetting("subtitleScale",view.subtitleScale)}}}}
            Item {Layout.fillHeight:true}
            Pill {text:"Cerrar";onClicked:tracks.close()}
        }
    }
    FileDialog {id:subtitlePicker;title:"Abrir subtítulos";nameFilters:["Subtítulos (*.srt *.ass *.ssa *.vtt)"];onAccepted:view.loadSubtitle(selectedFile)}
    function trackOptions(audioTrack){
        let tracks=audioTrack?player.audioTracks:player.subtitleTracks
        let streams=((media.probe||{}).streams||[]).filter(s=>s.codec_type===(audioTrack?"audio":"subtitle"))
        let options=[]
        for(let index=0;index<tracks.length;index++){
            let meta=tracks[index],stream=streams[index]||{},tags=stream.tags||{}
            options.push({index:index,id:String(stream.index===undefined?index:stream.index),lang:library.trackLanguage(meta.language)||library.trackLanguage(tags.language),title:meta.title||tags.title||"",forced:!!(stream.disposition||{}).forced,channels:stream.channels||0})
        }
        return options
    }
    function trackLabel(track){return library.trackLanguageName(track.lang)+(track.channels?" · "+(track.channels===6?"5.1":track.channels===8?"7.1":track.channels+".0"):"")+(track.forced?" · Forzados":"")+(track.external?" · "+track.name:track.title?" · "+track.title:"")}
    function chooseAudio(index,persistChoice){if(index<0||index>=audioOptions.length)return;if(player.activeAudioTrack!==index)player.activeAudioTrack=index;if(persistChoice)library.setPlaybackSetting("audioChoice",media.path,library.encodeTrack(audioOptions[index]))}
    function chooseSubtitle(index,persistChoice){
        if(index<0||index>=subtitleOptions.length){player.activeSubtitleTrack=-1;captions=[];externalSubtitle="";if(persistChoice){library.setPlaybackSetting("subtitleChoice",media.path,"off");library.setPlaybackSetting("externalSubtitleUrl",media.path,"")}return}
        let track=subtitleOptions[index]
        if(track.external)loadSubtitle(track.url,false)
        else {captions=[];externalSubtitle="";if(player.activeSubtitleTrack!==track.index)player.activeSubtitleTrack=track.index}
        if(persistChoice){library.setPlaybackSetting("subtitleChoice",media.path,library.encodeTrack(track));library.setPlaybackSetting("externalSubtitleUrl",media.path,track.external?track.url.toString():"")}
    }
    function applyDefaults(){
        if(!initialized||defaultsApplied)return
        defaultsApplied=true
        let audioSaved=String(library.playbackSetting("audioChoice",media.path,""))
        let audioIndex=library.pickTrack(audioOptions,audioSaved)
        if(audioIndex<0)audioIndex=library.preferredTrack(audioOptions,false)
        if(audioIndex<0)audioIndex=Number(library.playbackSetting("audioTrack",media.path,player.activeAudioTrack))
        chooseAudio(audioIndex,false)
        let subtitleSaved=String(library.playbackSetting("subtitleChoice",media.path,""))
        if(subtitleSaved==="off"){chooseSubtitle(-1,false);return}
        let subIndex=library.pickTrack(subtitleOptions,subtitleSaved)
        let externalUrl=String(library.playbackSetting("externalSubtitleUrl",media.path,""))
        if(subIndex<0&&externalUrl){let savedCaptions=library.readSubtitles(externalUrl);if(savedCaptions.length){captions=savedCaptions;externalSubtitle=externalUrl;player.activeSubtitleTrack=-1;return}}
        if(subIndex<0){let preferred=String(library.setting("languages","es,en")).split(',')[0];let audioLanguage=audioOptions[audioIndex]?audioOptions[audioIndex].lang:"";subIndex=library.preferredTrack(subtitleOptions,audioLanguage&&audioLanguage.split('-')[0]===preferred.split('-')[0])}
        chooseSubtitle(subIndex,false)
    }
    function loadSubtitle(url,persistChoice){captions=library.readSubtitles(url);externalSubtitle=url.toString();player.activeSubtitleTrack=-1;if(persistChoice!==false){let known=externalTracks.find(t=>t.url.toString()===url.toString());library.setPlaybackSetting("subtitleChoice",media.path,library.encodeTrack(known||{id:"ext:"+url.toString().split('/').pop(),lang:"",forced:false}));library.setPlaybackSetting("externalSubtitleUrl",media.path,url.toString())}}
    function changeDelay(delta){subtitleDelay+=delta;library.setPlaybackSetting("subDelay",media.path,subtitleDelay)}
    function subtitleText(){let position=player.position-view.subtitleDelay*1000;let output=[];for(let c of captions){if(c.start>position)break;if(c.end>position)output.push(c.text)}return output.join("\n")}
    function formatTime(ms){let s=Math.max(0,Math.floor(ms/1000));return Math.floor(s/3600)+":"+String(Math.floor(s/60)%60).padStart(2,"0")+":"+String(s%60).padStart(2,"0")}
}
