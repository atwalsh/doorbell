#include <Arduino.h>
#include <WiFi.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "esp_camera.h"

// Define camera pins according to the ESP-CAM
#define PWDN_PIN 32
#define RESET_PIN -1
#define XCLK_PIN 0
#define SIOD_PIN 26
#define SIOC_PIN 27
#define D7_PIN 35
#define D6_PIN 34
#define D5_PIN 39
#define D4_PIN 36
#define D3_PIN 21
#define D2_PIN 19
#define D1_PIN 18
#define D0_PIN 5
#define VSYNC_PIN 25
#define HREF_PIN 23
#define PCLK_PIN 22

// Built-in LED
const int LEDPIN = 33;

// Network and server settings
const char *ssid = "SSID";
const char *password = "PASS";
String serverUrl = "doorbell.atw.me";
String serverPath = "/pic";
const int webPort = 80;
WiFiClient client;

/**
 * @brief Configure the ESP-CAM
 */
void setup()
{
  pinMode(33, OUTPUT);       // Set the LED pin to output mode
  digitalWrite(LEDPIN, LOW); // Turn on the LED

  esp_sleep_enable_ext0_wakeup(GPIO_NUM_14, 1); // Configure sleep mode to wake from the button
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);    // Disable the ESP's brownout detector
  WiFi.mode(WIFI_STA);                          // Configure the ESP to connect to act as a client
  WiFi.begin(ssid, password);                   // Star the WiFi connection

  // Wait for WiFi to connect
  while (WiFi.status() != WL_CONNECTED)
    delay(50);

  camera_config_t config;
  config.pin_d0 = D0_PIN;
  config.pin_d1 = D1_PIN;
  config.pin_d2 = D2_PIN;
  config.pin_d3 = D3_PIN;
  config.pin_d4 = D4_PIN;
  config.pin_d5 = D5_PIN;
  config.pin_d6 = D6_PIN;
  config.pin_d7 = D7_PIN;
  config.pin_xclk = XCLK_PIN;
  config.pin_pclk = PCLK_PIN;
  config.pin_vsync = VSYNC_PIN;
  config.pin_href = HREF_PIN;
  config.pin_sscb_sda = SIOD_PIN;
  config.pin_sscb_scl = SIOC_PIN;
  config.pin_pwdn = PWDN_PIN;
  config.pin_reset = RESET_PIN;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAMESIZE_SVGA;
  config.jpeg_quality = 10;
  config.fb_count = 2;

  // Initialize the camera with the above config
  esp_err_t err = esp_camera_init(&config);
  if (err != 0)
    ESP.restart();

  sendPhoto();            // Snap and send a photo
  esp_deep_sleep_start(); // Go to sleep
}

void loop() {}

/**
 * @brief Take and send a photo.
 */
void sendPhoto()
{
  // Obtain the camera frame buffer
  camera_fb_t *fb = NULL;
  fb = esp_camera_fb_get();
  if (!fb)
    ESP.restart();

  // Send the photo to the server using a HTTP multipart request
  if (client.connect(serverUrl.c_str(), webPort))
  {
    String head = "--ATW\r\nContent-Disposition: form-data; name=\"imageFile\"; filename=\"pic.jpg\"\r\nContent-Type: image/jpeg\r\n\r\n";
    String tail = "\r\n--ATW--\r\n";
    uint32_t imageLen = fb->len;
    uint32_t extraLen = head.length() + tail.length();
    uint32_t totalLen = imageLen + extraLen;

    client.println("POST " + serverPath + " HTTP/1.1");
    client.println("Host: " + serverUrl);
    client.println("Content-Length: " + String(totalLen));
    client.println("Content-Type: multipart/form-data; boundary=ATW");
    client.println();
    client.print(head);

    // Send photo bytes to the server
    uint8_t *fbBuf = fb->buf;
    size_t fbLen = fb->len;
    for (size_t n = 0; n < fbLen; n = n + 1024)
    {
      if (n + 1024 < fbLen)
      {
        client.write(fbBuf, 1024);
        fbBuf += 1024;
      }
      else if (fbLen % 1024 > 0)
      {
        size_t remainder = fbLen % 1024;
        client.write(fbBuf, remainder);
      }
    }

    // Send the closing multipart request data, close the server connection and release the frame buffer
    client.print(tail);
    client.stop();
    esp_camera_fb_return(fb);
  }
}