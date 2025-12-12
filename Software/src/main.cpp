#include <Arduino.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <WiFiClientSecure.h>
#include "esp_camera.h"
#include "secrets.h"
#include <U8g2lib.h>
#include <ESP32Servo.h>
#include <HTTPClient.h>

#define XPOWERS_CHIP_AXP2101
#include "XPowersLib.h"
#include "utilities.h"

// Pin Defines
#define LED_PIN 16
#define SERVO_PIN 15

// LED Defines
#define LED_TOGGLE_PERIOD_MS 2500

// WiFi/Server Defines
#define WIFI_CHECK_PERIOD_MS 500

// Servo Defines
#define SERVO_MIN_ANGLE 0
#define SERVO_MAX_ANGLE 179
#define WAVE_ANGLE_RIGHT 120
#define WAVE_ANGLE_LEFT 30
#define WAVE_TIME_PERIOD_MS 300
#define NUM_TIMES_TO_WAVE 5

// Cloud Defines
#define IMAGE_SEND_TO_CLOUD_PERIOD_MS 60000 // Every minute send 1 image

// Global Variables

XPowersPMU PMU;

WiFiMulti wifiMulti;
bool wifi_initialized = false;
bool server_initialized = false;
uint32_t previous_wifi_check_time_ms = 0;

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R2, U8X8_PIN_NONE);
const static char* text = "Smile! You're on Camera";

Servo myServo;
volatile bool wave_enabled = false;
volatile uint32_t previous_wave_time_ms = 0;
uint8_t num_waves = 0;

volatile bool led_enabled = false;
volatile uint32_t previous_led_toggle_time_ms = 0;

uint32_t previous_cloud_send_time_ms = 0;
uint32_t image_num = 0;

// Function Prototypes
void StartCameraServer();

// Function Definitions

void InitializeSerial()
{
    Serial.begin(115200);
    delay(3000);
    Serial.println();
}

//
// Power Chip Functionality
//

void InitializePowerChip()
{
    if (!PMU.begin(Wire, AXP2101_SLAVE_ADDRESS, I2C_SDA, I2C_SCL)) {
        Serial.println("Failed to initialize power.....");
        while (1) {
            delay(5000);
        }
    }
    //Set the working voltage of the camera, please do not modify the parameters
    PMU.setALDO1Voltage(1800);  // CAM DVDD  1500~1800
    PMU.enableALDO1();
    PMU.setALDO2Voltage(2800);  // CAM DVDD 2500~2800
    PMU.enableALDO2();
    PMU.setALDO4Voltage(3000);  // CAM AVDD 2800~3000
    PMU.enableALDO4();

    // TS Pin detection must be disable, otherwise it cannot be charged
    PMU.disableTSPinMeasure();
}

//
// WiFi Functionality 
//

void AttemptWiFiConnect()
{
    if (wifiMulti.run() == WL_CONNECTED) {
        wifi_initialized = true;
        Serial.println("WiFi connected");
        delay(1000);
    }
    else
    {
        Serial.println("Failed to connect");
    }
}

void CheckWiFiStatus()
{
    if (WiFi.status() != WL_CONNECTED)
    {
        wifi_initialized = false;
    }
}

void InitializeWiFi()
{
    wifiMulti.addAP(WIFI_SSID, WIFI_SSID_PASSWORD);
    
    Serial.println("Connecting Wifi...");
    AttemptWiFiConnect();
}

void PrintIP()
{
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
    Serial.println();
}

void HandleWiFiServerState()
{
    if (previous_wifi_check_time_ms + WIFI_CHECK_PERIOD_MS < millis())
    {
        if (wifi_initialized)
        {
            if (!server_initialized)
            {
                StartCameraServer();
                server_initialized = true;
            }
            PrintIP();
            CheckWiFiStatus();
        }
        else
        {
            server_initialized = false;
            AttemptWiFiConnect();
        }
        previous_wifi_check_time_ms = millis();
    }
}

//
// Camera Functionality 
//

void InitializeCamera()
{
    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = Y2_GPIO_NUM;
    config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM;
    config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM;
    config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM;
    config.pin_d7 = Y9_GPIO_NUM;
    config.pin_xclk = XCLK_GPIO_NUM;
    config.pin_pclk = PCLK_GPIO_NUM;
    config.pin_vsync = VSYNC_GPIO_NUM;
    config.pin_href = HREF_GPIO_NUM;
    config.pin_sccb_sda = SIOD_GPIO_NUM;
    config.pin_sccb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;
    config.xclk_freq_hz = 20000000;
    config.frame_size = FRAMESIZE_HD;
    config.pixel_format = PIXFORMAT_JPEG; // for streaming
    config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
    config.fb_location = CAMERA_FB_IN_PSRAM;
    config.jpeg_quality = 20;
    config.fb_count = 1;

    // if PSRAM IC present, init with UXGA resolution and higher JPEG quality
    //                      for larger pre-allocated frame buffer.
    if (config.pixel_format == PIXFORMAT_JPEG) {
        if (psramFound()) {
            config.jpeg_quality = 20;
            config.fb_count = 2;
            config.grab_mode = CAMERA_GRAB_LATEST;
        } else {
            // Limit the frame size when PSRAM is not available
            config.frame_size = FRAMESIZE_HD;
            config.fb_location = CAMERA_FB_IN_DRAM;
        }

    } else {
        // Best option for face detection/recognition
        config.frame_size = FRAMESIZE_HD;
#if CONFIG_IDF_TARGET_ESP32S3
        config.fb_count = 2;
#endif
    }

    // camera init
    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("Camera init failed with error 0x%x Please check if the camera is connected well.", err);
        while (1) {
            delay(5000);
        }
    }

    sensor_t *s = esp_camera_sensor_get();

    // initial sensors are flipped vertically and colors are a bit saturated
    if (s->id.PID == OV3660_PID) {
        s->set_vflip(s, 1); // flip it back
        s->set_brightness(s, 1); // up the brightness just a bit
        s->set_saturation(s, -2); // lower the saturation
    }
    // drop down frame size for higher initial frame rate
    if (config.pixel_format == PIXFORMAT_JPEG) {
        s->set_framesize(s, FRAMESIZE_HD);
    }

#if defined(LILYGO_ESP32S3_CAM_PIR_VOICE)
    s->set_vflip(s, 1);
    s->set_hmirror(s, 1);
#endif
}

//
// Screen Functionality 
//

void InitializeScreen()
{
    u8g2.begin();
}

void DrawSmileyAndText()
{
    // Draw smiley face
    u8g2.clearBuffer();
    u8g2.drawCircle(64, 25, 25); 
    u8g2.drawDisc(54, 19, 3);
    u8g2.drawDisc(74, 19, 3);
    for (int i = -12; i <= 12; i++) {
        int x = 64 + i;
        int y = 38 - (i * i) / 25;
        u8g2.drawPixel(x, y);
    }
    // Draw text
    u8g2.setFont(u8g2_font_5x7_tf);
    int textWidth = u8g2.getStrWidth(text);
    int textX = (128 - textWidth) / 2;
    int textY = 62;
    u8g2.drawStr(textX, textY, text);

    u8g2.sendBuffer();
}

void HandleScreenState()
{
    DrawSmileyAndText();
}

//
// LED and Servo Functionality
//

void ToggleLED()
{
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
}

void DisableLED()
{
    digitalWrite(LED_PIN, LOW);
}

void HandleLEDState()
{
    // If LED enabled by user, blink LED
    if (led_enabled)
    {
        if ((previous_led_toggle_time_ms + LED_TOGGLE_PERIOD_MS) < millis())
        {
            previous_led_toggle_time_ms = millis();
            ToggleLED();
        }
        
    }
    // Otherwise disabled LED
    else
    {
        DisableLED();
    }
}

void MoveServo(int min_value, int max_value, int value)
{
    int angle = map(value, min_value, max_value, SERVO_MIN_ANGLE, SERVO_MAX_ANGLE);
    myServo.write(angle);
}

void HandleServoState()
{
    if (wave_enabled & (num_waves <= NUM_TIMES_TO_WAVE))
    {
        if ((previous_wave_time_ms + WAVE_TIME_PERIOD_MS) > millis())
        {
            MoveServo(SERVO_MIN_ANGLE, SERVO_MAX_ANGLE, WAVE_ANGLE_RIGHT);
        }
        else if ((previous_wave_time_ms + (2 * WAVE_TIME_PERIOD_MS)) > millis())
        {
            MoveServo(SERVO_MIN_ANGLE, SERVO_MAX_ANGLE, WAVE_ANGLE_LEFT);
        }
        else
        {
            previous_wave_time_ms = millis();
            num_waves++;
        }
    }
    else
    {
        num_waves = 0;
        wave_enabled = false;
    }
}

void InitializeLEDAndServo()
{
    // Servo
    myServo.attach(SERVO_PIN);

    // LED
    pinMode(LED_PIN, OUTPUT);
    DisableLED();
}

//
// Cloud Functionality
//

void UploadToBlobStorage(uint8_t* image_data, size_t len, char* file_name)
{
    char url[512];
    sprintf(url, "%s%s%s", BLOB_SAS_URL, file_name, BLOB_SAS_TOKEN);

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    http.begin(client, url);
    http.addHeader("x-ms-blob-type", "BlockBlob");
    http.addHeader("Content-Type", "image/jpeg");

    http.PUT(image_data, len);
}

void CaptureAndSendPhoto()
{
    camera_fb_t * fb = NULL;
    esp_err_t res = ESP_OK;

    fb = esp_camera_fb_get();
    if (!fb) {
        Serial.println("Camera capture failed");
        return;
    }

    char file_name[32];
    sprintf(file_name, "esp_cam_%d.jpeg", image_num);
    image_num++;

    UploadToBlobStorage(fb->buf, fb->len, file_name);

    esp_camera_fb_return(fb);
}

void HandleCloudState()
{
    if ((previous_cloud_send_time_ms + IMAGE_SEND_TO_CLOUD_PERIOD_MS) < millis())
    {
        CaptureAndSendPhoto();

        previous_cloud_send_time_ms = millis();
    }
}

void setup()
{
    // Initialize all peripherals on board
    InitializeSerial();
    InitializePowerChip();
    InitializeWiFi();
    InitializeCamera();
    InitializeScreen();
    InitializeLEDAndServo();

    // Start camera server
    if (wifi_initialized)
    {
        StartCameraServer();
        server_initialized = true;
    }
}

void loop()
{
    // Handle WiFi connection
    HandleWiFiServerState();

    // Handle screen state, currently displays only predefined drawing
    HandleScreenState();

    // Handle LED state, based off of user input
    HandleLEDState();

    // Handle servo state, based off of user input
    HandleServoState();

    // Handle cloud state, send image every 1 minute
    HandleCloudState();
}
