#include "raw_api.h"
#include "main.h"
#include <string.h>
#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_netif.h"
#include "esp_log.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>

static const char *TAG = "raw_api.c";

void raw_api_task(void *pvParams) {
    char addr_str[128];
    size_t frame_len = flipdot.width * 2;
    // one extra byte so oversized datagrams yield len > frame_len instead of
    // being silently truncated to exactly frame_len (lwIP's recvfrom() ignores
    // MSG_TRUNC and never reports more than the buffer size)
    size_t rx_buf_len = frame_len + 1;
    uint8_t* rx_buf = malloc(rx_buf_len);
    if (rx_buf == NULL) {
        ESP_LOGE(TAG, "Failed to allocate rx buf");
        vTaskDelete(NULL);
        return;
    }

    while (1) {
        int sock = socket(AF_INET6, SOCK_DGRAM, IPPROTO_IPV6);
        if (sock < 0) {
            ESP_LOGE(TAG, "Unable to create socket, errno: %d", errno);
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        int mode = 0;
        setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY, (char*)&mode, sizeof(mode));

        struct sockaddr_in6 dest_addr;
        bzero(&dest_addr.sin6_addr.un, sizeof(dest_addr.sin6_addr.un));
        dest_addr.sin6_family = AF_INET6;
        dest_addr.sin6_port = htons(1337);
        
        int err = bind(sock, (struct sockaddr*)&dest_addr, sizeof(dest_addr));
        if (err < 0) {
            ESP_LOGE(TAG, "Could not bind socket, errno: %d", errno);
            close(sock);
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        while (1) {
            struct sockaddr_in6 source_addr;
            socklen_t socklen = sizeof(source_addr);
            int len = recvfrom(sock, rx_buf, rx_buf_len, 0, (struct sockaddr*)&source_addr, &socklen);
            if (len < 0) {
                ESP_LOGE(TAG, "recvfrom failed: errno %d", errno);
                break;
            } else {
                if (source_addr.sin6_family == PF_INET) {
                    inet_ntoa_r(((struct sockaddr_in *)&source_addr)->sin_addr.s_addr, addr_str, sizeof(addr_str) - 1);
                } else if (source_addr.sin6_family == PF_INET6) {
                    inet6_ntoa_r(source_addr.sin6_addr, addr_str, sizeof(addr_str) - 1);
                }
                if (len != (int)frame_len) {
                    // ignore packets whose size does not match the matrix,
                    // they are most likely not meant for the display
                    ESP_LOGW(TAG, "Ignoring %s%d byte packet from %s, expected exactly %u bytes",
                            (len == (int)rx_buf_len) ? ">=" : "", len, addr_str, (unsigned)frame_len);
                } else {
                    ESP_LOGI(TAG, "Received frame via raw api from %s", addr_str);
                    memcpy(flipdot.framebuffer->columns, rx_buf, frame_len);
                    flipdot_set_dirty_flag(&flipdot);
                }
            }
        }

        close(sock);
    }
}

esp_err_t raw_api_initialize() {
    xTaskCreate(raw_api_task, "raw_api", 4096, NULL, 8, NULL);
    return ESP_OK;
}
