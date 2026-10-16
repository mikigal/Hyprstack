import QtQuick
import QtQuick.Layouts
import Quickshell
import Quickshell.Wayland

PanelWindow {
    id: bar
    required property var modelData
    screen: modelData

    anchors {
        bottom: true
        left: true
        right: true
    }
    implicitHeight: 40
    color: "#e01c1c1c"

    RowLayout {
        anchors.fill: parent
        anchors.margins: 4
        spacing: 4

        Repeater {
            model: ToplevelManager.toplevels

            delegate: Rectangle {
                id: button
                required property var modelData

                Layout.preferredWidth: 180
                Layout.fillHeight: true
                radius: 4
                color: modelData.activated ? "#3d6fb5" : (mouse.containsMouse ? "#3a3a3a" : "#2a2a2a")
                opacity: modelData.minimized ? 0.5 : 1.0

                // where minimize animations should go
                Component.onCompleted: modelData.setRectangle(bar, Qt.rect(x, y, width, height))

                Text {
                    anchors.fill: parent
                    anchors.margins: 8
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                    color: "white"
                    font.pixelSize: 12
                    text: button.modelData.title || button.modelData.appId
                }

                MouseArea {
                    id: mouse
                    anchors.fill: parent
                    hoverEnabled: true
                    acceptedButtons: Qt.LeftButton | Qt.MiddleButton
                    onClicked: event => {
                        const t = button.modelData;
                        if (event.button === Qt.MiddleButton)
                            t.close();
                        else if (t.activated && !t.minimized)
                            t.minimized = true;
                        else
                            t.activate();
                    }
                }
            }
        }

        Item {
            Layout.fillWidth: true
        }

        Text {
            Layout.rightMargin: 8
            color: "white"
            font.pixelSize: 13
            text: Qt.formatDateTime(clock.date, "HH:mm")

            SystemClock {
                id: clock
                precision: SystemClock.Minutes
            }
        }
    }
}
