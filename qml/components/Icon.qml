import QtQuick
import QtQuick.Shapes
import PersonalFinance

// Vector icon drawn from Theme.iconPaths (24x24 design grid).
Item {
    id: root

    property string name: ""
    property color color: Theme.text
    property real size: 24
    property real strokeWidth: 1.8

    implicitWidth: size
    implicitHeight: size

    Shape {
        width: 24
        height: 24
        scale: root.size / 24
        transformOrigin: Item.TopLeft

        // Render the vector at 3x so it stays crisp when scaled up on Retina screens.
        layer.enabled: true
        layer.samples: 4
        layer.textureSize: Qt.size(Math.ceil(root.size) * 3, Math.ceil(root.size) * 3)

        ShapePath {
            strokeColor: root.color
            strokeWidth: root.strokeWidth
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            joinStyle: ShapePath.RoundJoin

            PathSvg {
                path: Theme.iconPaths[root.name] || ""
            }
        }
    }
}
