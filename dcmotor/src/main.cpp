#if 0
#include <Arduino.h>
#include <IO7F32.h>
#include <WiFi.h>

// 핀 설정
const int IN1 = 27;
const int IN2 = 26;
const int ENA = 25;

// 상태 변수
bool motorOn = false;
bool switchChanged = false;

char* ssid_pfix = (char*)"IOT_DC_Motor";
// unsigned long pubInterval = 5000;  // 선언 제거 — 헤더에서 이미 선언됨
unsigned long lastPublishMillis = 0;

void publishData() {
    StaticJsonDocument<512> root;
    JsonObject data = root.createNestedObject("d");

    data["dcmotor"] = motorOn ? "on" : "off";

    serializeJson(root, msgBuffer);
    client.publish(evtTopic, msgBuffer);

    Serial.printf("[Publish] Motor: %s\n", motorOn ? "on" : "off");
}

void handleUserCommand(char* topic, JsonDocument* root) {
    JsonObject d = (*root)["d"];
    Serial.println(topic);

    if (d.containsKey("dcmotor")) {
        const char* motorState = d["dcmotor"];
        Serial.printf("[MQTT 수신] dcmotor = %s\n", motorState);  // 🔍 추가

        if (strcmp(motorState, "on") == 0) {
            if (!motorOn) {
                motorOn = true;
                switchChanged = true;
                Serial.println("[Server] Motor ON");
            }
        } else {
            // 🔄 문자열이 "off"가 아니더라도 일단 꺼줌 (비정상 값 방지)
            if (motorOn) {
                motorOn = false;
                switchChanged = true;
                Serial.println("[Server] Motor OFF (fallback)");
            }
        }
    }
}


void setup() {
    Serial.begin(115200);

    pinMode(IN1, OUTPUT);
    pinMode(IN2, OUTPUT);
    pinMode(ENA, OUTPUT);
    digitalWrite(ENA, LOW);

    initDevice();
    JsonObject meta = cfg["meta"];

    // pubInterval 변수는 헤더에서 이미 선언됐으니 값만 할당
    pubInterval = meta.containsKey("pubInterval") ? meta["pubInterval"] : 5000;

    lastPublishMillis = millis();

    WiFi.mode(WIFI_STA);
    WiFi.begin((const char*)cfg["ssid"], (const char*)cfg["w_pw"]);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.printf("\nIP address : ");
    Serial.println(WiFi.localIP());

    userCommand = handleUserCommand;

    set_iot_server();
    iot_connect();
}

void loop() {
    if (motorOn) {
        digitalWrite(IN1, HIGH);
        digitalWrite(IN2, LOW);
        digitalWrite(ENA, HIGH);
    } else {
        digitalWrite(ENA, LOW);
    }

    static unsigned long lastClientLoopTime = 0;
    if (millis() - lastClientLoopTime > 50) {
        client.loop();
        lastClientLoopTime = millis();
    }

    if ((pubInterval != 0) && (millis() - lastPublishMillis > pubInterval)) {
        publishData();
        lastPublishMillis = millis();
    }

    if (switchChanged) {
        publishData();
        switchChanged = false;
        lastPublishMillis = millis();
    }
}
#endif


// 이 코드는 MQTT로 "dcmotor": "on"/"off" 명령을 받아 DC 모터를 제어하는 예제입니다.

// ✅ 이 코드는 MQTT 명령을 통해 DC 모터를 on/off 제어하는 예제입니다.
/*
  ▒ 핵심 설명 ▒
  - Node-RED에서 받은 MQTT 메시지를 파싱하여 dcmotor 필드 값("on"/"off")에 따라 DC 모터 제어
  - 제어 상태는 IN1, IN2, ENA 핀으로 모터 드라이버(L293D)를 통해 직접 조작됨
  - 상태 변화가 있을 때만 MQTT로 상태를 publish하여 브로커에 전송

  ▒ 주요 포인트 ▒
  - motorState 문자열은 strcmp()로 정확하게 비교 ("on", "off")
  - 모터 ON → IN1=HIGH, IN2=LOW, ENA=HIGH
  - 모터 OFF → ENA=LOW
  - MQTT 주기 전송 및 상태 변경 시 전송을 분리하여 효율적으로 처리
*/

#include <Arduino.h>
#include <IO7F32.h>
#include <WiFi.h>

// 핀 설정
const int IN1 = 27;
const int IN2 = 26;
const int ENA = 25;

bool motorOn = false;          // 모터 상태
bool switchChanged = false;    // 상태 변경 감지

char* ssid_pfix = (char*)"IOT_DC_Motor";
unsigned long lastPublishMillis = 0;

// ▒ MQTT publish 함수 ▒
void publishData() {
    StaticJsonDocument<512> root;
    JsonObject data = root.createNestedObject("d");
    data["dcmotor"] = motorOn ? "on" : "off";

    serializeJson(root, msgBuffer);
    client.publish(evtTopic, msgBuffer);

    Serial.printf("[Publish] Motor: %s\n", motorOn ? "on" : "off");
}

// ▒ MQTT 명령 수신 처리 함수 ▒
void handleUserCommand(char* topic, JsonDocument* root) {
    JsonObject d = (*root)["d"];
    Serial.println("[MQTT 수신] " + String(topic));

    if (d.containsKey("dcmotor")) {
        const char* motorState = d["dcmotor"];
        Serial.printf("수신된 dcmotor 값: [%s]\n", motorState);

        // 정확히 "on"이면 모터 ON
        if (motorState && strcmp(motorState, "on") == 0) {
            if (!motorOn) {
                motorOn = true;
                switchChanged = true;
                Serial.println("[Server] Motor ON");
            }
        }
        // 정확히 "off"이면 모터 OFF
        else if (motorState && strcmp(motorState, "off") == 0) {
            if (motorOn) {
                motorOn = false;
                switchChanged = true;
                Serial.println("[Server] Motor OFF");
            }
        }
    }
}

// ▒ 초기 설정 ▒
void setup() {
    Serial.begin(115200);

    pinMode(IN1, OUTPUT);
    pinMode(IN2, OUTPUT);
    pinMode(ENA, OUTPUT);
    digitalWrite(ENA, LOW);

    initDevice();
    JsonObject meta = cfg["meta"];
    pubInterval = meta.containsKey("pubInterval") ? meta["pubInterval"] : 5000;

    lastPublishMillis = millis();

    WiFi.mode(WIFI_STA);
    WiFi.begin((const char*)cfg["ssid"], (const char*)cfg["w_pw"]);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.printf("\nIP address : ");
    Serial.println(WiFi.localIP());

    userCommand = handleUserCommand;
    set_iot_server();
    iot_connect();
}

// ▒ 메인 루프 ▒
void loop() {
    // 모터 동작
    if (motorOn) {
        digitalWrite(IN1, HIGH);
        digitalWrite(IN2, LOW);
        digitalWrite(ENA, HIGH);
    } else {
        digitalWrite(ENA, LOW);
    }

    // MQTT 통신 유지
    static unsigned long lastClientLoopTime = 0;
    if (millis() - lastClientLoopTime > 50) {
        client.loop();
        lastClientLoopTime = millis();
    }

    // 주기적 상태 전송
    if ((pubInterval != 0) && (millis() - lastPublishMillis > pubInterval)) {
        publishData();
        lastPublishMillis = millis();
    }

    // 상태 변경 시 즉시 전송
    if (switchChanged) {
        publishData();
        switchChanged = false;
        lastPublishMillis = millis();
    }
}
