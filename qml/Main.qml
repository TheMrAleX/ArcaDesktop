import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

ApplicationWindow {
    id: root
    width: 1280; height: 820; minimumWidth: 760; minimumHeight: 560
    visible: true; title: "Arca"; color: "#0A0B0E"
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
    property var allFiles: library.files
    property var movies: allFiles.filter(f => f.kind === "MOVIE" && f.confidence >= 80)
    property var episodes: allFiles.filter(f => f.kind === "EPISODE" && f.confidence >= 80)
    property var review: allFiles.filter(f => f.kind !== "IGNORED" && (f.confidence < 80 || f.kind === "UNKNOWN"))
    property var shows: library.shows
    property var continuing: library.continuing
    property var genres: Array.from(new Set(movies.concat(shows).reduce((all,f)=>all.concat((f.genres || "").split(" · ")),[]).filter(s=>s.length))).sort()
    property var years: Array.from(new Set(movies.concat(shows).map(f=>String(f.year || "")).filter(s=>s.length))).sort().reverse()
    property var selectedEpisodes: episodes.filter(f=>f.showKey===selected.showKey && (selectedSeason<0 || f.season===selectedSeason)).sort((a,b)=>episodeDescending ? b.season-a.season || b.episode-a.episode : a.season-b.season || a.episode-b.episode)
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
    Connections {target:library;function onChanged(){root.refreshSelected()}}

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
    FileDialog { id: exportDialog; title: "Exportar progreso"; fileMode: FileDialog.SaveFile; defaultSuffix: "json"; nameFilters: ["Copia de Arca (*.json)"]; onAccepted: library.exportBackup(selectedFile) }
    FileDialog { id: importDialog; title: "Importar progreso"; nameFilters: ["Copia de Arca (*.json)"]; onAccepted: library.importBackup(selectedFile) }
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
    Dialog { id: unavailable; anchors.centerIn: parent; modal: true; title: "Archivo no disponible"; standardButtons: Dialog.Ok; Label { text: "Monta el disco o vuelve a conectar la carpeta y explora de nuevo."; color: "#F4F5F7" } background: Rectangle { color: "#15181D"; radius: 16 } }
    Dialog {
        id: correction; anchors.centerIn: parent; modal: true; width: 560; title: "Corregir coincidencia"
        property var file: ({})
        onOpened: { correctionTitle.text=file.title || ""; correctionSeason.value=Math.max(0,file.season || 0); correctionEpisode.value=file.episode || 0; ruleCheck.checked=false }
        background: Rectangle { color: "#15181D"; radius: 18; border.color: "#FF9E7A" }
        contentItem: ColumnLayout {
            spacing: 14
            Text { Layout.fillWidth: true; text: correction.file.name || ""; font.family: "monospace"; color: "#C2C8D0"; wrapMode: Text.WrapAnywhere }
            TextField { id: correctionTitle; Layout.fillWidth: true; placeholderText: "Título de película o serie"; color: "white" }
            RowLayout { Label { text: "Temporada"; color: "#C2C8D0" } SpinBox { id: correctionSeason; from: 0; to: 999; editable: true } Label { text: "Episodio (0 = película)"; color: "#C2C8D0" } SpinBox { id: correctionEpisode; from: 0; to: 9999; editable: true } }
            CheckBox { id: ruleCheck; text: "Recordar la numeración para toda esta carpeta"; palette.windowText: "#C2C8D0" }
            RowLayout { Pill { text: "Guardar"; primary: true; onClicked: {library.correct(correction.file.path,correctionTitle.text,correctionSeason.value,correctionEpisode.value,ruleCheck.checked);correction.close()} } Pill { text: "Cancelar"; onClicked: correction.close() } }
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
                Text { Layout.fillWidth:true; text:!file.available?"No disponible":file.duplicate?"Duplicado":file.reason || (file.watched?"Visto":file.confidence+" % de confianza"); color:file.confidence<80?"#FF9E7A":"#8A919B"; font.pixelSize:11; elide:Text.ElideRight }
            }
            Pill { text:root.page==="Revisar"?"Es correcto":"Reproducir"; onClicked:root.page==="Revisar"?library.accept(file.path):root.play(file,false) }
            Pill { text:"Corregir"; onClicked:{correction.file=file;correction.open()} }
        }
        onClicked:root.open(file)
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
                Image {anchors.fill:parent;source:file.spoilerHidden?"":file.still||file.poster||"";asynchronous:true;fillMode:Image.PreserveAspectCrop;sourceSize.width:216}
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
                        Image { anchors.fill:parent;source:root.hero.backdrop || root.hero.poster || "";fillMode:Image.PreserveAspectCrop;asynchronous:true;opacity:0.3 }
                        Rectangle { anchors.fill:parent;gradient:Gradient { orientation:Gradient.Horizontal;GradientStop {position:0;color:"#EE0A0B0E"} GradientStop {position:1;color:"#200A0B0E"} } }
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
        ColumnLayout { anchors.fill:parent;anchors.margins:32;spacing:18
            Heading {text:"Buscar"}
            TextField { id:searchBox;Layout.fillWidth:true;placeholderText:"Películas, series o nombres de archivo";text:root.search;color:"white";font.pixelSize:18;onTextChanged:root.search=text;Component.onCompleted:forceActiveFocus() }
            RowLayout { Repeater {model:["Todo","Películas","Series","Archivos"];Pill {required property string modelData;text:modelData;selected:root.searchScope===modelData;onClicked:root.searchScope=modelData}} }
            ListView {id:results;Layout.fillWidth:true;Layout.fillHeight:true;clip:true;spacing:10
                model:root.allFiles.filter(f=>f.kind!=="IGNORED"&&(root.searchScope==="Todo"||root.searchScope==="Archivos"||root.searchScope==="Películas"&&f.kind==="MOVIE"||root.searchScope==="Series"&&f.kind==="EPISODE")&&(root.key(f.title+" "+f.name+" "+(f.episodeTitle||"")).indexOf(root.key(root.search))>=0))
                delegate:FileLine {required property var modelData;file:modelData;width:results.width}
                ScrollBar.vertical:ScrollBar{}
                Text {anchors.centerIn:parent;visible:results.count===0;text:"Sin resultados para «"+root.search+"»\nPrueba con el nombre del archivo o revisa las coincidencias.";color:"#A8AEB8";horizontalAlignment:Text.AlignHCenter}
            }
        }
    }
    Component {
        id:detailPage
        ScrollView {clip:true;contentWidth:availableWidth
            ColumnLayout {width:parent.width;spacing:22
                ColumnLayout {Layout.fillWidth:true;Layout.margins:32;spacing:22
                    Pill {text:"Volver";onClicked:root.page="Inicio"}
                    RowLayout {spacing:28;Layout.fillWidth:true
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
                                Pill {text:"Buscar metadatos";onClicked:{metadataQuery.text=root.selected.fileTitle||root.selected.title;metadataDialog.open();library.findMetadata(metadataQuery.text,root.selected.kind==="EPISODE")}}
                                Pill {text:"Mostrar en Continuar";visible:root.selected.updated>0;onClicked:library.hideContinue(root.selected.path,false)}
                            }
                        }
                    }
                    Muted {Layout.fillWidth:true;text:root.selected.plot||"Sin sinopsis. Puedes añadir metadatos locales con un archivo .nfo."}
                    Muted {Layout.fillWidth:true;text:root.selected.genres||""}
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
            ListView {id:fileList;Layout.fillWidth:true;Layout.fillHeight:true;clip:true;spacing:10;model:root.page==="Revisar"?root.review:root.allFiles.filter(f=>f.path.indexOf(root.folderPath+"/")===0);delegate:FileLine {required property var modelData;file:modelData;width:fileList.width} ScrollBar.vertical:ScrollBar{} }
            Muted {visible:fileList.count===0;text:"No hay archivos pendientes."}
        }
    }
    Component {
        id:profilesPage
        ScrollView {clip:true;contentWidth:availableWidth;leftPadding:32;rightPadding:32;topPadding:32;bottomPadding:32
        ColumnLayout {width:parent.width;spacing:18
            Heading {text:"Perfiles y copia del progreso"}
            Muted {text:"Cada perfil guarda su propio progreso. Todo se queda en este equipo."}
            Repeater {model:library.profiles;Pill {required property var modelData;text:modelData.name;selected:library.profile===modelData.name;onClicked:library.selectProfile(modelData.id)}}
            RowLayout {TextField {id:profileName;placeholderText:"Nombre del perfil";color:"white"} Pill {text:"Añadir perfil";onClicked:{library.createProfile(profileName.text);profileName.clear()}}}
            Heading {text:"Carpetas visibles para "+library.profile;font.pixelSize:22;Layout.topMargin:12}
            Repeater {model:library.folders;CheckBox {required property var modelData;Layout.fillWidth:true;text:modelData.path;font.family:"monospace";font.pixelSize:12;checked:modelData.visible;palette.windowText:"#C2C8D0";onToggled:library.setFolderVisible(modelData.id,checked);ToolTip.visible:hovered;ToolTip.text:modelData.path}}
            Pill {text:"Mostrar todas las carpetas, también las nuevas";onClicked:library.showAllFolders()}
            Heading {text:"Copia de seguridad";font.pixelSize:22;Layout.topMargin:20}
            RowLayout {Pill {text:"Exportar progreso";onClicked:exportDialog.open()} Pill {text:"Importar y unir";onClicked:importDialog.open()}}
            Muted {text:"La importación conserva el progreso más reciente por archivo y perfil."}
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
                    Repeater {model:[{key:"scanOnOpen",label:"Explorar al abrir la app"},{key:"ignoreSamples",label:"Ignorar muestras y extras"},{key:"resume",label:"Reanudar donde lo dejé"},{key:"autoNext",label:"Siguiente episodio automático"}]
                        Switch {required property var modelData;text:modelData.label;checked:library.setting(modelData.key,true);palette.windowText:"#F4F5F7";onToggled:library.setSetting(modelData.key,checked)}
                    }
                    Switch {text:"Evitar spoilers de episodios sin ver";checked:library.setting("spoilerFree",false);palette.windowText:"#F4F5F7";onToggled:library.setSetting("spoilerFree",checked)}
                    RowLayout {Muted {text:"Segundos atrás al reanudar"} SpinBox {from:0;to:10;value:library.setting("resumeRewindSeconds",3);onValueModified:library.setSetting("resumeRewindSeconds",value)}}
                    RowLayout {Muted {text:"Segundos al avanzar o retroceder"} SpinBox {from:1;to:60;value:library.setting("skipSeconds",10);onValueModified:library.setSetting("skipSeconds",value)}}
                    Heading {text:"Subtítulos y audio";font.pixelSize:22}
                    Muted {text:"Idiomas preferidos, por orden (por ejemplo: es,en)"}
                    TextField {text:library.setting("languages","es,en");placeholderText:"es,en";color:"white";onEditingFinished:library.setSetting("languages",text.trim().toLowerCase())}
                    Heading {text:"Perfiles y progreso";font.pixelSize:22}
                    Pill {text:"Perfiles y copia de seguridad";onClicked:root.page="Perfiles"}
                    Heading {text:"Metadatos de internet";font.pixelSize:22}
                    Muted {Layout.fillWidth:true;text:"Opcional. Usa tu clave o token de TMDB para elegir sinopsis y carátulas. Sin conexión, Arca utiliza la biblioteca local."}
                    TextField {id:tmdbKey;Layout.fillWidth:true;Layout.maximumWidth:560;placeholderText:"Clave API o token de TMDB";echoMode:TextInput.Password;color:"white";text:library.setting("tmdbKey","");onEditingFinished:library.setSetting("tmdbKey",text)}
                    ComboBox {model:["Español","English"];currentIndex:library.setting("metaLang","es-ES")==="en-US"?1:0;onActivated:library.setSetting("metaLang",currentIndex===1?"en-US":"es-ES")}
                    Heading {text:"Arca Desktop";font.pixelSize:22}
                    Muted {text:"Qt Quick · Biblioteca local · Sin cuenta"}
                    Muted {text:"Atajos: Ctrl+O añadir carpeta · Ctrl+F buscar · Alt+← volver\nReproductor: espacio pausa · ←/→ 10 s · F pantalla completa · Esc salir"}
                }
            }
        }
    }
}
