package com.musiclvme.tailscalewatchdog

import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class AccessibilitySettingTest {
    private val pkg = "com.musiclvme.tailscalewatchdog"

    @Test
    fun detectsFlattenedComponent() {
        val setting =
            "com.musiclvme.tailscalewatchdog/com.musiclvme.tailscalewatchdog.WatchdogAccessibilityService"
        assertTrue(AccessibilitySetting.isPackageEnabled(setting, pkg))
    }

    @Test
    fun detectsShortComponentAmongOthers() {
        val setting =
            "com.other/.Talkback:com.musiclvme.tailscalewatchdog/.WatchdogAccessibilityService"
        assertTrue(AccessibilitySetting.isPackageEnabled(setting, pkg))
    }

    @Test
    fun emptyOrUnrelatedIsDisabled() {
        assertFalse(AccessibilitySetting.isPackageEnabled(null, pkg))
        assertFalse(AccessibilitySetting.isPackageEnabled("", pkg))
        assertFalse(AccessibilitySetting.isPackageEnabled("com.google.android.marvin.talkback/.TalkBackService", pkg))
    }
}
