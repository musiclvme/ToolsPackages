package com.musiclvme.tailscalewatchdog

import android.app.Application
import android.app.NotificationChannel
import android.app.NotificationManager

class WatchdogApp : Application() {
    override fun onCreate() {
        super.onCreate()
        AppPrefs.init(this)
        EventLog.init(this)
        val manager = getSystemService(NotificationManager::class.java)
        manager.createNotificationChannel(
            NotificationChannel(
                WatchdogService.CHANNEL_ID,
                getString(R.string.notification_channel_name),
                NotificationManager.IMPORTANCE_LOW,
            ).apply {
                description = getString(R.string.notification_channel_desc)
                setShowBadge(false)
            },
        )
        manager.createNotificationChannel(
            NotificationChannel(
                TailscaleController.CHANNEL_ID,
                "Tailscale 拉起",
                NotificationManager.IMPORTANCE_HIGH,
            ).apply {
                description = "重启后无法后台打开 Tailscale 时提醒点击"
                setShowBadge(true)
            },
        )
    }
}
