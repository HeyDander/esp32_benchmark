/*
  MONSTER ULTRA GLOBAL — ESP32 firmware
  Version: 0.1.0
  Board: ESP32-WROOM-32

  Runs a repeatable set of lightweight tests and uploads the results
  to the user's MONSTER ULTRA GLOBAL API.

  IMPORTANT:
  - Copy config.example.h to config.h and fill in your own settings.
  - Never commit config.h or publish your upload token.
  - The v0.1.0 score is experimental. Compare only identical benchmark versions.
*/

#include <WiFi.h>
#include <HTTPClient.h>
#include <math.h>
#include <string.h>
#include "config.h"

static const char* BOARD_MODEL = "ESP32-WROOM-32";
static const char* CHIP_MODEL = "ESP32-D0WD-V3";
static const char* BENCHMARK_VERSION = "MONSTER-ULTRA-GLOBAL-0.1.0";

static const uint32_t TEST_DURATION_MS = 1200;
static const uint32_t STABILITY_DURATION_MS = 3000;

static volatile uint32_t gIntSink = 0;
static volatile float gFloatSink = 0.0f;
static volatile uint32_t gRamSink = 0;

static uint32_t cpuIntRate = 0;
static uint32_t cpuFloatRate = 0;
static uint32_t cpuMathRate = 0;
static uint32_t multicoreRate = 0;
static double ramReadMBps = 0.0;
static double ramWriteMBps = 0.0;
static double ramCopyMBps = 0.0;
static uint32_t freertosSwitches = 0;
static uint32_t stabilitySeconds = 0;
static uint32_t minFreeHeap = UINT32_MAX;
static uint32_t monsterScore = 0;

// Two small buffers kept in internal SRAM.
static uint32_t ramSource[2048];
static uint32_t ramDestination[2048];

static volatile bool multiRunning = false;
static volatile bool multiWorkerDone = true;
static volatile uint32_t multiCore0Count = 0;
static volatile uint32_t multiCore1Count = 0;

static uint32_t elapsedRate(uint32_t count, uint32_t elapsedMs) {
  if (elapsedMs == 0) return 0;
  return (uint32_t)(((uint64_t)count * 1000ULL) / elapsedMs);
}

static uint32_t runCpuIntegerTest(uint32_t durationMs) {
  uint32_t x = 0x12345678U;
  uint32_t y = 0x9E3779B9U;
  uint32_t iterations = 0;
  const uint32_t started = millis();

  while ((uint32_t)(millis() - started) < durationMs) {
    for (uint16_t i = 0; i < 1024; ++i) {
      x ^= x << 13;
      x ^= x >> 17;
      x ^= x << 5;
      y += (x ^ (iterations * 0x45D9F3BU));
      ++iterations;
    }
    taskYIELD();
  }

  gIntSink = x ^ y;
  return elapsedRate(iterations, millis() - started);
}

static uint32_t runCpuFloatTest(uint32_t durationMs) {
  float a = 1.000123f;
  float b = 0.999731f;
  float c = 0.314159f;
  uint32_t iterations = 0;
  const uint32_t started = millis();

  while ((uint32_t)(millis() - started) < durationMs) {
    for (uint16_t i = 0; i < 1024; ++i) {
      a = (a * 1.000001f) + (b * 0.000013f);
      b = (b * 0.999997f) + (c * 0.000017f);
      c = (c + 0.000031f) * 0.999991f;
      ++iterations;
    }
    taskYIELD();
  }

  gFloatSink = a + b + c;
  return elapsedRate(iterations, millis() - started);
}

static uint32_t runCpuMathTest(uint32_t durationMs) {
  float x = 0.12345f;
  float result = 0.0f;
  uint32_t iterations = 0;
  const uint32_t started = millis();

  while ((uint32_t)(millis() - started) < durationMs) {
    for (uint16_t i = 0; i < 128; ++i) {
      x += 0.00073f;
      if (x > 6.0f) x -= 6.0f;
      result += sinf(x) + cosf(x * 0.7f) + sqrtf(x + 0.01f);
      ++iterations;
    }
    taskYIELD();
  }

  gFloatSink = result;
  return elapsedRate(iterations, millis() - started);
}

static double runRamReadTest(uint32_t durationMs) {
  uint32_t sum = 0;
  uint64_t bytes = 0;
  const size_t bufferBytes = sizeof(ramSource);
  const uint32_t started = millis();

  while ((uint32_t)(millis() - started) < durationMs) {
    for (size_t i = 0; i < 2048; ++i) {
      sum += ramSource[i];
    }
    bytes += bufferBytes;
    taskYIELD();
  }

  const uint32_t elapsed = millis() - started;
  gRamSink = sum;
  if (elapsed == 0) return 0.0;
  return ((double)bytes * 1000.0) / ((double)elapsed * 1000000.0);
}

static double runRamWriteTest(uint32_t durationMs) {
  uint32_t value = 0xA5A5A5A5U;
  uint64_t bytes = 0;
  const size_t bufferBytes = sizeof(ramSource);
  const uint32_t started = millis();

  while ((uint32_t)(millis() - started) < durationMs) {
    for (size_t i = 0; i < 2048; ++i) {
      ramSource[i] = value + (uint32_t)i;
    }
    value = (value << 1) | (value >> 31);
    bytes += bufferBytes;
    taskYIELD();
  }

  const uint32_t elapsed = millis() - started;
  gRamSink = ramSource[0];
  if (elapsed == 0) return 0.0;
  return ((double)bytes * 1000.0) / ((double)elapsed * 1000000.0);
}

static double runRamCopyTest(uint32_t durationMs) {
  uint64_t bytes = 0;
  const size_t bufferBytes = sizeof(ramSource);
  const uint32_t started = millis();

  while ((uint32_t)(millis() - started) < durationMs) {
    memcpy(ramDestination, ramSource, bufferBytes);
    bytes += bufferBytes;
    taskYIELD();
  }

  const uint32_t elapsed = millis() - started;
  gRamSink = ramDestination[2047];
  if (elapsed == 0) return 0.0;
  return ((double)bytes * 1000.0) / ((double)elapsed * 1000000.0);
}

static void multicoreWorker(void* parameter) {
  (void)parameter;
  uint32_t x = 0xCAFEBABEU;

  while (multiRunning) {
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    ++multiCore1Count;

    if ((multiCore1Count & 0x3FFFU) == 0) {
      taskYIELD();
    }
  }

  gIntSink = x;
  multiWorkerDone = true;
  vTaskDelete(NULL);
}

static uint32_t runMulticoreTest(uint32_t durationMs) {
  multiCore0Count = 0;
  multiCore1Count = 0;
  multiWorkerDone = false;
  multiRunning = true;

  BaseType_t created = xTaskCreatePinnedToCore(
    multicoreWorker,
    "MonsterCore1",
    4096,
    nullptr,
    1,
    nullptr,
    1
  );

  if (created != pdPASS) {
    multiRunning = false;
    multiWorkerDone = true;
    Serial.println("[WARN] Could not create second-core benchmark task.");
    return 0;
  }

  const uint32_t started = millis();
  uint32_t x = 0x13579BDFU;

  while ((uint32_t)(millis() - started) < durationMs) {
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    ++multiCore0Count;

    if ((multiCore0Count & 0x3FFFU) == 0) {
      taskYIELD();
    }
  }

  multiRunning = false;

  const uint32_t waitStarted = millis();
  while (!multiWorkerDone && (uint32_t)(millis() - waitStarted) < 1000) {
    delay(1);
  }

  gIntSink = x;
  const uint32_t total = multiCore0Count + multiCore1Count;
  return elapsedRate(total, millis() - started);
}

static uint32_t runFreeRtosYieldTest(uint32_t durationMs) {
  uint32_t switches = 0;
  const uint32_t started = millis();

  while ((uint32_t)(millis() - started) < durationMs) {
    taskYIELD();
    ++switches;
  }

  return elapsedRate(switches, millis() - started);
}

static void runStabilityTest(uint32_t durationMs) {
  uint32_t x = 0x10203040U;
  minFreeHeap = ESP.getFreeHeap();
  const uint32_t started = millis();
  uint32_t lastHeapCheck = started;

  while ((uint32_t)(millis() - started) < durationMs) {
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;

    const uint32_t now = millis();
    if ((uint32_t)(now - lastHeapCheck) >= 250) {
      const uint32_t heapNow = ESP.getFreeHeap();
      if (heapNow < minFreeHeap) minFreeHeap = heapNow;
      lastHeapCheck = now;
      taskYIELD();
    }
  }

  gIntSink = x;
  stabilitySeconds = (millis() - started) / 1000U;
  const uint32_t finalHeap = ESP.getFreeHeap();
  if (finalHeap < minFreeHeap) minFreeHeap = finalHeap;
}

static uint32_t calculateMonsterScore() {
  // Experimental v0.1.0 reference anchors.
  // Keep these fixed within a benchmark version; recalibrate before a public ranking.
  const float refs[7] = {
    1000000.0f,  // CPU integer iterations/s
    300000.0f,   // CPU float iterations/s
    20000.0f,    // CPU math iterations/s
    1500000.0f,  // multicore iterations/s
    100.0f,      // RAM read MB/s
    100.0f,      // RAM write MB/s
    100.0f       // RAM copy MB/s
  };

  const float values[7] = {
    (float)cpuIntRate,
    (float)cpuFloatRate,
    (float)cpuMathRate,
    (float)multicoreRate,
    (float)ramReadMBps,
    (float)ramWriteMBps,
    (float)ramCopyMBps
  };

  float logSum = 0.0f;
  for (uint8_t i = 0; i < 7; ++i) {
    float ratio = values[i] / refs[i];
    if (ratio < 0.001f) ratio = 0.001f;
    logSum += logf(ratio);
  }

  const float geometricMean = expf(logSum / 7.0f);
  const float score = 1000.0f * geometricMean;

  if (!isfinite(score) || score < 0.0f) return 0;
  if (score > 2000000000.0f) return 2000000000UL;
  return (uint32_t)(score + 0.5f);
}

static void printResults() {
  Serial.println();
  Serial.println("========== MONSTER ULTRA RESULTS ==========");
  Serial.printf("CPU Integer : %lu iterations/s\n", (unsigned long)cpuIntRate);
  Serial.printf("CPU Float   : %lu iterations/s\n", (unsigned long)cpuFloatRate);
  Serial.printf("CPU Math    : %lu iterations/s\n", (unsigned long)cpuMathRate);
  Serial.printf("Multicore   : %lu iterations/s\n", (unsigned long)multicoreRate);
  Serial.printf("RAM Read    : %.2f MB/s\n", ramReadMBps);
  Serial.printf("RAM Write   : %.2f MB/s\n", ramWriteMBps);
  Serial.printf("RAM Copy    : %.2f MB/s\n", ramCopyMBps);
  Serial.printf("FreeRTOS    : %lu yields/s\n", (unsigned long)freertosSwitches);
  Serial.printf("Stability   : %lu seconds\n", (unsigned long)stabilitySeconds);
  Serial.printf("Min heap    : %lu bytes\n", (unsigned long)minFreeHeap);
  Serial.printf("MONSTER SCORE: %lu\n", (unsigned long)monsterScore);
  Serial.println("===========================================");
  Serial.println();
}

static bool connectWiFi() {
  Serial.printf("[WiFi] Connecting to: %s\n", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  const uint32_t started = millis();
  while (WiFi.status() != WL_CONNECTED &&
         (uint32_t)(millis() - started) < 25000) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WiFi] Connection failed. Results remain in Serial Monitor.");
    return false;
  }

  Serial.println("[WiFi] Connected.");
  Serial.print("[WiFi] ESP32 IP: ");
  Serial.println(WiFi.localIP());
  return true;
}

static bool uploadResults() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[UPLOAD] No Wi-Fi connection.");
    return false;
  }

  WiFiClient client;
  HTTPClient http;
  http.setTimeout(12000);

  Serial.println("[UPLOAD] Sending benchmark results...");

  if (!http.begin(client, SERVER_URL)) {
    Serial.println("[UPLOAD] Could not initialize HTTP connection.");
    return false;
  }

  String authorization = "Bearer ";
  authorization += UPLOAD_TOKEN;

  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", authorization);

  String json;
  json.reserve(700);
  json += "{";
  json += "\"board_model\":\"";
  json += BOARD_MODEL;
  json += "\",\"chip_model\":\"";
  json += CHIP_MODEL;
  json += "\",\"benchmark_version\":\"";
  json += BENCHMARK_VERSION;
  json += "\",\"score\":";
  json += String(monsterScore);
  json += ",\"cpu_int_ops\":";
  json += String(cpuIntRate);
  json += ",\"cpu_float_ops\":";
  json += String(cpuFloatRate);
  json += ",\"cpu_math_ops\":";
  json += String(cpuMathRate);
  json += ",\"multicore_ops\":";
  json += String(multicoreRate);
  json += ",\"ram_read_mbps\":";
  json += String(ramReadMBps, 2);
  json += ",\"ram_write_mbps\":";
  json += String(ramWriteMBps, 2);
  json += ",\"ram_copy_mbps\":";
  json += String(ramCopyMBps, 2);
  json += ",\"freertos_switches_s\":";
  json += String(freertosSwitches);
  json += ",\"stability_seconds\":";
  json += String(stabilitySeconds);
  json += ",\"min_free_heap_bytes\":";
  json += String(minFreeHeap);
  json += "}";

  const int responseCode = http.POST(json);
  Serial.printf("[UPLOAD] HTTP response: %d\n", responseCode);

  if (responseCode > 0) {
    Serial.println("[UPLOAD] Server response:");
    Serial.println(http.getString());
  } else {
    Serial.printf("[UPLOAD] Error: %s\n", http.errorToString(responseCode).c_str());
  }

  http.end();
  return responseCode >= 200 && responseCode < 300;
}

void setup() {
  Serial.begin(115200);
  delay(1200);

  Serial.println();
  Serial.println("  __  __  ____  _   _ ____ _____ _____ ____");
  Serial.println(" |  \\/  |/ ___|| \\ | / ___|_   _| ____|  _ \\");
  Serial.println(" | |\\/| | |  _ |  \\| \\___ \\ | | |  _| | |_) |");
  Serial.println(" | |  | | |_| || |\\  |___) || | | |___|  _ <");
  Serial.println(" |_|  |_|\\____||_| \\_|____/ |_| |_____|_| \\_\\");
  Serial.println("          MONSTER ULTRA GLOBAL v0.1.0");
  Serial.println();

  for (size_t i = 0; i < 2048; ++i) {
    ramSource[i] = (uint32_t)(i * 2654435761UL);
    ramDestination[i] = 0;
  }

  Serial.println("[1/8] CPU Integer...");
  cpuIntRate = runCpuIntegerTest(TEST_DURATION_MS);

  Serial.println("[2/8] CPU Float...");
  cpuFloatRate = runCpuFloatTest(TEST_DURATION_MS);

  Serial.println("[3/8] CPU Math...");
  cpuMathRate = runCpuMathTest(TEST_DURATION_MS);

  Serial.println("[4/8] RAM Read...");
  ramReadMBps = runRamReadTest(TEST_DURATION_MS);

  Serial.println("[5/8] RAM Write...");
  ramWriteMBps = runRamWriteTest(TEST_DURATION_MS);

  Serial.println("[6/8] RAM Copy...");
  ramCopyMBps = runRamCopyTest(TEST_DURATION_MS);

  Serial.println("[7/8] Multicore + FreeRTOS...");
  multicoreRate = runMulticoreTest(TEST_DURATION_MS);
  freertosSwitches = runFreeRtosYieldTest(TEST_DURATION_MS);

  Serial.println("[8/8] Stability...");
  runStabilityTest(STABILITY_DURATION_MS);

  monsterScore = calculateMonsterScore();
  printResults();

  if (connectWiFi()) {
    uploadResults();
  }

  Serial.println();
  Serial.println("[DONE] Benchmark run finished.");
  Serial.println("[DONE] Press EN to run it again.");
}

void loop() {
  delay(1000);
}
