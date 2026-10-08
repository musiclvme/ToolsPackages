package com.musiclvme.tailscalewatchdog

import android.content.Context
import android.content.pm.PackageManager
import android.provider.Settings
import java.net.InetSocketAddress
import java.net.Socket
import java.util.concurrent.TimeUnit

data class WirelessDebugSnapshot(
    val wifiToggleOn: Boolean,
    val tcpReady: Boolean,
    val desiredPort: Int,
    val persistPort: String,
    val livePort: String,
    val canWriteSecureSettings: Boolean,
    val rooted: Boolean,
    val detail: String,
) {
    val enabled: Boolean get() = tcpReady
}

enum class WirelessDebugAction {
    ALREADY_ON,
    TURNED_ON,
    NEED_PERMISSION,
    FAILED,
}

/**
 * After reboot Android clears service.adb.tcp.port, so the chosen TCP port dies.
 * persist.adb.tcp.port survives reboot, but only root or `adb shell setprop` can write it.
 * adb_wifi_enabled only turns on TLS wireless debugging, not this TCP port.
 */
class WirelessDebugKeeper(context: Context) {
    private val app = context.applicationContext
    private var lastLoggedKey: String? = null
    @Volatile private var rootedCache: Boolean? = null

    private fun desiredPort(): Int = AppPrefs.adbTcpPort
    private fun desiredPortText(): String = desiredPort().toString()

    fun snapshot(): WirelessDebugSnapshot {
        val port = desiredPort()
        val wifiOn = readGlobal(KEY_ADB_WIFI) == 1
        val persist = persistPort()
        val live = livePort()
        val ready = isTcpReady()
        val canWrite = hasWriteSecureSettings()
        val rooted = canSu()
        val persistText = persist.ifBlank { "空" }
        val liveText = live.ifBlank { "空" }
        val detail = buildString {
            append(if (wifiOn) "无线调试开关：开" else "无线调试开关：关")
            append("；TCP $port：")
            append(if (ready) "已监听" else "未监听")
            append("（persist=$persistText, live=$liveText")
            if (ready && live.toIntOrNull() != port) append(", 本机已接通")
            append("）")
        }
        return WirelessDebugSnapshot(
            wifiToggleOn = wifiOn,
            tcpReady = ready,
            desiredPort = port,
            persistPort = persist,
            livePort = live,
            canWriteSecureSettings = canWrite,
            rooted = rooted,
            detail = detail,
        )
    }

    fun ensureEnabled(): WirelessDebugAction {
        val port = desiredPort()
        val portText = desiredPortText()
        writeGlobal(KEY_DEVELOPMENT, 1)
        writeGlobal(Settings.Global.ADB_ENABLED, 1)
        writeGlobal(KEY_ADB_WIFI, 1)

        if (isTcpReady()) {
            remember(WirelessDebugAction.ALREADY_ON, "ADB TCP $port 已在监听")
            return WirelessDebugAction.ALREADY_ON
        }

        if (enableTcpPort()) {
            remember(WirelessDebugAction.TURNED_ON, "已把 ADB TCP 端口固定为 $port")
            return WirelessDebugAction.TURNED_ON
        }

        val persist = persistPort()
        val action = when {
            persist == portText -> WirelessDebugAction.FAILED
            canSu() || hasWriteSecureSettings() -> WirelessDebugAction.FAILED
            else -> WirelessDebugAction.NEED_PERMISSION
        }
        val message = if (persist == portText) {
            "persist.adb.tcp.port 已是 $port，但当前进程还没听这个端口，等 adbd 重启或再开一次无线调试"
        } else {
            "重启后 $port 会丢。请用 USB 执行一次：${persistCommand(port)}"
        }
        remember(action, message)
        return action
    }

    fun isTcpReady(): Boolean {
        if (livePort().toIntOrNull() == desiredPort()) return true
        return isPortOpen(desiredPort())
    }

    private fun isPortOpen(port: Int): Boolean {
        return try {
            Socket().use { socket ->
                socket.tcpNoDelay = true
                socket.connect(InetSocketAddress("127.0.0.1", port), 400)
                socket.isConnected
            }
        } catch (_: Exception) {
            false
        }
    }

    fun hasWriteSecureSettings(): Boolean {
        return app.checkSelfPermission(WRITE_SECURE) == PackageManager.PERMISSION_GRANTED
    }

    private fun enableTcpPort(): Boolean {
        val portText = desiredPortText()
        if (canSu()) {
            runSu("settings put global $KEY_DEVELOPMENT 1")
            runSu("settings put global ${Settings.Global.ADB_ENABLED} 1")
            runSu("settings put global $KEY_ADB_WIFI 1")
            for (key in PERSIST_KEYS) {
                runSu("setprop $key $portText")
                runSu("resetprop $key $portText")
            }
            runSu("setprop service.adb.tcp.port $portText")
            runSu("setprop ctl.restart adbd")
            runSu("stop adbd; start adbd")
            Thread.sleep(1_500)
            if (isTcpReady() || persistPort() == portText) return persistPort() == portText
        }
        for (key in PERSIST_KEYS) {
            runPlain(listOf("setprop", key, portText))
        }
        runPlain(listOf("setprop", "service.adb.tcp.port", portText))
        Thread.sleep(400)
        return isTcpReady() || persistPort() == portText
    }

    private fun persistPort(): String {
        for (key in PERSIST_KEYS) {
            val value = readProp(key)
            if (value.isNotBlank()) return value
        }
        return ""
    }

    private fun livePort(): String = readProp("service.adb.tcp.port")

    private fun writeGlobal(key: String, value: Int): Boolean {
        return try {
            Settings.Global.putInt(app.contentResolver, key, value)
            true
        } catch (_: Exception) {
            false
        }
    }

    private fun readGlobal(key: String): Int {
        return try {
            Settings.Global.getInt(app.contentResolver, key, 0)
        } catch (_: Exception) {
            0
        }
    }

    private fun canSu(): Boolean {
        rootedCache?.let { return it }
        val result = runSu("id")
        rootedCache = result
        return result
    }

    private fun runSu(command: String): Boolean {
        return try {
            val process = ProcessBuilder("su", "-c", command)
                .redirectErrorStream(true)
                .start()
            val finished = process.waitFor(5, TimeUnit.SECONDS)
            if (!finished) {
                process.destroyForcibly()
                false
            } else {
                process.exitValue() == 0
            }
        } catch (_: Exception) {
            false
        }
    }

    private fun runPlain(command: List<String>): Boolean {
        return try {
            val process = ProcessBuilder(command)
                .redirectErrorStream(true)
                .start()
            val finished = process.waitFor(3, TimeUnit.SECONDS)
            if (!finished) {
                process.destroyForcibly()
                false
            } else {
                process.exitValue() == 0
            }
        } catch (_: Exception) {
            false
        }
    }

    private fun readProp(name: String): String {
        return try {
            val process = ProcessBuilder("getprop", name)
                .redirectErrorStream(true)
                .start()
            val finished = process.waitFor(3, TimeUnit.SECONDS)
            if (!finished) {
                process.destroyForcibly()
                return ""
            }
            process.inputStream.bufferedReader().readText().trim()
        } catch (_: Exception) {
            ""
        }
    }

    private fun remember(action: WirelessDebugAction, message: String) {
        val key = "$action:${desiredPort()}"
        if (key != lastLoggedKey) {
            EventLog.add(message)
            lastLoggedKey = key
        }
    }

    companion object {
        const val PACKAGE = "com.musiclvme.tailscalewatchdog"
        const val WRITE_SECURE = "android.permission.WRITE_SECURE_SETTINGS"
        const val KEY_ADB_WIFI = "adb_wifi_enabled"
        const val KEY_DEVELOPMENT = "development_settings_enabled"
        val PERSIST_KEYS = listOf(
            "persist.adb.tcp.port",
            "persist.sys.adb.tcp.port",
        )
        const val GRANT_COMMAND =
            "adb shell pm grant $PACKAGE $WRITE_SECURE"

        fun persistCommand(port: Int = AppPrefs.adbTcpPort): String {
            return "adb shell setprop persist.adb.tcp.port $port"
        }
    }
}
