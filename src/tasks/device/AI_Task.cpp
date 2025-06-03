#include "AI_Task.h"
#include <SocketIOclient.h>
#include <ArduinoJson.h>
#include <math.h>

const char *server_host = "192.168.1.15";
const uint16_t server_port = 5000;

SocketIOclient socketIO;
bool connected = false;

// Task handles để quản lý các luồng FreeRTOS
TaskHandle_t socket_init_handle = NULL;
TaskHandle_t socket_loop_handle = NULL;
TaskHandle_t sensor_data_handle = NULL;
bool ai_tasks_running = false;

float temperature, humidity;

int currentIndex = 0; // Gửi lần lượt từng dòng

// Cơ sở dữ liệu giả cho temperature và humidity
float temp_base = 30.0, temp_slope = 0.01;
float humi_base = 60.0, humi_slope = -0.005;

// ======= Sinh dữ liệu temperature và humidity giả =======
void generateSensorData(float *temperature, float *humidity)
{
  // Tạo dữ liệu nhiệt độ và độ ẩm với biến động tự nhiên
  *temperature = temp_base + temp_slope * currentIndex + sin(currentIndex * 0.1) * 2.0 + cos(currentIndex * 0.05) * 1.0;
  *humidity = humi_base + humi_slope * currentIndex + cos(currentIndex * 0.12) * 3.0 + sin(currentIndex * 0.08) * 2.0;

  // Giới hạn trong khoảng hợp lý
  *temperature = constrain(*temperature, 20.0, 35.0);
  *humidity = constrain(*humidity, 40.0, 80.0);

  currentIndex++;
}

// ======= Hàm gửi dữ liệu cảm biến thực =======
void AI_Task_SendSensorData()
{
  if (!connected)
  {
    Serial.println("⚠️ Chưa kết nối tới server!");
    return;
  }

  // Lấy dữ liệu cảm biến thực
  // Giả sử bạn có cảm biến nhiệt độ và độ ẩm
  // Nếu không có, bạn có thể map từ các cảm biến khác hoặc dùng dữ liệu giả

  // Phương án 1: Nếu có cảm biến nhiệt độ và độ ẩm thực
  temperature = getTemperature();
  humidity = getHumidity();

  // Phương án 2: Sử dụng dữ liệu giả (cho demo)
  // generateSensorData(&temperature, &humidity);

  // Phương án 3: Map từ các cảm biến hiện có (ví dụ)
  /*
  float temp_sensor = data.get_latest_data(MEASURE_TEMP);
  float ph_sensor = data.get_latest_data(MEASURE_PH);
  */

  // Tạo JSON array theo format server mong đợi: [temperature, humidity]
  DynamicJsonDocument doc(256);
  JsonArray arr = doc.to<JsonArray>();

  arr.add(temperature);
  arr.add(humidity);

  String payload;
  serializeJson(doc, payload);

  String message = "[\"sensor_data\"," + payload + "]";
  socketIO.sendEVENT(message);

  Serial.printf("✅ Đã gửi dữ liệu: Temp=%.2f°C, Humidity=%.2f%%\n",
                temperature, humidity);
}

// ======= Gửi dữ liệu test =======
void sendTestSensorData()
{
  if (!connected)
  {
    Serial.println("⚠️ Chưa kết nối tới server!");
    return;
  }

  float temperature, humidity;
  generateSensorData(&temperature, &humidity);

  DynamicJsonDocument doc(256);
  JsonArray arr = doc.to<JsonArray>();

  arr.add(temperature);
  arr.add(humidity);

  String payload;
  serializeJson(doc, payload);

  String message = "[\"sensor_data\"," + payload + "]";
  socketIO.sendEVENT(message);

  Serial.printf("✅ Test data #%d: Temp=%.2f°C, Humidity=%.2f%%\n",
                currentIndex, temperature, humidity);
}

// ======= Xử lý phản hồi từ server =======
void socketIOEvent(socketIOmessageType_t type, uint8_t *payload, size_t length)
{
  switch (type)
  {
  case sIOtype_DISCONNECT:
    Serial.println("🔌 Mất kết nối server!");
    connected = false;
    break;

  case sIOtype_CONNECT:
    Serial.printf("✅ Đã kết nối tới server: %s\n", payload);
    socketIO.send(sIOtype_CONNECT, "/");
    connected = true;
    break;

  case sIOtype_EVENT:
  {
    Serial.println("📨 Nhận phản hồi từ server:");
    Serial.printf("Raw payload: %.*s\n", length, (char *)payload);

    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, payload, length);

    if (!error)
    {
      JsonArray arr = doc.as<JsonArray>();
      String eventName = arr[0];

      Serial.printf("Event: %s\n", eventName.c_str());

      if (eventName == "prediction")
      {
        JsonObject pred = arr[1];

        // Server trả về predicted_temperature và predicted_humidity
        float pred_temp = pred["predicted_temperature"];
        float pred_humi = pred["predicted_humidity"];
        String pred_status = pred["predicted_status"];

        float curr_temp = pred["current_temperature"];
        float curr_humi = pred["current_humidity"];
        String curr_status = pred["current_status"];

        Serial.println("=== DỰ ĐOÁN AI ===");
        Serial.printf("🔮 Dự đoán: Temp=%.2f°C, Humidity=%.2f%%, Status=%s\n",
                      pred_temp, pred_humi, pred_status.c_str());
        Serial.printf("📊 Hiện tại: Temp=%.2f°C, Humidity=%.2f%%, Status=%s\n",
                      curr_temp, curr_humi, curr_status.c_str());
        Serial.println("==================");

        // Tạo JSON để gửi telemetry
        DynamicJsonDocument telemetryDoc(512);
        telemetryDoc["predicted_temperature"] = pred_temp;
        telemetryDoc["predicted_humidity"] = pred_humi;

        String jsonStr;
        serializeJson(telemetryDoc, jsonStr);

        // Gửi dữ liệu lên telemetry (nếu có hàm này)
        sendTelemetry(jsonStr);

        Serial.printf("📤 Telemetry data: %s\n", jsonStr.c_str());
      }
      else if (eventName == "sensor_data_received")
      {
        JsonObject data = arr[1];
        Serial.printf("✅ Server đã nhận: Temp=%.2f°C, Humidity=%.2f%%, Status=%s, Buffer=%d/6\n",
                      (float)data["temperature"], (float)data["humidity"],
                      data["status"].as<String>().c_str(), (int)data["buffer_size"]);
      }
      else if (eventName == "connect_response")
      {
        Serial.println("✅ Server xác nhận kết nối thành công");
      }
      else if (eventName == "error")
      {
        String errorMsg = arr[1]["message"];
        Serial.printf("❌ Server error: %s\n", errorMsg.c_str());
      }
    }
    else
    {
      Serial.printf("❌ JSON parse error: %s\n", error.c_str());
    }
    break;
  }

  case sIOtype_ERROR:
    Serial.printf("❌ SocketIO Error [%u bytes]: %.*s\n", length, length, (char *)payload);
    break;

  default:
    Serial.printf("⚙️ Sự kiện khác [%u bytes]\n", length);
    break;
  }
}

// ======= Task khởi tạo socket =======
void socket_init_task(void *pvParameters)
{
  Serial.println("🔄 Đang khởi tạo SocketIO connection...");
  socketIO.begin(server_host, server_port, "/socket.io/?EIO=4");
  socketIO.onEvent(socketIOEvent);
  Serial.println("✅ SocketIO đã được khởi tạo");

  vTaskDelete(NULL);
}

// ======= Task xử lý socket =======
void socket_loop_task(void *pvParameters)
{
  Serial.println("🔄 Socket loop task đã bắt đầu");

  while (true)
  {
    socketIO.loop();
    vTaskDelay(10 / portTICK_PERIOD_MS); // Delay nhỏ để tránh chiếm CPU
  }
}

// ======= Task gửi dữ liệu cảm biến =======
void sensor_data_task(void *pvParameters)
{
  const TickType_t xDelay = 3000 / portTICK_PERIOD_MS; // Gửi mỗi 3 giây
  Serial.println("🔄 Sensor data task đã bắt đầu");

  while (true)
  {
    if (connected)
    {
      // Gửi dữ liệu cảm biến thực
      AI_Task_SendSensorData();

      // Hoặc gửi dữ liệu test
      // sendTestSensorData();
    }
    else
    {
      Serial.println("⏳ Đang chờ kết nối tới server...");
    }

    vTaskDelay(xDelay);
  }
}

// ======= Khởi tạo AI task =======
void AI_Task_Init()
{
  // Kiểm tra nếu các task đã được khởi tạo trước đó
  if (ai_tasks_running)
  {
    Serial.println("⚠️ AI tasks đã đang chạy, không cần khởi tạo lại");
    return;
  }

  Serial.println("🚀 Đang khởi tạo AI tasks...");
  Serial.printf("🌐 Server: %s:%d\n", server_host, server_port);

  // Khởi tạo task và lưu handle để có thể dừng sau này
  xTaskCreate(socket_init_task, "Socket_Init_Task", 4096, NULL, 1, &socket_init_handle);
  xTaskCreate(socket_loop_task, "Socket_Loop_Task", 8192, NULL, 2, &socket_loop_handle);
  xTaskCreate(sensor_data_task, "Sensor_Data_Task", 4096, NULL, 1, &sensor_data_handle);

  ai_tasks_running = true;
  Serial.println("✅ Đã khởi tạo xong các AI tasks");
}

// ======= Dừng các AI task =======
void AI_Task_Stop()
{
  if (!ai_tasks_running)
  {
    Serial.println("⚠️ AI tasks không chạy, không cần dừng");
    return;
  }

  Serial.println("🛑 Đang dừng các AI tasks...");

  // Dừng kết nối socket trước khi xóa task
  if (connected)
  {
    socketIO.disconnect();
    connected = false;
    Serial.println("🔌 Đã ngắt kết nối SocketIO");
    // Đợi một chút để đảm bảo socket đã xử lý việc ngắt kết nối
    delay(100);
  }

  // Xóa các task nếu chúng tồn tại
  if (socket_init_handle != NULL)
  {
    vTaskDelete(socket_init_handle);
    socket_init_handle = NULL;
    Serial.println("✅ Đã dừng Socket_Init_Task");
  }

  if (socket_loop_handle != NULL)
  {
    vTaskDelete(socket_loop_handle);
    socket_loop_handle = NULL;
    Serial.println("✅ Đã dừng Socket_Loop_Task");
  }

  if (sensor_data_handle != NULL)
  {
    vTaskDelete(sensor_data_handle);
    sensor_data_handle = NULL;
    Serial.println("✅ Đã dừng Sensor_Data_Task");
  }

  ai_tasks_running = false;
  Serial.println("✅ Đã dừng tất cả AI tasks");
}

// ======= Hàm tiện ích để test =======
void AI_Task_SendTestData()
{
  if (connected)
  {
    sendTestSensorData();
  }
  else
  {
    Serial.println("⚠️ Chưa kết nối tới server, không thể gửi test data");
  }
}

// ======= Kiểm tra trạng thái kết nối =======
bool AI_Task_IsConnected()
{
  return connected;
}

// ======= Lấy thông tin server =======
void AI_Task_GetServerInfo()
{
  Serial.println("=== THÔNG TIN SERVER ===");
  Serial.printf("Host: %s\n", server_host);
  Serial.printf("Port: %d\n", server_port);
  Serial.printf("Connected: %s\n", connected ? "Yes" : "No");
  Serial.printf("Tasks running: %s\n", ai_tasks_running ? "Yes" : "No");
  Serial.println("========================");
}