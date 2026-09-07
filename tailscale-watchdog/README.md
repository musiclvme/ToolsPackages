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

## 编译

需要 Android SDK 34 和 JDK 17+。

```bash
cd tailscale-watchdog
echo "sdk.dir=/path/to/android-sdk" > local.properties
./gradlew :app:assembleDebug
```

APK 输出在 `app/build/outputs/apk/debug/app-debug.apk`。
