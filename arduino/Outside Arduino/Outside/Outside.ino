#include <WiFiEsp.h>
#include <SoftwareSerial.h>
#include <DHT.h>

#define AP_SSID "embA"
#define AP_PASS "embA1234"
#define SERVER_NAME "10.10.141.80"
#define SERVER_PORT 5000  
#define LOGID "OUTSIDE_ARD"

#define CDS_PIN A0
#define DHTPIN 4
#define WIFIRX 6  //6:RX-->ESP8266 TX
#define WIFITX 7  //7:TX -->ESP8266 RX

#define CMD_SIZE 50
#define ARR_CNT 5
#define DHTTYPE DHT11
#define SENSOR_INTERVAL 10000  // 10초 간격으로 센서 데이터 전송

typedef struct {
  int cds;            // 조도 센서 값 (0-100)
  float temperature;  // 온도 (섭씨)
  float humidity;     // 습도 (%)
  bool cdsFlag;       // 조도 센서 상태 플래그
} SensorData;

// 통신 관련 구조체 정의
typedef struct {
  char sendId[10];     // 송신 ID
  char getSensorId[10]; // 센서 데이터 요청 ID
  int sensorTime;      // 센서 데이터 전송 주기
  char sendBuf[CMD_SIZE]; // 전송 버퍼
} CommunicationData;

// 전역 변수로 구조체 선언
SensorData sensorData = {0, 0.0, 0.0, false};
CommunicationData commData = {"Outside_ARD", "", 10, ""};  // sensorTime을 10으로 초기화

DHT dht(DHTPIN, DHTTYPE);
SoftwareSerial wifiSerial(WIFIRX, WIFITX);
WiFiEspClient client;

// 함수 선언
void readSensorData();
void socketEvent();
void wifi_Setup();
void wifi_Init();
int server_Connect();
void printWifiStatus();

void setup() {
  pinMode(CDS_PIN, INPUT);
  
  Serial.begin(115200);
  
  wifi_Setup();
  dht.begin();
}

void loop() {
  if (client.available()) {
    socketEvent();
  }

  static unsigned long lastReadTime = 0;
  static unsigned long lastSendTime = 0;
  unsigned long currentTime = millis();
  
  if (currentTime - lastReadTime >= 10000) {
    lastReadTime = currentTime;
    readSensorData();
    
    // 서버 연결 확인
    if (!client.connected()) {
      server_Connect();
    }
  }
  
  // 10초마다 센서 데이터 전송
  if (currentTime - lastSendTime >= SENSOR_INTERVAL) {
    lastSendTime = currentTime;
    
    // SENSOR 데이터 전송 형식 변경
    sprintf(commData.sendBuf, "[SERVER_SQL]SENSOR@%d@%d@%d\r\n", 
            sensorData.cds, 
            (int)sensorData.temperature, 
            (int)sensorData.humidity);
            
    client.write(commData.sendBuf, strlen(commData.sendBuf));
    client.flush();
    
    Serial.print("전송 데이터: ");
    Serial.print(commData.sendBuf);
  }
}

// 센서 데이터를 읽는 함수
void readSensorData() {
  // 조도 센서 읽기
  sensorData.cds = map(analogRead(CDS_PIN), 400, 1000, 0, 100);
  
  // 온습도 센서 읽기
  sensorData.humidity = dht.readHumidity();
  sensorData.temperature = dht.readTemperature();
  
  // 디버깅 출력
  Serial.print("조도: ");
  Serial.print(sensorData.cds);
  Serial.print("%, 온도: ");
  Serial.print(sensorData.temperature);
  Serial.print("°C, 습도: ");
  Serial.print(sensorData.humidity);
  Serial.println("%");
}

// 서버 소켓 이벤트 처리
void socketEvent() {
  int i = 0;
  char * pToken;
  char * pArray[ARR_CNT] = {0};
  char recvBuf[CMD_SIZE] = {0};
  int len;

  commData.sendBuf[0] = '\0';
  len = client.readBytesUntil('\n', recvBuf, CMD_SIZE);
  client.flush();
  
  Serial.print("recv : ");
  Serial.print(recvBuf);
  
  pToken = strtok(recvBuf, "[@]");
  while (pToken != NULL) {
    pArray[i] =  pToken;
    if (++i >= ARR_CNT)
      break;
    pToken = strtok(NULL, "[@]");
  }
  
  // GETSENSOR 명령 처리
  // if (!strncmp(pArray[1], "GETSENSOR", 9)) {
  //   if (pArray[2] != NULL) {
  //     commData.sensorTime = atoi(pArray[2]);
  //     strcpy(commData.getSensorId, pArray[0]);
  //     return;
  //   } else {
  //     commData.sensorTime = 10;  // 기본값을 10초로 설정
  //     sprintf(commData.sendBuf, "[%s]%s@%d@%d@%d\n", 
  //             pArray[0], 
  //             pArray[1], 
  //             sensorData.cds, 
  //             (int)sensorData.temperature, 
  //             (int)sensorData.humidity);
              
  //     client.write(commData.sendBuf, strlen(commData.sendBuf));
  //     client.flush();
  //   }
  // }
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

  if (client.connected()) {
    client.stop();
    delay(500);
  }
  

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