#include <Arduino.h>
#include <driver/rtc_io.h>
#include <ezButton.h>
#include "time.h"
#include "WiFi.h"
#include <PubSubClient.h>

// WiFi
const char* ssid = "IoT_H3/4";
const char* password = "98806829";

// MQTT Broker
const char *mqtt_broker = "192.168.0.130";
const char *topic = "sensor/device05";
const char *mqtt_username = "device05";
const char *mqtt_password = "device05-password";
const int mqtt_port = 1883;


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
const int LockDuration = 7000;

uint64_t bitmask =
  BUTTON_PIN_BITMASK(WAKEUP_GPIO_15) |
  BUTTON_PIN_BITMASK(WAKEUP_GPIO_2)  |
  BUTTON_PIN_BITMASK(WAKEUP_GPIO_4)     |
  BUTTON_PIN_BITMASK(WAKEUP_GPIO_13);

RTC_DATA_ATTR int bootCount = 0;

bool isLocked = false;
int activePin = 0;

WiFiClient espClient;
PubSubClient client(espClient);


/**
 * @brief Publishes a JSON-formatted message containing button press information and a timestamp.
 *
 * This function formats a JSON payload with the name of the button pressed and the provided timestamp,
 * then publishes it to a predefined MQTT topic. If formatting or publishing fails, an error message is printed.
 *
 * @param button_pressed A C-string representing the name or identifier of the button that was pressed.
 * @param timeStamp      A struct tm containing the timestamp of the button press event (in UTC).
 */
void publishMessage(char* button_pressed, struct tm timeStamp){
  char ts[32];
  strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%SZ", &timeStamp);

  char payload[128];
  int len = snprintf(
    payload, sizeof(payload),
    "{\"button\":\"%s\",\"timestamp\":\"%s\"}",
    button_pressed, ts
  );

  if (len < 0 || len >= sizeof(payload)) {
    Serial.println("Payload formatting error");
    return;
  }

  if (client.publish(topic, payload)) {
    Serial.printf("Published: %s\n", payload);
  } else {
    Serial.println("Publish failed");
  }
}

/**
 * @brief Toggles the lock state and sets the specified LED pin accordingly.
 *
 * This function inverts the global `isLocked` state, updates the `activePin` to the provided `LEDPin`,
 * and sets the digital output of the specified pin to HIGH if locked, or LOW if unlocked.
 *
 * @param LEDPin The pin number of the LED to toggle.
 */
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

/**
 * @brief Handles the event when the "Very Good" button is pressed.
 *
 * This function checks if the "Very Good" button is pressed. If so, it performs the following actions:
 * - Prints a message to the serial monitor.
 * - Starts a countdown timer by recording the current time in milliseconds.
 * - Toggles the state of the LED associated with LED_LIGHT_2.
 * - Publishes a message indicating the button press, along with a timestamp.
 *
 * @param timeStamp The current time as a struct tm, used for message publishing.
 */
void handleVeryGoodButton(struct tm timeStamp)
{
  if (buttonVeryGood.isPressed())
  {
    Serial.println("Very Good button pressed");
    countdownStart = millis();
    toggle_led(LED_LIGHT_1);
    publishMessage("Very Good button pressed", timeStamp);
  }
}

/**
 * @brief Handles the event when the "Good" button is pressed.
 *
 * This function checks if the "Good" button is pressed. If so, it performs the following actions:
 * - Prints a message to the serial monitor.
 * - Starts a countdown timer by recording the current time in milliseconds.
 * - Toggles the state of the LED associated with LED_LIGHT_2.
 * - Publishes a message indicating the button press, along with a timestamp.
 *
 * @param timeStamp The current time as a struct tm, used for message publishing.
 */
void handleGoodButton(struct tm timeStamp)
{
  if (buttonGood.isPressed())
  {
    Serial.println("Good button pressed");
    countdownStart = millis();
    toggle_led(LED_LIGHT_2);
    publishMessage("Good button pressed", timeStamp);
  }
}

/**
 * @brief Handles the event when the "Bad" button is pressed.
 *
 * This function checks if the "Bad" button is pressed. If so, it performs the following actions:
 * - Prints a message to the serial monitor.
 * - Records the current time as the start of a countdown.
 * - Toggles the LED associated with a bad feedback indication.
 * - Publishes a message with the provided timestamp.
 *
 * @param timeStamp The current time information to be included in the published message.
 */
void handleBadButton(struct tm timeStamp)
{
  if (buttonBad.isPressed())
  {
    Serial.println("Bad button pressed");
    countdownStart = millis();
    toggle_led(LED_LIGHT_3);
    publishMessage("Bad button pressed", timeStamp);
  }
}

/**
 * @brief Handles the event when the "Very Bad" button is pressed.
 *
 * This function checks if the "Very Bad" button is pressed. If so, it performs the following actions:
 * - Prints a message to the serial monitor.
 * - Records the current time as the start of a countdown.
 * - Toggles the LED associated with a bad feedback indication.
 * - Publishes a message with the provided timestamp.
 *
 * @param timeStamp The current time information to be included in the published message.
 */
void handleVeryBadButton(struct tm timeStamp)
{
  if (buttonVeryBad.isPressed())
  {
    Serial.println("Very Bad button pressed");
    countdownStart = millis();
    toggle_led(LED_LIGHT_4);
    publishMessage("Very Bad button pressed", timeStamp);
  }
}

/**
 * @brief Handles the button press event by obtaining the current local time and
 *        passing the timestamp to individual button handler functions.
 *
 * This function retrieves the current local time and stores it in a tm structure.
 * It then calls the handlers for "Very Good", "Good", "Bad", and "Very Bad" buttons,
 * passing the timestamp to each handler for further processing.
 */
void handleButtonPress()
{
  struct tm timeStamp;
  if (getLocalTime(&timeStamp)) {
    
  }
  handleVeryGoodButton(timeStamp);
  handleGoodButton(timeStamp);
  handleBadButton(timeStamp);
  handleVeryBadButton(timeStamp);
}

/**
 * @brief Prints the GPIO pin that triggered the wake-up from deep sleep and handles the corresponding button event.
 *
 * This function retrieves the current local time and checks which GPIO pin caused the device to wake up
 * using the ESP32's ext1 wakeup status. Depending on the pin, it prints the GPIO number to the Serial output
 * and calls the appropriate handler function for the button event, passing the timestamp.
 *
 * The function checks the following GPIO pins:
 * - WAKEUP_GPIO_15: Calls handleVeryGoodButton()
 * - WAKEUP_GPIO_2:  Calls handleGoodButton()
 * - WAKEUP_GPIO_4:  Calls handleBadButton()
 * - WAKEUP_GPIO_13: Calls handleVeryBadButton()
 */
void print_GPIO_wake_up(){
  struct tm timesStamp;
  if (getLocalTime(&timesStamp)) {
  uint64_t GPIO_reason = esp_sleep_get_ext1_wakeup_status();
  
  Serial.print("GPIO that triggered the wake up: GPIO:");
  if (GPIO_reason & BUTTON_PIN_BITMASK(WAKEUP_GPIO_15)) {
    Serial.print(" 15");
    handleVeryGoodButton(timesStamp);
  }
  else if (GPIO_reason & BUTTON_PIN_BITMASK(WAKEUP_GPIO_2)) {
    Serial.print(" 2");
    handleGoodButton(timesStamp);
  }
  else if (GPIO_reason & BUTTON_PIN_BITMASK(WAKEUP_GPIO_4)) {
    Serial.print(" 4");
    handleBadButton(timesStamp);
  }
  else if (GPIO_reason & BUTTON_PIN_BITMASK(WAKEUP_GPIO_13)) {
    Serial.print(" 13");
    handleVeryBadButton(timesStamp);
  }
  }
}

/**
 * @brief Prints the reason for the ESP32 waking up from sleep.
 *
 * This function retrieves the cause of the last wakeup from deep sleep
 * and prints a human-readable message to the Serial output. It handles
 * various wakeup sources such as external signals (RTC_IO, RTC_CNTL),
 * timer, touchpad, and ULP program. For external signal using RTC_CNTL,
 * it also calls print_GPIO_wake_up() to provide additional information.
 * If the wakeup was not caused by deep sleep, it prints the numeric cause.
 */
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

/**
 * @brief Prints the current local time to the Serial output in a formatted string.
 *
 * This function retrieves the current local time using getLocalTime(), formats it as
 * "Time: HH:MM:SS MM/DD/YYYY", and prints it to the Serial monitor. If the time cannot
 * be obtained, it prints an error message instead.
 */
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

/**
 * @brief Callback function to handle incoming MQTT messages.
 *
 * This function is called when a message is received on a subscribed MQTT topic.
 * It prints the topic and the message payload to the serial monitor.
 *
 * @param topic   The topic on which the message was received.
 * @param payload The message payload as a byte array.
 * @param length  The length of the payload.
 */
void callback(char *topic, byte *payload, unsigned int length) {
 Serial.print("Message arrived in topic: ");
 Serial.println(topic);
 Serial.print("Message:");
 for (int i = 0; i < length; i++) {
     Serial.print((char) payload[i]);
 }
 Serial.println();
 Serial.println("-----------------------");
}

/**
 * @brief Initializes the MQTT client connection.
 *
 * Sets the MQTT broker address and port, assigns the callback function,
 * and attempts to connect to the MQTT broker using the provided credentials.
 * If the connection fails, it retries until successful, printing the connection
 * status to the serial monitor.
 *
 * The client ID is generated using the ESP32's MAC address to ensure uniqueness.
 */
void initMQTT(){
 client.setServer(mqtt_broker, mqtt_port);
 client.setCallback(callback);
  while (!client.connected()) {
     String client_id = "esp32-client-";
     client_id += String(WiFi.macAddress());
     Serial.printf("The client %s connects to the public mqtt broker\n", client_id.c_str());
     if (client.connect(client_id.c_str(), mqtt_username, mqtt_password)) {
         Serial.println("Public emqx mqtt broker connected");
     } else {
         Serial.print("failed with state ");
         Serial.print(client.state());
         delay(2000);
     }
 }
}

/**
 * @brief Initializes the WiFi connection.
 *
 * This function attempts to connect to a WiFi network using the provided
 * SSID and password. It enables WiFi persistence, starts the connection
 * process, and waits in a loop until the connection is successfully established.
 * Progress is indicated via serial output.
 */
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


/**
 * @brief Initializes and synchronizes the system time using NTP.
 *
 * This function configures the system clock using the specified NTP server and waits
 * until the time is successfully synchronized. It prints progress to the serial monitor
 * while waiting for the time to be set. The function considers the time synchronized
 * when the current epoch time exceeds 8 * 3600 * 2 seconds (16 hours).
 */
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

  pinMode(LED_LIGHT_1, OUTPUT);
  pinMode(LED_LIGHT_2, OUTPUT);
  pinMode(LED_LIGHT_3, OUTPUT);
  pinMode(LED_LIGHT_4, OUTPUT);
  
  initWiFi();

  // Setup time
  initTime();
  printSimpleTime();

  initMQTT();
  
  buttonVeryGood.setDebounceTime(50);
  buttonGood.setDebounceTime(50);
  buttonBad.setDebounceTime(50);
  buttonVeryBad.setDebounceTime(50);

  Serial.println("Boot number: " + String(bootCount));
  
  if (bootCount > 0) {
    print_wakeup_reason();
  }
  
  ++bootCount;
  countdownStart = millis();
}

void loop() {
  client.loop();
  buttonVeryGood.loop();
  buttonGood.loop();
  buttonBad.loop();
  buttonVeryBad.loop();

  if(!isLocked){
    handleButtonPress();
  }
  
  unsigned long elapsed = millis() - countdownStart;
  unsigned long remaining = (countdownDuration - elapsed) / 1000;
  static unsigned long lastSecondPrinted = 0;

  if (remaining != lastSecondPrinted) {
    Serial.print("Will sleep in: ");
    Serial.println(remaining);
    lastSecondPrinted = remaining;
  }

  if(elapsed >= LockDuration && isLocked){
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
