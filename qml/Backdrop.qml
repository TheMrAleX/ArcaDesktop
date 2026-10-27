import QtQuick
import Qt5Compat.GraphicalEffects

Item {
    id:backdrop
    property string art:""
    property bool horizontal:false
    clip:true
    Rectangle {anchors.fill:parent;color:"#151A29"}
    Image {id:image;anchors.fill:parent;source:backdrop.art;fillMode:Image.PreserveAspectCrop;sourceSize.width:640;asynchronous:true;visible:false}
    FastBlur {anchors.fill:parent;source:image;radius:64;cached:true;visible:image.status===Image.Ready}
    Rectangle {anchors.fill:parent;gradient:Gradient {orientation:backdrop.horizontal?Gradient.Horizontal:Gradient.Vertical;GradientStop {position:0;color:backdrop.horizontal?"#F00A0B0E":"#650A0B0E"} GradientStop {position:0.76;color:backdrop.horizontal?"#700A0B0E":"#E60A0B0E"} GradientStop {position:1;color:backdrop.horizontal?"#300A0B0E":"#0A0B0E"}}}
}
