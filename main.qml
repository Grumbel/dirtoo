import QtQuick 2.7
import QtQuick.Window 2.2
import QtQuick.Controls 1.4

ScrollView {
    //width: 512
    //height: 512
    //anchors.fill: parent
    anchors.fill: parent
    //horizontalScrollBarPolicy: Qt.ScrollBarAlwaysOn
    //verticalScrollBarPolicy: Qt.ScrollBarAlwaysOn

    onWidthChanged: console.log("Size:", parent.width)

    GridView {
        id: mygrid
        //columns: 10
        // spacing: 10
        anchors.fill: parent
        model: menu2

        cellWidth: 128 + 16
        cellHeight: 128 + 16 + 16

        delegate: Rectangle {
            color: "grey"
            width: mygrid.cellWidth - 8
            height: mygrid.cellHeight - 8

            Image {
                source: "/home/ingo/.thumbnails/normal/ffdb804b9c4cd36556a921231182afcc.png"
                asynchronous: true
                anchors.verticalCenterOffset: -16
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.verticalCenter: parent.verticalCenter
            }

            Column {
                Text {
                    text: filename
                }

                Text {
                    text: mtime

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
