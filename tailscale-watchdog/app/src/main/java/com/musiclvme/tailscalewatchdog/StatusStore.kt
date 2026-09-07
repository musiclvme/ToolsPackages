package com.musiclvme.tailscalewatchdog

import java.util.concurrent.CopyOnWriteArrayList

data class UiStatus(
    val probe: ProbeResult = ProbeResult(),
    val monitoring: Boolean = false,
    val recovering: Boolean = false,
    val consecutiveFailures: Int = 0,
    val lastRecoveryText: String = "尚未恢复过",
    val accessibilityOn: Boolean = false,
    val tailscaleInstalled: Boolean = false,
)

object StatusStore {
    @Volatile
    var current: UiStatus = UiStatus()
        private set

    private val listeners = CopyOnWriteArrayList<(UiStatus) -> Unit>()

    fun update(block: (UiStatus) -> UiStatus) {
        val next = block(current)
        current = next
        listeners.forEach { it(next) }
    }

    fun addListener(listener: (UiStatus) -> Unit) {
        listeners.add(listener)
        listener(current)
    }

    fun removeListener(listener: (UiStatus) -> Unit) {
        listeners.remove(listener)
    }
}
