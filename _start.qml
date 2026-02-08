import QtQuick 2.5
import QtQuick.Window 2.2
import QtQuick.Controls 2.5

import net.asivery.AppLoad 1.0

Window {
    visible: true
    title: qsTr("AppLoad - PC emulator")
    width: 1620 / 4
    height: 2160 / 4
    id: window

    Rectangle {
        anchors.fill: parent
        color: "#f0f0f0"

        Loader {
            id: loader
            source: "qrc:/appload/qml/appload.qml"
            active: true

            width: 1620
            height: 2160

            transform: Scale {
                origin.x: loader.width / 2
                origin.y: loader.height / 2
                xScale: Math.min(window.width / loader.width, window.height / loader.height)
                yScale: Math.min(window.width / loader.width, window.height / loader.height)
            }

            anchors.centerIn: parent

            onLoaded: {
                loader.item.visible = true;
                loader.item.virtualKeyboardRef = keyboardLoader;
            }
        }
    }
    Loader {
        property var layout: null
        property var config: null
        id: keyboardLoader
        source: "qrc:/appload/qml/virtualKeyboard/Keyboard.qml"
        active: false
        width: parent.width
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter

        onLoaded: () => {
            keyboardLoader.item.rebuildKeyboard(keyboardLoader.layout, keyboardLoader.config);
        }
    }
}
