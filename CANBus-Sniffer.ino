// requires https://github.com/collin80/esp32_can and https://github.com/collin80/can_common

#include <esp32_can.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <WiFi.h>
#include <cmath>
#include <stdio.h>
#include <iostream>
#include "secrets.h"
#include "GaugeMinimal.h"

// Pin definitions
#define PIN_BATTERY 32

//uint8_t SpeedoAddress[] = SPEEDO_MAC_ADDR; // Speedo screen MAC Address
uint8_t LevelsAddress[] = LEVELS_MAC_ADDR; // Levels screen MAC Address

//int send_speedo_interval = 100;  // 10 times per second
int send_levels_interval = 500; // every 500 ms

esp_now_peer_info_t peerInfo;

typedef struct struct_speedo {
  uint8_t flag;
  uint8_t speed_mph;
  uint8_t speed_kmph;
}struct_speedo;

typedef struct struct_levels {
  uint8_t flag;
  bool car_started;
  uint8_t water_temp;
} struct_levels;

struct_speedo SpeedoData;
struct_levels LevelsData;

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Send Success" : "Send Failed");
}

void OnDataRecv(const uint8_t *mac_addr, const uint8_t *incomingData, int len) {
  if (len == 1) {
    uint8_t new_channel = incomingData[0];
    esp_wifi_set_channel(new_channel, WIFI_SECOND_CHAN_NONE);
    Serial.printf("Channel updated to: %d\n", new_channel);
  }
}

// void send_speedo(void *parameter) {
//   while(true) {
//     // no await reply
//     esp_now_send(SpeedoAddress, (uint8_t *)&SpeedoData, sizeof(SpeedoData));
//     vTaskDelay(send_speedo_interval / portTICK_PERIOD_MS);
//   }
// }

// float step_up = 4.704; // change from 3.3 max to 15.5 max

// float ReadVoltage(){

//   double reading = analogRead(PIN_BATTERY);
//   if(reading < 1 || reading > 4095) return 0;

//   // Improved polynomial conversion - https://github.com/G6EJD/ESP32-ADC-Accuracy-Improvement-function
//   double raw_value = -0.000000000000016 * pow(reading,4) + 0.000000000118171 * pow(reading,3)- 0.000000301211691 * pow(reading,2)+ 0.001109019271794 * reading + 0.034143524634089;
//   double full_value = raw_value * step_up; // convert to battery value (max 15.5V)
//   return std::round(full_value * 10) / 10; // convert to 1 decimal place and return
// }

void send_levels(void *parameter) {
  while(true) {
    // no await reply
    //LevelsData.battery_volts = ReadVoltage();

    esp_now_send(LevelsAddress, (uint8_t *)&LevelsData, sizeof(LevelsData));
    vTaskDelay(send_levels_interval / portTICK_PERIOD_MS); 
  }
}

void init_wifi(void) {
  // Set device as a Wi-Fi Station
    WiFi.mode(WIFI_STA);
    if (esp_now_init() != ESP_OK) {
        Serial.println("ESP-NOW Init Failed");
        return;
    }
    
    esp_now_register_send_cb(OnDataSent);
    esp_now_register_recv_cb(esp_now_recv_cb_t(OnDataRecv));

    // Add speedo peer
    peerInfo.channel = 0; // Default channel
    peerInfo.encrypt = false;

    // memcpy(peerInfo.peer_addr, SpeedoAddress, 6);
    // if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    //     Serial.println("Failed to add peer");
    //     return;
    // }

    // Add analysis peer
    memcpy(peerInfo.peer_addr, LevelsAddress, 6);
    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("Failed to add peer");
        return;
    }

    Serial.println("ESP-NOW Initialized");
}

void init_canbus(void) {
    CAN0.setCANPins(GPIO_NUM_4, GPIO_NUM_5); // CANBus pins
    CAN0.begin(500000); // 500Kbps
    CAN0.watchFor();
}

uint16_t process_endian(uint8_t *byte_msb, uint8_t *byte_lsb) {
  return (*byte_msb << 8) | *byte_lsb;
}

void process_power(uint8_t *byte_data) {
  // Power
  // PID:   0x23D 
  // Byte:  1
  // Bits:  ACC 1 | ON 2
  static int POWER_ACC_BIT = 1;
  static int POWER_ON_BIT = 2;

  uint8_t byte1 = byte_data[1];

  uint8_t is_acc_on = (byte1 >> POWER_ACC_BIT) & 0x01;
  uint8_t is_power_on = (byte1 >> POWER_ON_BIT) & 0x01;

  Serial.print("is acc on is ");
  Serial.println((is_acc_on) ? "true" : "false");
  Serial.print("is power on is ");
  Serial.println((is_power_on) ? "true" : "false");
  

}

void process_speed(uint8_t *byte_data) {
  // SPEED IN KPH
  // PID 0x2D1 
  // MSB 2 | LSB 1
  static int SPEED_MSB = 0;
  static int SPEED_LSB = 1;
  static float scale = 0.01; // scale from CANBus values
  static float safety_buffer = 1.02; // 2% saftey buffer
  static float mph_ratio = 0.621; // kmph to mph

  static float conversion_ratio = scale * safety_buffer;

  uint16_t raw_value = process_endian(&byte_data[SPEED_MSB], &byte_data[SPEED_LSB]);
  float raw_kmph = raw_value * conversion_ratio;

  SpeedoData.speed_mph = std::round(raw_kmph * mph_ratio);
  SpeedoData.speed_kmph = std::round(raw_kmph);
}

void process_rpm(uint8_t *byte_data) {
  // RPM
  // PID 0x23D 
  // MSB 4 | LSB 3
  static int SPEED_MSB = 4;
  static int SPEED_LSB = 3;
  static int SCALING_FACTOR = 3;

  uint16_t endian_value = process_endian(&byte_data[SPEED_MSB], &byte_data[SPEED_LSB]);
  uint16_t rpm_int = (int)(endian_value * SCALING_FACTOR);

  LevelsData.car_started = (rpm_int > 0);

  // SpeedoData.rpm = rpm_int;
}

void process_coolant_temp(uint8_t *byte_data) {
  // Coolant temp in C
  // PID 0x551 
  // B 0
  // NISSAN MODIFIER VALUE -40
  static int BYTE = 0;
  LevelsData.water_temp = byte_data[BYTE] - 40;

  Serial.print("Water Temp: ");
  Serial.print(LevelsData.water_temp);
}

void setup() {
    Serial.begin(115200);

    init_wifi();
    init_canbus();

    LevelsData.flag = FLAG_CANBUS; // define sender ID

    xTaskCreate(send_levels, "SendLevels", 4096, NULL, 1, NULL);
    //xTaskCreate(send_speedo, "SendSpeedo", 4096, NULL, 2, NULL);
}


void loop() {
    CAN_FRAME can_message;

    if (CAN0.read(can_message)) {     
      switch (can_message.id) {
        case 0x60D:
          process_power(can_message.data.byte);
          break;
        case 0x354:
          process_speed(can_message.data.byte);
          break;
        case 0x23D:
          process_rpm(can_message.data.byte);
          break;
        case 0x551:
          process_coolant_temp(can_message.data.byte);
          break;
        default:
          break;
      }
    }
}