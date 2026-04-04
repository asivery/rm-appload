import QtQuick

Item {
    id: root

    enum Method {
        UFast,
        Fast,
        Animate,
        Content,
        UI
    }

    property int displayMethod: Content

    onDisplayMethodChanged: () => {
        console.log("Would change display mode to: " + displayMethod);
    }
}
