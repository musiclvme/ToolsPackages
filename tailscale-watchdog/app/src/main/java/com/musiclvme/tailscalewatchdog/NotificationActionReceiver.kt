package com.musiclvme.tailscalewatchdog

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent

class NotificationActionReceiver : BroadcastReceiver() {
    override fun onReceive(context: Context, intent: Intent?) {
        when (intent?.action) {
            WatchdogService.ACTION_STOP -> WatchdogService.stop(context)
            WatchdogService.ACTION_RECOVER_NOW -> WatchdogService.recoverNow(context)
        }
    }
}
