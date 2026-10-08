package com.musiclvme.tailscalewatchdog

import android.content.Context
import android.content.SharedPreferences
import android.os.UserManager

object DirectBoot {
    fun deviceContext(context: Context): Context {
        return context.applicationContext.createDeviceProtectedStorageContext()
    }

    fun prefs(context: Context, name: String): SharedPreferences {
        val app = context.applicationContext
        val device = deviceContext(app)
        if (isUnlocked(app)) {
            try {
                device.moveSharedPreferencesFrom(app, name)
            } catch (_: Exception) {
                // Already migrated, or credential storage is unavailable.
            }
        }
        return device.getSharedPreferences(name, Context.MODE_PRIVATE)
    }

    fun isUnlocked(context: Context): Boolean {
        return try {
            context.getSystemService(UserManager::class.java).isUserUnlocked
        } catch (_: Exception) {
            false
        }
    }
}
