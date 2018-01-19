import QtQuick 2.7
import QtQuick.Window 2.2
import QtQuick.Controls 1.4

ScrollView {
    //width: 512
    //height: 512
    anchors.fill: parent

    horizontalScrollBarPolicy: Qt.ScrollBarAlwaysOn
    verticalScrollBarPolicy: Qt.ScrollBarAlwaysOn

    Item {
        width: 10024
        height: 50024

        Column {
            spacing: 10
            Repeater {
                model: 600
                Row {
                    spacing: 10
                    Repeater {
                        model: 10
                        Rectangle {
                            color: "grey"
                            x: 0
                            y: 0
                            width: 128
                            height: 128 + 16

                            Image {
                                source: "/home/ingo/.thumbnails/normal/ffdb804b9c4cd36556a921231182afcc.png"
                                asynchronous: true
                            }

                            Text {
                                text: "Hello world!"
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
    }
}
