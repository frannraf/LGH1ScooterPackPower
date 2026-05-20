// LG H1 scooter pack automatic wake controller.
//
// Blue Pill pins used by this sketch:
// - PC13 / C13: onboard user LED, software-PWM status breathing.
// - PA11 / A11: USB D-, used by the onboard USB serial terminal.
// - PA12 / A12: USB D+, used by the onboard USB serial terminal.
// - PB8  / B8 : CAN RX, connected to CAN transceiver RXD. See CAN tab.
// - PB9  / B9 : CAN TX, connected to CAN transceiver TXD. See CAN tab.
//
// The old PB5/B5 switch input is no longer used by this auto-wake behavior.

#include <Arduino.h>

static constexpr uint32_t kSerialBaud = 115200;
static constexpr uint32_t kUsbSerialConnectWaitMs = 3000;
static constexpr uint32_t kBootWakeDelayMs = 5000;
static constexpr uint32_t kWakeIntervalMs = 500;
static constexpr uint8_t kStartupWakeAttempts = 10;

// Blue Pill silkscreen: PC13 or C13. This is the onboard user LED.
static constexpr uint32_t kStatusLedPin = PC13;

// The Blue Pill onboard LED is normally active-low.
static constexpr uint8_t kLedOnLevel = LOW;
static constexpr uint8_t kLedOffLevel = HIGH;

static constexpr uint32_t kBreathingPeriodMs = 2200;
static constexpr uint32_t kSoftwarePwmPeriodUs = 4000;
static constexpr uint8_t kBreathingMinBrightness = 4;
static constexpr uint8_t kBreathingMaxBrightness = 220;

static uint32_t previousWakeMillis = 0;

static bool canBegin();
static void canPrintReceivedMessages();
static bool canSendPackWakeCommand(bool wakeEnabled);

static void beginDebugSerial();
static bool runStartupWakeSequence();
static void serviceWakeKeepAlive();
static void serviceBreathingLed();
static uint8_t breathingBrightness();
static void blinkCanFailure();

void setup()
{
  pinMode(kStatusLedPin, OUTPUT);
  digitalWrite(kStatusLedPin, kLedOffLevel);

  beginDebugSerial();

  Serial.println();
  Serial.println("LG H1 scooter pack automatic wake controller");
  Serial.println("CAN is remapped to PB8/PB9 so PA11/PA12 can be USB serial.");

  if (!canBegin()) {
    Serial.println("CAN1 initialize fail. Check transceiver wiring and selected pins.");
    blinkCanFailure();
  }

  Serial.println("CAN1 initialize ok");
  Serial.print("Waiting ");
  Serial.print(kBootWakeDelayMs / 1000);
  Serial.println(" seconds before wake attempt...");
  delay(kBootWakeDelayMs);

  if (!runStartupWakeSequence()) {
    Serial.println("Battery wake attempt failed.");
    blinkCanFailure();
  }

  Serial.println("Battery wake command transmitted successfully.");
}

void loop()
{
  serviceBreathingLed();
  serviceWakeKeepAlive();
  canPrintReceivedMessages();
}

static void beginDebugSerial()
{
  Serial.begin(kSerialBaud);

  const uint32_t startedAt = millis();
  while (!Serial && millis() - startedAt < kUsbSerialConnectWaitMs) {
    delay(10);
  }
}

static bool runStartupWakeSequence()
{
  for (uint8_t attempt = 1; attempt <= kStartupWakeAttempts; attempt++) {
    Serial.print("Wake attempt ");
    Serial.print(attempt);
    Serial.print("/");
    Serial.println(kStartupWakeAttempts);

    if (!canSendPackWakeCommand(true)) {
      return false;
    }

    previousWakeMillis = millis();
    canPrintReceivedMessages();

    if (attempt < kStartupWakeAttempts) {
      delay(kWakeIntervalMs);
    }
  }

  return true;
}

static void serviceWakeKeepAlive()
{
  const uint32_t now = millis();
  if (now - previousWakeMillis < kWakeIntervalMs) {
    return;
  }

  previousWakeMillis = now;
  if (!canSendPackWakeCommand(true)) {
    Serial.println("CAN keep-alive failed.");
    blinkCanFailure();
  }
}

static void serviceBreathingLed()
{
  const uint8_t brightness = breathingBrightness();
  const uint32_t pwmPosition = micros() % kSoftwarePwmPeriodUs;
  const uint32_t onTime = ((uint32_t)brightness * kSoftwarePwmPeriodUs) / 255;

  digitalWrite(kStatusLedPin, pwmPosition < onTime ? kLedOnLevel : kLedOffLevel);
}

static uint8_t breathingBrightness()
{
  const uint32_t position = millis() % kBreathingPeriodMs;
  const uint32_t halfPeriod = kBreathingPeriodMs / 2;
  const uint32_t ramp = position < halfPeriod ? position : kBreathingPeriodMs - position;
  const uint32_t range = kBreathingMaxBrightness - kBreathingMinBrightness;

  return kBreathingMinBrightness + ((ramp * range) / halfPeriod);
}

static void blinkCanFailure()
{
  digitalWrite(kStatusLedPin, kLedOffLevel);

  while (true) {
    for (uint8_t flash = 0; flash < 3; flash++) {
      digitalWrite(kStatusLedPin, kLedOnLevel);
      delay(120);
      digitalWrite(kStatusLedPin, kLedOffLevel);
      delay(120);
    }

    delay(900);
  }
}
