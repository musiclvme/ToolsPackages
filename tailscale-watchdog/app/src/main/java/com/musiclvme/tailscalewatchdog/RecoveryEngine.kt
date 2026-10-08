package com.musiclvme.tailscalewatchdog

import kotlinx.coroutines.delay

class RecoveryEngine(context: android.content.Context) {
    private val app = context.applicationContext
    private val wifi = WifiController(app)
    private val tailscale = TailscaleController(app)
    private val probe = NetworkProbe(app)

    suspend fun recover(reason: RecoverReason) {
        val before = probe.probe(AppPrefs.canary)
        val bounceWifi = reason == RecoverReason.PREVENTIVE ||
            reason == RecoverReason.MANUAL ||
            HealthPolicy.needsWifiBounce(before)
        val doTailscale = reason == RecoverReason.PREVENTIVE ||
            reason == RecoverReason.MANUAL ||
            HealthPolicy.needsTailscale(before, AppPrefs.toSettings())

        val label = when (reason) {
            RecoverReason.UNHEALTHY -> "连接异常"
            RecoverReason.PREVENTIVE -> "定时保洁"
            RecoverReason.MANUAL -> "手动触发"
        }
        EventLog.add(
            if (bounceWifi) {
                "开始恢复（$label）：先重连 Wi-Fi，再处理 Tailscale"
            } else {
                "开始恢复（$label）：网络正常，只拉起 Tailscale，不拨 Wi-Fi"
            },
        )
        StatusStore.update { it.copy(recovering = true) }
        try {
            if (bounceWifi) {
                val wifiResult = wifi.bounce()
                EventLog.add(wifiResult)
                waitForInternet(40_000)
                delay(2_000)
            }
            if (doTailscale) {
                if (bounceWifi && before.vpnUp) {
                    tailscale.toggleOnce()
                } else {
                    tailscale.ensureVpnUp()
                }
            }
            delay(1_000)
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
