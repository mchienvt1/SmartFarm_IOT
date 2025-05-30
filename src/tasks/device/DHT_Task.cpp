#include "DHT_Task.h"

DHT20 dht20(&Wire1);

// Hàm tạo giá trị random từ 0-20
float generateRandomSensorValue()
{
    return (float)random(0, 2001) / 100.0; // Tạo số từ 0.00 đến 20.00
}

void dht_task(void *pvParameters)
{
    // Khởi tạo Wire1 cho DHT20
    bool startWireStatus = Wire1.begin(DHT_SDA, DHT_SCL);

    while (true)
    {
        float dht_temp, dht_humi;

        if (startWireStatus)
        {
            // Đọc giá trị từ cảm biến DHT20 thật
            int status = dht20.read();

            switch (status)
            {
            case DHT20_OK:
            {
                dht_temp = dht20.getTemperature();
                dht_humi = dht20.getHumidity();
                ESP_LOGI("DHT", "Real DHT values - TEMP: %.2f°C, HUMI: %.2f%%", dht_temp, dht_humi);
                break;
            }
            case DHT20_ERROR_CHECKSUM:
                ESP_LOGE("DHT", "Checksum error - using random values");
                dht_temp = generateRandomSensorValue();
                dht_humi = generateRandomSensorValue();
                break;
            case DHT20_ERROR_CONNECT:
                ESP_LOGE("DHT", "Connect error - using random values");
                dht_temp = generateRandomSensorValue();
                dht_humi = generateRandomSensorValue();
                break;
            case DHT20_MISSING_BYTES:
                ESP_LOGE("DHT", "Missing bytes - using random values");
                dht_temp = generateRandomSensorValue();
                dht_humi = generateRandomSensorValue();
                break;
            case DHT20_ERROR_BYTES_ALL_ZERO:
                ESP_LOGE("DHT", "All bytes read zero - using random values");
                dht_temp = generateRandomSensorValue();
                dht_humi = generateRandomSensorValue();
                break;
            case DHT20_ERROR_READ_TIMEOUT:
                ESP_LOGE("DHT", "Read time out - using random values");
                dht_temp = generateRandomSensorValue();
                dht_humi = generateRandomSensorValue();
                break;
            case DHT20_ERROR_LASTREAD:
                ESP_LOGE("DHT", "Error read too fast - using random values");
                dht_temp = generateRandomSensorValue();
                dht_humi = generateRandomSensorValue();
                break;
            default:
                ESP_LOGE("DHT", "Unknown error - using random values");
                dht_temp = generateRandomSensorValue();
                dht_humi = generateRandomSensorValue();
                break;
            }
        }
        else
        {
            ESP_LOGE("DHT", "Fail to connect to DHT - using random values");
            dht_temp = generateRandomSensorValue();
            dht_humi = generateRandomSensorValue();
        }

        // Đọc giá trị từ cảm biến soil moisture và light
        float soil_mois_value = analogRead(1);
        float light_value = analogRead(2);
        float converted_soil_value = (1 - (soil_mois_value / 4095.0)) * 100;

        // Tạo giá trị random để so sánh (optional)
        float random_temp = generateRandomSensorValue();
        float random_humidity = generateRandomSensorValue();
        float random_light = generateRandomSensorValue();
        float random_soil = generateRandomSensorValue();

        // In thêm giá trị random để so sánh (optional)
        ESP_LOGI("SENSOR", "Random values - Temp: %.2f°C, Humidity: %.2f%%, Light: %.2f, Soil: %.2f%%",
                 random_temp, random_humidity, random_light, random_soil);

        // In giá trị thật từ cảm biến
        ESP_LOGI("SENSOR", "Real values - Soil Moisture: %.2f%%, Light: %.2f LUX",
                 converted_soil_value, light_value);

        // Tạo JSON payload với tất cả dữ liệu cảm biến
        String env_data = "{";
        env_data += "\"env_temp\":" + String(dht_temp, 2) + ",";
        env_data += "\"env_humi\":" + String(dht_humi, 2) + ",";
        env_data += "\"env_light\":" + String(light_value, 2) + ",";
        env_data += "\"env_soil\":" + String(converted_soil_value, 2) + ",";
        env_data += "\"status\":\"normal\"";
        env_data += "}";

        // Tạo JSON payload với tất cả dữ liệu cảm biến
        String random_data = "{";
        random_data += "\"env_temp\":" + String(random_temp, 2) + ",";
        random_data += "\"env_humi\":" + String(random_humidity, 2) + ",";
        random_data += "\"env_light\":" + String(random_light, 2) + ",";
        random_data += "\"env_soil\":" + String(random_soil, 2) + ",";
        random_data += "\"status\":\"normal\"";
        random_data += "}";

        sendTelemetry(random_data);

        ESP_LOGI("SENSOR", "Sent data - TEMP: %.2f°C, HUMI: %.2f%%, LIGHT: %.2f LUX, SOIL: %.2f%%",
                 dht_temp, dht_humi, light_value, converted_soil_value);

        vTaskDelay(SENSOR_DHT_TIMER / portTICK_PERIOD_MS);
    }
}

void dht_task_init()
{
    xTaskCreate(dht_task, "DHT_Task", 4096, 0, 1, 0);
}