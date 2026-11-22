pragma Singleton
import QtQuick

// Every gap, margin and padding: a 4px grid, in six steps. Containers (dialogs, toasts) get the
// roomier steps; rows and controls the tighter ones.
QtObject {
    readonly property int xs: 4
    readonly property int sm: 8
    readonly property int md: 12
    readonly property int lg: 16
    readonly property int xl: 24
    readonly property int xxl: 32
}
