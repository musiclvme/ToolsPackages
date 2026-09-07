package com.musiclvme.tailscalewatchdog

data class WatchdogSettings(
    val requireVpn: Boolean = true,
    val failureThreshold: Int = 3,
    val cooldownMs: Long = 180_000L,
    val preventiveIntervalMs: Long = 0L,
    val canary: String = "",
)

data class ProbeResult(
    val wifiEnabled: Boolean = false,
    val wifiConnected: Boolean = false,
    val hasInternet: Boolean = false,
    val vpnUp: Boolean = false,
    val canaryOk: Boolean? = null,
    val wifiSsid: String = "",
    val detail: String = "",
)

enum class RecoverReason {
    UNHEALTHY,
    PREVENTIVE,
    MANUAL,
}

object HealthPolicy {
    fun isUnhealthy(probe: ProbeResult, settings: WatchdogSettings): Boolean {
        if (!probe.hasInternet) return true
        if (settings.requireVpn && !probe.vpnUp) return true
        if (probe.canaryOk == false) return true
        return false
    }

    fun shouldRecover(
        probe: ProbeResult,
        settings: WatchdogSettings,
        consecutiveFailures: Int,
        nowMs: Long,
        lastRecoveryAtMs: Long,
        lastPreventiveAtMs: Long,
        forceManual: Boolean = false,
    ): RecoverReason? {
        if (forceManual) return RecoverReason.MANUAL
        if (nowMs - lastRecoveryAtMs < settings.cooldownMs) return null
        if (isUnhealthy(probe, settings) && consecutiveFailures >= settings.failureThreshold) {
            return RecoverReason.UNHEALTHY
        }
        if (settings.preventiveIntervalMs > 0 &&
            nowMs - lastPreventiveAtMs >= settings.preventiveIntervalMs
        ) {
            return RecoverReason.PREVENTIVE
        }
        return null
    }
}
