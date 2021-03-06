import QtQuick 2.0
import AsemanTools 1.1

AsemanObject {
    signal crashed()
    signal safe()

    property string logLockFile: AsemanApp.homePath + "/app.lock"

    function stopMonitoring() {
        unlock()
    }

    function startMonitoring() {
        unlock()
        Tools.writeText(logLockFile, "lock")
    }

    function unlock() {
        Tools.deleteFile(logLockFile)
    }

    function checkCrash() {
        var res = Tools.fileExists(logLockFile)
        if(res)
            crashed()
        else
            safe()
    }

    Connections {
        target: Devices
        onActivityPaused: stopMonitoring()
        onActivityResumed: startMonitoring()
    }

    Component.onDestruction: stopMonitoring()
    Component.onCompleted: {
        checkCrash()
        unlock()
        startMonitoring()
    }
}
