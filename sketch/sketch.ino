#define Q4_OUT1 5 // Replace with the actual GPIO pin for Channel 1

void setup() {
  pinMode(Q4_OUT1, OUTPUT);
}

void loop() {
  digitalWrite(Q4_OUT1, HIGH); // Turns Output 1 ON
  delay(1000);
  digitalWrite(Q4_OUT1, LOW);  // Turns Output 1 OFF
  delay(1000);
}