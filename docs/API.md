# API contract

The firmware sends one `POST` request to:

```text
/api/benchmark
```

Headers:

```text
Content-Type: application/json
Authorization: Bearer YOUR_UPLOAD_TOKEN
```

Example request body (illustrative values only):

```json
{
  "board_model": "ESP32-WROOM-32",
  "chip_model": "ESP32-D0WD-V3",
  "benchmark_version": "MONSTER-ULTRA-GLOBAL-0.1.0",
  "score": 1000,
  "cpu_int_ops": 1000000,
  "cpu_float_ops": 300000,
  "cpu_math_ops": 20000,
  "multicore_ops": 1500000,
  "ram_read_mbps": 100.0,
  "ram_write_mbps": 100.0,
  "ram_copy_mbps": 100.0,
  "freertos_switches_s": 10000,
  "stability_seconds": 3,
  "min_free_heap_bytes": 180000
}
```

The server should store incoming results as unverified (`verified = false`) until they pass review. The firmware does not and cannot prove that a submitted result is genuine.
