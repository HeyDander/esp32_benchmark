<div align="center">

# MONSTER ULTRA GLOBAL

### ESP32 benchmark · real measurements · global leaderboard

**Measure. Compare. Improve.**

![Platform](https://img.shields.io/badge/platform-ESP32-orange)
![Framework](https://img.shields.io/badge/framework-Arduino-00979D)
![Version](https://img.shields.io/badge/benchmark-v0.1.0-7c3aed)
![License](https://img.shields.io/badge/license-MIT-green)

</div>

---

MONSTER ULTRA GLOBAL is an experimental benchmark project for ESP32 boards. The firmware runs CPU, memory, multicore and FreeRTOS tests, calculates an experimental composite score, then submits the measurements to a MONSTER ULTRA GLOBAL API.

> **Status:** early development. The score is experimental and is not yet an official cross-device ranking standard. Compare results only when they use the same benchmark version and test conditions.

## Features

- CPU integer workload
- CPU floating-point workload
- CPU math workload (`sin`, `cos`, `sqrt`)
- Internal SRAM read, write and copy throughput
- Dual-core workload for ESP32
- FreeRTOS yield-rate measurement
- Short stability run and minimum free-heap measurement
- JSON upload to a private Next.js API endpoint
- Serial Monitor output at `115200` baud

## Repository layout

```text
MONSTER-ULTRA-GLOBAL/
├── firmware/
│   └── monster_ultra_global/
│       ├── monster_ultra_global.ino
│       └── config.example.h
├── docs/
│   └── API.md
├── .gitignore
├── LICENSE
└── README.md
```

## Hardware

Tested target:

- ESP32-WROOM-32
- ESP32 dual-core family
- Arduino IDE with Espressif ESP32 board support

The firmware uses the built-in `WiFi` and `HTTPClient` libraries. No additional Arduino library is required.

## Quick start

### 1. Prepare the firmware

1. Download or clone this repository.
2. Open `firmware/monster_ultra_global/monster_ultra_global.ino` in Arduino IDE.
3. In the same folder, make a copy of `config.example.h` named `config.h`.
4. Edit `config.h` with your Wi-Fi details, computer LAN address and upload token.

Example:

```cpp
#define WIFI_SSID       "YourWiFi"
#define WIFI_PASSWORD   "YourPassword"
#define SERVER_URL      "http://192.168.1.105:3000/api/benchmark"
#define UPLOAD_TOKEN    "your-private-token"
```

Use your computer's actual IPv4 address. Do not use `localhost`, `127.0.0.1` or `0.0.0.0` in the ESP32 URL.

### 2. Start the API

The Next.js API must be running on the same local network and listening on all interfaces:

```powershell
npm run dev -- --hostname 0.0.0.0
```

The API URL in `config.h` should look like:

```text
http://COMPUTER_IPV4:3000/api/benchmark
```

If Windows Firewall asks, allow Node.js on **Private networks only**. Keep the computer and ESP32 on the same trusted Wi-Fi network. Do not disable the firewall.

### 3. Upload the firmware

In Arduino IDE:

- Board: **ESP32 Dev Module**
- Upload the sketch
- Open Serial Monitor at **115200 baud**
- Press `EN` to run the benchmark again

The firmware prints measurements, connects to Wi-Fi and posts a JSON result. A successful API response should be HTTP `200` or another `2xx` status and include `success: true`.

## Measurements and score

The API receives these fields:

| Field | Meaning |
|---|---|
| `cpu_int_ops` | Integer workload iterations per second |
| `cpu_float_ops` | Floating-point workload iterations per second |
| `cpu_math_ops` | Math workload iterations per second |
| `multicore_ops` | Combined dual-core workload iterations per second |
| `ram_read_mbps` | Internal SRAM read throughput |
| `ram_write_mbps` | Internal SRAM write throughput |
| `ram_copy_mbps` | Internal SRAM copy throughput |
| `freertos_switches_s` | Yield calls per second |
| `stability_seconds` | Duration of the stability run |
| `min_free_heap_bytes` | Lowest observed free heap |

The current Monster Score uses a geometric mean of seven normalized measurements. Its reference anchors are initial development values. They must be calibrated and frozen before results are used for a serious public ranking.

**Do not compare scores across different `benchmark_version` values.** A change to test loops, compiler settings, timing or score references should produce a new benchmark version.

## Security

- `firmware/config.h` is intentionally ignored by Git.
- Never commit Wi-Fi credentials, API tokens, `.env.local` or Supabase secret keys.
- The upload token embedded in firmware can be extracted from a device. Use a dedicated test token and rotate it if it is exposed.
- Keep Supabase service-role/secret keys on the server only. Never place them in ESP32 firmware or browser code.
- Incoming results should remain unverified until server-side checks are implemented.

## Troubleshooting

**`HTTP response: -1` / connection refused**

- Confirm the Next.js server is running.
- Confirm `SERVER_URL` contains the computer's IPv4 address, not the ESP32's IP.
- Open `http://COMPUTER_IPV4:3000` on a phone connected to the same Wi-Fi.
- Allow Node.js on the Windows Private network profile.
- Check that guest Wi-Fi or AP/client isolation is not blocking device-to-device traffic.

**Wi-Fi connects but upload fails**

- Recheck the token and API route.
- Check the Next.js terminal for server errors.
- Confirm the endpoint is `/api/benchmark`.

**No upload token or config file**

- Copy `config.example.h` to `config.h` and fill in local values.

## Roadmap

- [x] ESP32-to-API proof of connection
- [x] Initial CPU, SRAM and multicore test suite
- [x] JSON result upload
- [ ] Repeated-run variance and thermal/clock reporting
- [ ] Calibration across multiple ESP32 boards
- [ ] Server-side validation and review workflow
- [ ] Public overall and per-model leaderboards
- [ ] Device-specific upload credentials

## License

MIT. See [LICENSE](LICENSE).
"# esp32-benchmark" 
"# esp32-benchmark" 
