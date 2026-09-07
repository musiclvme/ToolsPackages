package com.musiclvme.tailscalewatchdog

import android.Manifest
import android.content.Intent
import android.content.pm.PackageManager
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.os.PowerManager
import android.provider.Settings
import android.widget.Toast
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.ContextCompat
import com.musiclvme.tailscalewatchdog.databinding.ActivityMainBinding

class MainActivity : AppCompatActivity() {
    private lateinit var binding: ActivityMainBinding
    private val tailscale by lazy { TailscaleController(this) }
    private val notifyPermission = registerForActivityResult(
        ActivityResultContracts.RequestPermission(),
    ) { granted ->
        if (granted) {
            refreshSetup()
        } else {
            Toast.makeText(this, "没有通知权限时，前台监控可能被系统杀掉", Toast.LENGTH_LONG).show()
        }
    }

    private val statusListener: (UiStatus) -> Unit = { renderStatus(it) }
    private val logListener: () -> Unit = { renderLog() }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)
        bindPrefsToInputs()
        binding.switchMonitor.setOnCheckedChangeListener { _, checked ->
            AppPrefs.monitorEnabled = checked
            if (checked) {
                ensureNotificationPermission()
                WatchdogService.start(this)
            } else {
                WatchdogService.stop(this)
            }
        }
        binding.btnSave.setOnClickListener { saveInputs() }
        binding.btnRecover.setOnClickListener {
            saveInputs()
            AppPrefs.monitorEnabled = true
            binding.switchMonitor.isChecked = true
            WatchdogService.recoverNow(this)
            Toast.makeText(this, "已开始一次恢复", Toast.LENGTH_SHORT).show()
        }
        binding.btnOpenA11y.setOnClickListener {
            startActivity(Intent(Settings.ACTION_ACCESSIBILITY_SETTINGS))
        }
        binding.btnBattery.setOnClickListener { requestIgnoreBattery() }
        StatusStore.addListener(statusListener)
        EventLog.addListener(logListener)
        renderLog()
        refreshSetup()
        if (AppPrefs.monitorEnabled) {
            WatchdogService.start(this)
        }
    }

    override fun onResume() {
        super.onResume()
        refreshSetup()
        renderStatus(StatusStore.current)
    }

    override fun onDestroy() {
        StatusStore.removeListener(statusListener)
        EventLog.removeListener(logListener)
        super.onDestroy()
    }

    private fun bindPrefsToInputs() {
        binding.switchMonitor.isChecked = AppPrefs.monitorEnabled
        binding.switchRequireVpn.isChecked = AppPrefs.requireVpn
        binding.inputInterval.setText(AppPrefs.intervalSec.toString())
        binding.inputFailures.setText(AppPrefs.failureThreshold.toString())
        binding.inputCooldown.setText(AppPrefs.cooldownSec.toString())
        binding.inputCanary.setText(AppPrefs.canary)
        binding.inputPreventive.setText(AppPrefs.preventiveMin.toString())
    }

    private fun saveInputs() {
        AppPrefs.requireVpn = binding.switchRequireVpn.isChecked
        AppPrefs.intervalSec = binding.inputInterval.text?.toString()?.toIntOrNull() ?: 15
        AppPrefs.failureThreshold = binding.inputFailures.text?.toString()?.toIntOrNull() ?: 3
        AppPrefs.cooldownSec = binding.inputCooldown.text?.toString()?.toIntOrNull() ?: 180
        AppPrefs.canary = binding.inputCanary.text?.toString().orEmpty()
        AppPrefs.preventiveMin = binding.inputPreventive.text?.toString()?.toIntOrNull() ?: 0
        bindPrefsToInputs()
        Toast.makeText(this, "设置已保存", Toast.LENGTH_SHORT).show()
    }

    private fun renderStatus(status: UiStatus) {
        val probe = status.probe
        val ssid = if (probe.wifiSsid.isNotBlank()) "（${probe.wifiSsid}）" else ""
        binding.statusWifi.text = when {
            !probe.wifiEnabled -> "Wi-Fi：已关闭"
            probe.wifiConnected && probe.hasInternet -> "Wi-Fi：已连接$ssid，网络正常"
            probe.wifiConnected -> "Wi-Fi：已连接$ssid，但没有外网（典型的假连接）"
            else -> "Wi-Fi：未关联"
        }
        binding.statusWifi.setTextColor(
            ContextCompat.getColor(this, if (probe.hasInternet) R.color.ok else R.color.bad),
        )
        binding.statusVpn.text = if (probe.vpnUp) "Tailscale VPN：在线" else "Tailscale VPN：离线"
        binding.statusVpn.setTextColor(
            ContextCompat.getColor(this, if (probe.vpnUp) R.color.ok else R.color.warn),
        )
        binding.statusCanary.text = when (probe.canaryOk) {
            null -> "内网探测：未配置"
            true -> "内网探测：成功"
            false -> "内网探测：失败"
        }
        binding.statusMonitor.text = when {
            status.recovering -> "监控：正在恢复"
            status.monitoring -> "监控：运行中，连续失败 ${status.consecutiveFailures} 次"
            else -> "监控：未运行"
        }
        binding.statusLastRecovery.text = status.lastRecoveryText
        if (binding.switchMonitor.isChecked != AppPrefs.monitorEnabled) {
            binding.switchMonitor.isChecked = AppPrefs.monitorEnabled
        }
    }

    private fun renderLog() {
        val text = EventLog.text()
        binding.logView.text = text.ifBlank { "暂无日志" }
    }

    private fun refreshSetup() {
        val notifyOk = hasNotificationPermission()
        binding.setupNotify.text = if (notifyOk) "通知权限：已授权" else "通知权限：未授权"
        binding.setupNotify.setTextColor(color(if (notifyOk) R.color.ok else R.color.warn))

        val batteryOk = isIgnoringBattery()
        binding.setupBattery.text = if (batteryOk) "电池优化：已忽略" else "电池优化：未忽略（后台容易被杀）"
        binding.setupBattery.setTextColor(color(if (batteryOk) R.color.ok else R.color.warn))

        val a11yOk = WatchdogAccessibilityService.isEnabled(this)
        binding.setupA11y.text = if (a11yOk) {
            "无障碍：已开启"
        } else {
            "无障碍：未开启（Android 10+ 自动关 Wi-Fi 需要它）"
        }
        binding.setupA11y.setTextColor(color(if (a11yOk) R.color.ok else R.color.warn))

        val tsOk = tailscale.isInstalled()
        binding.setupTailscale.text = if (tsOk) "Tailscale：已安装" else "Tailscale：未安装 com.tailscale.ipn"
        binding.setupTailscale.setTextColor(color(if (tsOk) R.color.ok else R.color.bad))
        StatusStore.update {
            it.copy(accessibilityOn = a11yOk, tailscaleInstalled = tsOk)
        }
    }

    private fun ensureNotificationPermission() {
        if (Build.VERSION.SDK_INT >= 33 && !hasNotificationPermission()) {
            notifyPermission.launch(Manifest.permission.POST_NOTIFICATIONS)
        }
    }

    private fun hasNotificationPermission(): Boolean {
        if (Build.VERSION.SDK_INT < 33) return true
        return ContextCompat.checkSelfPermission(this, Manifest.permission.POST_NOTIFICATIONS) ==
            PackageManager.PERMISSION_GRANTED
    }

    private fun isIgnoringBattery(): Boolean {
        val pm = getSystemService(PowerManager::class.java)
        return pm.isIgnoringBatteryOptimizations(packageName)
    }

    private fun requestIgnoreBattery() {
        if (isIgnoringBattery()) {
            Toast.makeText(this, "已经忽略电池优化", Toast.LENGTH_SHORT).show()
            return
        }
        val intent = Intent(Settings.ACTION_REQUEST_IGNORE_BATTERY_OPTIMIZATIONS).apply {
            data = Uri.parse("package:$packageName")
        }
        startActivity(intent)
    }

    private fun color(id: Int): Int = ContextCompat.getColor(this, id)
}
