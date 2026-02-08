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
    id: key
    required property var vkb

    property string label
    property string label_alt
    property int code
    property int code_alt
    property int currentCode: (shiftActive && label_alt != '') ? code_alt : code
    property string currentLabel: (shiftActive && label_alt != '') ? label_alt : label
    property bool sticky        // can key be stickied?
    property bool becomesSticky // will this become sticky after release?
    property int stickiness     // current stickiness status
    property real labelOpacity: 1.0

    // mouse input handling
    property int clickThreshold: 20
    property bool isClick
    property int pressMouseY
    property int pressMouseX
    property bool shiftActive: (vkb.keyModifiers & 0x100000) && !sticky

    width: vkb.keyWidth   // some default
    height: vkb.keyHeight
    color: label=="" ? "transparent" : vkb.keyBgColor
    border.color: label=="" ? "transparent" : vkb.keyBorderColor
    border.width: 1
    radius: 2

    Image {
        id: keyImage
        anchors.centerIn: parent
        opacity: key.labelOpacity
        source: { if(key.label.length>1 && key.label.charAt(0)==':') return "qrc:/appload/keyboard/"+key.label.substring(1)+".png"; else return ""; }
    }

    Column {
        visible: keyImage.source == ""
        anchors.centerIn: parent
        spacing: -17

        Text {
            id: keyAltLabel
            property bool highlighted: key.shiftActive

            anchors.horizontalCenter: parent.horizontalCenter

            text: key.label_alt
            color: vkb.keyFgColor

            opacity: key.labelOpacity * (highlighted ? 1.0 : 0.2)
            Behavior on opacity { NumberAnimation { duration: 100 } }

            font.pointSize: 24 * (text.length > 1 ? 0.5 : 1.0)
            Behavior on font.pointSize { NumberAnimation { duration: 100 } }
        }

        Text {
            id: keyLabel
            property bool highlighted: key.label_alt == '' || !key.shiftActive

            anchors.horizontalCenter: parent.horizontalCenter

            text: {
                if (key.label.length == 1 && key.label_alt == '') {
                    if (key.shiftActive) {
                        return key.label.toUpperCase();
                    } else {
                        return key.label.toLowerCase();
                    }
                }

                return key.label;
            }

            color: vkb.keyFgColor

            opacity: key.labelOpacity * (highlighted ? 1.0 : 0.2)
            Behavior on opacity { NumberAnimation { duration: 100 } }

            font.pointSize: 24 * (text.length > 1 ? 0.5 : 1.0)
            Behavior on font.pointSize { NumberAnimation { duration: 100 } }
        }
    }

    Rectangle {
        id: stickIndicator
        visible: sticky && stickiness>0
        color: vkb.keyHilightBgColor
        anchors.fill: parent
        radius: key.radius
        opacity: 0.5
        anchors.topMargin: key.height/2
    }

    function handlePress(touchArea, x, y) {
        console.log("Press " + label)
        isClick = true;
        pressMouseX = x;
        pressMouseY = y;

        key.color = vkb.keyHilightBgColor

        vkb.config.keyDown(currentCode | vkb.keyModifiers);

        if (sticky) {
            vkb.keyModifiers |= code;
            key.becomesSticky = true;
            vkb.currentStickyPressed = key;
        } else {
            if (vkb.currentStickyPressed != null) {
                // Pressing a non-sticky key while a sticky key is pressed:
                // the sticky key will not become sticky when released
                vkb.currentStickyPressed.becomesSticky = false;
            }
        }
    }

    function handleMove(touchArea, x, y) {
        var mappedPoint = key.mapFromItem(touchArea, x, y)
        if (!key.contains(Qt.point(mappedPoint.x, mappedPoint.y))) {
            key.handleRelease(touchArea, x, y);
            return false;
        }

        if (key.isClick) {
            if (Math.abs(x - key.pressMouseX) > key.clickThreshold
                    || Math.abs(y - key.pressMouseY) > key.clickThreshold) {
                key.isClick = false
            }
        }

        return true;
    }

    function handleRelease(touchArea, x, y) {
        key.color = vkb.keyBgColor

        if (sticky && !becomesSticky) {
            vkb.keyModifiers &= ~code
            vkb.currentStickyPressed = null
        }
        if(!sticky) {
            vkb.config.keyUp(currentCode | vkb.keyModifiers);
        }

        if (vkb.keyAt(x, y) == key) {
            if (key.sticky && key.becomesSticky) {
                setStickiness(-1)
            }

            // first non-sticky press will cause the sticky to be released
            if (!sticky && vkb.resetSticky && vkb.resetSticky !== key) {
                resetSticky.setStickiness(0)
            }
        }
    }

    function setStickiness(val)
    {
        if(sticky) {
            if( vkb.resetSticky && vkb.resetSticky !== key ) {
                resetSticky.setStickiness(0)
            }

            if(val===-1)
                stickiness = (stickiness+1) % 3
            else
                stickiness = val

            // stickiness == 0 -> not pressed
            // stickiness == 1 -> release after next keypress
            // stickiness == 2 -> keep pressed

            if(stickiness>0) {
                vkb.keyModifiers |= code
            } else {
                vkb.config.keyUp(currentCode);
                vkb.keyModifiers &= ~code
            }

            vkb.resetSticky = null

            if(stickiness==1) {
                stickIndicator.anchors.topMargin = key.height/2
                vkb.resetSticky = key
            } else if(stickiness==2) {
                stickIndicator.anchors.topMargin = 0
            }
        }
    }
}
