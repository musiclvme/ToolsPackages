package com.musiclvme.tailscalewatchdog

import android.app.Notification
import android.app.PendingIntent
import android.content.ComponentName
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import androidx.core.app.NotificationCompat
import androidx.core.app.NotificationManagerCompat
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
            EventLog.add("未安装 Tailscale（$PACKAGE）")
            return
        }
        kickApp()
        delay(1_500)
        send(DISCONNECT)
        delay(3_000)
        connectVpn()
    }

    suspend fun ensureVpnUp() {
        if (!isInstalled()) {
            EventLog.add("未安装 Tailscale（$PACKAGE）")
            return
        }
        if (NetworkProbe(app).probe("").vpnUp) {
            cancelStartNotification()
            return
        }
        EventLog.add("Tailscale VPN 未连接，尝试拉起应用并发送 CONNECT")
        val launched = kickApp()
        if (!launched) {
            EventLog.add("后台无法直接打开 Tailscale 界面，已发出通知，请点通知或手动打开一次")
            notifyTapToStart()
        }
        delay(2_000)
        connectVpn()
    }

    private suspend fun connectVpn() {
        send(CONNECT)
        delay(2_000)
        send(CONNECT)
        if (waitForVpn(12_000)) {
            EventLog.add("Tailscale VPN 已重新拉起")
            cancelStartNotification()
            return
        }
        val a11y = WatchdogAccessibilityService.instance
        if (a11y != null) {
            EventLog.add("广播未拉起 VPN，改用无障碍打开 Tailscale")
            a11y.launchTailscale()
            delay(1_500)
            send(CONNECT)
            a11y.toggleTailscaleSwitch()
            if (waitForVpn(10_000)) {
                EventLog.add("Tailscale VPN 已由无障碍拉起")
                cancelStartNotification()
                return
            }
        }
        notifyTapToStart()
        EventLog.add("仍未检测到 VPN。请给 Tailscale 关闭电池优化，并在系统 VPN 里打开始终开启")
    }

    private suspend fun kickApp(): Boolean {
        val a11y = WatchdogAccessibilityService.instance
        if (a11y != null && a11y.launchTailscale()) {
            EventLog.add("已通过无障碍打开 Tailscale")
            return true
        }
        if (startLaunchIntent()) return true
        if (startKnownActivities()) return true
        if (startWithRoot()) return true
        return false
    }

    private fun startLaunchIntent(): Boolean {
        val launch = app.packageManager.getLaunchIntentForPackage(PACKAGE) ?: return false
        return startExported(launch)
    }

    private fun startKnownActivities(): Boolean {
        for (name in ACTIVITIES) {
            val intent = Intent().setComponent(ComponentName(PACKAGE, name))
            intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
            if (startExported(intent)) return true
        }
        return false
    }

    private fun startExported(intent: Intent): Boolean {
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_RESET_TASK_IF_NEEDED)
        return try {
            app.startActivity(intent)
            EventLog.add("已请求打开 Tailscale")
            true
        } catch (e: Exception) {
            EventLog.add("无法打开 Tailscale：${e.message}")
            false
        }
    }

    private fun startWithRoot(): Boolean {
        val launched = runSu("am start -a android.intent.action.MAIN -p $PACKAGE")
        if (launched) EventLog.add("已用 root 打开 Tailscale")
        return launched
    }

    private fun runSu(command: String): Boolean {
        return try {
            val process = ProcessBuilder("su", "-c", command)
                .redirectErrorStream(true)
                .start()
            val finished = process.waitFor(5, java.util.concurrent.TimeUnit.SECONDS)
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

    private fun notifyTapToStart() {
        val launch = app.packageManager.getLaunchIntentForPackage(PACKAGE) ?: return
        launch.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
        val pending = PendingIntent.getActivity(
            app,
            11,
            launch,
            PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE,
        )
        val notification: Notification = NotificationCompat.Builder(app, CHANNEL_ID)
            .setSmallIcon(R.drawable.ic_notification)
            .setContentTitle("需要打开 Tailscale")
            .setContentText("重启后系统不允许后台直接拉起 VPN，请点这里打开 Tailscale")
            .setPriority(NotificationCompat.PRIORITY_HIGH)
            .setCategory(NotificationCompat.CATEGORY_RECOMMENDATION)
            .setAutoCancel(true)
            .setContentIntent(pending)
            .setFullScreenIntent(pending, true)
            .build()
        try {
            NotificationManagerCompat.from(app).notify(NOTIFICATION_ID, notification)
        } catch (_: SecurityException) {
            EventLog.add("无法弹出 Tailscale 通知，请允许通知权限")
        }
    }

    fun cancelStartNotification() {
        NotificationManagerCompat.from(app).cancel(NOTIFICATION_ID)
    }

    companion object {
        const val PACKAGE = "com.tailscale.ipn"
        const val RECEIVER = "com.tailscale.ipn.IPNReceiver"
        const val CONNECT = "com.tailscale.ipn.CONNECT_VPN"
        const val DISCONNECT = "com.tailscale.ipn.DISCONNECT_VPN"
        const val CHANNEL_ID = "watchdog_tailscale"
        const val NOTIFICATION_ID = 43
        private val ACTIVITIES = listOf(
            "com.tailscale.ipn.MainActivity",
            "com.tailscale.ipn.IPNActivity",
            "com.tailscale.ipn.ui.MainActivity",
        )
    }
}
