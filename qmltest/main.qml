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
import QtQuick.Controls 2.2
import QtQuick.Controls 1.4
import QtQuick.Layouts 1.3

ApplicationWindow {
    width: 1024
    height: 768

    title: "dt-fileview - Qml edition"

    menuBar: MenuBar {
        Menu {
            title: "File"
            MenuItem { text: "Parent Directory" }
            MenuItem { text: "Save As" }
            MenuSeparator {}
            MenuItem { text: "Exit" }
        }
        Menu {
            title: "Edit"
            MenuItem { text: "Undo" }
            MenuItem { text: "Redo" }
        }

        Menu {
            title: "View"
            MenuItem { text: "Icon View" }
            MenuItem { text: "Small Icon View" }
            MenuItem { text: "Detail View" }
        }

        Menu {
            title: "History"
            MenuItem { text: "Go Back" }
            MenuItem { text: "Go Forward" }
        }

        Menu {
            title: "Help"
            MenuItem { text: "About" }
        }
    }

    toolBar: ToolBar {
        ColumnLayout {
            RowLayout {
                ToolButton {}

                ToolButton {
                    iconSource: "/usr/share/icons/Humanity/actions/24/go-home.svg"
                }

                ToolSeparator {}


                ToolButton {
                    iconSource: "save-as.png"
                }
            }

            RowLayout {
                Label {
                    id: text_label
                    text: "Location:"
                }

                Rectangle {
                    anchors.left: text_label.right

                    id: text_rectangle
                    color: "white"
                    width: 400
                    height: 20
                }

                TextInput {
                    anchors.fill: text_rectangle
                    text: "Text"
                }
            }
        }
    }

    statusBar: StatusBar {
        RowLayout {
            anchors.fill: parent
            Label { text: "Statusbar text" }
        }
    }

    ScrollView {
        anchors.fill: parent

        function on_resize_event() {
            mygrid.anchors.leftMargin = (this.viewport.width - (mygrid.cellWidth * Math.floor(this.viewport.width / mygrid.cellWidth))) / 2;
        }

        onWidthChanged: on_resize_event();

        GridView {
            id: mygrid
            anchors.fill: parent
            model: menu2

            cellWidth: 128 + 32
            cellHeight: 128 + 32 + 16 * 2

            //anchors.leftMargin: 100

            delegate: Item {
                width: mygrid.cellWidth
                height: mygrid.cellHeight

                Rectangle {
                    id: fileItem
                    color: "#e0e0e0"
                    width: mygrid.cellWidth - 16
                    height: mygrid.cellHeight - 16

                    //anchors.horizontalCenter: parent.horizontalCenter
                    //anchors.verticalCenter: parent.verticalCenter
                    //anchors.leftMargin: 8
                    //anchors.rightMargin: 8

                    anchors.centerIn: parent

                    Image {
                        id: my_thumbnail
                        source: thumbnail
                        asynchronous: true
                        anchors.verticalCenterOffset: -16
                        anchors.centerIn: parent

                        onStatusChanged: {
                            console.log("imageStatus:", my_thumbnail.status)
                            if (my_thumbnail.status == Image.Error) {
                                //thumbnail = "/usr/share/icons/mate/48x48/status/gtk-dialog-error.png";
                                my_thumbnail.source = "/usr/share/icons/mate/48x48/status/gtk-dialog-error.png";
                            }
                        }
                    }

                    Column {
                        width: parent.width
                        Text {
                            text: filename
                            font.pixelSize: 11
                            elide: Text.ElideRight
                            width: parent.width
                            clip:true
                            anchors.horizontalCenter: parent.horizontalCenter
                            horizontalAlignment: Text.AlignHCenter
                        }

                        Text {
                            text: mtime
                            font.pixelSize: 10
                            anchors.horizontalCenter: parent.horizontalCenter
                        }

                        anchors.bottom: parent.bottom
                    }

                    MouseArea {
                        hoverEnabled: true
                        anchors.fill: parent
                        onClicked: { parent.color = 'blue' }
                        onEntered: { parent.color = '#c0c0c0'; }
                        onExited: { parent.color = '#e0e0e0' }
                    }
                }
            }
        }
    }
}

/* EOF */
