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

static esp_err_t index_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, INDEX_HTML, strlen(INDEX_HTML));
}

static esp_err_t led_toggle_handler(httpd_req_t *req)
{
    led_enabled = !led_enabled;
    previous_led_toggle_time_ms = millis();

    return httpd_resp_send(req, "OK", 2);
}

static esp_err_t servo_wave_handler(httpd_req_t *req)
{
    wave_enabled = true;
    previous_wave_time_ms = millis();

    return httpd_resp_send(req, "OK", 2);
}

static esp_err_t capture_handler(httpd_req_t *req) {
    camera_fb_t * fb = NULL;
    esp_err_t res = ESP_OK;

    fb = esp_camera_fb_get();
    if (!fb) {
        Serial.println("Camera capture failed");
        httpd_resp_send_500(req);

        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "image/jpeg");
    httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=capture.jpg");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    
    if (fb->format == PIXFORMAT_JPEG)
    {
        res = httpd_resp_send(req, (const char *)fb->buf, fb->len);
    }
    esp_camera_fb_return(fb);

    return res;
}

void StartCameraServer()
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;

    httpd_handle_t server = NULL;
    if (httpd_start(&server, &config) == ESP_OK)
    {
        httpd_uri_t index_uri = {
            .uri      = "/",
            .method   = HTTP_GET,
            .handler  = index_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &index_uri);

        httpd_uri_t stream_uri = {
            .uri      = "/capture",
            .method   = HTTP_GET,
            .handler  = capture_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &stream_uri);

        httpd_uri_t led_uri = {
            .uri      = "/led/toggle",
            .method   = HTTP_GET,
            .handler  = led_toggle_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &led_uri);

        httpd_uri_t servo_uri = {
            .uri      = "/servo/wave",
            .method   = HTTP_GET,
            .handler  = servo_wave_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &servo_uri);

        Serial.println("Camera server started!");
    }
    else
    {
        Serial.println("Error starting server!");
    }
}