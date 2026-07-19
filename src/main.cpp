#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>

// --- DISPLAY CONFIGURATION ---
#define DISPLAY_OLED 1
#define DISPLAY_LCD  0

// Select which display is connected:
// Set to DISPLAY_OLED for the 0.96" SSD1306 OLED (128x64)
// Set to DISPLAY_LCD for the 16x2 / 20x4 I2C Character LCD (HW-61 backpack)
#define ACTIVE_DISPLAY DISPLAY_OLED

#if ACTIVE_DISPLAY == DISPLAY_OLED
  #include <Adafruit_GFX.h>
  #include <Adafruit_SSD1306.h>
  // Vector fonts that do not pixelate
  #include <Fonts/FreeSans9pt7b.h>
  #include <Fonts/FreeSansBold12pt7b.h>

  #define SCREEN_WIDTH 128
  #define SCREEN_HEIGHT 64
  #define OLED_RESET     -1
  #define SCREEN_ADDRESS 0x3C

  Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
#elif ACTIVE_DISPLAY == DISPLAY_LCD
  #include <LiquidCrystal_I2C.h>
  
  // Standard character LCD parameters
  #define LCD_ADDRESS 0x27  // Standard default address for HW-61 PCF8574 backpacks
  #define LCD_COLUMNS 16
  #define LCD_ROWS    2

  LiquidCrystal_I2C lcd(LCD_ADDRESS, LCD_COLUMNS, LCD_ROWS);
#endif

// Network credentials
const char* ssid = "SAM_2G";
const char* password = "learncodingfirst";

// Weather & Geolocation variables
float currentTemp = 0.0;
String weatherDesc = "Loading...";
String city = "Local";
unsigned long lastWeatherCheck = 0;
const unsigned long WEATHER_CHECK_INTERVAL = 900000; // Check every 15 minutes (900,000 ms)
bool initialWeatherFetched = false;

// --- STATE MACHINE FOR DISPLAY ---
enum DisplayState {
  STATE_CLOCK,
  STATE_QUOTE
};

DisplayState currentState = STATE_CLOCK;
String fullQuoteText = "";
unsigned long lastQuoteFetch = 0;
const unsigned long QUOTE_FETCH_INTERVAL = 30000; // 30 seconds

// Scrolling state variables
int scrollX = 0;          // OLED pixel scroll offset
int textWidth = 0;        // OLED measured text width
int scrollPos = 0;        // LCD character offset
String lcdPaddedQuote = "";
unsigned long lastScrollUpdate = 0;
unsigned int scrollSpeedMs = 30; // Delay between animation steps (set dynamically)

bool isOledScrolling = false;
unsigned long oledStaticTimerStart = 0;
const unsigned long OLED_STATIC_DURATION = 5000; // 5 seconds (how long to display a static quote)

void fetchWeather() {
  if (WiFi.status() != WL_CONNECTED) return;
  
  HTTPClient http;
  http.setTimeout(6000); // 6 seconds timeout
  
  // Step 1: Geolocation using IP
  Serial.println("Fetching IP Geolocation...");
  http.begin("http://ip-api.com/json/");
  int httpCode = http.GET();
  
  double lat = 0.0;
  double lon = 0.0;
  
  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);
    if (!error) {
      lat = doc["lat"] | 0.0;
      lon = doc["lon"] | 0.0;
      city = doc["city"] | String("Local");
      Serial.printf("Location: %s (Lat: %f, Lon: %f)\n", city.c_str(), lat, lon);
    } else {
      Serial.println("JSON parse error for Geolocation");
    }
  } else {
    Serial.printf("Geolocation HTTP request failed: %d\n", httpCode);
  }
  http.end();

  // Fallback default coordinates (Delhi, India) if geolocation fails
  if (lat == 0.0 && lon == 0.0) {
    lat = 28.6139;
    lon = 77.209;
    city = "Delhi";
  }

  // Step 2: Fetch Weather from Open-Meteo
  Serial.println("Fetching Weather data...");
  String weatherUrl = String("http://api.open-meteo.com/v1/forecast?latitude=") + String(lat, 4) + 
                      "&longitude=" + String(lon, 4) + "&current_weather=true";
  http.begin(weatherUrl);
  httpCode = http.GET();
  
  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);
    if (!error) {
      JsonObject current = doc["current_weather"];
      currentTemp = current["temperature"] | 0.0;
      int weatherCode = current["weathercode"] | 0;
      
      // Map WMO codes to clear descriptors
      switch (weatherCode) {
        case 0: weatherDesc = "Sunny"; break;
        case 1: case 2: case 3: weatherDesc = "Cloudy"; break;
        case 45: case 48: weatherDesc = "Foggy"; break;
        case 51: case 53: case 55: weatherDesc = "Drizzle"; break;
        case 61: case 63: case 65: weatherDesc = "Rainy"; break;
        case 71: case 73: case 75: weatherDesc = "Snowy"; break;
        case 80: case 81: case 82: weatherDesc = "Showers"; break;
        case 95: case 96: case 99: weatherDesc = "Stormy"; break;
        default: weatherDesc = "Clear"; break;
      }
      Serial.printf("Temp: %.1f C, Weather: %s\n", currentTemp, weatherDesc.c_str());
    } else {
      Serial.println("JSON parse error for Weather");
    }
  } else {
    Serial.printf("Weather HTTP request failed: %d\n", httpCode);
  }
  http.end();
}

void fetchQuote() {
  if (WiFi.status() != WL_CONNECTED) return;

  Serial.println("Fetching random quote...");
  
#if ACTIVE_DISPLAY == DISPLAY_OLED
  display.clearDisplay();
  display.setFont(NULL);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 20);
  display.println(F("Fetching Quote..."));
  display.display();
#elif ACTIVE_DISPLAY == DISPLAY_LCD
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Fetching Quote...");
#endif

  WiFiClientSecure client;
  client.setInsecure(); // Skip certificate verification for simplicity

  HTTPClient http;
  http.setTimeout(5000);
  http.begin(client, "https://dummyjson.com/quotes/random");
  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);
    if (!error) {
      String quote = doc["quote"] | "";
      String author = doc["author"] | "";
      if (quote.length() > 0) {
        fullQuoteText = "\"" + quote + "\" - " + author;
        Serial.println("Quote: " + fullQuoteText);
        
#if ACTIVE_DISPLAY == DISPLAY_OLED
        // Measure text width using FreeSansBold12pt7b to know if it fits on a single line
        int16_t x1, y1;
        uint16_t w, h;
        display.setFont(&FreeSansBold12pt7b);
        display.setTextSize(1);
        display.setTextWrap(false); // Disable wrapping so it measures true single-line width
        display.getTextBounds(fullQuoteText, 0, 40, &x1, &y1, &w, &h);
        textWidth = w;
        
        if (textWidth <= SCREEN_WIDTH) {
          // Fits within the screen! Just center it statically.
          scrollX = (SCREEN_WIDTH - textWidth) / 2;
          isOledScrolling = false;
          oledStaticTimerStart = millis();
          Serial.printf("OLED Static Mode. Width: %d, X: %d\n", textWidth, scrollX);
        } else {
          // Too long! Enable scrolling.
          scrollX = SCREEN_WIDTH;
          isOledScrolling = true;
          Serial.printf("OLED Scroll Mode. Width: %d\n", textWidth);
        }
        scrollSpeedMs = 35; // 35ms per step
#elif ACTIVE_DISPLAY == DISPLAY_LCD
        // Create scroll padding so it starts from the right and scrolls off the left
        lcdPaddedQuote = "                " + fullQuoteText + "                ";
        scrollPos = 0;
        scrollSpeedMs = 250; // 250ms per character shift
#endif
        currentState = STATE_QUOTE;
        lastScrollUpdate = millis();
        return;
      }
    }
  }

  Serial.printf("Failed to fetch quote. HTTP Code: %d\n", httpCode);
  currentState = STATE_CLOCK;
}

#if ACTIVE_DISPLAY == DISPLAY_OLED
// Helper to draw centered strings with a specific font
void drawCenteredString(const String& buf, int y, const GFXfont* f) {
  int16_t x1, y1;
  uint16_t w, h;
  display.setFont(f);
  display.getTextBounds(buf, 0, y, &x1, &y1, &w, &h);
  int x = (SCREEN_WIDTH - w) / 2;
  display.setCursor(x, y);
  display.print(buf);
}

// Helper to draw centered strings with the default system font (very crisp at size 1)
void drawCenteredStringDefault(const String& buf, int y) {
  display.setFont(NULL);
  display.setTextSize(1);
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(buf, 0, y, &x1, &y1, &w, &h);
  int x = (SCREEN_WIDTH - w) / 2;
  display.setCursor(x, y);
  display.print(buf);
}
#endif

void updateClockDisplay() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
#if ACTIVE_DISPLAY == DISPLAY_OLED
    display.clearDisplay();
    display.setFont(NULL); // Reset to default font for standard text
    display.setTextSize(1);
    display.setCursor(0, 20);
    display.println(F("Syncing NTP Time..."));
    display.display();
#elif ACTIVE_DISPLAY == DISPLAY_LCD
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Syncing NTP...");
#endif
    return;
  }

  // Format Time: 12-hour AM/PM format (e.g. "2:30 PM")
  int hr = timeinfo.tm_hour;
  const char* ampm = (hr >= 12) ? "PM" : "AM";
  hr = hr % 12;
  if (hr == 0) hr = 12;
  
  char timeStr[16];
  sprintf(timeStr, "%d:%02d %s", hr, timeinfo.tm_min, ampm);

  // Format Bottom Row: Date & Temp (e.g., "Sun, Jul 19  |  32 C")
  char dateTempStr[40];
  char datePart[16];
  strftime(datePart, sizeof(datePart), "%a, %b %d", &timeinfo);
  sprintf(dateTempStr, "%s  |  %.0f C", datePart, currentTemp);

  // Format Top Row: City & Weather (e.g., "Kolkata | Sunny")
  String topStr = city + " | " + weatherDesc;

#if ACTIVE_DISPLAY == DISPLAY_OLED
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  // Line 1: City & Weather (using default font at Y=4)
  drawCenteredStringDefault(topStr, 4);

  // Line 2: Time in AM/PM (using FreeSansBold12pt7b at Y=38)
  drawCenteredString(String(timeStr), 38, &FreeSansBold12pt7b);

  // Line 3: Date & Temp (using default font at Y=54)
  drawCenteredStringDefault(String(dateTempStr), 54);

  display.display();
#elif ACTIVE_DISPLAY == DISPLAY_LCD
  // Character LCD formatting (16x2)
  char line1[17];
  char shortDate[10];
  strftime(shortDate, sizeof(shortDate), "%b %d", &timeinfo);
  
  // Calculate padding space to align time to left and date to right
  int spaces = 16 - strlen(timeStr) - strlen(shortDate);
  if (spaces < 1) spaces = 1;
  String spacePadding = "";
  for (int i = 0; i < spaces; i++) spacePadding += " ";
  
  snprintf(line1, sizeof(line1), "%s%s%s", timeStr, spacePadding.c_str(), shortDate);
  
  char line2[17];
  snprintf(line2, sizeof(line2), "%.0fC | %s", currentTemp, weatherDesc.c_str());

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(line1);
  
  lcd.setCursor(0, 1);
  lcd.print(line2);
#endif
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n--- I2C Scanner ---");
  Wire.begin(); // Explicitly start Wire
  
  byte error, address;
  int nDevices = 0;
  for(address = 1; address < 127; address++ ) {
    Wire.beginTransmission(address);
    error = Wire.endTransmission();
    if (error == 0) {
      Serial.printf("I2C device found at address 0x%02X\n", address);
      nDevices++;
    }
  }
  if (nDevices == 0) {
    Serial.println("No I2C devices found\n");
  } else {
    Serial.println("Scan complete\n");
  }

  // Initialize display
#if ACTIVE_DISPLAY == DISPLAY_OLED
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }
  
  display.clearDisplay();
  display.setFont(NULL); // Reset to default font
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setTextWrap(false); // Disable text wrapping globally
  display.setCursor(0, 10);
  display.println(F("Connecting to:"));
  display.println(ssid);
  display.display();
#elif ACTIVE_DISPLAY == DISPLAY_LCD
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Connecting to:");
  lcd.setCursor(0, 1);
  lcd.print(String(ssid).substring(0, 16));
#endif

  Serial.printf("Connecting to SSID: %s\n", ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  unsigned long startAttemptTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 15000) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");

#if ACTIVE_DISPLAY == DISPLAY_OLED
  display.clearDisplay();
  display.setCursor(0, 10);
#elif ACTIVE_DISPLAY == DISPLAY_LCD
  lcd.clear();
  lcd.setCursor(0, 0);
#endif

  wl_status_t wifiStatus = WiFi.status();
  if (wifiStatus == WL_CONNECTED) {
    Serial.print("Connected! IP address: ");
    Serial.println(WiFi.localIP());

#if ACTIVE_DISPLAY == DISPLAY_OLED
    display.println(F("WiFi Connected!"));
    display.print(F("IP: "));
    display.println(WiFi.localIP());
    display.display();
#elif ACTIVE_DISPLAY == DISPLAY_LCD
    lcd.print("WiFi Connected!");
    lcd.setCursor(0, 1);
    lcd.print(WiFi.localIP().toString().substring(0, 16));
#endif
    delay(1500);

    // Sync NTP Time (UTC+5:30 offset is 19800 seconds)
    configTime(19800, 0, "pool.ntp.org", "time.nist.gov");
  } else {
    Serial.printf("Connection Failed: %d\n", wifiStatus);
#if ACTIVE_DISPLAY == DISPLAY_OLED
    display.println(F("WiFi Failed!"));
    display.printf("Reason: %d\n", wifiStatus);
    display.display();
#elif ACTIVE_DISPLAY == DISPLAY_LCD
    lcd.print("WiFi Failed!");
    lcd.setCursor(0, 1);
    lcd.print("Reason: " + String(wifiStatus));
#endif
    delay(3000);
  }

  // Setup OTA hostname & handlers
  ArduinoOTA.setHostname("esp32-oled-ota");
  ArduinoOTA
    .onStart([]() {
#if ACTIVE_DISPLAY == DISPLAY_OLED
      display.clearDisplay();
      display.setFont(NULL);
      display.setCursor(0, 10);
      display.println(F("OTA Update Started..."));
      display.display();
#elif ACTIVE_DISPLAY == DISPLAY_LCD
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("OTA Started...");
#endif
    })
    .onEnd([]() {
#if ACTIVE_DISPLAY == DISPLAY_OLED
      display.clearDisplay();
      display.setFont(NULL);
      display.setCursor(0, 10);
      display.println(F("OTA Success!"));
      display.println(F("Rebooting..."));
      display.display();
#elif ACTIVE_DISPLAY == DISPLAY_LCD
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("OTA Success!");
      lcd.setCursor(0, 1);
      lcd.print("Rebooting...");
#endif
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      int percentage = progress / (total / 100);
#if ACTIVE_DISPLAY == DISPLAY_OLED
      display.clearDisplay();
      display.setFont(NULL);
      display.setCursor(0, 10);
      display.println(F("OTA Updating..."));
      display.drawRect(0, 28, 128, 10, SSD1306_WHITE);
      display.fillRect(0, 28, (percentage * 128) / 100, 10, SSD1306_WHITE);
      display.setCursor(0, 46);
      display.printf("%d%% completed", percentage);
      display.display();
#elif ACTIVE_DISPLAY == DISPLAY_LCD
      lcd.setCursor(0, 1);
      lcd.print("Updating: ");
      lcd.print(percentage);
      lcd.print("%");
#endif
    });

  ArduinoOTA.begin();
  Serial.println("OTA Service Ready.");
  
  // Fetch a quote immediately on bootup before showing the clock
  if (WiFi.status() == WL_CONNECTED) {
    fetchQuote();
  }
  lastQuoteFetch = millis(); // Initialize fetch timer
}

void loop() {
  ArduinoOTA.handle();
  
  unsigned long now = millis();

  // Fetch weather at intervals if connected
  if (WiFi.status() == WL_CONNECTED) {
    if (!initialWeatherFetched || (now - lastWeatherCheck >= WEATHER_CHECK_INTERVAL)) {
      fetchWeather();
      lastWeatherCheck = now;
      initialWeatherFetched = true;
    }
  }

  // Handle display state machine
  if (currentState == STATE_CLOCK) {
    updateClockDisplay();
    
    // Check if it's time to fetch a new quote (every 30 seconds)
    if (WiFi.status() == WL_CONNECTED && (now - lastQuoteFetch >= QUOTE_FETCH_INTERVAL)) {
      fetchQuote();
      lastQuoteFetch = millis(); // Reset timer
    }
    delay(1000); // Regular clock update delay
  } 
  else if (currentState == STATE_QUOTE) {
    if (now - lastScrollUpdate >= scrollSpeedMs) {
      lastScrollUpdate = now;

#if ACTIVE_DISPLAY == DISPLAY_OLED
      display.clearDisplay();
      display.setFont(&FreeSansBold12pt7b);
      display.setTextSize(1);
      display.setTextColor(SSD1306_WHITE);
      display.setTextWrap(false); // Prevent text wrapping onto a second line
      display.setCursor(scrollX, 40);
      display.print(fullQuoteText);
      display.display();

      if (isOledScrolling) {
        scrollX -= 2; // scroll by 2 pixels
        if (scrollX < -textWidth) {
          currentState = STATE_CLOCK;
          lastQuoteFetch = millis(); // Reset timer to prevent double-firing
        }
      } else {
        // Static display: hold for the static duration
        if (millis() - oledStaticTimerStart >= OLED_STATIC_DURATION) {
          currentState = STATE_CLOCK;
          lastQuoteFetch = millis();
        }
      }
#elif ACTIVE_DISPLAY == DISPLAY_LCD
      lcd.setCursor(0, 0);
      lcd.print("Random Quote:   "); // 16 chars static top row
      
      lcd.setCursor(0, 1);
      String visibleText = lcdPaddedQuote.substring(scrollPos, scrollPos + 16);
      lcd.print(visibleText);
      
      scrollPos++;
      if (scrollPos >= lcdPaddedQuote.length() - 16) {
        currentState = STATE_CLOCK;
        lastQuoteFetch = millis(); // Reset timer to prevent double-firing
      }
#endif
    }
    delay(10); // Maintain high responsiveness in animation state
  }
}
