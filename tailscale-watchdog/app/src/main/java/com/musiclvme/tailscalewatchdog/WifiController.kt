package com.musiclvme.tailscalewatchdog

import android.content.Context
import android.net.wifi.WifiManager
import android.os.Build
import java.util.concurrent.TimeUnit

class WifiController(context: Context) {
    private val app = context.applicationContext
    private val wifi = app.applicationContext.getSystemService(Context.WIFI_SERVICE) as WifiManager

    suspend fun bounce(): String {
        if (tryRootBounce()) {
            return "已通过 root 开关 Wi-Fi 射频"
        }
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.Q) {
            if (legacyToggle()) return "已通过系统 API 开关 Wi-Fi"
        } else {
            @Suppress("DEPRECATION")
            if (wifi.setWifiEnabled(false)) {
                Thread.sleep(2_000)
                @Suppress("DEPRECATION")
                if (wifi.setWifiEnabled(true)) {
                    return "已通过系统 API 开关 Wi-Fi"
                }
            }
        }
        val a11y = WatchdogAccessibilityService.instance
        if (a11y != null) {
            val ok = a11y.bounceWifi()
            if (ok) return "已通过无障碍服务开关 Wi-Fi"
            return "无障碍服务未能点到 Wi-Fi 开关，请在系统设置里确认开关可见"
        }
        return "当前系统不允许普通应用直接关 Wi-Fi。请打开无障碍权限，或在有 root 的手机上重试"
    }

    private fun legacyToggle(): Boolean {
        @Suppress("DEPRECATION")
        val off = wifi.setWifiEnabled(false)
        Thread.sleep(2_000)
        @Suppress("DEPRECATION")
        val on = wifi.setWifiEnabled(true)
        return off && on
    }

    private fun tryRootBounce(): Boolean {
        if (!canSu()) return false
        val off = runSu("svc wifi disable")
        Thread.sleep(2_500)
        val on = runSu("svc wifi enable")
        return off && on
    }

    private fun canSu(): Boolean {
        return runSu("id")
    }

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
}
