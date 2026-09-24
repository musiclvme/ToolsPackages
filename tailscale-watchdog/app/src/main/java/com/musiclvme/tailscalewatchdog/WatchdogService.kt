package com.musiclvme.tailscalewatchdog

import android.app.Notification
import android.app.PendingIntent
import android.app.Service
import android.content.Context
import android.content.Intent
import android.os.Build
import android.os.IBinder
import androidx.core.app.NotificationCompat
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.cancel
import kotlinx.coroutines.delay
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

class WatchdogService : Service() {
    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.Default)
    private var loopJob: Job? = null
    private lateinit var probe: NetworkProbe
    private lateinit var recovery: RecoveryEngine
    private lateinit var tailscale: TailscaleController
    private var consecutiveFailures = 0
    private var manualRequested = false

    override fun onCreate() {
        super.onCreate()
        probe = NetworkProbe(this)
        recovery = RecoveryEngine(this)
        tailscale = TailscaleController(this)
    }

    override fun onBind(intent: Intent?): IBinder? = null

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        when (intent?.action) {
            ACTION_STOP -> {
                AppPrefs.monitorEnabled = false
                stopForeground(STOP_FOREGROUND_REMOVE)
                stopSelf()
                return START_NOT_STICKY
            }
            ACTION_RECOVER_NOW -> manualRequested = true
        }
        startForeground(NOTIFICATION_ID, buildNotification("正在检测 Wi-Fi 和 Tailscale"))
        if (loopJob?.isActive != true) {
            loopJob = scope.launch { monitorLoop() }
        }
        return START_STICKY
    }

    override fun onDestroy() {
        loopJob?.cancel()
        scope.cancel()
        StatusStore.update { it.copy(monitoring = false, recovering = false) }
        super.onDestroy()
    }

    private suspend fun monitorLoop() {
        EventLog.add("监控服务已启动")
        StatusStore.update { it.copy(monitoring = true) }
        while (scope.isActive && AppPrefs.monitorEnabled) {
            val settings = AppPrefs.toSettings()
            val snap = probe.probe(settings.canary)
            if (HealthPolicy.isUnhealthy(snap, settings)) {
                consecutiveFailures += 1
            } else {
                consecutiveFailures = 0
            }
            val now = System.currentTimeMillis()
            val reason = HealthPolicy.shouldRecover(
                probe = snap,
                settings = settings,
                consecutiveFailures = consecutiveFailures,
                nowMs = now,
                lastRecoveryAtMs = AppPrefs.lastRecoveryAtMs,
                lastPreventiveAtMs = AppPrefs.lastPreventiveAtMs,
                forceManual = manualRequested,
            )
            manualRequested = false
            val lastText = if (AppPrefs.lastRecoveryAtMs == 0L) {
                "尚未恢复过"
            } else {
                "上次恢复 ${TIME.format(Date(AppPrefs.lastRecoveryAtMs))}"
            }
            StatusStore.update {
                it.copy(
                    probe = snap,
                    monitoring = true,
                    consecutiveFailures = consecutiveFailures,
                    lastRecoveryText = lastText,
                    accessibilityOn = WatchdogAccessibilityService.isEnabled(this@WatchdogService),
                    tailscaleInstalled = tailscale.isInstalled(),
                )
            }
            startForeground(NOTIFICATION_ID, buildNotification(snap.detail))
            if (reason != null && !StatusStore.current.recovering) {
                EventLog.add(
                    when (reason) {
                        RecoverReason.UNHEALTHY -> "连续失败 ${consecutiveFailures} 次，开始自动恢复"
                        RecoverReason.PREVENTIVE -> "到达定时保洁间隔，开始自动恢复"
                        RecoverReason.MANUAL -> "收到手动恢复请求"
                    },
                )
                recovery.recover(reason)
                consecutiveFailures = 0
            }
            delay(settings.let { AppPrefs.intervalSec * 1000L })
        }
        EventLog.add("监控服务已停止")
        stopForeground(STOP_FOREGROUND_REMOVE)
        stopSelf()
    }

    private fun buildNotification(text: String): Notification {
        val open = PendingIntent.getActivity(
            this,
            0,
            Intent(this, MainActivity::class.java),
            pendingFlags(),
        )
        val recover = PendingIntent.getBroadcast(
            this,
            1,
            Intent(this, NotificationActionReceiver::class.java).setAction(ACTION_RECOVER_NOW),
            pendingFlags(),
        )
        val stop = PendingIntent.getBroadcast(
            this,
            2,
            Intent(this, NotificationActionReceiver::class.java).setAction(ACTION_STOP),
            pendingFlags(),
        )
        return NotificationCompat.Builder(this, CHANNEL_ID)
            .setSmallIcon(R.drawable.ic_notification)
            .setContentTitle(getString(R.string.notification_title))
            .setContentText(text)
            .setOngoing(true)
            .setOnlyAlertOnce(true)
            .setContentIntent(open)
            .addAction(0, getString(R.string.action_recover_now), recover)
            .addAction(0, getString(R.string.action_stop), stop)
            .build()
    }

    private fun pendingFlags(): Int {
        return PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE
    }

    companion object {
        const val CHANNEL_ID = "watchdog_monitor"
        const val NOTIFICATION_ID = 42
        const val ACTION_STOP = "com.musiclvme.tailscalewatchdog.STOP"
        const val ACTION_RECOVER_NOW = "com.musiclvme.tailscalewatchdog.RECOVER_NOW"
        private val TIME = SimpleDateFormat("MM-dd HH:mm:ss", Locale.CHINA)

        fun start(context: Context) {
            val intent = Intent(context, WatchdogService::class.java)
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                context.startForegroundService(intent)
            } else {
                context.startService(intent)
            }
        }

        fun stop(context: Context) {
            val intent = Intent(context, WatchdogService::class.java).setAction(ACTION_STOP)
            context.startService(intent)
        }

        fun recoverNow(context: Context) {
            val intent = Intent(context, WatchdogService::class.java).setAction(ACTION_RECOVER_NOW)
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                context.startForegroundService(intent)
            } else {
                context.startService(intent)
            }
        }
    }
}
