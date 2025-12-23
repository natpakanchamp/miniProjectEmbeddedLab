#include <Stepper.h>
#include <WiFi.h>
#include <MQTT.h>

// --- Hardware Objects ---
WiFiClient net;
MQTTClient client;
const int stepsPerRev = 2048;  
// ลำดับขา IN1, IN3, IN2, IN4 (19, 5, 18, 17)
Stepper myStepper(stepsPerRev, 19, 5, 18, 17); 

// ------ WiFi ------
const char ssid[] = "@JumboPlusIoT";
const char password[] = "rebplhzu";

// ----- MQTT Setting -----
const char mqtt_broker[] = "test.mosquitto.org";
const int mqtt_port = 1883; 
const char mqtt_client_id[] = "natpakan_bin_15099"; // แก้ ID ให้ unique
const char mqtt_topic_cmd[] = "group33/command";
const char mqtt_topic_status[] = "group33/status";

// ----- Setting PIN -----
const int trigPin = 23;
const int echoPin = 22;
const int ledPin = 21; 

// ----- Variable -----
bool isOpen = false;
unsigned long prevMillis = 0;
const long waitInterval = 3000; 
bool isWaitingToClose = false; 

// --- Sensor Settings ---
const int numReadings = 10;
float readings[numReadings];
int readIndex = 0;
float total = 0;
float averageDistance = 0;

// ----- Function MQTT Dashboard ----------------------------------------
void messageReceived(String &topic, String &payload){
  Serial.println("Incoming: "+topic+" - "+ payload);

  if(payload == "on"){
    if(!isOpen){
      Serial.println("Command: OPEN");
      digitalWrite(ledPin, HIGH);
      myStepper.step(stepsPerRev / 4);
      isOpen = true;
      isWaitingToClose = false;
      client.publish(mqtt_topic_status, "OPEN BY DASHBOARD");
    }
  }else if(payload == "off"){
    if(isOpen){
      Serial.println("Command: CLOSE");
      myStepper.step(-stepsPerRev / 4);
      digitalWrite(ledPin, LOW);
      isOpen = false;
      isWaitingToClose = false;
      client.publish(mqtt_topic_status, "CLOSE BY DASHBOARD");
    }
  }
}

// ----- Connect Function (รวม WiFi + MQTT) ----------------------------
void connect() {
  // 1. เช็ค WiFi
  Serial.print("Checking WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(1000);
  }
  Serial.println("\nWiFi Connected!");

  // 2. เช็ค MQTT
  Serial.print("Connecting MQTT...");
  // วนลูปเชื่อมต่อ MQTT
  while (!client.connect(mqtt_client_id)) {
    Serial.print(".");
    delay(1000);
  }
  Serial.println("\nMQTT Connected!");

  // Subscribe รอฟังคำสั่ง
  client.subscribe(mqtt_topic_cmd);
  client.publish(mqtt_topic_status, "SYSTEM READY");
}

// --------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);

  // Hardware Setup
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW); 
  myStepper.setSpeed(12);    
  
  for (int i = 0; i < numReadings; i++) readings[i] = 0;
  
  // WiFi Setup
  WiFi.begin(ssid, password);
  
  // *** MQTT Setup (ที่เคยหายไป ใส่กลับมาแล้วครับ) ***
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
    connect(); // แก้คำผิด connnect -> connect เรียบร้อย
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

  // --- 2. Logic Control ---
  
  // เปิด: ระยะเฉลี่ยน้อยกว่า 10 ซม.
  if (averageDistance > 0 && averageDistance < 10) {
    if(!isOpen){
      Serial.println("Action: AUTO OPEN");
      digitalWrite(ledPin, HIGH);      
      myStepper.step(stepsPerRev / 4); 
      isOpen = true;
      client.publish(mqtt_topic_status, "AUTO OPEN");
    }
    isWaitingToClose = false;
  } 
  
  // ปิด: ระยะเฉลี่ยมากกว่า 20 ซม.
  else if (averageDistance > 20 && isOpen) {
    if(!isWaitingToClose){
      prevMillis = currentMillis;
      isWaitingToClose = true;
      Serial.println("Closing in 3s...");
    }
    if(isWaitingToClose && (currentMillis - prevMillis >= waitInterval)){
      Serial.println("Action: AUTO CLOSE");
      myStepper.step(-stepsPerRev / 4); 
      digitalWrite(ledPin, LOW);       
      
      isOpen = false;
      isWaitingToClose = false;
      client.publish(mqtt_topic_status, "AUTO CLOSE");
    }
  }
  delay(10);
}
