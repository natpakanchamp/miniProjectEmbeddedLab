#include <ESP32Servo.h>

const int trigPin = 5;
const int echoPin = 18;
const int servoPin = 19;

Servo myServo;
long duration;
float distance;
bool isOpen = false;

void setup() {
  Serial.begin(115200);
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  
  ESP32PWM::allocateTimer(0);
  myServo.setPeriodHertz(50);
  myServo.attach(servoPin, 500, 2400); 
  
  myServo.write(0); 
  Serial.println("Fast Response Mode Ready");
}

void loop() {
  // 1. วัดระยะทาง (ปรับให้ส่งสัญญาณเร็วขึ้น)
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(5); // ลดลงจาก 10
  digitalWrite(trigPin, LOW);
  
  duration = pulseIn(echoPin, HIGH, 20000); // เพิ่ม Timeout เพื่อไม่ให้ค้างถ้าไม่เจอวัตถุ
  distance = duration * 0.034 / 2;
  
  // 2. เงื่อนไขการทำงาน
  if (distance > 0 && distance < 10 && !isOpen) {
    openLid();
  } 
  else if (distance > 20 && isOpen) {
    delay(3000);
    closeLid();
  }

  delay(50); // ลด Delay รวมเหลือ 50ms เพื่อให้ Loop วิ่งไวขึ้น
}

// ฟังก์ชันเปิดฝา (ปรับให้กระโดดทีละ 5 องศา และลด delay)
void openLid() {
  Serial.println("Opening...");
  for (int pos = 0; pos <= 90; pos += 5) { // เพิ่ม Step จาก 2 เป็น 5
    myServo.write(pos);
    delay(5); // ลด Delay จาก 10 เป็น 5
  }
  myServo.write(90); // มั่นใจว่าเปิดสุด
  isOpen = true;
}

// ฟังก์ชันปิดฝา (ปรับให้กระโดดทีละ 5 องศา และลด delay)
void closeLid() {
  Serial.println("Closing...");
  for (int pos = 90; pos >= 0; pos -= 5) { // เพิ่ม Step จาก 2 เป็น 5
    myServo.write(pos);
    delay(5); // ลด Delay จาก 15 เป็น 5
  }
  myServo.write(0); // มั่นใจว่าปิดสนิท
  isOpen = false;
}
