package com.musiclvme.tailscalewatchdog

import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class AdbAuthPolicyTest {
    @Test
    fun doesNotRestartAdbdWhileLocked() {
        assertFalse(AdbAuthPolicy.shouldRestartAdbd(unlocked = false))
    }

    @Test
    fun restartsAdbdAfterUnlockSoKeysLoad() {
        assertTrue(AdbAuthPolicy.shouldRestartAdbd(unlocked = true))
    }
}
