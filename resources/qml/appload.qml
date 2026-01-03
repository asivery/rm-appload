import QtQuick 2.5
import QtQuick.Window 2.2
import QtQuick.Controls 2.5
import net.asivery.AppLoad 1.0
import net.asivery.Framebuffer 1.0

Rectangle {
    id: _appLoadView
    anchors.fill: parent
    color: "#f0f0f0"

    property var windowArchetype: Qt.createComponent("window.qml")
    property var absoluteRoot: _appLoadView

    signal requestClose

    AppLoadLibrary {
        id: library
    }

    Rectangle {
        anchors.fill: parent
        MouseArea {
            // Eat the events.
            anchors.fill: parent
        }

        Rectangle {
            id: header
            anchors.top: parent.top
            width: parent.width
            height: 100

            Rectangle {
                anchors.topMargin: 25
                anchors.leftMargin: 25
                anchors.top: parent.top
                anchors.left: parent.left
                height: 60
                width: 250

                MouseArea {
                    anchors.fill: parent
                    onClicked: () => {
                        _appLoadView.requestClose();
                    }
                }

                Image {
                    id: arrowBack
                    source: "qrc:/appload/icons/exit"
                    sourceSize.width: 60
                    sourceSize.height: 60
                    anchors.left: parent.left
                }
                Text {
                    anchors.left: arrowBack.right
                    anchors.leftMargin: 10
                    anchors.verticalCenter: parent.verticalCenter
                    font.pointSize: 24
                    text: qsTr("Back")
                }
            }

            Rectangle {
                anchors.topMargin: 25
                anchors.rightMargin: 25
                anchors.top: parent.top
                anchors.right: parent.right
                height: 60
                width: 250

                MouseArea {
                    anchors.fill: parent
                    onClicked: () => library.reloadList()
                }

                Image {
                    id: reloadIcon
                    source: "qrc:/appload/icons/reload"
                    sourceSize.width: 60
                    sourceSize.height: 60
                    anchors.right: parent.right
                }
                Text {
                    anchors.right: reloadIcon.left
                    anchors.rightMargin: 10
                    anchors.verticalCenter: parent.verticalCenter
                    font.pointSize: 24
                    text: qsTr("Reload")
                }
            }

            Text {
                anchors.fill: parent
                text: "AppLoad"
                font.pointSize: 40
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
        }
        GridView {
            anchors.top: header.bottom
            anchors.topMargin: 20
            anchors.horizontalCenter: parent.horizontalCenter
            width: Math.floor((parent.width - 20) / cellWidth) * cellWidth
            height: parent.height - 100
            id: gridView
            model: library.applications
            cellWidth: 200 + 20
            cellHeight: 200 + 20
            interactive: false

            delegate: Rectangle {
                required property var modelData
                anchors.margins: 10
                width: gridView.cellWidth - 20
                height: gridView.cellHeight - 20

                Image {
                    anchors.top: parent.top
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.margins: 10
                    id: appIcon
                    source: modelData.icon
                    width: 150
                    height: 150
                }

                Text {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: appIcon.bottom
                    wrapMode: Text.WordWrap
                    horizontalAlignment: Text.AlignHCenter
                    anchors.topMargin: 5
                    text: modelData.name
                    font.pointSize: 24
                    elide: Text.ElideRight
                }

                MouseArea {
                    anchors.fill: parent
                    function getMinResolutionFor(device) {
                        switch(device) {
                            case "original": return [400, 533];
                            case "move": return [400, 688];
                        }
                    }
                    function getResolutionOf(device) {
                        switch(device) {
                            case "original": return [1620, 2160];
                            case "move": return [954, 1696];
                        }
                    }
                    function launchWindow() {
                        let qtfbKey = -1;
                        let win;

                        if(modelData.externalType === 2 /* EXTERNAL_QTFB */) {
                            qtfbKey = Math.floor(Math.random() * 10000000);
                        }
                        if(modelData.externalType != 1 /* EXTERNAL_NOGUI */) {
                            /* Create a new window*/
                            if(library.isFrontendRunningFor(modelData.id) && !modelData.canHaveMultipleFrontends) {
                                console.log("Cannot load multiple frontends for app. It doesn't support it.");
                                return;
                            }
                            if(windowArchetype.status !== Component.Ready) {
                                console.log("Window object not ready: " + windowArchetype.status);
                                console.log(windowArchetype.errorString());
                                return;
                            }
                            win = windowArchetype.createObject(absoluteRoot, { x: 100, y: 100 });
                            if(win == null) {
                                console.log("Failed to instantiate a window object!");
                                return;
                            }

                            win.appName = modelData.name;
                            win.supportsScaling = modelData.supportsScaling;
                            win.disablesWindowedMode = modelData.disablesWindowedMode;

                            win.globalWidth = Qt.binding(function() { return _appLoadView.width; })
                            win.globalHeight = Qt.binding(function() { return _appLoadView.height; })
                            let deviceAspectRatio, applicationAspectRatio = modelData.aspectRatio;
                            const aspectRatioId = _appLoadView.width < _appLoadView.height ? Math.round(100 * _appLoadView.width / _appLoadView.height) : Math.round(100 * _appLoadView.height / _appLoadView.width);
                            switch(aspectRatioId) {
                                case 75:
                                    deviceAspectRatio = "original";
                                    break;
                                case 56:
                                    deviceAspectRatio = "move";
                                    break
                            }
                            const realAspectRatio = applicationAspectRatio == "auto" ? deviceAspectRatio : applicationAspectRatio;
                            console.log(`Application starting on device with ${deviceAspectRatio} aspect ratio (${aspectRatioId}). Real aspect ratio of the application is going to be ${realAspectRatio}`);
                            [win.minWidth, win.minHeight] = [win.implicitWidth, win.implicitHeight] = getMinResolutionFor(realAspectRatio);
                            [win.scaledContentWidth, win.scaledContentHeight] = getResolutionOf(realAspectRatio);

                            win.qtfbKey = qtfbKey;

                            win.closed.connect(() => win.destroy());

                        }
                        if(modelData.externalType == 0 /* INTERNAL */) {
                            win.loadApplication(modelData.id);
                        } else if(modelData.externalType == 1 /* EXTERNAL_NOGUI */ || modelData.externalType == 2 /* EXTERNAL_QTFB */) {
                            win.appPid = library.launchExternal(modelData.id, qtfbKey);
                        }

                        return win;
                    }

                    onClicked: () => {
                        launchWindow().maximize();
                    }
                    onPressAndHold: () => {
                        const window = launchWindow();
                        if(modelData.disablesWindowedMode) {
                            window.maximize();
                        }
                    }
                }
            }
        }
    }
}
