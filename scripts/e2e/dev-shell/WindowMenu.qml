import QtQuick
import QtQuick.Layouts
import Quickshell
import Quickshell.Hyprland
import Quickshell.Wayland

// Window menu (right click on a titlebar, Alt+Space, or an app's own request): the compositor
// only sends `windowmenu>>ADDR,X,Y`, the shell draws the menu.
Scope {
    id: root

    property bool open: false
    property string address: ""
    property var ipc: null
    property point pos: Qt.point(0, 0)
    property var screen: null

    function dispatch(dsp) {
        Hyprland.dispatch(`hl.dsp.window.${dsp}`.replace("WINDOW", `window = 'address:0x${root.address}'`));
        root.open = false;
    }

    Connections {
        target: Hyprland

        function onRawEvent(event) {
            if (event.name !== "windowmenu")
                return;

            const [addr, x, y] = event.data.split(",");
            root.address = addr;
            root.pos = Qt.point(parseInt(x), parseInt(y));
            root.screen = Quickshell.screens.find(s => root.pos.x >= s.x && root.pos.x < s.x + s.width && root.pos.y >= s.y && root.pos.y < s.y + s.height) ?? Quickshell.screens[0];

            Hyprland.refreshToplevels();
            root.ipc = Hyprland.toplevels.values.find(t => t.address === addr)?.lastIpcObject ?? null;
            root.open = true;
        }
    }

    PanelWindow {
        id: menu
        visible: root.open
        screen: root.screen
        color: "transparent"
        exclusionMode: ExclusionMode.Ignore
        WlrLayershell.layer: WlrLayer.Overlay
        WlrLayershell.namespace: "hyprstack-window-menu"

        anchors {
            top: true
            left: true
        }
        margins {
            left: root.screen ? root.pos.x - root.screen.x : 0
            top: root.screen ? root.pos.y - root.screen.y : 0
        }
        implicitWidth: 220
        implicitHeight: column.implicitHeight + 8

        HyprlandFocusGrab {
            windows: [menu]
            active: root.open
            onCleared: root.open = false
        }

        Rectangle {
            anchors.fill: parent
            radius: 6
            color: "#f02a2a2a"
            border.color: "#505050"

            ColumnLayout {
                id: column
                anchors.fill: parent
                anchors.margins: 4
                spacing: 0

                Repeater {
                    model: [
                        { label: "Minimize", dsp: "minimize({ WINDOW })" },
                        { label: root.ipc && root.ipc.maximized === 3 ? "Restore" : "Maximize", dsp: "maximize({ WINDOW })" },
                        { label: root.ipc && root.ipc.stackLayer === 2 ? "✓ Keep above others" : "Keep above others", dsp: "keep_above({ WINDOW })" },
                        { label: "Move to left monitor", dsp: "move_to_monitor({ direction = 'left', WINDOW })" },
                        { label: "Move to right monitor", dsp: "move_to_monitor({ direction = 'right', WINDOW })" },
                        { label: "Close", dsp: "close({ WINDOW })" },
                    ]

                    delegate: Rectangle {
                        required property var modelData
                        Layout.fillWidth: true
                        Layout.preferredHeight: 28
                        radius: 4
                        color: area.containsMouse ? "#3d6fb5" : "transparent"

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.left: parent.left
                            anchors.leftMargin: 10
                            color: "white"
                            font.pixelSize: 13
                            text: parent.modelData.label
                        }

                        MouseArea {
                            id: area
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: root.dispatch(parent.modelData.dsp)
                        }
                    }
                }
            }
        }
    }
}
