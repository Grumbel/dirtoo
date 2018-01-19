// dirtool.py - diff tool for directories
// Copyright (C) 2018 Ingo Ruhnke <grumbel@gmail.com>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

import QtQuick 2.7
import QtQuick.Window 2.2
import QtQuick.Controls 1.4

ScrollView {
    anchors.fill: parent

    GridView {
        id: mygrid
        anchors.fill: parent
        model: menu2

        cellWidth: 128 + 16
        cellHeight: 128 + 16 + 16

        delegate: Rectangle {
            color: "grey"
            width: mygrid.cellWidth - 8
            height: mygrid.cellHeight - 8

            Image {
                source: mtime
                asynchronous: true
                anchors.verticalCenterOffset: -16
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.verticalCenter: parent.verticalCenter
            }

            Column {
                Text {
                    text: filename
                }

                anchors.bottom: parent.bottom
                anchors.horizontalCenter: parent.horizontalCenter
            }

            MouseArea {
                hoverEnabled: true
                anchors.fill: parent
                onClicked: { parent.color = 'blue' }
                onEntered: { parent.color = 'red'; console.log("Enter", this) }
                onExited: { parent.color = 'grey' }
            }
        }
    }
}

/* EOF */
