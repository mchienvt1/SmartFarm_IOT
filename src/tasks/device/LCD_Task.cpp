#include "LCD_Task.h"
#include "DHT_Task.h"

LiquidCrystal_I2C lcd(0x27,16,2);

void LCDTask(void *pvParameters) {
  while(1) {
    lcd.init();
    lcd.clear();
    lcd.backlight();
    float temperature = getTemperature();
    float humidity = getHumidity();
    int light = getLightValue();
    float soil_moisture = getSoilValue();
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("RT:");
    lcd.setCursor(3,0);
    lcd.print(temperature);
    lcd.setCursor(7,0);
    lcd.print("*C");
    lcd.setCursor(10,0);
    lcd.print("RH:");
    lcd.setCursor(13,0);
    lcd.print(humidity);
    lcd.setCursor(15,0);
    lcd.print("%");
    lcd.setCursor(0,1);
    lcd.print("LUX:");
    lcd.setCursor(4,1);
    lcd.print(light);
    lcd.setCursor(10,1);
    lcd.print("SM:");
    lcd.setCursor(13,1);
    lcd.print(soil_moisture);
    lcd.setCursor(15,1);
    lcd.print("%");
    vTaskDelay(SENSOR_DHT_TIMER / portTICK_PERIOD_MS);
  }
}

void lcd_task_init(){
    Wire.begin(DHT_SDA, DHT_SCL);
    xTaskCreate(LCDTask, "LCDTask", 4096, 0, 1, 0);
}