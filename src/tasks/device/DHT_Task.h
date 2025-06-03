#ifndef DHT_TASK_
#define DHT_TASK_

#include "global.h"

void dht_task_init();
void dht_task(void *pvParameters);

// Hàm để lấy dữ liệu DHT an toàn
float getTemperature();

float getHumidity();

float getRandomTemperature();

float getRandomHumidity();

#endif
