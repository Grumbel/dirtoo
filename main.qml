import QtQuick 2.7
import QtQuick.Window 2.2
import QtQuick.Controls 1.4

ScrollView {
    //width: 512
    //height: 512
    //anchors.fill: parent
    anchors { // to have a real size, items should grow horizontally in their parent
        left: parent.left;
        right: parent.right;
    }

    horizontalScrollBarPolicy: Qt.ScrollBarAlwaysOn
    verticalScrollBarPolicy: Qt.ScrollBarAlwaysOn

    onWidthChanged: console.log("Size:", parent.width)


    Item {
        width: 10024
        height: 50024

        Grid {
            id: mygrid
            columns: 10
            spacing: 10

            //model: ContactModel {}
            Repeater {
                model: 6000

                Rectangle {
                    color: "grey"
                    x: 0
                    y: 0
                    width: 128
                    height: 128 + 16

                    Image {
                        source: "/home/ingo/.thumbnails/normal/ffdb804b9c4cd36556a921231182afcc.png"
                        asynchronous: true
                        anchors.verticalCenterOffset: -16
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    Column {
                        Text {
                            text: "2222222"
                        }

                        Text {
                            text: "XXXXXXX"

                        }
                        anchors.bottom: parent.bottom
                        anchors.horizontalCenter: parent.horizontalCenter
                    }

                    MouseArea {
                        hoverEnabled: true
                        anchors.fill: parent
                        onClicked: { parent.color = 'blue' }
                        onEntered: { parent.color = 'red' }
                        onExited: { parent.color = 'grey' }
                    }
                }
            }
        }
    }
}
