import QtQuick
import QtQuick.Layouts
import Quickshell
import Quickshell.Hyprland
import Quickshell.Wayland

// Alt+Tab: windows in most-recently-used order, live thumbnails, Alt release activates.
Scope {
    id: root

    property bool open: false
    property int index: 0
    property var windows: []

    function mruWindows() {
        Hyprland.refreshToplevels();
        return Hyprland.toplevels.values.filter(t => t.wayland && t.lastIpcObject).sort((a, b) => a.lastIpcObject.focusHistoryID - b.lastIpcObject.focusHistoryID);
    }

    function step(delta) {
        if (!open) {
            windows = mruWindows();
            if (windows.length === 0)
                return;
            // like Windows: the first Alt+Tab goes to the previously used window
            index = windows.length > 1 ? 1 : 0;
            open = true;
            return;
        }

        index = (index + delta + windows.length) % windows.length;
    }

    function commit() {
        if (!open)
            return;
        const w = windows[index];
        open = false;
        if (w && w.wayland)
            w.wayland.activate();
    }

    GlobalShortcut {
        appid: "hyprstack"
        name: "alttab"
        onPressed: root.step(1)
    }

    GlobalShortcut {
        appid: "hyprstack"
        name: "alttabback"
        onPressed: root.step(-1)
    }

    PanelWindow {
        visible: root.open
        color: "transparent"
        exclusionMode: ExclusionMode.Ignore
        WlrLayershell.layer: WlrLayer.Overlay
        WlrLayershell.keyboardFocus: WlrKeyboardFocus.Exclusive
        WlrLayershell.namespace: "hyprstack-switcher"
        implicitWidth: row.implicitWidth + 32
        implicitHeight: 220

        Rectangle {
            anchors.fill: parent
            radius: 12
            color: "#e0202020"
            focus: true

            Keys.onReleased: event => {
                if (event.key === Qt.Key_Alt || event.key === Qt.Key_Meta)
                    root.commit();
            }
            Keys.onPressed: event => {
                if (event.key === Qt.Key_Escape)
                    root.open = false;
                else if (event.key === Qt.Key_Tab)
                    root.step(1);
                else if (event.key === Qt.Key_Backtab)
                    root.step(-1);
            }

            RowLayout {
                id: row
                anchors.centerIn: parent
                spacing: 12

                Repeater {
                    model: root.open ? root.windows : []

                    delegate: Rectangle {
                        id: item
                        required property var modelData
                        required property int index

                        Layout.preferredWidth: 240
                        Layout.preferredHeight: 180
                        radius: 8
                        color: index === root.index ? "#3d6fb5" : "transparent"

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 8

                            ScreencopyView {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                captureSource: item.modelData.wayland
                                live: true
                            }

                            Text {
                                Layout.fillWidth: true
                                color: "white"
                                elide: Text.ElideRight
                                horizontalAlignment: Text.AlignHCenter
                                text: item.modelData.title
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            onClicked: {
                                root.index = item.index;
                                root.commit();
                            }
                        }
                    }
                }
            }
        }
    }
}
