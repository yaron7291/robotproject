
#include <Arduino.h>
#include <WiFi.h>
#include <NetworkUdp.h>
#include "mbedtls/md.h"
#include "mbedtls/aes.h"

NetworkUDP udp;
unsigned int udpPort = 4210;
const unsigned long p = 104729;
const unsigned long g = 5;
unsigned long mySecret_a;
unsigned long myPublic_A;
unsigned long sharedKey_K;
bool keyComputed = false;
IPAddress raspberryPiIP;
unsigned char aes_key[16];
String msg = "";
unsigned long power_mod(unsigned long base, unsigned long exp, unsigned long mod) {
    unsigned long long res = 1;
    unsigned long long b = base % mod;

    while (exp > 0) {
        if (exp % 2 == 1) {
            res = (res * b) % mod;
        }
        b = (b * b) % mod;
        exp = exp / 2;
    }
    return (unsigned long)res;
}

void derive_aes_key(unsigned long key, unsigned char* output_key) {
    String key_str = String(key);
    unsigned char hash[32];
    mbedtls_md_context_t ctx;
    mbedtls_md_type_t md_type = MBEDTLS_MD_SHA256;
    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type), 0);
    mbedtls_md_starts(&ctx);
    mbedtls_md_update(&ctx, (const unsigned char*)key_str.c_str(), key_str.length());
    mbedtls_md_finish(&ctx, hash);
    mbedtls_md_free(&ctx);
    memcpy(output_key, hash, 16);
}
void decrypt_aes_ecb(unsigned char* encrypted_input, char* output_str, unsigned char* key) {
    unsigned char decrypted_raw[16];

    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    mbedtls_aes_setkey_dec(&aes, key, 128);
    mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_DECRYPT, encrypted_input, decrypted_raw);
    mbedtls_aes_free(&aes);

    // העתקה למחרוזת וסגירתה
    memcpy(output_str, decrypted_raw, 16);
    output_str[16] = '\0';
}
unsigned long performDiffieHellman(unsigned long p, unsigned long g) {
    while (udp.parsePacket() > 0) {
        udp.flush();
    }
    mySecret_a = random(1000, 5000);
    myPublic_A = power_mod(g, mySecret_a, p);
    Serial.print("Generated Public A: "); Serial.println(myPublic_A);
    udp.beginPacket(IPAddress(255,255,255,255), udpPort);
    udp.print("ROBOT_START:" + String(myPublic_A));
    udp.endPacket();
    Serial.println("Broadcasting: ROBOT_START with Public A");
    for (int i = 0; i < 30; i++) {
        delay(200);

        int packetSize = udp.parsePacket();
        if (packetSize > 0){
            char buffer[32];
            int len = udp.read(buffer, sizeof(buffer) - 1);
            buffer[len] = '\0';
            unsigned long public_B = strtoul(buffer, NULL, 10);
            if (public_B == 0) continue;

            raspberryPiIP = udp.remoteIP();
            Serial.print("Received Public B from Pi: "); Serial.println(public_B);
            return power_mod(public_B, mySecret_a, p);
        }
    }
    return 0;
}
void encrypt_aes_ecb(const char* input_str, unsigned char* output_encrypted, unsigned char* key) {
    unsigned char padded_input[16];
    memset(padded_input, ' ', 16); // ריפוד ברווחים עד גודל 16 בייט

    int len = strlen(input_str);
    if (len > 16) len = 16;
    memcpy(padded_input, input_str, len);

    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    mbedtls_aes_setkey_enc(&aes, key, 128);
    mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_ENCRYPT, padded_input, output_encrypted);
    mbedtls_aes_free(&aes);
}

#define SENSOR_LEFT  13
#define SENSOR_RIGHT 14

#define leftup 25
#define leftdown 26
#define rightup 32
#define rightdown 33
unsigned int leftupvalue = 0;
unsigned int leftdownvalue = 0;
unsigned int rightupvalue = 0;
unsigned int rightdownvalue = 0;
unsigned int whiteLineCountleft = 0;
unsigned int whiteLineCountright = 0;
bool lastleft = false;
bool lastright = false;
const float WHEEL_DIAMETER = 79.0;
const float PI_VAL = 3.14159;
const int TOTAL_WHITE_LINES = 6;
const int whiteLineThreshold = 700;
unsigned long readSensor(int sensorPin) {
pinMode(sensorPin, OUTPUT);
digitalWrite(sensorPin, HIGH);
delayMicroseconds(10);

pinMode(sensorPin, INPUT);

unsigned long start = micros();
while (digitalRead(sensorPin) == HIGH) {
if (micros() - start > 3000) break;
}

return micros() - start;
}



const float DISTANCE_PER_LINE = (WHEEL_DIAMETER * PI_VAL) / TOTAL_WHITE_LINES;
const float timeinterval = 2000;

unsigned long lastCheckTime = 0;
float currentSpeedleft = 0.0;
float currentSpeedright = 0.0;
float targetSpeedleft = 200;
float targetSpeedright = 200;

void sendmsg(String msg) {
    unsigned char encrypted_output[16];
    encrypt_aes_ecb(msg.c_str(), encrypted_output, aes_key);
    udp.beginPacket(raspberryPiIP, udpPort);
    udp.write(encrypted_output, 16);
    udp.endPacket();
}

void CheckSpeed()
{
    const int minPWM = 150;
  // === מנוע שמאלי ===
  if(targetSpeedleft > 0)
  {
    leftdownvalue = 0;
    if (targetSpeedleft > currentSpeedleft){
      if (targetSpeedleft - currentSpeedleft > 50) leftupvalue += 10;
      else leftupvalue += 2;
    }
    else{
      if (currentSpeedleft - targetSpeedleft > 50) leftupvalue -= 10;
      else leftupvalue -= 2;
    }
  }
  else
  {
    leftupvalue = 0;
    if (targetSpeedleft < currentSpeedleft){
      // targetSpeedleft שלילי, אז פחות targetSpeedleft זה בעצם פלוס (חיובי)
      if (currentSpeedleft - targetSpeedleft > 50) leftdownvalue -= 10;
      else leftdownvalue -= 2;
    }
    else{
      if (targetSpeedleft - currentSpeedleft > 50) leftdownvalue += 10;
      else leftdownvalue += 2;
    }
  }

  // === מנוע ימני ===
  if(targetSpeedright > 0)
  {
    rightdownvalue = 0;
    if (targetSpeedright > currentSpeedright){
      if (targetSpeedright - currentSpeedright > 50) rightupvalue += 10;
      else rightupvalue += 2;
    }
    else{
      if (currentSpeedright - targetSpeedright > 50) rightupvalue -= 10;
      else rightupvalue -= 2;
    }
  }
  else
  {
    rightupvalue = 0;
    if (targetSpeedright < currentSpeedright){
      // תיקון חיסור מספר שלילי עבור מנוע ימין
      if (currentSpeedright - targetSpeedright > 50) rightdownvalue -= 10;
      else rightdownvalue -= 2;
    }
    else{
      if (targetSpeedright - currentSpeedright > 50) rightdownvalue += 10;
      else rightdownvalue += 2;
    }
  }

// הגנה על גבולות ה-PWM (חסימה מדויקת בין minPWM ל-255)
  if (leftupvalue > 0)    leftupvalue    = constrain(leftupvalue, (unsigned int)minPWM, 255U);
  if (leftdownvalue > 0)  leftdownvalue  = constrain(leftdownvalue, (unsigned int)minPWM, 255U);
  if (rightupvalue > 0)   rightupvalue   = constrain(rightupvalue, (unsigned int)minPWM, 255U);
  if (rightdownvalue > 0) rightdownvalue = constrain(rightdownvalue, (unsigned int)minPWM, 255U);

  // כתיבת הערכים למנועים
  analogWrite(leftup, leftupvalue);
  analogWrite(leftdown, leftdownvalue);
  analogWrite(rightup, rightupvalue);
  analogWrite(rightdown, rightdownvalue);
}

void stopmoving(){
    analogWrite(leftdown,0);
    analogWrite(leftup,0);
    analogWrite(rightdown,0);
    analogWrite(rightup,0);
}

void setup() {
  Serial.begin(115200);
  pinMode(leftup, OUTPUT);
  pinMode(leftdown, OUTPUT);
  pinMode(rightup, OUTPUT);
  pinMode(rightdown, OUTPUT);
  WiFi.begin("Zvia", "0529217667");
  while (WiFi.status() != WL_CONNECTED) {
      delay(1000);
      Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi!");
  Serial.print("My IP address: "); Serial.println(WiFi.localIP());
  udp.begin(udpPort);
}


void loop(){

  if (!keyComputed) {
      sharedKey_K = performDiffieHellman(p, g);

      if (sharedKey_K > 0) {
          Serial.print("🔑 Done! Shared Key K: "); Serial.println(sharedKey_K);
          derive_aes_key(sharedKey_K, aes_key);
          Serial.println("AES Key Derived Successfully!");

          keyComputed = true;
      }
      delay(1000);
  }
  else{
    int packetSize = udp.parsePacket();
    if (packetSize > 0) {
        unsigned char encrypted_input[16];
        char decrypted_string[17];
        udp.read(encrypted_input, 16);
        decrypt_aes_ecb(encrypted_input, decrypted_string, aes_key);
        String command = String(decrypted_string);
        command.trim();
        Serial.print("🔒 Decrypted Command: ["); Serial.print(command); Serial.println("]");
        if (command == "rotate"){

            analogWrite(leftup,0);
            analogWrite(rightup,0);
            analogWrite(leftdown, 255);
            analogWrite(rightdown,255);
            delay(500);
            analogWrite(rightup,255);
            analogWrite(rightdown,0);
            delay(1000);
            stopmoving();
            msg = "rotated";
            sendmsg(msg);
            command = "";
        }
        if(command == "stop"){
          stopmoving();
          sendmsg( "stoped");

        }
        if(command == "slow down"){

            targetSpeedleft = 100;
            targetSpeedright = 100;
            msg = "slowed";
            sendmsg(msg);
            command = "";
        }
        if(command == "good"){
          targetSpeedleft = 200;
            targetSpeedright = 200;
            msg = "staygood";
            sendmsg("staygood");
            command = "";
        }


      }
      unsigned long currentTime = millis();
      unsigned long leftValue = readSensor(SENSOR_LEFT);
      unsigned long rightValue = readSensor(SENSOR_RIGHT);
      bool nowleft = (leftValue < whiteLineThreshold);
      bool nowright = (rightValue < whiteLineThreshold);
      if (currentTime - lastCheckTime <= timeinterval)
      {

        if (nowleft && !lastleft)whiteLineCountleft++;
        if (nowright && !lastright)whiteLineCountright++;

        lastleft = nowleft;
        lastright = nowright;

      }
      else
      {
        currentSpeedleft = (whiteLineCountleft * DISTANCE_PER_LINE) / (timeinterval / 1000.0);
        currentSpeedright = (whiteLineCountright * DISTANCE_PER_LINE) / (timeinterval / 1000.0);
        CheckSpeed();
        Serial.print("Speed Left: ");
        Serial.print(currentSpeedleft);
        Serial.print(" mm/s | PWM Value Left: ");
        if (targetSpeedleft >= 0) Serial.println(leftupvalue);
        else Serial.println(leftdownvalue);
        Serial.print("Speed Right: ");
        Serial.print(currentSpeedright);
        Serial.print(" mm/s | PWM Value Right: ");
        if (targetSpeedright >= 0) Serial.println(rightupvalue);
        else Serial.println(rightdownvalue);

        whiteLineCountleft = 0;
        whiteLineCountright = 0;
        lastCheckTime = currentTime;
        }
    }
}


