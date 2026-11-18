
/*
  MONSTER ULTRA GLOBAL v0.2.0
  Target: ESP32-WROOM-32
  Arduino ESP32 Core 2.x / 3.x

  CPU Integer / Float / Math
  RAM Read / Write / Copy
  Dual Core
  FreeRTOS Yield Rate
  Stability / Heap
  WiFi / HTTPS Upload

  IMPORTANT:
  setInsecure() disables TLS certificate verification.
  Use only as a simplified test configuration.
*/

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <esp_system.h>
#include <esp_chip_info.h>
#include <math.h>
#include <string.h>

// =====================================================
// CONFIGURATION — EDIT THESE 4 VALUES
// =====================================================

#define WIFI_SSID       "your_wifi"
#define WIFI_PASSWORD   "your_wifi_password"
//use this token and domain for publish your results
#define SERVER_URL      "https://esp32-benchmarksite.onrender.com/api/benchmark"
#define UPLOAD_TOKEN    "9b12763b31c8d9254170254bcd47f15441e7b88b4788b5069a40cc1c670d691a"

#define BOARD_MODEL     "your_esp32_model"

static const char* VERSION =
  "MONSTER-ULTRA-GLOBAL-0.2.0";

static const uint32_t TEST_MS = 1200;
static const uint32_t STABILITY_MS = 3000;

static const size_t RAM_WORDS = 2048;

// =====================================================
// RESULTS
// =====================================================

volatile uint32_t intSink = 0;
volatile float floatSink = 0;
volatile uint32_t ramSink = 0;

uint32_t cpuInt = 0;
uint32_t cpuFloat = 0;
uint32_t cpuMath = 0;

uint32_t multicore = 0;
uint32_t yieldRate = 0;

double ramRead = 0;
double ramWrite = 0;
double ramCopy = 0;

uint32_t minHeap = UINT32_MAX;
uint32_t stabilitySeconds = 0;

uint32_t monsterScore = 0;

uint32_t ramA[RAM_WORDS];
uint32_t ramB[RAM_WORDS];

// =====================================================
// MULTICORE SYNCHRONIZATION
// =====================================================

portMUX_TYPE multiMux = portMUX_INITIALIZER_UNLOCKED;

bool workerRunning = false;
bool workerFinished = true;

uint32_t workerIterations = 0;

// =====================================================
// HELPERS
// =====================================================

uint32_t ratePerSecond(uint64_t count, uint32_t ms) {
  if (ms == 0) return 0;
  return (uint32_t)(count * 1000ULL / ms);
}

double mbPerSecond(uint64_t bytes, uint32_t ms) {
  if (ms == 0) return 0;
  return ((double)bytes * 1000.0) /
         ((double)ms * 1000000.0);
}

String chipName() {
  String s = ESP.getChipModel();
  s += "-REV";
  s += String(ESP.getChipRevision());
  return s;
}

// =====================================================
// SYSTEM INFO
// =====================================================

void printResetReason() {
  esp_reset_reason_t reason = esp_reset_reason();

  Serial.print("[SYSTEM] Reset reason: ");

  switch (reason) {
    case ESP_RST_POWERON:
      Serial.println("POWER ON");
      break;
    case ESP_RST_EXT:
      Serial.println("EXTERNAL RESET");
      break;
    case ESP_RST_SW:
      Serial.println("SOFTWARE RESET");
      break;
    case ESP_RST_PANIC:
      Serial.println("PANIC");
      break;
    case ESP_RST_INT_WDT:
      Serial.println("INTERRUPT WATCHDOG");
      break;
    case ESP_RST_TASK_WDT:
      Serial.println("TASK WATCHDOG");
      break;
    case ESP_RST_WDT:
      Serial.println("WATCHDOG");
      break;
    case ESP_RST_DEEPSLEEP:
      Serial.println("DEEP SLEEP");
      break;
    case ESP_RST_BROWNOUT:
      Serial.println("BROWNOUT");
      break;
    default:
      Serial.printf("OTHER (%d)\n", (int)reason);
      break;
  }
}

void printHardware() {
  esp_chip_info_t info;
  esp_chip_info(&info);

  Serial.println("\n========== HARDWARE ==========");

  Serial.printf("Board: %s\n", BOARD_MODEL);
  Serial.printf("Chip: %s\n", chipName().c_str());
  Serial.printf("CPU: %u MHz\n", ESP.getCpuFreqMHz());
  Serial.printf("Cores: %u\n", info.cores);
  Serial.printf("Flash: %u bytes\n", ESP.getFlashChipSize());
  Serial.printf("Free heap: %u bytes\n", ESP.getFreeHeap());

  Serial.println("==============================\n");
}

// =====================================================
// CPU INTEGER
// =====================================================

uint32_t testInteger() {
  uint32_t x = 0x12345678;
  uint32_t y = 0x9E3779B9;
  uint64_t count = 0;

  uint32_t start = millis();

  while ((uint32_t)(millis() - start) < TEST_MS) {
    for (int i = 0; i < 1024; i++) {
      x ^= x << 13;
      x ^= x >> 17;
      x ^= x << 5;
      y += x ^ (uint32_t)count;
      count++;
    }
    taskYIELD();
  }

  intSink = x ^ y;
  return ratePerSecond(count, millis() - start);
}

// =====================================================
// CPU FLOAT
// =====================================================

uint32_t testFloat() {
  float a = 1.000123f;
  float b = 0.999731f;
  float c = 0.314159f;

  uint64_t count = 0;
  uint32_t start = millis();

  while ((uint32_t)(millis() - start) < TEST_MS) {
    for (int i = 0; i < 1024; i++) {
      a = a * 1.000001f + b * 0.000013f;
      b = b * 0.999997f + c * 0.000017f;
      c = (c + 0.000031f) * 0.999991f;
      count++;
    }
    taskYIELD();
  }

  floatSink = a + b + c;
  return ratePerSecond(count, millis() - start);
}

// =====================================================
// CPU MATH
// =====================================================

uint32_t testMath() {
  float x = 0.12345f;
  float result = 0;

  uint64_t count = 0;
  uint32_t start = millis();

  while ((uint32_t)(millis() - start) < TEST_MS) {
    for (int i = 0; i < 128; i++) {
      x += 0.00073f;
      if (x > 6.0f) x -= 6.0f;

      result += sinf(x) + cosf(x * 0.7f)
                + sqrtf(x + 0.01f);

      count++;
    }
    taskYIELD();
  }

  floatSink = result;
  return ratePerSecond(count, millis() - start);
}

// =====================================================
// RAM TESTS
// =====================================================

double testRamRead() {
  uint32_t sum = 0;
  uint64_t bytes = 0;

  uint32_t start = millis();

  while ((uint32_t)(millis() - start) < TEST_MS) {
    for (size_t i = 0; i < RAM_WORDS; i++) {
      sum += ramA[i];
    }
    bytes += sizeof(ramA);
    taskYIELD();
  }

  uint32_t elapsed = millis() - start;
  ramSink = sum;

  return mbPerSecond(bytes, elapsed);
}

double testRamWrite() {
  uint32_t value = 0xA5A5A5A5;
  uint64_t bytes = 0;

  uint32_t start = millis();

  while ((uint32_t)(millis() - start) < TEST_MS) {
    for (size_t i = 0; i < RAM_WORDS; i++) {
      ramA[i] = value + i;
    }

    value = (value << 1) | (value >> 31);
    bytes += sizeof(ramA);
    taskYIELD();
  }

  uint32_t elapsed = millis() - start;
  ramSink = ramA[0];

  return mbPerSecond(bytes, elapsed);
}

double testRamCopy() {
  uint64_t bytes = 0;

  uint32_t start = millis();

  while ((uint32_t)(millis() - start) < TEST_MS) {
    memcpy(ramB, ramA, sizeof(ramA));
    bytes += sizeof(ramA);
    taskYIELD();
  }

  uint32_t elapsed = millis() - start;
  ramSink = ramB[RAM_WORDS - 1];

  return mbPerSecond(bytes, elapsed);
}

// =====================================================
// MULTICORE WORKER
// =====================================================

void multicoreWorker(void* parameter) {
  (void)parameter;

  uint32_t x = 0xCAFEBABE;
  uint32_t localCount = 0;

  while (true) {
    for (int i = 0; i < 16384; i++) {
      x ^= x << 13;
      x ^= x >> 17;
      x ^= x << 5;
      localCount++;
    }

    bool running;

    portENTER_CRITICAL(&multiMux);
    running = workerRunning;
    portEXIT_CRITICAL(&multiMux);

    if (!running) break;

    taskYIELD();
  }

  intSink = x;

  portENTER_CRITICAL(&multiMux);
  workerIterations = localCount;
  workerFinished = true;
  portEXIT_CRITICAL(&multiMux);

  vTaskDelete(NULL);
}

// =====================================================
// MULTICORE TEST
// =====================================================

uint32_t testMulticore() {
#if defined(CONFIG_FREERTOS_UNICORE) && CONFIG_FREERTOS_UNICORE
  Serial.println("[MULTICORE] Single-core build. Skipped.");
  return 0;
#else

  int mainCore = xPortGetCoreID();
  int workerCore = (mainCore == 0) ? 1 : 0;

  portENTER_CRITICAL(&multiMux);
  workerRunning = true;
  workerFinished = false;
  workerIterations = 0;
  portEXIT_CRITICAL(&multiMux);

  BaseType_t created = xTaskCreatePinnedToCore(
    multicoreWorker,
    "MonsterWorker",
    4096,
    NULL,
    1,
    NULL,
    workerCore
  );

  if (created != pdPASS) {
    portENTER_CRITICAL(&multiMux);
    workerRunning = false;
    workerFinished = true;
    portEXIT_CRITICAL(&multiMux);

    Serial.println("[MULTICORE] Worker creation failed.");
    return 0;
  }

  Serial.printf("[MULTICORE] Main core: %d\n", mainCore);
  Serial.printf("[MULTICORE] Worker core: %d\n", workerCore);

  uint32_t x = 0x13579BDF;
  uint64_t localCount = 0;

  uint32_t start = millis();

  while ((uint32_t)(millis() - start) < TEST_MS) {
    for (int i = 0; i < 16384; i++) {
      x ^= x << 13;
      x ^= x >> 17;
      x ^= x << 5;
      localCount++;
    }
    taskYIELD();
  }

  uint32_t elapsed = millis() - start;

  portENTER_CRITICAL(&multiMux);
  workerRunning = false;
  portEXIT_CRITICAL(&multiMux);

  uint32_t waitStart = millis();
  bool done = false;

  while ((uint32_t)(millis() - waitStart) < 1000) {
    portENTER_CRITICAL(&multiMux);
    done = workerFinished;
    portEXIT_CRITICAL(&multiMux);

    if (done) break;
    delay(1);
  }

  if (!done) {
    Serial.println("[MULTICORE] Worker timeout.");
    return 0;
  }

  uint32_t otherCount;

  portENTER_CRITICAL(&multiMux);
  otherCount = workerIterations;
  portEXIT_CRITICAL(&multiMux);

  intSink = x;

  return ratePerSecond(localCount + otherCount, elapsed);

#endif
}

// =====================================================
// FREERTOS YIELD RATE
// =====================================================

uint32_t testYieldRate() {
  uint64_t count = 0;
  uint32_t start = millis();

  while ((uint32_t)(millis() - start) < TEST_MS) {
    taskYIELD();
    count++;
  }

  return ratePerSecond(count, millis() - start);
}

// =====================================================
// STABILITY TEST
// =====================================================

void testStability() {
  uint32_t x = 0x10203040;

  minHeap = ESP.getFreeHeap();

  uint32_t start = millis();
  uint32_t lastCheck = start;

  while ((uint32_t)(millis() - start) < STABILITY_MS) {
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;

    uint32_t now = millis();

    if ((uint32_t)(now - lastCheck) >= 250) {
      uint32_t heap = ESP.getFreeHeap();

      if (heap < minHeap) minHeap = heap;

      lastCheck = now;
      taskYIELD();
    }
  }

  intSink = x;

  stabilitySeconds = (millis() - start) / 1000;

  uint32_t finalHeap = ESP.getFreeHeap();
  if (finalHeap < minHeap) minHeap = finalHeap;
}

// =====================================================
// SCORE
// =====================================================

uint32_t calculateScore() {
  /*
    Experimental reference values.
    Not yet calibrated for a public global ranking.
  */

  const float refs[7] = {
    1000000.0f,
    300000.0f,
    20000.0f,
    1500000.0f,
    100.0f,
    100.0f,
    100.0f
  };

  const float values[7] = {
    (float)cpuInt,
    (float)cpuFloat,
    (float)cpuMath,
    (float)multicore,
    (float)ramRead,
    (float)ramWrite,
    (float)ramCopy
  };

  float logSum = 0;

  for (int i = 0; i < 7; i++) {
    float ratio = values[i] / refs[i];

    if (ratio < 0.001f) ratio = 0.001f;

    logSum += logf(ratio);
  }

  float score = 1000.0f * expf(logSum / 7.0f);

  if (!isfinite(score) || score < 0) return 0;
  if (score > 2000000000.0f) return 2000000000UL;

  return (uint32_t)(score + 0.5f);
}

// =====================================================
// PRINT RESULTS
// =====================================================

void printResults() {
  Serial.println("\n========== MONSTER RESULTS ==========");

  Serial.printf("CPU Integer: %lu ops/s\n", (unsigned long)cpuInt);
  Serial.printf("CPU Float: %lu ops/s\n", (unsigned long)cpuFloat);
  Serial.printf("CPU Math: %lu ops/s\n", (unsigned long)cpuMath);

  Serial.printf("Multicore: %lu ops/s\n", (unsigned long)multicore);

  Serial.printf("RAM Read: %.2f MB/s\n", ramRead);
  Serial.printf("RAM Write: %.2f MB/s\n", ramWrite);
  Serial.printf("RAM Copy: %.2f MB/s\n", ramCopy);

  Serial.printf("FreeRTOS Yield Rate: %lu yields/s\n",
                (unsigned long)yieldRate);

  Serial.printf("Stability: %lu seconds\n",
                (unsigned long)stabilitySeconds);

  Serial.printf("Minimum Free Heap: %lu bytes\n",
                (unsigned long)minHeap);

  Serial.printf("MONSTER SCORE: %lu\n",
                (unsigned long)monsterScore);

  Serial.println("=====================================\n");
}

// =====================================================
// WIFI
// =====================================================

bool connectWiFi() {
  Serial.printf("[WIFI] Connecting to %s\n", WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  uint32_t start = millis();

  while (WiFi.status() != WL_CONNECTED &&
         (uint32_t)(millis() - start) < 20000) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WIFI] Connection failed.");
    WiFi.disconnect();
    return false;
  }

  Serial.println("[WIFI] Connected.");
  Serial.print("[WIFI] IP: ");
  Serial.println(WiFi.localIP());

  return true;
}

// =====================================================
// JSON UPLOAD
// =====================================================

bool uploadResults() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[UPLOAD] No WiFi.");
    return false;
  }

  if (strncmp(SERVER_URL, "https://", 8) != 0) {
    Serial.println("[UPLOAD] SERVER_URL must use HTTPS.");
    return false;
  }

  WiFiClientSecure client;

  // Simplified TLS mode; server identity is not verified.
  client.setInsecure();

  HTTPClient http;
  http.setTimeout(12000);

  if (!http.begin(client, SERVER_URL)) {
    Serial.println("[UPLOAD] HTTPS initialization failed.");
    return false;
  }

  http.addHeader("Content-Type", "application/json");
  http.addHeader(
    "Authorization",
    String("Bearer ") + UPLOAD_TOKEN
  );

  String json;
  json.reserve(800);

  json += "{";

  json += "\"board_model\":\"" + String(BOARD_MODEL) + "\",";
  json += "\"chip_model\":\"" + chipName() + "\",";
  json += "\"benchmark_version\":\"" + String(VERSION) + "\",";

  json += "\"score\":" + String(monsterScore) + ",";
  json += "\"cpu_int_ops\":" + String(cpuInt) + ",";
  json += "\"cpu_float_ops\":" + String(cpuFloat) + ",";
  json += "\"cpu_math_ops\":" + String(cpuMath) + ",";
  json += "\"multicore_ops\":" + String(multicore) + ",";

  json += "\"ram_read_mbps\":" + String(ramRead, 2) + ",";
  json += "\"ram_write_mbps\":" + String(ramWrite, 2) + ",";
  json += "\"ram_copy_mbps\":" + String(ramCopy, 2) + ",";

  json += "\"freertos_yields_s\":" + String(yieldRate) + ",";
  json += "\"stability_seconds\":" + String(stabilitySeconds) + ",";
  json += "\"min_free_heap_bytes\":" + String(minHeap);

  json += "}";

  Serial.println("[UPLOAD] Sending results...");

  int code = http.POST(json);

  Serial.printf("[UPLOAD] HTTP code: %d\n", code);

  bool success = false;

  if (code > 0) {
    String response = http.getString();
    Serial.println("[UPLOAD] Server response:");
    Serial.println(response);

    success = (code >= 200 && code < 300);
  } else {
    Serial.println(http.errorToString(code));
  }

  http.end();

  if (success) {
    Serial.println("[UPLOAD] Successful.");
  } else {
    Serial.println("[UPLOAD] Failed.");
  }

  return success;
}

// =====================================================
// SETUP
// =====================================================

void setup() {
  Serial.begin(115200);
  delay(1200);

  Serial.println();
  Serial.println("========================================");
  Serial.println("       MONSTER ULTRA GLOBAL");
  Serial.println("              v0.2.0");
  Serial.println("========================================");

  printResetReason();
  printHardware();

  Serial.println("[SYSTEM] Initializing RAM...");

  for (size_t i = 0; i < RAM_WORDS; i++) {
    ramA[i] = (uint32_t)(i * 2654435761UL);
    ramB[i] = 0;
  }

  Serial.println("[1/8] CPU Integer...");
  cpuInt = testInteger();

  Serial.println("[2/8] CPU Float...");
  cpuFloat = testFloat();

  Serial.println("[3/8] CPU Math...");
  cpuMath = testMath();

  Serial.println("[4/8] RAM Read...");
  ramRead = testRamRead();

  Serial.println("[5/8] RAM Write...");
  ramWrite = testRamWrite();

  Serial.println("[6/8] RAM Copy...");
  ramCopy = testRamCopy();

  Serial.println("[7/8] Multicore...");
  multicore = testMulticore();

  Serial.println("[8/8] FreeRTOS + Stability...");
  yieldRate = testYieldRate();
  testStability();

  monsterScore = calculateScore();

  // Always print local results before networking.
  printResults();

  Serial.println("[SYSTEM] Local benchmark completed.");

  if (connectWiFi()) {
    uploadResults();
  } else {
    Serial.println("[SYSTEM] Upload skipped.");
  }

  Serial.println("[DONE] Benchmark finished.");
}

// =====================================================
// LOOP
// =====================================================

void loop() {
  delay(1000);
}
