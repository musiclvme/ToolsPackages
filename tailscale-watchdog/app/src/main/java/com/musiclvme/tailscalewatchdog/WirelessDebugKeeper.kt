package com.musiclvme.tailscalewatchdog

import android.content.Context
import android.content.pm.PackageManager
import android.os.Build
import android.provider.Settings
import java.util.concurrent.TimeUnit

data class WirelessDebugSnapshot(
    val enabled: Boolean,
    val canWriteSecureSettings: Boolean,
    val rooted: Boolean,
    val detail: String,
)

enum class WirelessDebugAction {
    ALREADY_ON,
    TURNED_ON,
    NEED_PERMISSION,
    FAILED,
}

/**
 * Re-enables Android 11+ wireless debugging after reboot.
 * A normal app cannot flip this switch unless the user has granted
 * WRITE_SECURE_SETTINGS once over USB, or the phone is rooted.
 */
class WirelessDebugKeeper(context: Context) {
    private val app = context.applicationContext
    private var lastLoggedAction: WirelessDebugAction? = null

    fun snapshot(): WirelessDebugSnapshot {
        val enabled = isWirelessDebugEnabled()
        val canWrite = hasWriteSecureSettings()
        val rooted = canSu()
        val detail = when {
            enabled -> "无线调试：已开启"
            canWrite || rooted -> "无线调试：已关闭，可自动打开"
            else -> "无线调试：已关闭，需要先授权 WRITE_SECURE_SETTINGS 或 root"
        }
        return WirelessDebugSnapshot(enabled, canWrite, rooted, detail)
    }

    fun ensureEnabled(): WirelessDebugAction {
        if (isWirelessDebugEnabled()) {
            remember(WirelessDebugAction.ALREADY_ON, "无线调试已开启")
            return WirelessDebugAction.ALREADY_ON
        }
        val wroteSettings = writeGlobal(KEY_DEVELOPMENT, 1) &&
            writeGlobal(Settings.Global.ADB_ENABLED, 1) &&
            writeGlobal(KEY_ADB_WIFI, 1)
        if (wroteSettings && isWirelessDebugEnabled()) {
            remember(WirelessDebugAction.TURNED_ON, "已用系统设置重新打开无线调试")
            return WirelessDebugAction.TURNED_ON
        }
        if (enableWithRoot()) {
            remember(WirelessDebugAction.TURNED_ON, "已用 root 重新打开无线调试")
            return WirelessDebugAction.TURNED_ON
        }
        val action = if (hasWriteSecureSettings() || canSu()) {
            WirelessDebugAction.FAILED
        } else {
            WirelessDebugAction.NEED_PERMISSION
        }
        val message = if (action == WirelessDebugAction.NEED_PERMISSION) {
            "无法打开无线调试。请用 USB 执行：adb shell pm grant $PACKAGE $WRITE_SECURE"
        } else {
            "尝试打开无线调试失败，请确认开发者选项和 Wi-Fi 已打开"
        }
        remember(action, message)
        return action
    }

    fun isWirelessDebugEnabled(): Boolean {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            if (readGlobal(KEY_ADB_WIFI) == 1) return true
        }
        return tcpPortEnabled()
    }

    fun hasWriteSecureSettings(): Boolean {
        return app.checkSelfPermission(WRITE_SECURE) == PackageManager.PERMISSION_GRANTED
    }

    private fun writeGlobal(key: String, value: Int): Boolean {
        return try {
            Settings.Global.putInt(app.contentResolver, key, value)
            true
        } catch (_: SecurityException) {
            false
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

    private fun tcpPortEnabled(): Boolean {
        val persist = readProp("persist.adb.tcp.port")
        val live = readProp("service.adb.tcp.port")
        return persist.toIntOrNull()?.let { it > 0 } == true ||
            live.toIntOrNull()?.let { it > 0 } == true
    }

    private fun enableWithRoot(): Boolean {
        if (!canSu()) return false
        runSu("settings put global $KEY_DEVELOPMENT 1")
        runSu("settings put global ${Settings.Global.ADB_ENABLED} 1")
        runSu("settings put global $KEY_ADB_WIFI 1")
        runSu("setprop persist.adb.tcp.port 5555")
        runSu("setprop service.adb.tcp.port 5555")
        runSu("stop adbd; start adbd")
        Thread.sleep(1_200)
        return isWirelessDebugEnabled() || tcpPortEnabled()
    }

    private fun canSu(): Boolean = runSu("id")

    private fun runSu(command: String): Boolean {
        return try {
            val process = ProcessBuilder("su", "-c", command)
                .redirectErrorStream(true)
                .start()
            val finished = process.waitFor(8, TimeUnit.SECONDS)
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
        if (action != lastLoggedAction) {
            EventLog.add(message)
            lastLoggedAction = action
        }
    }

    companion object {
        const val PACKAGE = "com.musiclvme.tailscalewatchdog"
        const val WRITE_SECURE = "android.permission.WRITE_SECURE_SETTINGS"
        const val KEY_ADB_WIFI = "adb_wifi_enabled"
        const val KEY_DEVELOPMENT = "development_settings_enabled"
        const val GRANT_COMMAND =
            "adb shell pm grant $PACKAGE $WRITE_SECURE"
    }
}
