package com.musiclvme.tailscalewatchdog

import android.content.Context
import android.content.SharedPreferences
import android.os.UserManager

object DirectBoot {
    fun deviceContext(context: Context): Context {
        val app = context.applicationContext
        return if (app.isDeviceProtectedStorage) {
            app
        } else {
            app.createDeviceProtectedStorageContext()
        }
    }

    fun prefs(context: Context, name: String): SharedPreferences {
        val device = deviceContext(context)
        if (isUnlocked(context)) {
            try {
                val credential = if (context.applicationContext.isDeviceProtectedStorage) {
                    context.applicationContext.createCredentialProtectedStorageContext()
                } else {
                    context.applicationContext
                }
                device.moveSharedPreferencesFrom(credential, name)
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
