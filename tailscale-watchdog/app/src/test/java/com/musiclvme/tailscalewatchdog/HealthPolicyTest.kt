package com.musiclvme.tailscalewatchdog

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

class HealthPolicyTest {
    private val healthy = ProbeResult(
        wifiEnabled = true,
        wifiConnected = true,
        hasInternet = true,
        vpnUp = true,
        canaryOk = true,
    )

    @Test
    fun unhealthyWhenInternetIsDown() {
        val probe = healthy.copy(hasInternet = false)
        assertTrue(HealthPolicy.isUnhealthy(probe, WatchdogSettings()))
    }

    @Test
    fun unhealthyWhenVpnRequiredButDown() {
        val probe = healthy.copy(vpnUp = false)
        assertTrue(HealthPolicy.isUnhealthy(probe, WatchdogSettings(requireVpn = true)))
        assertFalse(HealthPolicy.isUnhealthy(probe, WatchdogSettings(requireVpn = false)))
    }

    @Test
    fun recoversAfterEnoughFailuresOutsideCooldown() {
        val reason = HealthPolicy.shouldRecover(
            probe = healthy.copy(hasInternet = false),
            settings = WatchdogSettings(failureThreshold = 3, cooldownMs = 60_000),
            consecutiveFailures = 3,
            nowMs = 100_000,
            lastRecoveryAtMs = 10_000,
            lastPreventiveAtMs = 0,
        )
        assertEquals(RecoverReason.UNHEALTHY, reason)
    }

    @Test
    fun respectsCooldown() {
        val reason = HealthPolicy.shouldRecover(
            probe = healthy.copy(hasInternet = false),
            settings = WatchdogSettings(failureThreshold = 1, cooldownMs = 60_000),
            consecutiveFailures = 5,
            nowMs = 50_000,
            lastRecoveryAtMs = 10_000,
            lastPreventiveAtMs = 0,
        )
        assertNull(reason)
    }

    @Test
    fun preventiveWhenIntervalElapsed() {
        val reason = HealthPolicy.shouldRecover(
            probe = healthy,
            settings = WatchdogSettings(preventiveIntervalMs = 30 * 60_000L, cooldownMs = 1_000),
            consecutiveFailures = 0,
            nowMs = 40 * 60_000L,
            lastRecoveryAtMs = 0,
            lastPreventiveAtMs = 5 * 60_000L,
        )
        assertEquals(RecoverReason.PREVENTIVE, reason)
    }
}
