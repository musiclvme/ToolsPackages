# Tailscale 网络看门狗

在手机上持续检测 Wi-Fi 和 Tailscale 连接。发现“连着 Wi-Fi 但没网”或 Tailscale VPN 掉线后，自动：

1. 断开再打开 Wi-Fi
2. 向 Tailscale 发送一次关闭再打开（`DISCONNECT_VPN` → `CONNECT_VPN`）

用来处理“Wi-Fi 挂一段时间后网络变脏、Tailscale 也跟着不行”的情况。

## 安装（覆盖旧版时请先卸载）

云端每次编出来的 debug 包签名可能不同。手机上已经装过旧版时，直接 `adb install -r` 会失败：

```
INSTALL_FAILED_UPDATE_INCOMPATIBLE: signatures do not match previously installed version
```

请按这个顺序：

1. USB 连接手机，打开 USB 调试，确认设备是 `device`：

```bat
adb devices
```

2. **先卸载旧版**（应用里的设置会清掉）：

```bat
adb uninstall com.musiclvme.tailscalewatchdog
```

若提示没有这个包，可以忽略，继续安装。

3. 安装新 APK（把路径换成你下好的文件）：

```bat
adb install TSWatchdog-debug.apk
```

当前包：`tailscale-watchdog/dist/TSWatchdog-debug.apk`

4. 授权一次（卸载重装后必须再做）：

```bat
adb shell pm grant com.musiclvme.tailscalewatchdog android.permission.WRITE_SECURE_SETTINGS
```

5. **打开一次应用**，确认通知、忽略电池优化、无障碍，以及「开机后自动打开无线调试」。

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
8. 在 Tailscale 里关闭电池优化，并在系统 **VPN → Tailscale → 始终开启**（有的机型叫 Always-on VPN）

重启后 Android 通常不允许普通应用在后台直接打开别的界面。看门狗会先发 `CONNECT_VPN`，并用无障碍尝试打开 Tailscale；若仍起不来会发一条可点击通知。**网络正常、只是 VPN 没开时，不会再去拨 Wi-Fi**，避免把看门狗自己弄重启。

可选：把“定时保洁”设成 30 或 60 分钟，即使当前看起来正常也会定期重连一次。

## 开机后自动打开无线调试

手机重启后，系统会关掉无线调试。看门狗可以自检这个开关，发现没开就重新打开。

普通应用默认没权限改这个开关，卸载重装后需要**再用 USB 授权一次**：

```bat
adb shell pm grant com.musiclvme.tailscalewatchdog android.permission.WRITE_SECURE_SETTINGS
```

然后在应用里打开「开机后自动打开无线调试」。忽略电池优化后，开机并连上 Wi-Fi，看门狗会自己把无线调试打开。

已 root 的手机也可以不授予上述权限，应用会改用 root 打开开关，并把配置的 TCP 端口写成 persist。

### 用 TCP 端口远程 adb（默认 5555）

`adb_wifi_enabled=1` 只表示无线调试开关开了，**不等于**正在听 TCP 端口。应用里可以改 **ADB TCP 端口**，默认 `5555`。

当前这次要立刻用网络调试（USB 已连上时）：

```bat
adb tcpip 5555
adb shell ip addr show wlan0
adb connect 手机WLAN的IP:5555
adb devices
```

用手机 `wlan0` 的 `inet` 地址，不要用网关地址。

断开电脑这边的连接：

```bat
adb disconnect 手机WLAN的IP:5555
```

断开全部网络 adb：

```bat
adb disconnect
```

把手机改回只走 USB：

```bat
adb usb
```

`adb tcpip 5555` 只对**到下次重启之前**有效。重启后 `service.adb.tcp.port` 通常会被清空，需要再执行一次 `adb tcpip`，或把端口写进 persist。

USB 执行一次（端口若在应用里改过，把 `5555` 换成那个值）：

```bat
adb shell setprop persist.adb.tcp.port 5555
adb shell getprop persist.adb.tcp.port
```

应返回同一个数字。若提示权限不足，需要 root。即使 persist 已经是 `5555`，也还要 `adb tcpip 5555`（或重启且该 ROM 认 persist）之后，`adb connect` 才不会 `10061 积极拒绝`。

## 编译

需要 Android SDK 34 和 JDK 17+。

```bash
cd tailscale-watchdog
echo "sdk.dir=/path/to/android-sdk" > local.properties
./gradlew :app:assembleDebug
```

APK 输出在 `app/build/outputs/apk/debug/app-debug.apk`。新编的 debug 包如果签名和手机上已装的不一致，安装前仍要先 `adb uninstall`。
