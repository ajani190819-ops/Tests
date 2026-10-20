# Bluetooth Guard

Stops your Windows laptop from automatically connecting to your earbuds or headphones at startup, on wake, or when turning Bluetooth on — so your laptop stops hijacking audio away from your phone.

## Install / Update

Double-click **`Setup.bat`** (no Administrator prompt needed).

It builds **`BluetoothGuard.exe`**, adds **Desktop** and **Start Menu** shortcuts, sets it to start quietly when you sign in to Windows, and opens the control window.

## How It Works

Every paired Bluetooth device appears in a clean card with a single switch:

* **Allow auto-connect ON** *(default for mice, keyboards, controllers, or dedicated PC headsets)* — behaves just like normal Windows Bluetooth.
* **Allow auto-connect OFF (`Manual Only — Phone Safe`)** — keeps your earbuds paired in Windows, disables Windows auto-connect audio profiles while idle so Windows won't grab them away from your phone on boot or wake, and drops any unsolicited auto-connect immediately.
* **One-Click `Connect Now` (Temporary Laptop Session):** Whenever you actually want your blocked earbuds connected to your laptop, click **Connect Now** in the window (or right-click the **Bluetooth Guard** system tray icon and click **Connect \<Your Earbuds\>**). As soon as you disconnect, put your earbuds back in their case, or close your laptop lid, Bluetooth Guard **automatically re-locks auto-connect** so your phone stays safe next time too.

## Where Settings & Logs Live

| Item | Path |
|---|---|
| Device preferences | `%LOCALAPPDATA%\BluetoothGuard\settings.json` |
| Activity logs | `%LOCALAPPDATA%\BluetoothGuard\logs\guard.log` |
| Startup entry | `HKCU\Software\Microsoft\Windows\CurrentVersion\Run\BluetoothGuard` |
