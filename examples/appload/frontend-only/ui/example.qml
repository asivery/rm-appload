import QtQuick 2.5
import QtQuick.Controls 2.5
import net.asivery.ApploadUtils

Rectangle {
    anchors.fill: parent
    property var counter: 1

    signal close
    function unloading() {
        console.log("We're unloading!");
    }

    Text {
        id: text
        anchors.centerIn: parent
        width: parent.width
        horizontalAlignment: Text.AlignHCenter
        text: `Hello, world! Counter is ${counter}`
        font.pointSize: 48

        DisplayMethodArea {
            anchors.fill: parent
            displayMethod: DisplayMethodArea.Fast
        }
    }

    Rectangle {
        anchors.top: text.bottom
        anchors.topMargin: 20
        anchors.horizontalCenter: parent.horizontalCenter
        border.width: 2
        border.color: "black"
        width: 300
        height: 150

        Text {
            text: "Click me!"
            font.pointSize: 24
            anchors.centerIn: parent
        }

        MouseArea {
            anchors.fill: parent
            onClicked: () => counter += 1
        }
    }
}
