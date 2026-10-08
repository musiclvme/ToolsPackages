# Tailscale 网络看门狗

在手机上持续检测 Wi-Fi 和 Tailscale 连接。发现“连着 Wi-Fi 但没网”或 Tailscale VPN 掉线后，自动：

1. 断开再打开 Wi-Fi
2. 向 Tailscale 发送一次关闭再打开（`DISCONNECT_VPN` → `CONNECT_VPN`）

用来处理“Wi-Fi 挂一段时间后网络变脏、Tailscale 也跟着不行”的情况。

## 安装

用 USB 连接手机并打开 USB 调试后：

```bat
adb devices
adb install -r dist\TSWatchdog-debug.apk
```

若 `adb devices` 显示 `unauthorized`：

1. 解锁手机，允许 USB 调试（可勾选始终允许这台计算机）
2. 若没有弹窗：开发者选项里撤销 USB 调试授权，换线或换口后再插
3. 再次运行 `adb devices`，应显示 `device`

## 使用步骤

1. 安装并打开 **TS看门狗**
2. 允许通知
3. 忽略电池优化（否则后台监控会被杀）
4. **打开无障碍服务**（Android 10 起普通应用不能直接关 Wi-Fi，需要无障碍去点系统开关）
5. 确认 Tailscale 已安装，并同样关闭电池优化
6. 建议填写一个只在 Tailscale 内网能通的地址，例如 `100.x.x.x` 或 `nas.xxx.ts.net:443`
7. 打开“实时监控”

可选：把“定时保洁”设成 30 或 60 分钟，即使当前看起来正常也会定期重连一次。

## 开机后自动打开无线调试

手机重启后，系统会关掉无线调试。看门狗可以自检这个开关，发现没开就重新打开。

普通应用默认没权限改这个开关，需要**用 USB 授权一次**（只需做一次）：

```bat
adb shell pm grant com.musiclvme.tailscalewatchdog android.permission.WRITE_SECURE_SETTINGS
```

然后在应用里打开「开机后自动打开无线调试」。忽略电池优化后，开机并连上 Wi-Fi，看门狗会自己把无线调试打开。已配对过的电脑一般不必再配对，执行 `adb connect 手机IP:端口` 即可。

已 root 的手机也可以不授予上述权限，应用会改用 root 打开开关，并把 `5555` 写成持久 TCP 端口。

`adb_wifi_enabled=1` 只表示无线调试开关开了，**不等于**在听配置的 TCP 端口。应用里可以改 **ADB TCP 端口**，默认 `5555`。重启后 `service.adb.tcp.port` 会被清空。要让 `adb connect 手机IP:端口` 开机后仍可用，需要把端口写进 persist（USB 执行一次即可，有 root 时应用会自己写）：

```bat
adb shell setprop persist.adb.tcp.port 5555
adb shell getprop persist.adb.tcp.port
```

端口若改过，把 `5555` 换成应用里填的值。应返回同一个数字。若提示权限不足，需要 root。写成功后再重启，用手机 WLAN 地址连接，不要用网关地址。

## 编译

需要 Android SDK 34 和 JDK 17+。

```bash
cd tailscale-watchdog
echo "sdk.dir=/path/to/android-sdk" > local.properties
./gradlew :app:assembleDebug
```

APK 输出在 `app/build/outputs/apk/debug/app-debug.apk`。
