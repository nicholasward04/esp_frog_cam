#include "esp_http_server.h"
#include "esp_timer.h"
#include "esp_camera.h"
#include "FS.h"
#include "SD_MMC.h"
#include "web_app.h"

extern volatile bool led_enabled;
extern volatile uint32_t previous_led_toggle_time_ms;

extern volatile bool wave_enabled;
extern volatile uint32_t previous_wave_time_ms;

static const char* STREAM_CONTENT_TYPE = "multipart/x-mixed-replace; boundary=frame";
static const char* STREAM_BOUNDARY = "\r\n--frame\r\n";
static const char* STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

static esp_err_t IndexHandler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, INDEX_HTML, strlen(INDEX_HTML));
}

static esp_err_t LEDToggleHandler(httpd_req_t *req)
{
    led_enabled = !led_enabled;
    previous_led_toggle_time_ms = millis();

    return httpd_resp_send(req, "OK", 2);
}

static esp_err_t ServoWaveHandler(httpd_req_t *req)
{
    wave_enabled = true;
    previous_wave_time_ms = millis();

    return httpd_resp_send(req, "OK", 2);
}

static esp_err_t CaptureHandler(httpd_req_t *req) {
    camera_fb_t * fb = NULL;
    esp_err_t res = ESP_OK;
    char * part_buf[64];

    if(httpd_resp_set_type(req, STREAM_CONTENT_TYPE) != ESP_OK)
    {
        return ESP_FAIL;
    }

    while(true){
        fb = esp_camera_fb_get();
        if (!fb) {
            Serial.println("Camera capture failed");
            res = ESP_FAIL;
            break;
        }

        res = res == ESP_OK ? httpd_resp_send_chunk(req, STREAM_BOUNDARY, strlen(STREAM_BOUNDARY)) : res;
        size_t hlen = snprintf((char *)part_buf, 64, STREAM_PART, fb->len);
        res = res == ESP_OK ? httpd_resp_send_chunk(req, (const char *)part_buf, hlen) : res;
        res = res == ESP_OK ? httpd_resp_send_chunk(req, (const char *)fb->buf, fb->len) : res;
        
        esp_camera_fb_return(fb);

        if(res != ESP_OK) break;

        vTaskDelay(20); 
    }
    return res;
}

void StartCameraServer()
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.ctrl_port = 8080;

    httpd_handle_t main_server = NULL;
    if (httpd_start(&main_server, &config) == ESP_OK)
    {
        httpd_uri_t index_uri = {
            .uri      = "/",
            .method   = HTTP_GET,
            .handler  = IndexHandler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(main_server, &index_uri);

        httpd_uri_t led_uri = {
            .uri      = "/led/toggle",
            .method   = HTTP_GET,
            .handler  = LEDToggleHandler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(main_server, &led_uri);

        httpd_uri_t servo_uri = {
            .uri      = "/servo/wave",
            .method   = HTTP_GET,
            .handler  = ServoWaveHandler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(main_server, &servo_uri);

        Serial.println("Camera server started!");
    }
    
    config.server_port = 81;
    config.ctrl_port = 8181;

    httpd_handle_t stream_server = NULL;
    if (httpd_start(&stream_server, &config) == ESP_OK)
    {
        httpd_uri_t stream_uri = {
            .uri      = "/capture",
            .method   = HTTP_GET,
            .handler  = CaptureHandler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(stream_server, &stream_uri);
    }   
}