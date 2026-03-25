Item {
    id: root
    
    enum Method {
        FastLocalised,
        FastBW,
        FastMotion,
        Interactivity,
        Quality
    }
    
    property int displayMethod: Interactive
    
    readonly property var displayMethodMapping: { "from": "to" }
    
    targetDisplayMethod: displayMethodMapping[displayMethod]
}
