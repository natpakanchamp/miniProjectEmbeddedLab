#include <Stepper.h>

// --- ตั้งค่า Stepper (28BYJ-48) ---
const int stepsPerRev = 2048;  
// ลำดับขาต้องเป็น IN1, IN3, IN2, IN4 (19, 5, 18, 17) เพื่อให้หมุนสมูท
Stepper myStepper(stepsPerRev, 19, 5, 18, 17); 

// --- ขา Ultrasonic ---
const int trigPin = 23;
const int echoPin = 22;

bool isOpen = false;

// --- Hysteresis Settings (กรองสัญญาณรบกวน) ---
const int numReadings = 10;
float readings[numReadings];
int readIndex = 0;
float total = 0;
float averageDistance = 0;

void setup() {
  Serial.begin(115200);
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  
  myStepper.setSpeed(12); // ปรับความเร็วให้ไวขึ้นเล็กน้อย (12-15 RPM)
  
  // ล้างค่า Array
  for (int i = 0; i < numReadings; i++) readings[i] = 0;
  
  Serial.println("Stepper Trash Can: 1s Delay Mode Ready");
}

void loop() {
  // --- 1. การอ่านค่าเซนเซอร์พร้อม Hysteresis (Moving Average) ---
  total = total - readings[readIndex];
  
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  
  float duration = pulseIn(echoPin, HIGH, 30000);
  float currentDist = duration * 0.034 / 2;
  
  if (currentDist > 0 && currentDist < 400) {
    readings[readIndex] = currentDist;
  }
  
  total = total + readings[readIndex];
  readIndex = (readIndex + 1) % numReadings;
  averageDistance = total / numReadings;

  // --- 2. Logic การทำงาน ---
  
  // เปิด: ระยะเฉลี่ยน้อยกว่า 10 ซม.
  if (averageDistance > 0 && averageDistance < 10 && !isOpen) {
    Serial.println("Action: OPENING");
    myStepper.step(stepsPerRev / 4); // หมุนเปิด 90 องศา
    isOpen = true;
  } 
  
  // ปิด: ระยะเฉลี่ยมากกว่า 20 ซม.
  else if (averageDistance > 20 && isOpen) {
    Serial.println("Status: Waiting 3 second...");
    delay(3000); // *** ลดเหลือ 1 วินาทีตามสั่ง ***
    
    Serial.println("Action: CLOSING");
    myStepper.step(-stepsPerRev / 4); // หมุนกลับ 90 องศา
    isOpen = false;
  }

  delay(20); 
}
