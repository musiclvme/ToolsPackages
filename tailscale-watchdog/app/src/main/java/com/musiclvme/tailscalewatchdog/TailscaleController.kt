package com.musiclvme.tailscalewatchdog

import android.content.ComponentName
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import kotlinx.coroutines.delay

class TailscaleController(context: Context) {
    private val app = context.applicationContext

    fun isInstalled(): Boolean {
        return try {
            app.packageManager.getPackageInfo(PACKAGE, 0)
            true
        } catch (_: PackageManager.NameNotFoundException) {
            false
        }
    }

    suspend fun toggleOnce() {
        if (!isInstalled()) {
            EventLog.add("未安装 Tailscale（com.tailscale.ipn）")
            return
        }
        kickApp()
        delay(1_500)
        send(DISCONNECT)
        delay(3_000)
        send(CONNECT)
        delay(2_000)
        send(CONNECT)
        val vpnUp = waitForVpn(12_000)
        if (vpnUp) {
            EventLog.add("Tailscale VPN 已重新拉起")
            return
        }
        val a11y = WatchdogAccessibilityService.instance
        if (a11y != null) {
            EventLog.add("广播未拉起 VPN，改用无障碍点击 Tailscale 开关")
            a11y.toggleTailscale()
        } else {
            EventLog.add("Tailscale 开关广播已发送，但尚未检测到 VPN。请给 Tailscale 关闭电池优化")
        }
    }

    private fun kickApp() {
        val launch = app.packageManager.getLaunchIntentForPackage(PACKAGE)
        if (launch != null) {
            launch.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
            try {
                app.startActivity(launch)
            } catch (e: Exception) {
                EventLog.add("无法打开 Tailscale：${e.message}")
            }
        }
    }

    private fun send(action: String) {
        val intent = Intent(action).apply {
            component = ComponentName(PACKAGE, RECEIVER)
            `package` = PACKAGE
        }
        try {
            app.sendBroadcast(intent)
            EventLog.add("已发送 $action")
        } catch (e: Exception) {
            EventLog.add("发送 $action 失败：${e.message}")
        }
    }

    private suspend fun waitForVpn(timeoutMs: Long): Boolean {
        val probe = NetworkProbe(app)
        val deadline = System.currentTimeMillis() + timeoutMs
        while (System.currentTimeMillis() < deadline) {
            if (probe.probe("").vpnUp) return true
            delay(1_000)
        }
        return false
    }

    companion object {
        const val PACKAGE = "com.tailscale.ipn"
        const val RECEIVER = "com.tailscale.ipn.IPNReceiver"
        const val CONNECT = "com.tailscale.ipn.CONNECT_VPN"
        const val DISCONNECT = "com.tailscale.ipn.DISCONNECT_VPN"
    }
}
