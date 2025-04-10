#include <SPI.h>
#include <WiFiEsp.h>
#include <SoftwareSerial.h>
#include <MFRC522.h>
#include <Servo.h>

#define AP_SSID "embA"
#define AP_PASS "embA1234"
#define SERVER_NAME "10.10.141.80"
#define SERVER_PORT 5000  
#define LOGID "CLOSET_ARD"

#define CMD_SIZE 50
#define RST_PIN  9
#define SS_PIN   10
#define SERVO_PIN 3
#define BUZZER_PIN 2
#define WIFIRX 6
#define WIFITX 7 

bool servoState = false; 
byte allowedCards[2][4] = {
    {0x79, 0x73, 0xA6, 0x98},  
    {0x63, 0x4E, 0x8A, 0xF5}   
};


typedef struct {
  char sendId[10];
  char getSensorId[10];
  char sendBuf[CMD_SIZE];
} CommunicationData;

Servo myservo;
MFRC522 mfrc522(SS_PIN, RST_PIN);

CommunicationData commData = {"Outside_ARD", "", ""};

SoftwareSerial wifiSerial(WIFIRX, WIFITX);
WiFiEspClient client;

void wifi_Setup();
void wifi_Init();
int server_Connect();
void printWifiStatus();

void setup() {
    Serial.begin(9600);
    wifi_Setup();
    SPI.begin();
    mfrc522.PCD_Init();
    myservo.attach(SERVO_PIN);
    myservo.write(0);
    pinMode(BUZZER_PIN, OUTPUT);

    Serial.println(F("Waiting for registered RFID card..."));
}

bool isAuthorizedCard() {
    for (int i = 0; i < 2; i++) {
        bool match = true;
        for (int j = 0; j < 4; j++) {
            if (mfrc522.uid.uidByte[j] != allowedCards[i][j]) {
                match = false;
                break;
            }
        }
        if (match) return true; 
    }
    return false; 
}


void loop() {
    if (!mfrc522.PICC_IsNewCardPresent()) {
        Serial.println(F("No card detected"));
        delay(500);
    } else if (!mfrc522.PICC_ReadCardSerial()) {
        Serial.println(F("Card detected, but UID reading failed"));
        delay(500);
    } else {
        Serial.print(F("Card UID: "));
        for (byte i = 0; i < mfrc522.uid.size; i++) {
            Serial.print(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ");
            Serial.print(mfrc522.uid.uidByte[i], HEX);
        }
        Serial.println();

        if (isAuthorizedCard()) {
            tone(BUZZER_PIN, 1000);
            delay(200);  
            noTone(BUZZER_PIN); 

            servoState = !servoState;
            if (servoState) {
                Serial.println(F("Authorized card detected! Servo ON (90 degrees)"));
                myservo.write(90); 
            } else {
                Serial.println(F("Authorized card detected! Servo OFF (0 degrees)"));
                myservo.write(0); 
            }
        } else {
            Serial.println(F("Unauthorized card detected. Access denied."));
            for (int i = 0; i < 2; i++) {
                tone(BUZZER_PIN, 1000); 
                delay(100); 
                noTone(BUZZER_PIN); 
                delay(100);  
            }
        }

        mfrc522.PICC_HaltA();
        mfrc522.PCD_StopCrypto1();
        delay(1000);
    }

    receiveServerMessage();
}


void receiveServerMessage() {
    if (client.available()) {
        String receivedMsg = client.readStringUntil('\n');
        receivedMsg.trim(); 

        Serial.print("Received from server: ");
        Serial.println(receivedMsg);

        if (receivedMsg == "[SERVER_AND]]DOOR@UN") {
            Serial.println("Lock command received, moving servo.");
            myservo.write(90);
            servoState = false;
            tone(BUZZER_PIN, 1000);
            delay(200);  
            noTone(BUZZER_PIN);
        }
        else if(receivedMsg == "[SERVER_AND]]DOOR@LOCK") {
            Serial.println("UNLock command received, moving servo.");
            myservo.write(0);
            servoState = true;
            tone(BUZZER_PIN, 1000);
            delay(200);  
            noTone(BUZZER_PIN);
        }
    }
}

// WiFi 설정
void wifi_Setup() {
  wifiSerial.begin(38400);
  wifi_Init();
  server_Connect();
}

// WiFi 초기화
void wifi_Init() {
  do {
    WiFi.init(&wifiSerial);
    if (WiFi.status() == WL_NO_SHIELD) {
      Serial.println("WiFi shield not present");
    }
    else
      break;
  } while (1);

  Serial.print("Attempting to connect to WPA SSID: ");
  Serial.println(AP_SSID);
  
  while (WiFi.begin(AP_SSID, AP_PASS) != WL_CONNECTED) {
    Serial.print("Attempting to connect to WPA SSID: ");
    Serial.println(AP_SSID);
  }
  
  Serial.println("You're connected to the network");
  printWifiStatus();
}

// 서버 연결
int server_Connect() {
  Serial.println("Starting connection to server...");

  if (client.connect(SERVER_NAME, SERVER_PORT)) {
    Serial.println("Connect to server");
    client.print("["LOGID"]");
    return 1;
  }
  else {
    Serial.println("server connection failure");
    return 0;
  }
}

// WiFi 상태 출력
void printWifiStatus() {
  // SSID 출력
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // IP 주소 출력
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // 신호 강도 출력
  long rssi = WiFi.RSSI();
  Serial.print("Signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
}