#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>

// 字体
#include <Fonts/FreeSansBold9pt7b.h>   // 标签
#include <Fonts/FreeSansBold12pt7b.h>  // 额外额度
#include <Fonts/FreeSansBold24pt7b.h>  // 大号百分比
#include <Fonts/FreeSans9pt7b.h>       // 重置时间

// 引脚定义 (SuperMini ESP32-C3, 与原行情屏一致)
#define TFT_CS      7
#define TFT_RST     3
#define TFT_DC      2
#define TFT_MOSI    6
#define TFT_SCLK    4
#define TFT_BLK     10

// 配色
#define CL_BG       0x0000 // 纯黑
#define CL_DIVIDER  0x3186 // 深灰
#define CL_WHITE    0xFFFF
#define CL_ORANGE   0xFB60 // 额度橙
#define CL_GREEN    0x07E0 // 纯绿 (时间/额外额度)
#define CL_INFO     0x9CD3 // 浅灰蓝 (辅助信息)

// 网络配置 (与原草图一致)
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";
const char* apiUrl = "http://YOUR_SERVER/gadget-img/token.json";
const char* ntpServer = "ntp.aliyun.com";

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);

// 状态缓存 (防闪烁)
String lastTime = "";
int    lastUsed = -1;
String lastReset = "";
String lastExtra = "";
unsigned long lastNetCheck = 0;
const unsigned long NET_INTERVAL = 60000; // 60 秒拉一次

void drawCentered(const String& text, int y, const GFXfont* font, uint16_t color) {
  tft.setFont(font);
  tft.setTextColor(color);
  int16_t x1, y1; uint16_t w, h;
  tft.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
  tft.setCursor((128 - (int)w) / 2, y);
  tft.print(text);
}

// 静态框架 (启动画一次)
void drawInterface() {
  tft.fillScreen(CL_BG);
  tft.drawFastHLine(10, 30, 108, CL_DIVIDER);
  drawCentered("WEEK USED", 52, &FreeSansBold9pt7b, CL_INFO);
  tft.drawFastHLine(10, 116, 108, CL_DIVIDER);
}

// 顶部时间 (每分钟变才重画)
void updateHeader() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return;
  char timeBuf[6];
  sprintf(timeBuf, "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
  String newTime = String(timeBuf);
  if (newTime != lastTime) {
    tft.fillRect(0, 0, 128, 28, CL_BG);
    drawCentered(newTime, 21, &FreeSansBold9pt7b, CL_GREEN);
    lastTime = newTime;
  }
}

// 数据区 (变了才重画)
void updateData(int used, String resetTxt, String extraTxt) {
  if (used != lastUsed) {
    // 大百分比
    tft.fillRect(0, 58, 128, 40, CL_BG);
    String pct = (used < 0) ? "--%" : (String(used) + "%");
    drawCentered(pct, 92, &FreeSansBold24pt7b, CL_ORANGE);
    // 进度条
    tft.fillRect(14, 102, 100, 8, CL_DIVIDER);
    if (used > 0) {
      int fw = map(min(used, 100), 0, 100, 0, 100);
      if (fw < 3) fw = 3;
      tft.fillRect(14, 102, fw, 8, CL_ORANGE);
    }
    lastUsed = used;
  }
  if (resetTxt != lastReset) {
    tft.fillRect(0, 120, 128, 18, CL_BG);
    drawCentered("RESET " + resetTxt, 134, &FreeSans9pt7b, CL_INFO);
    lastReset = resetTxt;
  }
  if (extraTxt != lastExtra) {
    tft.fillRect(0, 140, 128, 20, CL_BG);
    drawCentered(extraTxt, 155, &FreeSansBold9pt7b, CL_GREEN);
    lastExtra = extraTxt;
  }
}

void fetchData() {
  if (WiFi.status() != WL_CONNECTED) return;
  HTTPClient http;
  http.begin(apiUrl);
  http.setTimeout(3000);
  int code = http.GET();
  Serial.printf("[net] GET token.json -> %d\n", code);
  if (code == 200) {
    String payload = http.getString();
    JsonDocument doc;
    if (!deserializeJson(doc, payload)) {
      int used = doc["used"] | -1;
      String md = doc["reset_md"].as<const char*>();
      String resetTxt = md.substring(3) + " " + String(doc["reset_hm"].as<const char*>()); // "29 16:47"
      String extraTxt = "EXTRA " + String(doc["extra_left"].as<const char*>());
      Serial.printf("[net] used=%d reset=%s extra=%s\n", used, resetTxt.c_str(), extraTxt.c_str());
      updateData(used, resetTxt, extraTxt);
    }
  }
  http.end();
}

void setup() {
  Serial.begin(115200);

  pinMode(TFT_BLK, OUTPUT); digitalWrite(TFT_BLK, HIGH);
  tft.initR(INITR_BLACKTAB);
  tft.setRotation(2);
  tft.fillScreen(CL_BG);

  tft.setFont(&FreeSansBold12pt7b);
  tft.setTextColor(CL_WHITE);
  tft.setCursor(28, 70); tft.print("TOKEN");

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  int dots = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    tft.print(".");
    if (++dots > 5) {
      tft.fillRect(0, 80, 128, 20, CL_BG);
      tft.setCursor(28, 70);
      dots = 0;
    }
  }
  Serial.printf("[wifi] connected, IP=%s\n", WiFi.localIP().toString().c_str());

  configTime(8 * 3600, 0, ntpServer);
  drawInterface();
  updateHeader();
  fetchData();
  lastNetCheck = millis();
}

void loop() {
  updateHeader();
  if (millis() - lastNetCheck > NET_INTERVAL) {
    fetchData();
    lastNetCheck = millis();
  }
  delay(1000);
}
