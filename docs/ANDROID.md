# IUT-Food on Android

Since v2.9.0 every release ships an **arm64-v8a APK** of the desktop
client, built by CI from the same sources as the Windows/Linux/macOS
artifacts.  The APK is signed with a debug key, so it installs on any
modern Android device (Android 8.0 / API 26 or newer) without Play
Store involvement.

## What the Android build is (and is not)

| | |
|---|---|
| **Shipped** | the customer/restaurant/admin **GUI client** (`iut-food-client`) |
| **Not shipped** | the TCP server — it is a desktop/console app and keeps running on a PC, a laptop or a Docker host |
| **Data location** | the client's SQLite database lives in the app's private storage (`QStandardPaths::AppDataLocation`), not next to the binary |

## Install

1. Grab `IUT-Food-<version>-android-arm64.apk` from the
   [latest release](https://github.com/Abolfazlrwm/IUT_Food/releases/latest)
   (checksum in `IUT-Food-<version>-android-checksums.txt`).
2. Copy it to the phone and open it.  Android will ask to allow
   installation from that app ("install unknown apps") because the APK
   is debug-signed — this is normal for direct-APK distribution.
3. Launch **IUT-Food**.  First run seeds a local demo database, exactly
   like the desktop client.

## Connect it to your server

The phone needs to reach a running `iut-food-server`:

1. Start the server on your machine (PC on the same Wi-Fi/LAN):
   ```bash
   ./iut-food-server --port 1234          # desktop binary
   # or Docker:
   docker run -p 1234:1234 iut-food-server
   ```
2. Find your PC's LAN IP (`ip addr` / `ipconfig`).
3. In the client's login dialog open the **connection settings**
   (host, port, TLS switch) and enter the IP and port `1234`.
4. Sign in — order status updates, chat and the live tracker are
   broadcast to the phone in real time, same as on the desktop.

Enable TLS on the server (`--tls-cert/--tls-key`, see
`docs/PROTOCOL.md`) and switch TLS on in the client's connection
section; traffic is then encrypted end-to-end.

## Troubleshooting

- **"Connection refused"** — the server is not listening on that
  address/port, or a firewall blocks it.  `adb logcat | grep IUT-Food`
  shows the client's view of the failure.
- **App closes immediately** — run `adb logcat` and check for a missing
  SQLite/plugin message; if you can reproduce it, please open an issue
  with the logcat output attached.
- **Where is my database?** — private app storage; clearing the app's
  data from Android settings returns the client to a fresh first run.

## Building the APK yourself

```bash
# one-time: Qt 6.8 android_arm64_v8a via aqt or the Qt online installer,
# plus the Android NDK r26+ and JDK 17
cmake -B build-android -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE="$QT_ROOT_DIR/lib/cmake/Qt6/qt.toolchain.cmake" \
  -DANDROID_ABI=arm64-v8a -DQT_ANDROID_BUILD_ALL_ABIS=OFF
cmake --build build-android
cmake --build build-android --target apk
```

The CI does exactly this in `.github/workflows/release.yml`
(`android-apk` job) — the APK attached to each release is reproducible
from the tag.
