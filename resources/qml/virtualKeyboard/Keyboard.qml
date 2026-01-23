/*
    Copyright 2011-2012 Heikki Holstila <heikki.holstila@gmail.com>

    This work is free software. you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This work is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this work.  If not, see <http://www.gnu.org/licenses/>.
*/

import QtQuick 2.0

Rectangle {
    id: keyboard

    property int keyModifiers
    property Key resetSticky
    property Key currentStickyPressed
    property string keyFgColor: "#000000"
    property string keyBgColor: "#ffffff"
    property string keyHilightBgColor: "#000000"
    property string keyBorderColor: "#000000"


    property int outmargins: 2
    property int keyspacing: 6
    property int keysPerRow: layout?.columns || 0
    property real keywidth: (keyboard.width - keyspacing*keysPerRow - outmargins*2)/keysPerRow;
    property int keyWidth: width / (layout.columns + 1)
    property int keyHeight: window.height / 14 < 55 ? window.height / 14 : 55
    property var config: null

    property var layout: null

    width: parent.width
    height: keyboard.outmargins + keyboard.keyspacing * (layout?.rows ?? 0 - 1) + keyboard.keyHeight * (layout?.rows ?? 0)

    Component {
        id: keyboardContents
        Column {
            id: col

            x: (keyboard.width-width)/2
            spacing: keyboard.keyspacing

            Repeater {
                id: rowRepeater

                model: layout?.rows || 0
                delegate: Row {
                    spacing: keyboard.keyspacing
                    Repeater {
                        id: colRepeater
                        property int rowIndex: index

                        model: layout.columnsInRow(index)
                        delegate: Key {
                            required property int index
                            property var keydata: layout.key(index, colRepeater.rowIndex)
                            vkb: keyboard

                            label: keydata.label
                            code: keydata.code
                            label_alt: keydata.altLabel
                            code_alt: keydata.altCode
                            width: keyboard.keywidth * keydata.width + ((keydata.width-1)*keyboard.keyspacing) + 1
                            sticky: keydata.isModifier && (keyboard.config?.enableStickyness ?? true)
                        }
                    }
                }
            }
        }
    }

    Loader {
        id: keyboardLoader
        anchors.fill: parent
    }

    function rebuildKeyboard(layoutData, config) {
        keyboard.config = config;
        keyboard.layout = layoutData;
        keyboardLoader.sourceComponent = undefined;
        keyboardLoader.sourceComponent = keyboardContents;
    }

    //borrowed from nemo-keyboard
    //Parameters: (x, y) in view coordinates
    function keyAt(x, y) {
        var item = keyboardLoader.item
        x -= keyboard.x
        y -= keyboard.y

        while ((item = item.childAt(x, y)) != null) {
            //return the first "Key" element we find
            if (typeof item.currentCode !== 'undefined') {
                return item
            }

            // Cheaper mapToItem, assuming we're not using anything fancy.
            x -= item.x
            y -= item.y
        }

        return null
    }

    MultiPointTouchArea {
        id: multiTouchArea
        anchors.fill: parent
        property var pressedKeys: ({})

        onPressed: (touchPoints) => {
            touchPoints.forEach(function (touchPoint) {
                var key = keyboard.keyAt(touchPoint.x, touchPoint.y)
                if (key != null) {
                    key.handlePress(multiTouchArea, touchPoint.x, touchPoint.y)
                }
                multiTouchArea.pressedKeys[touchPoint.pointId] = key
            })
        }
        onUpdated: (touchPoints) => {
            touchPoints.forEach(function (touchPoint) {
                var key = multiTouchArea.pressedKeys[touchPoint.pointId]
                if (key != null) {
                    if (!key.handleMove(multiTouchArea, touchPoint.x, touchPoint.y)) {
                        delete multiTouchArea.pressedKeys[touchPoint.pointId];
                    }
                }
            })
        }
        onReleased: (touchPoints) => {
            touchPoints.forEach(function (touchPoint) {
                var key = multiTouchArea.pressedKeys[touchPoint.pointId]
                if (key != null) {
                    key.handleRelease(multiTouchArea, touchPoint.x, touchPoint.y)
                }
                delete multiTouchArea.pressedKeys[touchPoint.pointId]
            })
        }
    }
}
