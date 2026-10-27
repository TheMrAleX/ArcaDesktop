import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import "Search.js" as Search

ApplicationWindow {
    id: root
    width: 1280; height: 820; minimumWidth: isPlayer&&playerLoader.item&&playerLoader.item.pip?320:760; minimumHeight:isPlayer&&playerLoader.item&&playerLoader.item.pip?180:560
    visible: true; title: "Arca"; color: "#0A0B0E"
    palette.window:"#15181D";palette.windowText:"#F4F5F7";palette.base:"#11141B";palette.text:"#F4F5F7";palette.button:"#202530";palette.buttonText:"#F4F5F7";palette.highlight:"#8B9CFF";palette.highlightedText:"#0A0B0E";palette.mid:"#343B49";palette.dark:"#0A0B0E";palette.light:"#343B49"
    property string page: library.onboarded ? "Inicio" : "Bienvenida"
    property var selected: ({})
    property var playing: ({})
    property string search: ""
    property string libraryTab: "Películas"
    property string filter: "Todo"
    property bool descending: false
    property int orderBy: 0
    property string selectedGenre: ""
    property string selectedYear: ""
    property int selectedSeason: -1
    property bool episodeDescending: false
    property string searchScope: "Todo"
    property string folderPath: ""
    property string fileFilter:"Todo"
    property var allFiles: library.files
    property var movies: allFiles.filter(f => f.kind === "MOVIE" && f.confidence >= 80 && !f.hidden && !f.duplicate)
    property var episodes: allFiles.filter(f => f.kind === "EPISODE" && f.confidence >= 80 && !f.hidden && !f.duplicate)
    property var review: allFiles.filter(f => !f.hidden && f.kind !== "IGNORED" && (f.confidence < 80 || f.kind === "UNKNOWN"))
    property var shows: library.shows
    property var continuing: library.continuing
    property var genres: Array.from(new Set(movies.concat(shows).reduce((all,f)=>all.concat((f.genres || "").split(" · ")),[]).filter(s=>s.length))).sort()
    property var years: Array.from(new Set(movies.concat(shows).map(f=>String(f.year || "")).filter(s=>s.length))).sort().reverse()
    property var selectedEpisodes: episodes.filter(f=>f.showKey===selected.showKey && (selectedSeason<0 || f.season===selectedSeason)).sort((a,b)=>episodeDescending ? b.season-a.season || b.episode-a.episode : a.season-b.season || a.episode-b.episode)
    property var selectedMetaJob:library.metadataJobs[selected.isShow?selected.showKey:selected.path]||({})
    property string selectedArt:selected.isShow&&library.setting("seasonArt",true)&&selectedSeason>=0?(selected.seasonPosters||{})[String(selectedSeason)]||selected.backdrop||selected.poster||selected.thumb||"":selected.backdrop||selected.poster||selected.thumb||""
    property var hero: continuing.length ? continuing[0] : movies.length ? movies.slice().sort((a,b) => b.mtime-a.mtime)[0] : episodes.length ? episodes[0] : ({})
    property bool isPlayer: page === "Reproductor"
    function key(s) { return s.normalize("NFD").replace(/[\u0300-\u036f]/g, "").toLowerCase().replace(/[^a-z0-9]+/g," ").trim() }
    function duration(ms) { let s=Math.floor((ms||0)/1000); return Math.floor(s/3600)>0 ? Math.floor(s/3600)+":"+String(Math.floor(s/60)%60).padStart(2,"0")+":"+String(s%60).padStart(2,"0") : Math.floor(s/60)+":"+String(s%60).padStart(2,"0") }
    function open(f) { selected=f.kind==="EPISODE" ? shows.find(s=>s.showKey===f.showKey)||f : f; selectedSeason=f.season>=0?f.season:-1; page="Detalles" }
    function refreshSelected() { if(selected.path)selected=selected.isShow ? shows.find(s=>s.showKey===selected.showKey)||library.media(selected.path) : library.media(selected.path) }
    function play(f, restart) {
        if(f.isShow)f=library.media(f.path)
        if (!f.available) { unavailable.open(); return }
        playing=Object.assign({},f,{startPosition: restart || !library.setting("resume",true) ? 0 : Math.max(0,(f.position||0)-Number(library.setting("resumeRewindSeconds",3))*1000)})
        page="Reproductor"
    }
    function back() { if (page === "Reproductor") playerLoader.item.closePlayer(); else page="Inicio" }
    function catalog() {
        let items = libraryTab === "Series" ? shows : movies
        if(filter === "Sin ver") items=items.filter(f=>!f.watched)
        if(filter === "En curso") items=items.filter(f=>f.isShow ? f.inProgress : !f.watched && f.position>5000)
        if(filter === "4K") items=items.filter(f=>f.width>=3200)
        if(selectedGenre)items=items.filter(f=>(f.genres || "").split(" · ").indexOf(selectedGenre)>=0)
        if(selectedYear)items=items.filter(f=>String(f.year)===selectedYear)
        items=items.slice().sort((a,b)=>orderBy===1 ? (a.year||0)-(b.year||0) : orderBy===2 ? a.mtime-b.mtime : a.title.localeCompare(b.title))
        return descending ? items.reverse() : items
    }
    Shortcut { sequence: "Alt+Left"; onActivated: root.back() }
    Shortcut { sequence: "Ctrl+O"; onActivated: chooseFolder.open() }
    Shortcut { sequence: "Ctrl+F"; onActivated: root.page="Buscar" }
    onClosing: function(close) { if(isPlayer && playerLoader.item) playerLoader.item.persist() }
    Connections {target:library;function onChanged(){root.refreshSelected()}function onMetadataChoicesReady(path){if(root.selected.path===path){metadataQuery.text=root.selected.fileTitle||root.selected.title;metadataDialog.open()}}function onMetadataChanged(){if(library.absolutePrompt.showKey)absoluteDialog.open()}}

    FolderDialog { id: chooseFolder; title: "Elige una carpeta multimedia"; onAccepted: folderKind.open() }
    Dialog {
        id: folderKind; anchors.centerIn: parent; modal: true; title: "¿Qué contiene esta carpeta?"; width: 480
        background: Rectangle { color: "#15181D"; radius: 18; border.color: "#333844" }
        contentItem: ColumnLayout {
            spacing: 12
            Text { Layout.fillWidth: true; text: chooseFolder.selectedFolder.toString(); color: "#A8AEB8"; font.family: "monospace"; wrapMode: Text.WrapAnywhere }
            Repeater { model: [{label:"Series",kind:"SERIES"},{label:"Películas",kind:"MOVIES"},{label:"Detectar sola",kind:"AUTO"}]
                Pill { required property var modelData; text: modelData.label; Layout.fillWidth: true; onClicked: { library.addFolder(chooseFolder.selectedFolder,modelData.kind); folderKind.close(); root.page="Carpetas" } }
            }
            Pill { text: "Cancelar"; onClicked: folderKind.close() }
        }
    }
    FileDialog { id: exportDialog; title: "Exportar progreso"; fileMode: FileDialog.SaveFile; defaultSuffix: "json"; nameFilters: ["Copia de Arca (*.json)"]; onAccepted: library.exportBackup(selectedFile,backupFolders.checked,backupDecisions.checked,backupSettings.checked) }
    FileDialog { id: importDialog; title: "Importar progreso"; nameFilters: ["Copia de Arca (*.json)"]; onAccepted: {backupPreview.summary=library.inspectBackup(selectedFile);if(backupPreview.summary.items!==undefined)backupPreview.open()} }
    Dialog {
        id:backupOptions;anchors.centerIn:parent;modal:true;width:520;title:"Exportar copia de seguridad"
        background:Rectangle {color:"#15181D";radius:18}
        contentItem:ColumnLayout {spacing:12
            Muted {Layout.fillWidth:true;text:"El progreso del perfil actual se incluye siempre."}
            CheckBox {id:backupFolders;text:"Carpetas vigiladas";checked:true;palette.windowText:"#F4F5F7"}
            CheckBox {id:backupDecisions;text:"Correcciones, reglas y decisiones de series";checked:true;palette.windowText:"#F4F5F7"}
            CheckBox {id:backupSettings;text:"Ajustes, incluida tu clave de TMDB";checked:true;palette.windowText:"#F4F5F7"}
            RowLayout {Pill {text:"Elegir archivo";primary:true;onClicked:{backupOptions.close();exportDialog.open()}} Pill {text:"Cancelar";onClicked:backupOptions.close()}}
        }
    }
    Dialog {
        id:backupPreview;anchors.centerIn:parent;modal:true;width:540;title:"Unir copia de seguridad";property var summary:({})
        background:Rectangle {color:"#15181D";radius:18}
        contentItem:ColumnLayout {spacing:12
            Muted {Layout.fillWidth:true;text:(backupPreview.summary.profile||"Perfil")+" · "+(backupPreview.summary.items||0)+" progresos. Se conserva el más reciente de cada archivo."}
            CheckBox {id:restoreFolders;text:"Restaurar carpetas ("+(backupPreview.summary.folders||0)+")";checked:true;palette.windowText:"#F4F5F7"}
            CheckBox {id:restoreDecisions;text:"Restaurar correcciones y reglas ("+(backupPreview.summary.decisions||0)+")";checked:true;palette.windowText:"#F4F5F7"}
            CheckBox {id:restoreSettings;text:"Restaurar ajustes y clave de TMDB";checked:true;enabled:!!backupPreview.summary.settings;palette.windowText:"#F4F5F7"}
            Muted {Layout.fillWidth:true;text:"Las carpetas de Android se vinculan a sus carpetas de este equipo después de importar."}
            RowLayout {Pill {text:"Importar y unir";primary:true;onClicked:{library.importBackup(importDialog.selectedFile,restoreFolders.checked,restoreDecisions.checked,restoreSettings.checked);backupPreview.close()}} Pill {text:"Cancelar";onClicked:backupPreview.close()}}
        }
    }
    FolderDialog {id:importFolder;title:"Localiza esta carpeta en Linux";property string reference:"";property string kind:"AUTO";onAccepted:library.bindImportedFolder(reference,selectedFolder,kind)}
    Dialog {
        id:profileEditor;anchors.centerIn:parent;modal:true;width:460;title:"Editar perfil";property var profile:({})
        onOpened:{editProfileName.text=profile.name||"";editProfileColor.text=profile.color||"#8B9CFF"}
        background:Rectangle {color:"#15181D";radius:18}
        contentItem:ColumnLayout {spacing:14
            TextField {id:editProfileName;Layout.fillWidth:true;placeholderText:"Nombre";color:"white"}
            RowLayout {Rectangle {width:32;height:32;radius:16;color:/^#[0-9a-fA-F]{6}$/.test(editProfileColor.text)?editProfileColor.text:"#8B9CFF"} TextField {id:editProfileColor;placeholderText:"#8B9CFF";color:"white"}}
            Flow {Layout.fillWidth:true;spacing:8;Repeater {model:["#8B9CFF","#FF9E7A","#7BCBA7","#E6A3CE","#D9BC75"];Button {required property string modelData;width:36;height:36;background:Rectangle {radius:18;color:parent.modelData}onClicked:editProfileColor.text=modelData}}}
            RowLayout {Pill {text:"Guardar";primary:true;onClicked:{library.editProfile(profileEditor.profile.id,editProfileName.text,editProfileColor.text);profileEditor.close()}} Pill {text:"Eliminar";visible:library.profiles.length>1;onClicked:{deleteProfileDialog.profile=profileEditor.profile;deleteProfileDialog.open()}} Pill {text:"Cancelar";onClicked:profileEditor.close()}}
        }
    }
    Dialog {id:deleteProfileDialog;anchors.centerIn:parent;modal:true;title:"¿Eliminar este perfil?";property var profile:({});standardButtons:Dialog.Ok|Dialog.Cancel;onAccepted:{library.deleteProfile(profile.id);profileEditor.close()} Label {text:"Se eliminará el progreso de "+(deleteProfileDialog.profile.name||"")+". Tus archivos se conservan.";color:"#C2C8D0"}background:Rectangle {color:"#15181D";radius:18}}
    Dialog {
        id:metadataDialog;anchors.centerIn:parent;modal:true;width:640;height:560;title:"Buscar metadatos"
        background:Rectangle {color:"#15181D";radius:18;border.color:"#333844"}
        contentItem:ColumnLayout {
            RowLayout {TextField {id:metadataQuery;Layout.fillWidth:true;placeholderText:"Título";color:"white"} Pill {text:"Buscar";onClicked:library.findMetadata(metadataQuery.text,root.selected.kind==="EPISODE")}}
            BusyIndicator {running:library.searching;visible:running}
            ListView {id:metadataList;Layout.fillWidth:true;Layout.fillHeight:true;clip:true;spacing:8;model:library.matches
                delegate:ItemDelegate {required property var modelData;width:metadataList.width;height:110
                    background:Rectangle {color:parent.hovered?"#252B3A":"#1C2029";radius:12}
                    contentItem:ColumnLayout {Text {text:modelData.title+" · "+modelData.year;color:"white";font.bold:true;Layout.fillWidth:true;elide:Text.ElideRight} Text {text:modelData.plot;color:"#A8AEB8";wrapMode:Text.Wrap;maximumLineCount:3;elide:Text.ElideRight;Layout.fillWidth:true}}
                    onClicked:{library.useMetadata(root.selected.path,modelData.id,root.selected.kind==="EPISODE");metadataDialog.close()}
                }
            }
            Pill {text:"Cerrar";onClicked:metadataDialog.close()}
        }
    }
    Dialog {
        id:absoluteDialog;anchors.centerIn:parent;modal:true;width:600;title:"¿Cómo están numerados tus episodios?";closePolicy:Popup.NoAutoClose
        background:Rectangle {color:"#15181D";radius:20;border.color:"#8B9CFF"}
        contentItem:ColumnLayout {spacing:16
            Text {Layout.fillWidth:true;text:(library.absolutePrompt.title||"")+" tiene "+(library.absolutePrompt.total||0)+" episodios repartidos en temporadas. Tus archivos parecen numerados de corrido.";color:"#F4F5F7";wrapMode:Text.Wrap}
            Repeater {model:library.absolutePrompt.ranges||[];Text {required property var modelData;text:"Temporada "+modelData.season+" · episodios "+modelData.first+"–"+modelData.last;color:"#A8AEB8"}}
            Pill {text:"Repartir en temporadas";primary:true;Layout.fillWidth:true;onClicked:{absoluteDialog.close();library.resolveAbsolute(1)}}
            Pill {text:"Conservar mis números y usar solo los datos";Layout.fillWidth:true;onClicked:{absoluteDialog.close();library.resolveAbsolute(0)}}
            Pill {text:"Decidir después";onClicked:{absoluteDialog.close();library.resolveAbsolute(-1)}}
        }
    }
    Dialog {
        id:artDialog;anchors.centerIn:parent;modal:true;width:720;height:570;title:backdrop?"Elegir fondo":"Elegir carátula";property bool backdrop:false
        background:Rectangle {color:"#15181D";radius:20}
        contentItem:ColumnLayout {GridView {id:artGrid;Layout.fillWidth:true;Layout.fillHeight:true;cellWidth:160;cellHeight:artDialog.backdrop?120:235;clip:true;model:library.artwork;delegate:ItemDelegate {required property var modelData;width:150;height:artGrid.cellHeight-10;contentItem:Image {source:modelData.url;asynchronous:true;fillMode:Image.PreserveAspectCrop} onClicked:{library.chooseArtwork(root.selected.path,modelData.remote,artDialog.backdrop);artDialog.close()}} ScrollBar.vertical:ScrollBar{}} Pill {text:"Cerrar";onClicked:artDialog.close()}}
    }
    Dialog { id: unavailable; anchors.centerIn: parent; modal: true; title: "Archivo no disponible"; standardButtons: Dialog.Ok; Label { text: "Monta el disco o vuelve a conectar la carpeta y explora de nuevo."; color: "#F4F5F7" } background: Rectangle { color: "#15181D"; radius: 16 } }
    Dialog {
        id: correction; anchors.centerIn: parent; modal: true; width: 560; title: "Corregir coincidencia"
        property var file: ({})
        onOpened: { correctionTitle.text=file.title || ""; correctionSeason.value=Math.max(0,file.season || 0); correctionEpisode.value=file.episode || 0;correctionYear.value=file.year||0; ruleCheck.checked=false }
        background: Rectangle { color: "#15181D"; radius: 18; border.color: "#FF9E7A" }
        contentItem: ColumnLayout {
            spacing: 14
            Text { Layout.fillWidth: true; text: correction.file.name || ""; font.family: "monospace"; color: "#C2C8D0"; wrapMode: Text.WrapAnywhere }
            TextField { id: correctionTitle; Layout.fillWidth: true; placeholderText: "Título de película o serie"; color: "white" }
            RowLayout { Label { text: "Temporada"; color: "#C2C8D0" } SpinBox { id: correctionSeason; from: 0; to: 999; editable: true } Label { text: "Episodio (0 = película)"; color: "#C2C8D0" } SpinBox { id: correctionEpisode; from: 0; to: 9999; editable: true } }
            RowLayout {Label {text:"Año (0 = sin año)";color:"#C2C8D0"} SpinBox {id:correctionYear;from:0;to:2099;editable:true}}
            RowLayout {visible:!!correction.file.compactNumber
                Pill {text:"T"+Math.floor((correction.file.compactNumber||0)/100)+" · E"+((correction.file.compactNumber||0)%100);onClicked:{correctionSeason.value=Math.floor(correction.file.compactNumber/100);correctionEpisode.value=correction.file.compactNumber%100}}
                Pill {visible:(correction.file.compactNumber||0)%10>0;text:"T"+Math.floor((correction.file.compactNumber||0)/10)+" · E"+((correction.file.compactNumber||0)%10);onClicked:{correctionSeason.value=Math.floor(correction.file.compactNumber/10);correctionEpisode.value=correction.file.compactNumber%10}}
            }
            CheckBox { id: ruleCheck; text: "Recordar la numeración para toda esta carpeta"; palette.windowText: "#C2C8D0" }
            RowLayout { Pill { text: "Guardar"; primary: true; onClicked: {library.correct(correction.file.path,correctionTitle.text,correctionSeason.value,correctionEpisode.value,ruleCheck.checked,correctionYear.value);correction.close()} } Pill { text: "Cancelar"; onClicked: correction.close() } }
        }
    }

    Rectangle {
        visible: !root.isPlayer && root.page !== "Bienvenida"
        width: 180; anchors { top: parent.top; bottom: parent.bottom; left: parent.left }
        color: "#0C0E12"
        ColumnLayout {
            anchors { fill: parent; margins: 14 } spacing: 8
            RowLayout { Layout.bottomMargin: 32; Image { source: "qrc:/assets/arca.png"; sourceSize.width: 42; Layout.preferredWidth: 42; Layout.preferredHeight: 42 } Text { text: "arca"; color: "white"; font.family: "Bricolage Grotesque"; font.pixelSize: 28; font.bold: true } }
            Repeater { model: ["Inicio","Buscar","Biblioteca","Carpetas","Revisar","Ajustes"]
                Pill { required property string modelData; Layout.fillWidth: true; text: modelData === "Revisar" && root.review.length ? modelData+" · "+root.review.length : modelData; selected: root.page === modelData; onClicked: {root.page=modelData;root.filter="Todo"} }
            }
            Item { Layout.fillHeight: true }
            Pill { Layout.fillWidth: true; text: library.profile; onClicked: root.page="Perfiles" }
        }
    }
    ColumnLayout {
        anchors { fill: parent; leftMargin: root.isPlayer || root.page==="Bienvenida" ? 0 : 180 }
        spacing: 0
        Rectangle {
            visible: library.error.length>0; Layout.fillWidth: true; implicitHeight: errorText.implicitHeight+28; color: "#38231F"
            RowLayout { anchors { fill: parent; margins: 12 } Text { id:errorText; Layout.fillWidth:true; text:library.error; color:"#FFD0BA"; wrapMode:Text.Wrap } Pill { text:"Cerrar"; onClicked:library.clearError() } }
        }
        Rectangle {
            visible: library.scanning && !root.isPlayer; Layout.fillWidth: true; implicitHeight: 54; color: "#171E35"
            RowLayout { anchors { fill: parent; leftMargin: 24; rightMargin: 24 } BusyIndicator { running: library.scanning && !library.paused; Layout.preferredWidth: 30; Layout.preferredHeight: 30 } Text { Layout.fillWidth: true; text: library.scanStatus; color: "#C2C8D0"; elide: Text.ElideLeft } Pill { text: library.paused ? "Continuar" : "Pausar"; onClicked:library.pauseScan() } }
        }
        Loader {
            id: pageLoader; Layout.fillWidth:true; Layout.fillHeight:true; visible:!root.isPlayer
            sourceComponent: root.page==="Bienvenida" ? welcomePage : root.page==="Inicio" ? homePage : root.page==="Biblioteca" ? catalogPage : root.page==="Buscar" ? searchPage : root.page==="Detalles" ? detailPage : root.page==="Carpetas" ? foldersPage : root.page==="Archivos" || root.page==="Revisar" ? filesPage : root.page==="Perfiles" ? profilesPage : settingsPage
        }
        Loader { id: playerLoader; Layout.fillWidth:true; Layout.fillHeight:true; active:root.isPlayer; visible:active; sourceComponent: Player { media:root.playing; onClosed: {library.reload();root.page="Detalles";root.refreshSelected()} onPlayNext: function(file){root.playing=Object.assign({},file,{startPosition:0})} } }
    }
    component Heading: Text { color:"#F4F5F7"; font.family:"Bricolage Grotesque"; font.bold:true; font.pixelSize:28; wrapMode:Text.Wrap }
    component Muted: Text { color:"#A8AEB8"; font.pixelSize:13; wrapMode:Text.Wrap }
    component Overline: Text { color:"#8B9CFF"; font.pixelSize:10; font.bold:true; font.letterSpacing:2 }
    component EmptyState: ColumnLayout {
        spacing:18; Layout.fillWidth:true
        Heading { text:"Aquí no hay nada todavía" }
        Muted { text:"Añade una carpeta. Arca reconoce tus películas y series y recuerda por dónde ibas."; Layout.maximumWidth:520 }
        Pill { text:"Añadir una carpeta";primary:true;onClicked:chooseFolder.open() }
    }
    component PosterShelf: ColumnLayout {
        required property string title
        required property var items
        Layout.fillWidth:true; spacing:14; visible:items.length>0
        Heading { text: parent.title; font.pixelSize:23 }
        ListView { Layout.fillWidth:true; Layout.preferredHeight:292; orientation:ListView.Horizontal; spacing:18; clip:true; model:parent.items; delegate:Poster { required property var modelData; media:modelData; onClicked:root.open(modelData) } ScrollBar.horizontal:ScrollBar {} }
    }
    component FileLine: ItemDelegate {
        required property var file
        Layout.fillWidth:true; implicitHeight:94
        background:Rectangle { radius:14; color:parent.hovered?"#20242D":"#15181D"; border.color:parent.activeFocus?"#8B9CFF":"#262A32" }
        contentItem:RowLayout {
            spacing:16
            ColumnLayout {
                Layout.fillWidth:true
                Text { Layout.fillWidth:true; text:file.name || ""; color:"#F4F5F7"; font.family:"monospace"; font.pixelSize:12; elide:Text.ElideMiddle }
                Text { Layout.fillWidth:true; text:file.title+(file.kind==="EPISODE"?" · T"+file.season+" · E"+file.episode:""); color:"#C2C8D0"; elide:Text.ElideRight }
                Text { Layout.fillWidth:true; text:!file.available?"No disponible":file.hidden?"Oculto":file.duplicate?"Duplicado":file.reason || (file.watched?"Visto":file.confidence+" % de confianza"); color:file.confidence<80?"#FF9E7A":"#8A919B"; font.pixelSize:11; elide:Text.ElideRight }
            }
            Pill { visible:file.kind!=="SUBTITLE";text:root.page==="Revisar"?"Es correcto":"Reproducir"; onClicked:root.page==="Revisar"?library.accept(file.path):root.play(file,false) }
            Pill { text:"Opciones"; onClicked:fileActions.open() }
            Menu {id:fileActions
                MenuItem {text:"Corregir coincidencia";onTriggered:{correction.file=file;correction.open()}}
                MenuItem {text:file.hidden?"Mostrar en la biblioteca":"Ocultar de la biblioteca";onTriggered:library.setFileHidden(file.path,!file.hidden)}
                MenuItem {visible:file.hasDuplicates||file.duplicate;height:visible?implicitHeight:0;text:"Usar esta versión";onTriggered:library.preferFile(file.path)}
                MenuItem {text:"Volver a la detección automática";onTriggered:library.resetDecision(file.path)}
            }
        }
        onClicked:if(file.kind!=="SUBTITLE")root.open(file)
    }
    component EpisodeLine: ItemDelegate {
        required property var file
        implicitHeight:94
        background:Rectangle {radius:14;color:parent.hovered?"#232733":file.position>5000&&!file.watched?"#1E2330":"#15181D";border.width:parent.activeFocus?2:1;border.color:parent.activeFocus?"#8B9CFF":"#262A32"
            Rectangle {anchors {left:parent.left;bottom:parent.bottom} height:3;color:"#FF9E7A";radius:2;width:parent.width*Math.min(1,(file.position||0)/Math.max(1,file.progressDuration||file.duration||1))}
        }
        contentItem:RowLayout {
            spacing:16
            Rectangle {Layout.preferredWidth:108;Layout.preferredHeight:61;radius:11;color:"#252A3B";clip:true
                Image {anchors.fill:parent;source:file.spoilerHidden?"":file.still||file.thumb||file.poster||"";asynchronous:true;fillMode:Image.PreserveAspectCrop;sourceSize.width:216}
                Text {anchors.centerIn:parent;text:"E"+String(file.episode).padStart(2,"0");color:"#A8AEB8";visible:file.spoilerHidden||!(file.still||file.poster)}
            }
            ColumnLayout {Layout.fillWidth:true
                Text {Layout.fillWidth:true;text:"E"+String(file.episode).padStart(2,"0")+" · "+(file.episodeTitle||"Episodio "+file.episode);color:"#F4F5F7";font.bold:true;elide:Text.ElideRight}
                Text {Layout.fillWidth:true;text:!file.available?"Archivo no disponible":file.watched?"Visto":file.position>5000?"Quedan "+root.duration((file.progressDuration||file.duration)-file.position):"Sin ver · "+root.duration(file.duration);color:file.position>5000&&!file.watched?"#FF9E7A":"#A8AEB8";font.pixelSize:12;elide:Text.ElideRight}
            }
            Pill {text:file.watched?"Sin ver":"Visto";onClicked:library.markWatched(file.path,!file.watched)}
        }
        onClicked:root.play(file,false)
    }
    Component {
        id:welcomePage
        ScrollView { clip:true; contentWidth:availableWidth
            ColumnLayout { width:parent.width; spacing:24
                Item { Layout.preferredHeight:70 }
                ColumnLayout { Layout.alignment:Qt.AlignHCenter; Layout.maximumWidth:640; Layout.margins:40; spacing:24
                    Image { source:"qrc:/assets/arca.png"; Layout.preferredWidth:64;Layout.preferredHeight:64 }
                    Heading { text:"Tu cine,\nen tus carpetas.";font.pixelSize:52 }
                    Muted { Layout.fillWidth:true; text:"Dile dónde están tus archivos. Yo los ordeno, saco las temporadas y los episodios de cada nombre, y recuerdo por dónde ibas.";font.pixelSize:16 }
                    Heading { text:"Permiso de lectura";font.pixelSize:19 }
                    Muted { text:"Solo leo. Nunca muevo, renombro ni borro un archivo tuyo." }
                    Heading { text:"Elige tus carpetas";font.pixelSize:19 }
                    Muted { text:"Marca cada una como películas o series, o deja que lo deduzca." }
                    Pill { text:"Elegir carpeta";primary:true;onClicked:chooseFolder.open() }
                    Pill { text:"Ya sé cómo va, sáltatelo";onClicked:{library.finishWelcome();root.page="Inicio"} }
                }
            }
        }
    }
    Component {
        id:homePage
        ScrollView { clip:true;contentWidth:availableWidth
            ColumnLayout { width:parent.width; spacing:28
                ColumnLayout { Layout.fillWidth:true;Layout.margins:32;spacing:28
                    RowLayout { Layout.fillWidth:true;Heading { text:"Inicio";Layout.fillWidth:true } Pill {text:"Explorar";onClicked:library.scan()} }
                    EmptyState { visible:!root.movies.length&&!root.shows.length }
                    Rectangle {
                        visible:!!root.hero.path;Layout.fillWidth:true;Layout.preferredHeight:310;radius:24;color:"#191D30";clip:true
                        Backdrop {anchors.fill:parent;art:root.hero.backdrop||root.hero.poster||root.hero.thumb||"";horizontal:true}
                        ColumnLayout { anchors {fill:parent;margins:30}spacing:12
                            Overline { text:root.continuing.length?"POR DONDE LO DEJASTE":"RECIÉN AÑADIDO" }
                            Heading { text:root.hero.title || "";font.pixelSize:40;Layout.fillWidth:true }
                            Muted { text:root.hero.kind==="EPISODE"?"Temporada "+root.hero.season+" · Episodio "+root.hero.episode+(root.hero.episodeTitle?" · "+root.hero.episodeTitle:""):(root.hero.year || "Película local")+"" }
                            Item { Layout.fillHeight:true }
                            ProgressBar { visible:root.hero.position>0;Layout.fillWidth:true;Layout.maximumWidth:480;value:(root.hero.position||0)/Math.max(1,root.hero.progressDuration||root.hero.duration||1);background:Rectangle {implicitHeight:4;radius:2;color:"#30323A"} contentItem:Item { Rectangle {width:parent.width*parent.parent.visualPosition;height:4;radius:2;color:"#FF9E7A"} } }
                            Muted { visible:root.hero.position>0;text:root.duration(root.hero.position)+" / "+root.duration(root.hero.progressDuration||root.hero.duration) }
                            RowLayout { Pill {text:root.hero.position>0?"Reanudar":"Reproducir";primary:true;onClicked:root.play(root.hero,false)} Pill {text:"Detalles";onClicked:root.open(root.hero)} }
                        }
                    }
                    ColumnLayout { visible:root.continuing.length>0;Layout.fillWidth:true;spacing:14
                        Heading {text:"Continuar viendo";font.pixelSize:23}
                        ListView { Layout.fillWidth:true;Layout.preferredHeight:112;orientation:ListView.Horizontal;spacing:12;clip:true;model:root.continuing
                            delegate:ItemDelegate { required property var modelData;width:320;height:112;onClicked:root.play(modelData,false);background:Rectangle {color:"#1A1E29";radius:14;border.color:parent.activeFocus?"#8B9CFF":"#2A2D35"} contentItem:RowLayout { ColumnLayout {Layout.fillWidth:true;Text {text:modelData.title;color:"white";font.bold:true;Layout.fillWidth:true;elide:Text.ElideRight} Muted {text:modelData.kind==="EPISODE"?"T"+modelData.season+" · E"+modelData.episode:"Película"} Muted {text:modelData.position>5000?"Quedan "+root.duration((modelData.progressDuration||modelData.duration)-modelData.position):"Siguiente episodio";color:"#FF9E7A"} } Pill {text:"Ocultar";leftPadding:10;rightPadding:10;onClicked:library.hideContinue(modelData.path,true)} } }
                        }
                    }
                    PosterShelf {title:"Películas";items:root.movies}
                    PosterShelf {title:"Series";items:root.shows}
                    Muted { text:library.folders.length+" carpetas vigiladas · "+root.allFiles.length+" archivos" }
                }
            }
        }
    }
    Component {
        id:catalogPage
        ColumnLayout { anchors.fill:parent;anchors.margins:32;spacing:18
            Heading {text:"Biblioteca"}
            RowLayout { Repeater {model:["Películas","Series","Carpetas"];Pill {required property string modelData;text:modelData;selected:root.libraryTab===modelData;onClicked:{if(modelData==="Carpetas")root.page="Carpetas";else root.libraryTab=modelData}}} }
            Flow {Layout.fillWidth:true;spacing:8;Repeater {model:["Todo","Sin ver","En curso","4K"];Pill {required property string modelData;text:modelData;selected:root.filter===modelData;onClicked:root.filter=modelData}}
                ComboBox {model:["Todos los géneros"].concat(root.genres);onActivated:root.selectedGenre=currentIndex?currentText:""}
                ComboBox {model:["Todos los años"].concat(root.years);onActivated:root.selectedYear=currentIndex?currentText:""}
                ComboBox {model:["Título","Año","Añadidos recientemente"];currentIndex:root.orderBy;onActivated:{root.orderBy=currentIndex;root.descending=currentIndex===2}}
                Pill {text:root.descending?"Descendente":"Ascendente";onClicked:root.descending=!root.descending}
            }
            Muted {text:root.catalog().length+" títulos"}
            EmptyState {visible:!root.movies.length&&!root.shows.length}
            GridView {Layout.fillWidth:true;Layout.fillHeight:true;clip:true;cellWidth:180;cellHeight:312;model:root.catalog();delegate:Poster {required property var modelData;media:modelData;onClicked:root.open(modelData)} ScrollBar.vertical:ScrollBar{}}
        }
    }
    Component {
        id:searchPage
        ColumnLayout {id:searchView;anchors.fill:parent;anchors.margins:32;spacing:18
            property var recent:library.setting("recentSearches",[])
            property var found:Search.results(root.search,root.searchScope,root.movies,root.shows,root.allFiles)
            property var suggestion:Search.closest(root.search,root.searchScope==="Películas"?root.movies:root.searchScope==="Series"?root.shows:root.movies.concat(root.shows))
            function rememberQuery(){let q=root.search.trim();if(!q)return;recent=[q].concat(Array.from(recent).filter(s=>s!==q)).slice(0,12);library.setSetting("recentSearches",recent)}
            Heading {text:"Buscar"}
            RowLayout {TextField {id:searchBox;Layout.fillWidth:true;placeholderText:"Título, episodio o nombre de archivo";text:root.search;color:"white";font.pixelSize:18;onTextChanged:root.search=text;onAccepted:searchView.rememberQuery();Component.onCompleted:forceActiveFocus()} Pill {visible:!!root.search;text:"Cancelar";onClicked:root.search=""}}
            RowLayout {Repeater {model:["Todo","Películas","Series","Archivos"];Pill {required property string modelData;text:modelData;selected:root.searchScope===modelData;onClicked:root.searchScope=modelData}}}
            ColumnLayout {visible:!root.search.trim();Layout.fillWidth:true;spacing:12
                RowLayout {Overline {text:"BÚSQUEDAS RECIENTES";visible:searchView.recent.length>0} Pill {visible:searchView.recent.length>0;text:"Borrar";onClicked:{library.setSetting("recentSearches",[]);searchView.recent=[]}}}
                Repeater {model:searchView.recent;Pill {required property string modelData;text:modelData;onClicked:root.search=modelData}}
                Muted {visible:!searchView.recent.length;Layout.fillWidth:true;text:"Busca películas, series, títulos de episodios o el nombre de archivo. No distingue acentos ni mayúsculas."}
            }
            ListView {id:results;Layout.fillWidth:true;Layout.fillHeight:true;clip:true;spacing:10;model:searchView.found
                section.property:"_group";section.delegate:Overline {required property string section;text:section.toUpperCase();height:38;verticalAlignment:Text.AlignVCenter}
                delegate:ItemDelegate {required property var modelData;width:results.width;height:96
                    background:Rectangle {radius:12;color:parent.hovered?"#20242D":"#15181D";border.color:parent.activeFocus?"#8B9CFF":"#262A32"}
                    contentItem:RowLayout {spacing:16
                        Image {Layout.preferredWidth:54;Layout.preferredHeight:76;source:modelData.poster||modelData.thumb||"";asynchronous:true;fillMode:Image.PreserveAspectCrop}
                        ColumnLayout {Layout.fillWidth:true
                            Text {Layout.fillWidth:true;text:Search.highlight(modelData._display,root.search);textFormat:Text.RichText;color:"#F4F5F7";font.bold:true;font.family:modelData._group.indexOf("Archivos")===0?"monospace":"Manrope";elide:Text.ElideRight}
                            Muted {Layout.fillWidth:true;text:modelData.isShow?modelData.episodeCount+" episodios · "+modelData.watchedCount+" vistos":modelData.kind==="EPISODE"?modelData.title+" · T"+modelData.season+" · E"+modelData.episode:(modelData.year||"Local")+" · "+root.duration(modelData.duration);elide:Text.ElideRight}
                            Muted {text:modelData.hidden?"Oculto":modelData.watched?"Visto":modelData.position>5000?"En curso":"Sin ver";color:modelData.position>5000?"#FF9E7A":"#8A919B"}
                        }
                    }
                    onClicked:{searchView.rememberQuery();if(modelData.confidence<80){correction.file=modelData;correction.open()}else root.open(modelData)}
                }
                ScrollBar.vertical:ScrollBar{}
                ColumnLayout {anchors.centerIn:parent;visible:!!root.search.trim()&&results.count===0;spacing:18
                    Heading {text:"Sin resultados"}
                    Muted {Layout.maximumWidth:440;text:"No encuentro «"+root.search+"». Prueba con el nombre del archivo o revisa las coincidencias."}
                    Pill {visible:!!searchView.suggestion.title;text:"¿Querías decir «"+(searchView.suggestion.title||"")+"»?";onClicked:{searchView.rememberQuery();root.open(searchView.suggestion)}}
                    Pill {text:"Revisar coincidencias · "+root.review.length;onClicked:root.page="Revisar"}
                }
            }
        }
    }
    Component {
        id:detailPage
        ScrollView {clip:true;contentWidth:availableWidth
            ColumnLayout {width:parent.width;spacing:22
                ColumnLayout {Layout.fillWidth:true;Layout.margins:32;spacing:22
                    Pill {text:"Volver";onClicked:root.page="Inicio"}
                    Pane {Layout.fillWidth:true;padding:24;background:Backdrop {art:root.selectedArt;horizontal:true}
                    contentItem:RowLayout {spacing:28
                        Poster {media:root.selected;enabled:false}
                        ColumnLayout {Layout.fillWidth:true;spacing:16
                            Overline {text:root.selected.kind==="EPISODE"?"SERIE":"PELÍCULA"}
                            Heading {text:root.selected.title||"";font.pixelSize:38;Layout.fillWidth:true}
                            Muted {text:root.selected.isShow ? root.selected.seasons.length+" temporadas · "+root.selected.episodeCount+" episodios · "+root.selected.watchedCount+" vistos" : (root.selected.year||"Local")+" · "+root.duration(root.selected.duration)}
                            Muted {visible:root.selected.position>0;text:"Te quedaste en "+root.duration(root.selected.position);color:"#FF9E7A"}
                            Flow {Layout.fillWidth:true;spacing:8
                                Pill {text:root.selected.position>0?"Reanudar":root.selected.isShow?"Ver T"+root.selected.season+" · E"+root.selected.episode:"Reproducir";primary:true;onClicked:root.play(root.selected,false)}
                                Pill {text:"Desde el inicio";onClicked:root.play(root.selected,true)}
                                Pill {text:root.selected.watched?"Marcar sin ver":"Marcar visto";onClicked:{library.markWatched(root.selected.path,!root.selected.watched);root.selected=library.media(root.selected.path)}}
                                Pill {text:"Corregir";onClicked:{correction.file=root.selected;correction.open()}}
                                Pill {text:"Descargar metadatos";enabled:!root.selectedMetaJob.running;onClicked:library.downloadMetadata(root.selected.path)}
                                Pill {text:"Buscar otra ficha";onClicked:{metadataQuery.text=root.selected.fileTitle||root.selected.title;metadataDialog.open();library.findMetadata(metadataQuery.text,root.selected.kind==="EPISODE")}}
                                Pill {text:"Cambiar carátula";visible:root.selected.tmdbId>0;onClicked:{artDialog.backdrop=false;library.findArtwork(root.selected.path,false);artDialog.open()}}
                                Pill {text:"Cambiar fondo";visible:root.selected.tmdbId>0;onClicked:{artDialog.backdrop=true;library.findArtwork(root.selected.path,true);artDialog.open()}}
                                Pill {text:"Mostrar en Continuar";visible:root.selected.updated>0;onClicked:library.hideContinue(root.selected.path,false)}
                            }
                        }
                    }
                    }
                    RowLayout {visible:!!root.selectedMetaJob.text;BusyIndicator {running:!!root.selectedMetaJob.running;visible:running;Layout.preferredWidth:26;Layout.preferredHeight:26} Muted {text:root.selectedMetaJob.text||"";Layout.fillWidth:true}}
                    Muted {Layout.fillWidth:true;text:root.selected.plot||"Sin sinopsis. Puedes añadir metadatos locales con un archivo .nfo."}
                    Muted {Layout.fillWidth:true;text:root.selected.genres||""}
                    Muted {visible:!!root.selected.episodeSource;text:"Episodios: "+(root.selected.episodeSource||"")}
                    RowLayout {visible:!!root.selected.episodeStructureUrl;Pill {text:"Guía de emisión";onClicked:Qt.openUrlExternally(root.selected.episodeStructureUrl)} Pill {visible:!!root.selected.episodeSourceUrl;text:"Texto de episodios";onClicked:Qt.openUrlExternally(root.selected.episodeSourceUrl)}}
                    Heading {text:root.selected.kind==="EPISODE"?"Episodios":"Ficha técnica";font.pixelSize:23}
                    RowLayout {visible:!!root.selected.isShow;Switch {text:"Evitar spoilers en esta serie";checked:root.selected.spoilerEnabled||false;palette.windowText:"#C2C8D0";onToggled:library.setShowSpoilers(root.selected.showKey,checked?1:0)} Pill {text:"Usar ajuste general";onClicked:library.setShowSpoilers(root.selected.showKey,-1)}}
                    Flow {visible:!!root.selected.isShow;Layout.fillWidth:true;spacing:8
                        Pill {text:"Todas";selected:root.selectedSeason<0;onClicked:root.selectedSeason=-1}
                        Repeater {model:root.selected.seasons||[];Pill {required property var modelData;text:modelData===0?"Especiales":"Temporada "+modelData;selected:root.selectedSeason===modelData;onClicked:root.selectedSeason=modelData}}
                        Pill {text:root.episodeDescending?"Más recientes primero":"Orden de emisión";onClicked:root.episodeDescending=!root.episodeDescending}
                    }
                    ListView {id:episodeList;visible:!!root.selected.isShow;Layout.fillWidth:true;Layout.preferredHeight:Math.min(500,root.selectedEpisodes.length*104);clip:true;spacing:10;reuseItems:true;model:root.selectedEpisodes;delegate:EpisodeLine {required property var modelData;file:modelData;width:episodeList.width} ScrollBar.vertical:ScrollBar{}}
                    Text {Layout.fillWidth:true;text:root.selected.name||"";font.family:"monospace";font.pixelSize:13;color:"#C2C8D0";wrapMode:Text.WrapAnywhere}
                    Muted {text:(root.selected.codec||"Códec desconocido")+" · "+(root.selected.width||0)+" × "+(root.selected.height||0)+" · "+((root.selected.size||0)/1073741824).toFixed(2)+" GB"}
                    Repeater {model:(root.selected.probe||{}).streams||[];Muted {required property var modelData;text:(modelData.codec_type||"")+" · "+(modelData.codec_name||"")+" · "+((modelData.tags||{}).language||"Idioma desconocido")+(modelData.channels?" · "+modelData.channels+" canales":"")}}
                }
            }
        }
    }
    Component {
        id:foldersPage
        ColumnLayout {anchors.fill:parent;anchors.margins:32;spacing:18
            Heading {text:"Carpetas vigiladas"}
            RowLayout {Pill {text:"Añadir una carpeta";primary:true;onClicked:chooseFolder.open()} Pill {text:"Volver a explorar";enabled:!library.scanning;onClicked:library.scan()}}
            Muted {text:"Arca solo lee tus archivos. El escaneo continúa en segundo plano."}
            ListView {id:folderList;Layout.fillWidth:true;Layout.fillHeight:true;clip:true;spacing:10;model:library.folders
                delegate:ItemDelegate {required property var modelData;width:folderList.width;height:100;onClicked:{root.folderPath=modelData.path;root.page="Archivos"}
                    background:Rectangle {color:"#15181D";radius:14}
                    contentItem:RowLayout {ColumnLayout {Layout.fillWidth:true;Text {text:modelData.path;color:"#F4F5F7";font.family:"monospace";elide:Text.ElideMiddle;Layout.fillWidth:true} Muted {text:modelData.kind+" · "+(modelData.lastScan?"Última exploración: "+new Date(Number(modelData.lastScan)).toLocaleString():"Pendiente de explorar")} } Pill {text:"Quitar";enabled:!library.scanning;onClicked:library.removeFolder(modelData.id)} }
                }
                ScrollBar.vertical:ScrollBar{}
            }
        }
    }
    Component {
        id:filesPage
        ColumnLayout {anchors.fill:parent;anchors.margins:32;spacing:18
            Heading {text:root.page==="Revisar"?"Revisar coincidencias":"Archivos de la carpeta"}
            Muted {Layout.fillWidth:true;text:root.page==="Revisar"?"Confirma las coincidencias o corrige la temporada y el episodio. Puedes guardar una regla para toda la carpeta.":root.folderPath}
            RowLayout {visible:root.page!=="Revisar";Repeater {model:["Todo","Vídeos","Subtítulos","Dudosos","Duplicados","Ocultos"];Pill {required property string modelData;text:modelData;selected:root.fileFilter===modelData;onClicked:root.fileFilter=modelData}}}
            Pill {visible:root.page==="Revisar"&&root.review.some(f=>f.confidence>=50&&f.altSeason<0&&(f.kind==="MOVIE"||f.kind==="EPISODE"));text:"Aceptar las coincidencias seguras";onClicked:library.acceptSafeGuesses()}
            ListView {id:fileList;Layout.fillWidth:true;Layout.fillHeight:true;clip:true;spacing:10;model:root.page==="Revisar"?root.review:root.allFiles.filter(f=>f.path.indexOf(root.folderPath+"/")===0&&(root.fileFilter==="Todo"||root.fileFilter==="Vídeos"&&f.kind!=="SUBTITLE"||root.fileFilter==="Subtítulos"&&f.kind==="SUBTITLE"||root.fileFilter==="Dudosos"&&f.confidence<80||root.fileFilter==="Duplicados"&&(f.duplicate||f.hasDuplicates)||root.fileFilter==="Ocultos"&&f.hidden));delegate:FileLine {required property var modelData;file:modelData;width:fileList.width} ScrollBar.vertical:ScrollBar{} }
            Muted {visible:fileList.count===0;text:"No hay archivos pendientes."}
        }
    }
    Component {
        id:profilesPage
        ScrollView {clip:true;contentWidth:availableWidth;leftPadding:32;rightPadding:32;topPadding:32;bottomPadding:32
        ColumnLayout {width:parent.width;spacing:18
            Heading {text:"Perfiles y copia del progreso"}
            Muted {text:"Cada perfil guarda su propio progreso. Todo se queda en este equipo."}
            Repeater {model:library.profiles;RowLayout {required property var modelData;Layout.fillWidth:true
                Rectangle {Layout.preferredWidth:44;Layout.preferredHeight:44;radius:22;color:modelData.color;Text {anchors.centerIn:parent;text:modelData.name.slice(0,1).toUpperCase();font.bold:true;font.pixelSize:22;color:"#0A0B0E"}}
                Pill {text:modelData.name;selected:library.profile===modelData.name;onClicked:library.selectProfile(modelData.id)}
                Muted {text:(modelData.inProgress||0)+" en curso · "+(modelData.watched||0)+" vistos"}
                Pill {text:"Editar";onClicked:{profileEditor.profile=modelData;profileEditor.open()}}
            }}
            RowLayout {TextField {id:profileName;placeholderText:"Nombre del perfil";color:"white"} Pill {text:"Añadir perfil";onClicked:{library.createProfile(profileName.text);profileName.clear()}}}
            Heading {text:"Carpetas visibles para "+library.profile;font.pixelSize:22;Layout.topMargin:12}
            Repeater {model:library.folders;CheckBox {required property var modelData;Layout.fillWidth:true;text:modelData.path;font.family:"monospace";font.pixelSize:12;checked:modelData.visible;palette.windowText:"#C2C8D0";onToggled:library.setFolderVisible(modelData.id,checked);ToolTip.visible:hovered;ToolTip.text:modelData.path}}
            Pill {text:"Mostrar todas las carpetas, también las nuevas";onClicked:library.showAllFolders()}
            Heading {text:"Copia de seguridad";font.pixelSize:22;Layout.topMargin:20}
            RowLayout {Pill {text:"Exportar progreso";onClicked:backupOptions.open()} Pill {text:"Importar y unir";onClicked:importDialog.open()}}
            Muted {Layout.fillWidth:true;text:library.backupStatus||"Compatible con las copias de Arca Android. La importación conserva el progreso más reciente por archivo y perfil."}
            Repeater {model:library.pendingFolders;RowLayout {required property var modelData;Layout.fillWidth:true
                Muted {Layout.fillWidth:true;text:"Pendiente: "+modelData.name;elide:Text.ElideMiddle}
                Pill {text:"Elegir carpeta";onClicked:{importFolder.reference=modelData.treeUri;importFolder.kind=modelData.kind;importFolder.open()}}
            }}
            Pill {visible:library.pendingFolders.length>0;text:"Descartar datos pendientes";onClicked:discardPendingDialog.open()}
        }
        }
    }
    Component {
        id:settingsPage
        ScrollView {clip:true;contentWidth:availableWidth
            ColumnLayout {width:parent.width
                ColumnLayout {Layout.fillWidth:true;Layout.margins:32;spacing:18
                    Heading {text:"Ajustes"}
                    Heading {text:"Biblioteca";font.pixelSize:22}
                    Pill {text:"Carpetas vigiladas · "+library.folders.length;onClicked:root.page="Carpetas"}
                    Pill {text:"Revisar coincidencias · "+root.review.length;onClicked:root.page="Revisar"}
                    Repeater {model:[{key:"scanOnOpen",label:"Explorar al abrir la app"},{key:"ignoreSamples",label:"Ignorar muestras y extras"},{key:"resume",label:"Reanudar donde lo dejé"},{key:"autoNext",label:"Siguiente episodio automático"},{key:"playerProgressBar",label:"Barra al esconder los controles"}]
                        Switch {required property var modelData;text:modelData.label;checked:library.setting(modelData.key,true);palette.windowText:"#F4F5F7";onToggled:library.setSetting(modelData.key,checked)}
                    }
                    Switch {text:"Evitar spoilers de episodios sin ver";checked:library.setting("spoilerFree",false);palette.windowText:"#F4F5F7";onToggled:library.setSetting("spoilerFree",checked)}
                    Switch {text:"Usar la carátula de la temporada en la ficha";checked:library.setting("seasonArt",true);palette.windowText:"#F4F5F7";onToggled:library.setSetting("seasonArt",checked)}
                    ComboBox {model:["Numeración corrida: preguntar","Conservar mis números","Repartir en temporadas"];currentIndex:["ASK","DATA","RENUMBER"].indexOf(library.setting("absoluteMode","ASK"));onActivated:library.setSetting("absoluteMode",["ASK","DATA","RENUMBER"][currentIndex])}
                    RowLayout {Muted {text:"Segundos atrás al reanudar"} SpinBox {from:0;to:10;value:library.setting("resumeRewindSeconds",3);onValueModified:library.setSetting("resumeRewindSeconds",value)}}
                    RowLayout {Muted {text:"Segundos al avanzar o retroceder"} SpinBox {from:1;to:60;value:library.setting("skipSeconds",10);onValueModified:library.setSetting("skipSeconds",value)}}
                    ComboBox {model:["Hardware, con recurso a software","Siempre por software"];currentIndex:library.setting("decoder","HARDWARE")==="SOFTWARE"?1:0;onActivated:library.setSetting("decoder",currentIndex===1?"SOFTWARE":"HARDWARE")}
                    Heading {text:"Subtítulos y audio";font.pixelSize:22}
                    Muted {text:"Idiomas preferidos, por orden (por ejemplo: es,en)"}
                    TextField {text:library.setting("languages","es,en");placeholderText:"es,en";color:"white";onEditingFinished:library.setSetting("languages",text.trim().toLowerCase())}
                    ComboBox {model:["Subtítulos pequeños","Subtítulos medianos","Subtítulos grandes"];currentIndex:library.setting("subtitleScale",1)<0.9?0:library.setting("subtitleScale",1)>1.1?2:1;onActivated:library.setSetting("subtitleScale",[0.8,1,1.3][currentIndex])}
                    Heading {text:"Perfiles y progreso";font.pixelSize:22}
                    Pill {text:"Perfiles y copia de seguridad";onClicked:root.page="Perfiles"}
                    Heading {text:"Metadatos de internet";font.pixelSize:22}
                    Muted {Layout.fillWidth:true;text:"Opcional. Usa tu clave o token de TMDB para elegir sinopsis y carátulas. Sin conexión, Arca utiliza la biblioteca local."}
                    TextField {id:tmdbKey;Layout.fillWidth:true;Layout.maximumWidth:560;placeholderText:"Clave API o token de TMDB";echoMode:TextInput.Password;color:"white";text:library.setting("tmdbKey","");onEditingFinished:library.setSetting("tmdbKey",text)}
                    ComboBox {model:["Español","English"];currentIndex:library.setting("metaLang","es-ES")==="en-US"?1:0;onActivated:library.setSetting("metaLang",currentIndex===1?"en-US":"es-ES")}
                    Pill {text:"Vaciar metadatos y carátulas · "+(library.cacheSize()/1048576).toFixed(1)+" MB";onClicked:clearCacheDialog.open()}
                    Heading {text:"Arca Desktop";font.pixelSize:22}
                    Muted {text:"Qt Quick · libmpv · Biblioteca local · Sin cuenta"}
                    Muted {Layout.fillWidth:true;text:"Este producto usa la API de TMDB pero no está respaldado ni certificado por TMDB."}
                    Muted {text:"Atajos: Ctrl+O añadir carpeta · Ctrl+F buscar · Alt+← volver\nReproductor: espacio pausa · ←/→ 10 s · F pantalla completa · Esc salir"}
                }
            }
        }
    }
    Dialog {id:discardPendingDialog;anchors.centerIn:parent;modal:true;title:"¿Descartar lo que sigue pendiente?";standardButtons:Dialog.Ok|Dialog.Cancel;onAccepted:library.clearPendingBackup();Label {text:"Se conserva todo lo que ya se ha restaurado."}}
    Dialog {id:clearCacheDialog;anchors.centerIn:parent;modal:true;title:"¿Vaciar los metadatos descargados?";standardButtons:Dialog.Ok|Dialog.Cancel;onAccepted:library.clearMetadata();Label {text:"Se conservarán tus archivos, correcciones y progreso."}}
}
