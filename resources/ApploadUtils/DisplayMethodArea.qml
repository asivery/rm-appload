import QtQuick

Item {
    id: root
    
    enum Method {
        FastLocalised,
        FastBW,
        FastMotion,
        Interactivity,
        Quality
    }
    
    property int displayMethod: Interactivity
    
    readonly property var displayMethodMapping: { "from": "to" }
}
