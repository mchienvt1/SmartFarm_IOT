#include "DHT_Task.h"

DHT20 dht20(&Wire);

float random_temp;
float random_humidity;
float random_light;
float random_soil;
float dht_temp, dht_humi,light_value,soil_value;

// Hàm tạo giá trị random từ 0-20 (cho light và soil)
float generateRandomSensorValue()
{
    return (float)random(0, 2001) / 100.0; // Tạo số từ 0.00 đến 20.00
}

// Hàm tạo giá trị random cho nhiệt độ từ 31.0 đến 31.1 độ C
float generateRandomTemperature()
{
    return 31.0 + (float)random(0, 101) / 1000.0; // Tạo số từ 31.00 đến 31.10
}

// Hàm tạo giá trị random cho độ ẩm từ 65.4 đến 65.5%
float generateRandomHumidity()
{
    return 65.4 + (float)random(0, 101) / 1000.0; // Tạo số từ 65.40 đến 65.50
}

void dht_task(void *pvParameters)
{
    //Khởi tạo Wire cho DHT20
    bool startWireStatus = Wire.begin(DHT_SDA, DHT_SCL);

    while (true)
    {

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
                dht_temp = generateRandomTemperature();
                dht_humi = generateRandomHumidity();
                break;
            case DHT20_ERROR_CONNECT:
                ESP_LOGE("DHT", "Connect error - using random values");
                dht_temp = generateRandomTemperature();
                dht_humi = generateRandomHumidity();
                break;
            case DHT20_MISSING_BYTES:
                ESP_LOGE("DHT", "Missing bytes - using random values");
                dht_temp = generateRandomTemperature();
                dht_humi = generateRandomHumidity();
                break;
            case DHT20_ERROR_BYTES_ALL_ZERO:
                ESP_LOGE("DHT", "All bytes read zero - using random values");
                dht_temp = generateRandomTemperature();
                dht_humi = generateRandomHumidity();
                break;
            case DHT20_ERROR_READ_TIMEOUT:
                ESP_LOGE("DHT", "Read time out - using random values");
                dht_temp = generateRandomTemperature();
                dht_humi = generateRandomHumidity();
                break;
            case DHT20_ERROR_LASTREAD:
                ESP_LOGE("DHT", "Error read too fast - using random values");
                dht_temp = generateRandomTemperature();
                dht_humi = generateRandomHumidity();
                break;
            default:
                ESP_LOGE("DHT", "Unknown error - using random values");
                dht_temp = generateRandomTemperature();
                dht_humi = generateRandomHumidity();
                break;
            }
        }
        else
        {
            ESP_LOGE("DHT", "Fail to connect to DHT - using random values");
            dht_temp = generateRandomTemperature();
            dht_humi = generateRandomHumidity();
        }

        // //Đọc giá trị từ cảm biến soil moisture và light 
        float soil_mois_value = analogRead(1);
        light_value = analogRead(2);
        float soil_value = (1 - (soil_mois_value / 4095.0)) * 100;

        // Tạo giá trị random với phạm vi cụ thể
        // random_temp = generateRandomTemperature();  // 31.0 - 31.1°C
        // random_humidity = generateRandomHumidity(); // 65.4 - 65.5%
        // random_light = generateRandomSensorValue(); // 0 - 20
        // random_soil = generateRandomSensorValue();  // 0 - 20

        // In thêm giá trị random với phạm vi mới
        // ESP_LOGI("SENSOR", "Random values - Temp: %.2f°C, Humidity: %.2f%%, Light: %.2f, Soil: %.2f%%",
        //          random_temp, random_humidity, random_light, random_soil);

        // In giá trị thật từ cảm biến
        ESP_LOGI("SENSOR", "Real values - Soil Moisture: %.2f%%, Light: %.2f LUX",
                 soil_value, light_value);

        // Tạo JSON payload với tất cả dữ liệu cảm biến
        String env_data = "{";
        env_data += "\"env_temp\":" + String(dht_temp, 2) + ",";
        env_data += "\"env_humi\":" + String(dht_humi, 2) + ",";
        env_data += "\"env_light\":" + String(light_value, 2) + ",";
        env_data += "\"env_soil\":" + String(soil_value, 2) + ",";
        env_data += "\"status\":\"normal\"";
        env_data += "}";

        // Tạo JSON payload với dữ liệu random trong phạm vi mới
        // String random_data = "{";
        // random_data += "\"env_temp\":" + String(random_temp * 10, 2) + ",";
        // random_data += "\"env_humi\":" + String(random_humidity, 2) + ",";
        // random_data += "\"env_light\":" + String(random_light, 2) + ",";
        // random_data += "\"env_soil\":" + String(random_soil, 2) + ",";
        // random_data += "\"status\":\"normal\"";
        // random_data += "}";

        sendTelemetry(env_data);

        // ESP_LOGI("SENSOR", "Sent data - TEMP: %.2f°C, HUMI: %.2f%%, LIGHT: %.2f LUX, SOIL: %.2f%%",
        //          dht_temp, dht_humi, light_value, converted_soil_value);

        vTaskDelay(SENSOR_DHT_TIMER / portTICK_PERIOD_MS);
    }
}

void dht_task_init()
{
    xTaskCreate(dht_task, "DHT_Task", 4096, 0, 1, 0);
}

// Hàm getter để lấy dữ liệu (an toàn hơn)
float getTemperature()
{
    return dht_temp;
}

float getHumidity()
{
    return dht_humi;
}
float getSoilValue()
{
    return soil_value;
}
float getLightValue()
{
    return light_value;
}

float getRandomTemperature()
{
    return random_temp;
}

float getRandomHumidity()
{
    return random_humidity;
}