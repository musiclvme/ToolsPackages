package com.musiclvme.tailscalewatchdog

/**
 * adbd loads RSA authorizations from credential-encrypted storage.
 * Restarting it before unlock makes every client show up as unauthorized,
 * including ones that already tapped "always allow".
 */
object AdbAuthPolicy {
    fun shouldRestartAdbd(unlocked: Boolean): Boolean = unlocked
}
