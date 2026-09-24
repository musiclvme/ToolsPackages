package com.musiclvme.tailscalewatchdog

import android.accessibilityservice.AccessibilityService
import android.accessibilityservice.AccessibilityServiceInfo
import android.content.Context
import android.content.Intent
import android.provider.Settings
import android.view.accessibility.AccessibilityEvent
import android.view.accessibility.AccessibilityManager
import android.view.accessibility.AccessibilityNodeInfo
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.withContext

class WatchdogAccessibilityService : AccessibilityService() {
    override fun onServiceConnected() {
        super.onServiceConnected()
        instance = this
        serviceInfo = serviceInfo?.apply {
            flags = flags or
                AccessibilityServiceInfo.FLAG_RETRIEVE_INTERACTIVE_WINDOWS or
                AccessibilityServiceInfo.FLAG_REPORT_VIEW_IDS or
                AccessibilityServiceInfo.FLAG_INCLUDE_NOT_IMPORTANT_VIEWS
        }
        EventLog.add("无障碍服务已连接")
        StatusStore.update { it.copy(accessibilityOn = true) }
    }

    override fun onAccessibilityEvent(event: AccessibilityEvent?) = Unit

    override fun onInterrupt() = Unit

    override fun onDestroy() {
        if (instance === this) instance = null
        StatusStore.update { it.copy(accessibilityOn = false) }
        super.onDestroy()
    }

    suspend fun bounceWifi(): Boolean = withContext(Dispatchers.Main) {
        openWifiSettings()
        delay(1_400)
        val master = waitForWifiSwitch(6) ?: return@withContext false
        val wasOn = master.isChecked
        if (wasOn) {
            click(master)
            delay(2_400)
        }
        val offSwitch = waitForWifiSwitch(5)
        if (offSwitch != null && !offSwitch.isChecked) {
            click(offSwitch)
            delay(3_000)
        } else if (!wasOn) {
            waitForWifiSwitch(4)?.let { click(it) }
            delay(3_000)
        }
        performGlobalAction(GLOBAL_ACTION_BACK)
        true
    }

    suspend fun toggleTailscale(): Boolean = withContext(Dispatchers.Main) {
        val launch = packageManager.getLaunchIntentForPackage(TailscaleController.PACKAGE)
            ?: return@withContext false
        launch.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
        startActivity(launch)
        delay(1_800)
        val toggle = waitForAnySwitch(6) ?: return@withContext false
        val checked = toggle.isChecked
        click(toggle)
        delay(2_500)
        val again = waitForAnySwitch(5)
        if (again != null && again.isChecked == checked) {
            click(again)
            delay(2_000)
        } else if (again != null && !again.isChecked) {
            click(again)
            delay(2_000)
        }
        performGlobalAction(GLOBAL_ACTION_HOME)
        true
    }

    private fun openWifiSettings() {
        startActivity(Intent(Settings.ACTION_WIFI_SETTINGS).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK))
    }

    private suspend fun waitForWifiSwitch(attempts: Int): AccessibilityNodeInfo? {
        repeat(attempts) {
            findWifiSwitch()?.let { return it }
            delay(350)
        }
        return waitForAnySwitch(1)
    }

    private suspend fun waitForAnySwitch(attempts: Int): AccessibilityNodeInfo? {
        repeat(attempts) {
            val root = rootInActiveWindow
            if (root != null) {
                val switches = mutableListOf<AccessibilityNodeInfo>()
                collectCheckable(root, switches)
                switches.firstOrNull { it.isVisibleToUser }?.let { return it }
            }
            delay(300)
        }
        return null
    }

    private fun findWifiSwitch(): AccessibilityNodeInfo? {
        val root = rootInActiveWindow ?: return null
        val keywords = listOf("wi-fi", "wifi", "wlan", "无线")
        val matches = mutableListOf<AccessibilityNodeInfo>()
        findByText(root, keywords, matches)
        for (node in matches) {
            findCheckable(node)?.let { return it }
            node.parent?.let { parent -> findCheckable(parent)?.let { return it } }
        }
        val switches = mutableListOf<AccessibilityNodeInfo>()
        collectCheckable(root, switches)
        return switches.firstOrNull { it.isVisibleToUser }
    }

    private fun findByText(
        node: AccessibilityNodeInfo,
        keywords: List<String>,
        out: MutableList<AccessibilityNodeInfo>,
    ) {
        val text = ((node.text?.toString() ?: "") + " " + (node.contentDescription?.toString() ?: ""))
            .lowercase()
        if (keywords.any { text.contains(it) }) {
            out.add(node)
        }
        for (i in 0 until node.childCount) {
            val child = node.getChild(i) ?: continue
            findByText(child, keywords, out)
        }
    }

    private fun collectCheckable(node: AccessibilityNodeInfo, out: MutableList<AccessibilityNodeInfo>) {
        if (node.isCheckable) out.add(node)
        for (i in 0 until node.childCount) {
            val child = node.getChild(i) ?: continue
            collectCheckable(child, out)
        }
    }

    private fun findCheckable(node: AccessibilityNodeInfo): AccessibilityNodeInfo? {
        if (node.isCheckable) return node
        for (i in 0 until node.childCount) {
            val child = node.getChild(i) ?: continue
            findCheckable(child)?.let { return it }
        }
        return null
    }

    private fun click(node: AccessibilityNodeInfo): Boolean {
        var current: AccessibilityNodeInfo? = node
        while (current != null) {
            if (current.isClickable) {
                return current.performAction(AccessibilityNodeInfo.ACTION_CLICK)
            }
            current = current.parent
        }
        return node.performAction(AccessibilityNodeInfo.ACTION_CLICK)
    }

    companion object {
        @Volatile
        var instance: WatchdogAccessibilityService? = null
            private set

        fun isEnabled(context: Context): Boolean {
            val manager = context.getSystemService(Context.ACCESSIBILITY_SERVICE) as AccessibilityManager
            val enabled = manager.getEnabledAccessibilityServiceList(AccessibilityServiceInfo.FEEDBACK_ALL_MASK)
            return enabled.any { it.resolveInfo.serviceInfo.packageName == context.packageName }
        }
    }
}
