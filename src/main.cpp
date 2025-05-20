#include <Arduino.h>
#include <driver/rtc_io.h>
#include <ezButton.h>

#define BUTTON_PIN_BITMASK(GPIO) (1ULL << GPIO)
#define WAKEUP_GPIO_15 GPIO_NUM_15
#define WAKEUP_GPIO_2 GPIO_NUM_2
#define WAKEUP_GPIO_4 GPIO_NUM_4
#define WAKEUP_GPIO_13 GPIO_NUM_13

ezButton buttonVeryGood(GPIO_NUM_15);
ezButton buttonGood(GPIO_NUM_2);
ezButton buttonBad(GPIO_NUM_4);
ezButton buttonVeryBad(GPIO_NUM_13);

unsigned long countdownStart = 0;
const unsigned long countdownDuration = 30000;

uint64_t bitmask =
  BUTTON_PIN_BITMASK(WAKEUP_GPIO_15) |
  BUTTON_PIN_BITMASK(WAKEUP_GPIO_2)  |
  BUTTON_PIN_BITMASK(WAKEUP_GPIO_4)     |
  BUTTON_PIN_BITMASK(WAKEUP_GPIO_13);

RTC_DATA_ATTR int bootCount = 0;

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

void setup() {
  Serial.begin(115200);
  buttonVeryGood.setDebounceTime(50);
  buttonGood.setDebounceTime(50);
  buttonBad.setDebounceTime(50);
  buttonVeryBad.setDebounceTime(50);
  delay(1000);

  ++bootCount;
  Serial.println("Boot number: " + String(bootCount));

  print_wakeup_reason();
}

void loop() {
  buttonVeryGood.loop();
  buttonGood.loop();
  buttonBad.loop();
  buttonVeryBad.loop();

  if (buttonVeryGood.isPressed()){
    Serial.println("Very Good button pressed");
    countdownStart = millis();
    }
  if (buttonGood.isPressed()){
    Serial.println("Good button pressed");
    countdownStart = millis();
  }
  if (buttonBad.isPressed()){
    Serial.println("Bad button pressed");
    countdownStart = millis();
  }
  if (buttonVeryBad.isPressed()){
    Serial.println("Very Bad button pressed");
    countdownStart = millis();
  }
  
  unsigned long elapsed = millis() - countdownStart;
  unsigned long remaining = (countdownDuration - elapsed) / 1000;
  static unsigned long lastSecondPrinted = 0;

  if (remaining != lastSecondPrinted) {
    Serial.print("Will sleep in: ");
    Serial.println(remaining);
    lastSecondPrinted = remaining;
  }

  if (remaining <= 0) {
    esp_sleep_enable_ext1_wakeup(bitmask, ESP_EXT1_WAKEUP_ANY_HIGH);
    rtc_gpio_pulldown_en(WAKEUP_GPIO_15);
    rtc_gpio_pullup_dis(WAKEUP_GPIO_15);
    rtc_gpio_pulldown_en(WAKEUP_GPIO_2);
    rtc_gpio_pullup_dis(WAKEUP_GPIO_2);

    Serial.println("Going to sleep now");
    esp_deep_sleep_start();
  }
}
