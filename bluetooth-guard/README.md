# Bluetooth Guard (`BluetoothGuard.exe`)

Standalone native 64-bit Windows desktop & system-tray application (`BluetoothGuard.exe` — ~85 KB, zero console window, zero external runtime dependencies) that stops Windows from automatically connecting to your phone earbuds/headphones when your laptop boots, wakes from sleep, or turns Bluetooth on.

## How to Use

1. Download **`BluetoothGuard.exe`** (or click **Open / Start** on the **Bluetooth Guard** card inside **`ProjectHub.exe`**).
2. Select your phone earbuds/headphones in the list and click **`Block Auto-Connect`**.
   * Your pairing stays saved in Windows Settings — you never have to re-pair!
   * Auto-connect profiles are disabled when idle (`BluetoothSetServiceState`), and any unwanted connection is dropped immediately (`IOCTL_BTH_DISCONNECT_DEVICE`).
3. Whenever you *do* want to use those earbuds on your laptop, click **`Connect Now`** (either in the window or by right-clicking the system tray icon).
   * Once you disconnect or put your PC to sleep, **Bluetooth Guard automatically re-locks auto-connect** so tomorrow's boot stays phone-safe.
