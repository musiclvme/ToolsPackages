package com.musiclvme.tailscalewatchdog

import android.content.Context
import android.content.SharedPreferences

object AppPrefs {
    private const val FILE = "watchdog_prefs"
    private const val KEY_MONITOR = "monitor_enabled"
    private const val KEY_REQUIRE_VPN = "require_vpn"
    private const val KEY_INTERVAL_SEC = "interval_sec"
    private const val KEY_FAILURES = "failure_threshold"
    private const val KEY_COOLDOWN_SEC = "cooldown_sec"
    private const val KEY_CANARY = "canary"
    private const val KEY_PREVENTIVE_MIN = "preventive_min"
    private const val KEY_LAST_RECOVERY = "last_recovery_ms"
    private const val KEY_LAST_PREVENTIVE = "last_preventive_ms"

    private lateinit var prefs: SharedPreferences

    fun init(context: Context) {
        prefs = context.applicationContext.getSharedPreferences(FILE, Context.MODE_PRIVATE)
    }

    var monitorEnabled: Boolean
        get() = prefs.getBoolean(KEY_MONITOR, false)
        set(value) { prefs.edit().putBoolean(KEY_MONITOR, value).apply() }

    var requireVpn: Boolean
        get() = prefs.getBoolean(KEY_REQUIRE_VPN, true)
        set(value) { prefs.edit().putBoolean(KEY_REQUIRE_VPN, value).apply() }

    var intervalSec: Int
        get() = prefs.getInt(KEY_INTERVAL_SEC, 15).coerceIn(5, 300)
        set(value) { prefs.edit().putInt(KEY_INTERVAL_SEC, value.coerceIn(5, 300)).apply() }

    var failureThreshold: Int
        get() = prefs.getInt(KEY_FAILURES, 3).coerceIn(1, 20)
        set(value) { prefs.edit().putInt(KEY_FAILURES, value.coerceIn(1, 20)).apply() }

    var cooldownSec: Int
        get() = prefs.getInt(KEY_COOLDOWN_SEC, 180).coerceIn(30, 3600)
        set(value) { prefs.edit().putInt(KEY_COOLDOWN_SEC, value.coerceIn(30, 3600)).apply() }

    var canary: String
        get() = prefs.getString(KEY_CANARY, "") ?: ""
        set(value) { prefs.edit().putString(KEY_CANARY, value.trim()).apply() }

    var preventiveMin: Int
        get() = prefs.getInt(KEY_PREVENTIVE_MIN, 0).coerceIn(0, 24 * 60)
        set(value) { prefs.edit().putInt(KEY_PREVENTIVE_MIN, value.coerceIn(0, 24 * 60)).apply() }

    var lastRecoveryAtMs: Long
        get() = prefs.getLong(KEY_LAST_RECOVERY, 0L)
        set(value) { prefs.edit().putLong(KEY_LAST_RECOVERY, value).apply() }

    var lastPreventiveAtMs: Long
        get() = prefs.getLong(KEY_LAST_PREVENTIVE, 0L)
        set(value) { prefs.edit().putLong(KEY_LAST_PREVENTIVE, value).apply() }

    fun toSettings(): WatchdogSettings = WatchdogSettings(
        requireVpn = requireVpn,
        failureThreshold = failureThreshold,
        cooldownMs = cooldownSec * 1000L,
        preventiveIntervalMs = if (preventiveMin <= 0) 0L else preventiveMin * 60_000L,
        canary = canary,
    )
}
