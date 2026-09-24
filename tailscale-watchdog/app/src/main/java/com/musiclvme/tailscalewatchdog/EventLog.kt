package com.musiclvme.tailscalewatchdog

import android.content.Context
import android.content.SharedPreferences
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale
import java.util.concurrent.CopyOnWriteArrayList

object EventLog {
    private const val FILE = "watchdog_log"
    private const val KEY = "lines"
    private const val MAX = 80
    private val time = SimpleDateFormat("HH:mm:ss", Locale.CHINA)
    private val listeners = CopyOnWriteArrayList<() -> Unit>()
    private lateinit var prefs: SharedPreferences
    private val lock = Any()

    fun init(context: Context) {
        prefs = context.applicationContext.getSharedPreferences(FILE, Context.MODE_PRIVATE)
    }

    fun add(message: String) {
        val line = "${time.format(Date())}  $message"
        synchronized(lock) {
            val current = loadLocked().toMutableList()
            current.add(0, line)
            while (current.size > MAX) current.removeAt(current.lastIndex)
            prefs.edit().putString(KEY, current.joinToString("\n")).apply()
        }
        listeners.forEach { it() }
    }

    fun text(): String = synchronized(lock) { loadLocked().joinToString("\n") }

    fun addListener(listener: () -> Unit) {
        listeners.add(listener)
    }

    fun removeListener(listener: () -> Unit) {
        listeners.remove(listener)
    }

    private fun loadLocked(): List<String> {
        val raw = prefs.getString(KEY, "") ?: return emptyList()
        if (raw.isBlank()) return emptyList()
        return raw.split('\n')
    }
}
