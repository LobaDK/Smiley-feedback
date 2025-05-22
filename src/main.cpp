#include <Arduino.h>
#include <driver/rtc_io.h>
#include <ezButton.h>
#include "time.h"
#include "WiFi.h"


const char* ssid = "IoT_H3/4";
const char* password = "98806829";

// NTP
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 3600;
const int   daylightOffset_sec = 3600;


// Wakeup buttons
#define BUTTON_PIN_BITMASK(GPIO) (1ULL << GPIO)
#define WAKEUP_GPIO_15 GPIO_NUM_15
#define WAKEUP_GPIO_2 GPIO_NUM_2
#define WAKEUP_GPIO_4 GPIO_NUM_4
#define WAKEUP_GPIO_13 GPIO_NUM_13

#define LED_LIGHT_1 26
#define LED_LIGHT_2 25
#define LED_LIGHT_3 33
#define LED_LIGHT_4 32


ezButton buttonVeryGood(GPIO_NUM_15);
ezButton buttonGood(GPIO_NUM_2);
ezButton buttonBad(GPIO_NUM_4);
ezButton buttonVeryBad(GPIO_NUM_13);

unsigned long countdownStart = 0;
const unsigned long countdownDuration = 15000;

uint64_t bitmask =
  BUTTON_PIN_BITMASK(WAKEUP_GPIO_15) |
  BUTTON_PIN_BITMASK(WAKEUP_GPIO_2)  |
  BUTTON_PIN_BITMASK(WAKEUP_GPIO_4)     |
  BUTTON_PIN_BITMASK(WAKEUP_GPIO_13);

RTC_DATA_ATTR int bootCount = 0;

bool isLocked = false;
int activePin = 0;


void print_GPIO_wake_up(){
  uint64_t GPIO_reason = esp_sleep_get_ext1_wakeup_status();
  Serial.print("GPIO that triggered the wake up: GPIO:");
  if (GPIO_reason & BUTTON_PIN_BITMASK(WAKEUP_GPIO_15)) Serial.print(" 15");
  if (GPIO_reason & BUTTON_PIN_BITMASK(WAKEUP_GPIO_2))  Serial.print(" 2");
  if (GPIO_reason & BUTTON_PIN_BITMASK(WAKEUP_GPIO_4))  Serial.print(" 4");
  if (GPIO_reason & BUTTON_PIN_BITMASK(WAKEUP_GPIO_13)) Serial.print(" 13");
  Serial.println();

}

void print_wakeup_reason(){
  esp_sleep_wakeup_cause_t wakeup_reason;

  wakeup_reason = esp_sleep_get_wakeup_cause();

  switch (wakeup_reason) {
    case ESP_SLEEP_WAKEUP_EXT0:     
      Serial.println("Wakeup caused by external signal using RTC_IO");
      break;
    case ESP_SLEEP_WAKEUP_EXT1:
      Serial.println("Wakeup caused by external signal using RTC_CNTL");
      print_GPIO_wake_up();
      break;
    case ESP_SLEEP_WAKEUP_TIMER:
      Serial.println("Wakeup caused by timer");
      break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD:
      Serial.println("Wakeup caused by touchpad");
      break;
    case ESP_SLEEP_WAKEUP_ULP:
      Serial.println("Wakeup caused by ULP program");
      break;
    default:
      Serial.printf("Wakeup was not caused by deep sleep: %d\n", wakeup_reason);
      break;
  }
}

void toggle_led(int LEDPin) {
  isLocked = !isLocked;
  activePin = LEDPin;
  if (isLocked) {
    digitalWrite(LEDPin, HIGH);
  }
  else {
    digitalWrite(LEDPin, LOW);
  }
}

void printSimpleTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return;
  }
  char buf[32];
  // Format: MM,HH,DD,YYYY
  strftime(buf, sizeof(buf), "Time: %H:%M:%S %m/%d/%Y", &timeinfo);


  Serial.println(buf);
}


void initWiFi(){
  Serial.print("Waiting for WiFi");
  WiFi.persistent(true);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected.");
}


void initTime(){
  // Configures clock
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  Serial.print("Waiting for time sync");

  // Time() returns seconds since 1970 epoch time.
  // When startet it might be only a few seconds or 0
  // If time jumps over over 57 600 seconds then time should be synced
  time_t now = time(nullptr);
  while (now < 8 * 3600 * 2) {
    Serial.print(".");
    delay(500);
    now = time(nullptr);
  }
  Serial.println();
}

void setup() {
  Serial.begin(115200);

  initWiFi();

  // Setup time
  initTime();
  printSimpleTime();
  
  buttonVeryGood.setDebounceTime(50);
  buttonGood.setDebounceTime(50);
  buttonBad.setDebounceTime(50);
  buttonVeryBad.setDebounceTime(50);
  
  pinMode(LED_LIGHT_1, OUTPUT);
  pinMode(LED_LIGHT_2, OUTPUT);
  pinMode(LED_LIGHT_3, OUTPUT);
  pinMode(LED_LIGHT_4, OUTPUT);

  Serial.println("Boot number: " + String(bootCount));

  if (bootCount > 0) {
  print_wakeup_reason();
}

  ++bootCount;
void loop() {
  buttonVeryGood.loop();
  buttonGood.loop();
  buttonBad.loop();
  buttonVeryBad.loop();

  if(!isLocked){
    if (buttonVeryGood.isPressed()){
      Serial.println("Very Good button pressed");
      countdownStart = millis();
      toggle_led(LED_LIGHT_1);
      }
    if (buttonGood.isPressed()){
      Serial.println("Good button pressed");
      countdownStart = millis();
      toggle_led(LED_LIGHT_2);
    }
    if (buttonBad.isPressed()){
      Serial.println("Bad button pressed");
      countdownStart = millis();
      toggle_led(LED_LIGHT_3);
    }
    if (buttonVeryBad.isPressed()){
      Serial.println("Very Bad button pressed");
      countdownStart = millis();
      toggle_led(LED_LIGHT_4);
    }
  }
  
  unsigned long elapsed = millis() - countdownStart;
  unsigned long remaining = (countdownDuration - elapsed) / 1000;
  static unsigned long lastSecondPrinted = 0;

  if (remaining != lastSecondPrinted) {
    Serial.print("Will sleep in: ");
    Serial.println(remaining);
    lastSecondPrinted = remaining;
  }

  if(elapsed >= 7000 && isLocked){
    toggle_led(activePin);
  }

  if (remaining <= 0) {

    // Disconnect WiFi
    WiFi.disconnect(true);

    // Configure your EXT1 wake sources
    esp_sleep_enable_ext1_wakeup(bitmask, ESP_EXT1_WAKEUP_ANY_HIGH);
    rtc_gpio_pulldown_en(WAKEUP_GPIO_15);
    rtc_gpio_pullup_dis(WAKEUP_GPIO_15);
    rtc_gpio_pulldown_en(WAKEUP_GPIO_2);
    rtc_gpio_pullup_dis(WAKEUP_GPIO_2);
    rtc_gpio_pulldown_en(WAKEUP_GPIO_4);
    rtc_gpio_pullup_dis(WAKEUP_GPIO_4);
    rtc_gpio_pulldown_en(WAKEUP_GPIO_13);
    rtc_gpio_pullup_dis(WAKEUP_GPIO_13);

    Serial.println("Going to sleep now");
    esp_deep_sleep_start();
  }
}
