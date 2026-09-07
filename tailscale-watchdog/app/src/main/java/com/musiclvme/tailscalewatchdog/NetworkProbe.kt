package com.musiclvme.tailscalewatchdog

import android.content.Context
import android.net.ConnectivityManager
import android.net.NetworkCapabilities
import android.net.wifi.WifiManager
import java.net.HttpURLConnection
import java.net.InetSocketAddress
import java.net.Socket
import java.net.URL

class NetworkProbe(context: Context) {
    private val app = context.applicationContext
    private val connectivity = app.getSystemService(Context.CONNECTIVITY_SERVICE) as ConnectivityManager
    private val wifi = app.applicationContext.getSystemService(Context.WIFI_SERVICE) as WifiManager

    fun probe(canary: String): ProbeResult {
        val wifiEnabled = wifi.isWifiEnabled
        var wifiConnected = false
        var vpnUp = false
        var validated = false
        val networks = connectivity.allNetworks
        for (network in networks) {
            val caps = connectivity.getNetworkCapabilities(network) ?: continue
            if (caps.hasTransport(NetworkCapabilities.TRANSPORT_WIFI)) {
                wifiConnected = true
                if (caps.hasCapability(NetworkCapabilities.NET_CAPABILITY_VALIDATED)) {
                    validated = true
                }
            }
            if (caps.hasTransport(NetworkCapabilities.TRANSPORT_VPN)) {
                vpnUp = true
            }
        }
        val httpOk = httpGenerate204()
        val hasInternet = validated || httpOk
        val canaryOk = probeCanary(canary)
        val ssid = try {
            wifi.connectionInfo?.ssid?.trim('"') ?: ""
        } catch (_: SecurityException) {
            ""
        }
        val detail = buildString {
            append(if (wifiEnabled) "Wi-Fi开" else "Wi-Fi关")
            append(" / ")
            append(if (wifiConnected) "已关联" else "未关联")
            append(" / ")
            append(if (hasInternet) "有外网" else "无外网")
            append(" / ")
            append(if (vpnUp) "VPN在线" else "VPN离线")
            if (canary.isNotBlank()) {
                append(" / 探测")
                append(if (canaryOk == true) "成功" else "失败")
            }
        }
        return ProbeResult(
            wifiEnabled = wifiEnabled,
            wifiConnected = wifiConnected,
            hasInternet = hasInternet,
            vpnUp = vpnUp,
            canaryOk = canaryOk,
            wifiSsid = if (ssid.isBlank() || ssid == "<unknown ssid>") "" else ssid,
            detail = detail,
        )
    }

    private fun httpGenerate204(): Boolean {
        val urls = listOf(
            "https://connectivitycheck.gstatic.com/generate_204",
            "https://www.gstatic.com/generate_204",
        )
        for (item in urls) {
            if (httpCheck(item)) return true
        }
        return false
    }

    private fun probeCanary(canary: String): Boolean? {
        val target = canary.trim()
        if (target.isEmpty()) return null
        return try {
            when {
                target.startsWith("http://") || target.startsWith("https://") -> httpCheck(target)
                target.contains(':') && !target.contains('/') -> {
                    val host = target.substringBeforeLast(':')
                    val port = target.substringAfterLast(':').toIntOrNull() ?: 443
                    tcpCheck(host, port)
                }
                else -> tcpCheck(target, 443) || tcpCheck(target, 80)
            }
        } catch (_: Exception) {
            false
        }
    }

    private fun httpCheck(url: String): Boolean {
        var conn: HttpURLConnection? = null
        return try {
            conn = (URL(url).openConnection() as HttpURLConnection).apply {
                instanceFollowRedirects = false
                connectTimeout = 5000
                readTimeout = 5000
                useCaches = false
                requestMethod = "GET"
            }
            val code = conn.responseCode
            code in 200..399 || code == 204
        } catch (_: Exception) {
            false
        } finally {
            conn?.disconnect()
        }
    }

    private fun tcpCheck(host: String, port: Int): Boolean {
        return try {
            Socket().use { socket ->
                socket.soTimeout = 5000
                socket.connect(InetSocketAddress(host, port), 5000)
                socket.isConnected
            }
        } catch (_: Exception) {
            false
        }
    }
}
