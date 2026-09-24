package com.musiclvme.tailscalewatchdog

import android.content.Context
import kotlinx.coroutines.delay

class RecoveryEngine(context: Context) {
    private val app = context.applicationContext
    private val wifi = WifiController(app)
    private val tailscale = TailscaleController(app)
    private val probe = NetworkProbe(app)

    suspend fun recover(reason: RecoverReason) {
        val label = when (reason) {
            RecoverReason.UNHEALTHY -> "连接异常"
            RecoverReason.PREVENTIVE -> "定时保洁"
            RecoverReason.MANUAL -> "手动触发"
        }
        EventLog.add("开始恢复（$label）：先重连 Wi-Fi，再开关 Tailscale")
        StatusStore.update { it.copy(recovering = true) }
        try {
            val wifiResult = wifi.bounce()
            EventLog.add(wifiResult)
            waitForInternet(40_000)
            delay(2_000)
            tailscale.toggleOnce()
            delay(2_000)
            val after = probe.probe(AppPrefs.canary)
            EventLog.add("恢复结束：${after.detail}")
            AppPrefs.lastRecoveryAtMs = System.currentTimeMillis()
            if (reason == RecoverReason.PREVENTIVE) {
                AppPrefs.lastPreventiveAtMs = System.currentTimeMillis()
            }
        } catch (e: Exception) {
            EventLog.add("恢复失败：${e.message}")
        } finally {
            StatusStore.update { it.copy(recovering = false) }
        }
    }

    private suspend fun waitForInternet(timeoutMs: Long) {
        val deadline = System.currentTimeMillis() + timeoutMs
        while (System.currentTimeMillis() < deadline) {
            val snap = probe.probe(AppPrefs.canary)
            if (snap.wifiConnected && snap.hasInternet) {
                EventLog.add("Wi-Fi 已重新获得网络")
                return
            }
            delay(1_500)
        }
        EventLog.add("等待 Wi-Fi 恢复超时，仍继续尝试开关 Tailscale")
    }
}
