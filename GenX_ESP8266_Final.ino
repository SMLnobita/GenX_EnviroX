#include <SoftwareSerial.h>
#include <ESP8266WiFi.h>
#include <FirebaseESP8266.h>
#include <ArduinoJson.h>

// Cấu hình WiFi
#define WIFI_SSID "Iphone Harley"
#define WIFI_PASSWORD "12345678"

// Cấu hình Firebase
#define FIREBASE_HOST "dht11-51c19-default-rtdb.asia-southeast1.firebasedatabase.app" // Không bao gồm https:// và / ở cuối
#define FIREBASE_SECRET "ZwZqIXo3HqNmk4VaUTqbganscWPyDeJomTFmJsx0" // Sử dụng secret key thay vì auth token

// Cấu hình UART với STM32
#define RX D2  // GPIO4 – từ STM32 TX
#define TX D1  // GPIO5 – về STM32 RX

SoftwareSerial mySerial(RX, TX);
String inputBuffer = "";

// Khởi tạo Firebase
FirebaseData firebaseData;
FirebaseAuth auth;
FirebaseConfig config;
FirebaseJson json;

// Biến lưu giá trị cảm biến
float tempValue = 0.0;
float humidValue = 0.0;
float gasValue = 0.0;

void setup() {
  Serial.begin(115200);
  mySerial.begin(115200);
  
  // Kết nối WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("🌐 Đang kết nối WiFi");
  
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }
  
  Serial.println();
  Serial.print("📶 Đã kết nối WiFi, IP: ");
  Serial.println(WiFi.localIP());
  
  // Cấu hình Firebase với legacy secret
  config.host = FIREBASE_HOST;
  config.signer.tokens.legacy_token = FIREBASE_SECRET;
  
  // Kết nối Firebase
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
  
  Serial.println("🔥 Đã kết nối Firebase");
  Serial.println("🔌 ESP sẵn sàng nhận dữ liệu...");
}

void loop() {
  // Nhận UART từ STM32
  while (mySerial.available()) {
    char c = mySerial.read();
    if (c == '\n') {
      inputBuffer.trim();

      // Kiểm tra và phân tích
      if (inputBuffer.startsWith("DATA: ")) {
        Serial.println("📥 Nhận được: " + inputBuffer);
        parseAndPrint(inputBuffer);
        
        // Cập nhật JSON và gửi ngay lên Firebase
        updateFirebaseJson();
        sendToFirebase();
      }

      inputBuffer = "";
    } else {
      inputBuffer += c;
    }
  }
}

void parseAndPrint(String data) {
  int tIdx = data.indexOf("TEMP=");
  int hIdx = data.indexOf("HUMID=");
  int gIdx = data.indexOf("GAS=");

  if (tIdx != -1 && hIdx != -1 && gIdx != -1) {
    String temp = data.substring(tIdx + 5, data.indexOf("°", tIdx));
    String humid = data.substring(hIdx + 6, data.indexOf("%", hIdx));
    String gas = data.substring(gIdx + 4, data.indexOf("ppm", gIdx));

    // Chuyển đổi từ String sang float
    tempValue = temp.toFloat();
    humidValue = humid.toFloat();
    gasValue = gas.toFloat();

    Serial.println("🌡️  Nhiệt độ: " + temp + "°C");
    Serial.println("💧 Độ ẩm   : " + humid + "%");
    Serial.println("🧪 Gas     : " + gas + " ppm");
    Serial.println("——————————————");
  }
}

void updateFirebaseJson() {
  // Xóa JSON cũ
  json.clear();
  
  // Tạo cấu trúc JSON theo yêu cầu
  FirebaseJson sensorJson;
  sensorJson.add("temp", tempValue);
  sensorJson.add("humid", humidValue);
  sensorJson.add("gas", gasValue);
  
  json.add("sensor", sensorJson);
  
  // In JSON ra Serial để kiểm tra
  String jsonStr;
  json.toString(jsonStr, true);
  Serial.println("📊 JSON: " + jsonStr);
}

void sendToFirebase() {
  if (WiFi.status() == WL_CONNECTED) {
    // Gửi dữ liệu lên Firebase
    if (Firebase.setJSON(firebaseData, "/", json)) {
      Serial.println("🔥 Đã gửi dữ liệu lên Firebase");
    } else {
      Serial.println("❌ Lỗi Firebase: " + firebaseData.errorReason());
    }
  } else {
    Serial.println("❌ WiFi ngắt kết nối!");
  }
}
//HelloHello
