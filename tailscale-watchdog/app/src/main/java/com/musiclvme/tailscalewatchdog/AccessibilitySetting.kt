package com.musiclvme.tailscalewatchdog

object AccessibilitySetting {
    fun isPackageEnabled(setting: String?, packageName: String): Boolean {
        if (setting.isNullOrBlank() || packageName.isBlank()) return false
        return setting.split(':', ';').any { item ->
            item.trim().startsWith(packageName)
        }
    }
}
