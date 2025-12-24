#include <Stepper.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <MQTT.h>
#include <Ticker.h>

// --- Hardware Objects ---
WiFiClient net;
MQTTClient client;
WiFiMulti wifiMulti;
const int stepsPerRev = 2048;  
// ลำดับขา IN1, IN3, IN2, IN4 (19, 5, 18, 17)
Stepper myStepper(stepsPerRev, 19, 5, 18, 17); 
Ticker blinker;

// ------ WiFi ------
const char* WIFI_SSID_1 = "@JumboPlusIoT";
const char* WIFI_PASS_1 = "rebplhzu";

const char* WIFI_SSID_2 = "JumboPlus_DormIoT";
const char* WIFI_PASS_2 = "rebplhzu";

// ----- MQTT Setting -----
const char mqtt_broker[] = "test.mosquitto.org";
const int mqtt_port = 1883; 
const char mqtt_client_id[] = "natpakan_bin_15099"; // แก้ ID ให้ unique
const char mqtt_topic_cmd[] = "group33/command";
const char mqtt_topic_status[] = "group33/status";
const char mqtt_topic_distance[] = "group33/distance"; // ส่งค่าระยะทางไปที่ dashboard

// ----- Setting PIN -----
const int trigPin = 23;
const int echoPin = 22;
const int ledPin = 21;

const int RGB_RED_PIN = 25;      // R
const int RGB_GREEN_PIN = 26;    // G
const int RGB_BLUE_PIN = 27;     // B

// ----- Variable -----
bool isOpen = false;
unsigned long prevMillis = 0;
const long waitInterval = 3000; 
bool isWaitingToClose = false; 
unsigned long lastCountTime = 0;
unsigned long lastDistance = 0; // หน่วงเวลาค่า distance

// --- Sensor Settings ---
const int numReadings = 10;
float readings[numReadings];
int readIndex = 0;
float total = 0;
float averageDistance = 0;

// ------ Stop coil Function ----- (กันมอเตอร์ร้อน และช่วยประหยัดพลังงาน)
void stopCoils(){
  digitalWrite(19, LOW);
  digitalWrite(5, LOW);
  digitalWrite(18, LOW);
  digitalWrite(17, LOW);
}

// ----- Function MQTT Dashboard ----------------------------------------
void messageReceived(String &topic, String &payload){
  Serial.println("Incoming: "+topic+" - "+ payload);

  if(payload == "on"){
    if(!isOpen){
      Serial.println("Command: OPEN");
      digitalWrite(ledPin, HIGH);
      client.publish(mqtt_topic_status, "OPEN BY DASHBOARD");
      setColor(0, 0, 255);
      myStepper.step(stepsPerRev / 4);
      stopCoils();
      isOpen = true;
      isWaitingToClose = false;
    }
  }else if(payload == "off"){
    if(isOpen){
      Serial.println("Command: CLOSE");
      client.publish(mqtt_topic_status, "CLOSE BY DASHBOARD");
      myStepper.step((-stepsPerRev / 4) - 100); // motor จะหมุนมากกว่าปกติเล็กน้อย เพื่อให้มั่นใจว่าปิดสนิท
      stopCoils();
      setColor(0, 255, 0);
      digitalWrite(ledPin, LOW);
      isOpen = false;
      isWaitingToClose = false;
    }
  }
}

// ----- Connect Function (รวม WiFi + MQTT) ----------------------------
void connect() {
  blinker.attach(0.5, tick);
  // 1. เช็ค WiFi
  Serial.print("Checking WiFi...");
  setColor(255, 255, 0);
  while (wifiMulti.run() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("\nWiFi Connected!");
  Serial.print("Connected to: "); // บอกชื่อ WiFi ที่เชื่อมอยู่
  Serial.println(WiFi.SSID());

  // 2. เช็ค MQTT
  Serial.print("Connecting MQTT...");
  //setColor(255, 255, 0);
  // วนลูปเชื่อมต่อ MQTT
  while (!client.connect(mqtt_client_id)) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("\nMQTT Connected!");
  blinker.detach();
  setColor(0, 255, 0); // Green RGB

  // Subscribe รอฟังคำสั่ง
  client.subscribe(mqtt_topic_cmd);
  client.publish(mqtt_topic_status, "SYSTEM READY");
}

// ---------------- Set Colors ----------------------------------------------
  void setColor(int r, int g, int b){
    analogWrite(RGB_RED_PIN, r);
    analogWrite(RGB_GREEN_PIN, g);
    analogWrite(RGB_BLUE_PIN, b);
  }

  bool isLedOn = false;
  void tick() {
    if (isLedOn) {
      setColor(0, 0, 0); // ดับ
      isLedOn = false;
    } else {
      setColor(255, 255, 0); // ติด (สีเหลือง)
      isLedOn = true;
    }
  }

// --------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);

  // Hardware Setup
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  pinMode(ledPin, OUTPUT);

  //---RGB---
  pinMode(RGB_RED_PIN, OUTPUT);
  pinMode(RGB_GREEN_PIN, OUTPUT);
  pinMode(RGB_BLUE_PIN, OUTPUT);

  digitalWrite(ledPin, LOW); 
  myStepper.setSpeed(12);    
  
  for (int i = 0; i < numReadings; i++) readings[i] = 999; // แก้การทำงานในครั้งแรก
  total = 999 * numReadings;

  Serial.println("Booting System...");
  setColor(255, 255, 255);
  delay(3000);
  setColor(0, 0, 0);

  // WiFi Setup
  wifiMulti.addAP(WIFI_SSID_1, WIFI_PASS_1); // เพิ่มชื่อที่ 1
  wifiMulti.addAP(WIFI_SSID_2, WIFI_PASS_2); // เพิ่มชื่อที่ 2
  
  client.begin(mqtt_broker, mqtt_port, net);
  client.onMessage(messageReceived); // ผูกฟังก์ชัน callback

  // เรียกเชื่อมต่อครั้งแรก
  connect(); 
  
  Serial.println("System Ready: Stepper + Sensor + MQTT");
}

// ----------------------------------------------------------------------------------------
void loop() {
  // MQTT loop
  client.loop(); // หัวใจสำคัญ ห้ามเอาออก

  // ถ้าเน็ตหลุด ให้ต่อใหม่
  if(!client.connected()){
    setColor(255, 0, 0);
    connect();
  }

  // Millis()
  unsigned long currentMillis = millis();

  // --- 1. Sensor Processing ---
  total = total - readings[readIndex];
  
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  
  float duration = pulseIn(echoPin, HIGH, 30000);
  float currentDist = (duration * 0.034 / 2);
  
  // กรองค่า 0 หรือค่าโดด
  if (currentDist > 0 && currentDist < 400) readings[readIndex] = currentDist;
  else readings[readIndex] = total / numReadings; // ใช้ค่าเฉลี่ยเดิมแทนถ้าอ่านพลาด
  
  total = total + readings[readIndex];
  readIndex = (readIndex + 1) % numReadings;
  averageDistance = total / numReadings;

  if(currentMillis - lastDistance >= 500){
    lastDistance = currentMillis;
    // Convert float to String
    client.publish(mqtt_topic_distance, String(averageDistance));
  }

  // --- 2. Logic Control ---
  
  // เปิด: ระยะเฉลี่ยน้อยกว่า 10 ซม.
  // Auto Open
  if (averageDistance > 0 && averageDistance < 10) {
    if(!isOpen){
      Serial.println("Action: AUTO OPEN");
      digitalWrite(ledPin, HIGH); 
      client.publish(mqtt_topic_status, "AUTO OPEN"); // ส่งค่าไปที่ dashboard ก่อนเพื่อแก้ความหน่วงที่เกิดขึ้น   
      setColor(0, 0, 255); // RGB สีน้ำเงิน (เปิด)
      myStepper.step(stepsPerRev / 4); 
      stopCoils();
      isOpen = true;
    }
    if(isOpen){
      setColor(0, 0, 255); // Blue RGB
      isWaitingToClose = false;
    }
  } 
  
  // ปิด: ระยะเฉลี่ยมากกว่า 20 ซม.
  // Auto Close
  else if (averageDistance > 20 && isOpen) {
    if(!isWaitingToClose){
      prevMillis = currentMillis;
      isWaitingToClose = true;
      Serial.println("Waiting...");
      setColor(128, 0, 128); // RGB สีม่วง (เริ่มนับถอยหลัง)
    }
    if(isWaitingToClose){
      long remaining = waitInterval - (currentMillis - prevMillis);
      if(currentMillis - lastCountTime >= 1000){
        lastCountTime = currentMillis;
        int secondsLeft = ((remaining -1) / 1000)+1; // +1
        Serial.print("Closing in ");
        Serial.print(secondsLeft);
        Serial.println("s...");
      }
      if(remaining <= 0){
        Serial.println("Action: AUTO CLOSE");
        client.publish(mqtt_topic_status, "AUTO CLOSE");
        setColor(255, 0, 0); // Red RGB Closing...
        myStepper.step((-stepsPerRev / 4) -100); // motor จะหมุนมากกว่าปกติเล็กน้อยเพื่อให้มั่นใจว่าจะปิดสนิท 
        stopCoils();
        digitalWrite(ledPin, LOW);  
        setColor(0, 255, 0); // Green RGB (Ready)
        isOpen = false;
        isWaitingToClose = false;
      }
    }
  }
  delay(10);
}
