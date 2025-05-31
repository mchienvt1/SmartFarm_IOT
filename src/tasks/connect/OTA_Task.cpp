#include "OTA_Task.h"

Espressif_Updater<> updater;

void update_starting_callback()
{
    Serial.println("OTA: Starting update...\n");
    set_rgb_color(WHITE_RGB);
}

void finished_callback(const bool &success)
{
    if (success)
    {
        Serial.println("OTA: Done, Reboot now\n");
        esp_restart();
        return;
    }
    Serial.println("OTA: Downloading firmware failed\n");
}

void progress_callback(const size_t &current, const size_t &total)
{
    Serial.printf("OTA: Progress %.2f%%\n", static_cast<float>(current * 100U) / total);
}

const OTA_Update_Callback callback(
    CURRENT_FIRMWARE_TITLE,   // ESP32-AQUAPONICS
    CURRENT_FIRMWARE_VERSION, // 1.0.0
    &updater,
    &finished_callback,
    &progress_callback,
    &update_starting_callback,
    FIRMWARE_FAILURE_RETRIES, // 12
    FIRMWARE_PACKET_SIZE      // 4096
);

bool ota_setup()
{
    Serial.printf("OTA: Current FW: %s %s\n", CURRENT_FIRMWARE_TITLE, CURRENT_FIRMWARE_VERSION);

    ota.Firmware_Send_Info(CURRENT_FIRMWARE_TITLE, CURRENT_FIRMWARE_VERSION);

    if (!ota.Subscribe_Firmware_Update(callback))
    {
        Serial.println("OTA: Failed to subscribe to firmware update\n");
        return false;
    }
    Serial.println("OTA: Subscribed to firmware update\n");
    return true;
}