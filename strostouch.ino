#include <Arduino_GFX_Library.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <XPT2046_Touchscreen.h>
#include <SPI.h>

// Display pins
#define TFT_CS   15
#define TFT_DC   2
#define TFT_RST  -1
#define TFT_SCK  14
#define TFT_MOSI 13
#define TFT_MISO 12
#define TFT_BL   27

// Touchscreen pins
#define T_CS 33
#define T_IRQ 36

// WiFi credentials
const char* ssid = "yourwifinetwork";
const char* password = "password";


// MLB API URLs and constants
const char* mlbStandingsUrl = "https://statsapi.mlb.com/api/v1/standings?leagueId=103";
const char* astrosScheduleUrl = "https://statsapi.mlb.com/api/v1/schedule?hydrate=team&sportId=1&teamId=117&hydrate=probablePitcher";
const char* previousGameUrl = "https://statsapi.mlb.com/api/v1/teams/117?hydrate=previousSchedule&fields=teams,previousGameSchedule,dates,games,gamePk,status,abstractGameCode";
const int AL_WEST_DIVISION_ID = 200;
const int ASTROS_TEAM_ID = 117;

// Display and touch objects
Arduino_DataBus *bus;
Arduino_GFX *gfx;
SPIClass touchSPI(VSPI);
XPT2046_Touchscreen touch(T_CS, T_IRQ);

// Display modes
enum DisplayMode { STANDINGS_MODE, PITCHER_MODE, BATTER_MODE };
DisplayMode currentMode = STANDINGS_MODE;
unsigned long lastTouchTime = 0;
const unsigned long touchDebounce = 500;
unsigned long lastUpdateTime = 0;
const unsigned long updateInterval = 1800000; // 30 minutes
int lastGamePk = 0; // Variable to store the last gamePk

int batterPage = 0;
const int BATTERS_PER_PAGE = 6;  // Batters per page

// Function prototypes
void connectToWiFi();
void updateDisplay();
void fetchLastGamePk();
void fetchAndDisplayMLBStandings();
void fetchAndDisplayNextAstrosGame();
void fetchAndDisplayAstrosPitcherStats();
void fetchAndDisplayAstrosBatterStats();

String cleanName(String name) {
  name.replace("ñ", "n");
  name.replace("ó", "o");
  name.replace("á", "a");
  name.replace("é", "e");
  name.replace("í", "i");
  name.replace("ú", "u");
  return name;
}

void setup() {
  Serial.begin(115200);
  
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
  delay(100);

  // Initialize display
  bus = new Arduino_ESP32SPI(TFT_DC, TFT_CS, TFT_SCK, TFT_MOSI);
  gfx = new Arduino_ST7789(bus, TFT_RST, 1, false, 240, 320);
  
  if (!gfx->begin()) {
    Serial.println("Display init failed!");
    while(1);
  }

  // Initialize touchscreen
  touchSPI.begin(TFT_SCK, TFT_MISO, TFT_MOSI, T_CS);
  touch.begin(touchSPI);

  // Connect to WiFi
  connectToWiFi();
  
  // Initial display update
  updateDisplay();
}

void loop() {
  // Handle touch input
  if (touch.touched() && millis() - lastTouchTime > touchDebounce) {
    TS_Point p = touch.getPoint();
    if (p.z > 400) { // Only register if pressure is sufficient
      lastTouchTime = millis();
      
      if (currentMode == BATTER_MODE) {
        HTTPClient http;
        http.begin("https://statsapi.mlb.com/api/v1/game/" + String(lastGamePk) + 
                  "/boxscore?fields=teams,batters");
        if (http.GET() > 0) {
          DynamicJsonDocument doc(1000);
          if (!deserializeJson(doc, http.getString())) {
            JsonObject astrosTeam = doc["teams"]["home"]["team"]["name"] == "Houston Astros" ? 
                                   doc["teams"]["home"] : doc["teams"]["away"];
            int totalBatters = astrosTeam["batters"].size();
            int totalPages = (totalBatters + BATTERS_PER_PAGE - 1) / BATTERS_PER_PAGE;
            
            batterPage++;
            if (batterPage >= totalPages) {
              batterPage = 0;
              currentMode = STANDINGS_MODE;
            }
          }
        }
        http.end();
      } else {
        // Original mode cycling logic
        batterPage = 0; // Reset batter page when leaving batter mode
        switch(currentMode) {
          case STANDINGS_MODE:
            currentMode = PITCHER_MODE;
            fetchLastGamePk();
            break;
          case PITCHER_MODE:
            currentMode = BATTER_MODE;
            break;
          case BATTER_MODE:
            currentMode = STANDINGS_MODE;
            break;
        }
      }
      
      Serial.println("Switched to mode: " + String(
        currentMode == STANDINGS_MODE ? "STANDINGS" : 
        currentMode == PITCHER_MODE ? "PITCHER" : "BATTER"));
      
      updateDisplay();
    }
  }

  // Auto-update in STANDINGS_MODE
  if (currentMode == STANDINGS_MODE && millis() - lastUpdateTime > updateInterval) {
    if (WiFi.status() == WL_CONNECTED) {
      updateDisplay();
      lastUpdateTime = millis();
    } else {
      Serial.println("WiFi disconnected, attempting to reconnect...");
      connectToWiFi();
    }
  }
  
  delay(100);
}

void connectToWiFi() {
  gfx->fillScreen(BLACK);
  gfx->setTextSize(2);
  gfx->setTextColor(WHITE);
  gfx->setCursor(10, 10);
  gfx->println("Connecting to WiFi...");
  
  WiFi.begin(ssid);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    gfx->print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected");
    gfx->fillScreen(BLACK);
    gfx->setCursor(10, 10);
    gfx->println("WiFi connected!");
    delay(1000);
  } else {
    Serial.println("\nWiFi connection failed");
    gfx->fillScreen(BLACK);
    gfx->setCursor(10, 10);
    gfx->println("WiFi connection");
    gfx->setCursor(10, 40);
    gfx->println("failed!");
    delay(2000);
  }
}

void fetchLastGamePk() {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  http.begin(previousGameUrl);
  
  if (http.GET() > 0) {
    DynamicJsonDocument doc(5000);
    if (!deserializeJson(doc, http.getString())) {
      // Get the most recent completed game (last in the array)
      JsonArray dates = doc["teams"][0]["previousGameSchedule"]["dates"];
      for (int i = dates.size() - 1; i >= 0; i--) {
        JsonArray games = dates[i]["games"];
        for (JsonObject game : games) {
          if (game["status"]["abstractGameCode"] == "F") { // Only finished games
            lastGamePk = game["gamePk"];
            break; // We only want the most recent one
          }
        }
        if (lastGamePk != 0) break; // Exit if we found a game
      }
    }
  }
  http.end();
}

void updateDisplay() {
  gfx->fillScreen(BLACK);
  
  switch(currentMode) {
    case STANDINGS_MODE:
      if (WiFi.status() == WL_CONNECTED) {
        fetchAndDisplayMLBStandings();
        fetchAndDisplayNextAstrosGame();
      } else {
        gfx->setCursor(10, 10);
        gfx->println("WiFi disconnected");
      }
      break;
      
    case PITCHER_MODE:
      if (lastGamePk > 0 && WiFi.status() == WL_CONNECTED) {
        fetchAndDisplayAstrosPitcherStats();
      } else {
        gfx->setTextSize(2);
        gfx->setTextColor(WHITE);
        gfx->setCursor(10, 10);
        gfx->println("No game data available");
        gfx->setCursor(10, 40);
        gfx->println("Touch to change mode");
      }
      break;
      
    case BATTER_MODE:
      if (lastGamePk > 0 && WiFi.status() == WL_CONNECTED) {
        fetchAndDisplayAstrosBatterStats();
      } else {
        gfx->setTextSize(2);
        gfx->setTextColor(WHITE);
        gfx->setCursor(10, 10);
        gfx->println("No game data available");
        gfx->setCursor(10, 40);
        gfx->println("Touch to change mode");
      }
      break;
  }
}

void fetchAndDisplayAstrosBatterStats() {
  if (lastGamePk == 0) return;
  
  String boxscoreUrl = "https://statsapi.mlb.com/api/v1/game/" + String(lastGamePk) + 
                     "/boxscore?fields=teams,name,abbreviation,pitching,batting,stats,person,fullName,summary,batters,battingOrder";
  
  HTTPClient http;
  http.begin(boxscoreUrl);
  
  int httpResponseCode = http.GET();
  
  if (httpResponseCode > 0) {
    String payload = http.getString();
    DynamicJsonDocument doc(16000);
    DeserializationError error = deserializeJson(doc, payload);
    
    if (!error) {
      gfx->setTextColor(WHITE);

      // Find Astros team
      JsonObject astrosTeam = doc["teams"]["home"]["team"]["name"] == "Houston Astros" ? 
                             doc["teams"]["home"] : doc["teams"]["away"];
      
      JsonArray batters = astrosTeam["batters"];
      JsonObject players = astrosTeam["players"];
      

      int totalBatters = batters.size();
      int totalPages = (totalBatters + BATTERS_PER_PAGE - 1) / BATTERS_PER_PAGE;
      int startBatter = batterPage * BATTERS_PER_PAGE;
      int endBatter = min(startBatter + BATTERS_PER_PAGE, totalBatters);
      

      int y = 0;
      for (int i = startBatter; i < endBatter; i++) {
        int batterId = batters[i];
        String playerKey = "ID" + String(batterId);
        
        if (players.containsKey(playerKey)) {
          JsonObject player = players[playerKey];
          
          if (player.containsKey("stats") && 
              player["stats"].containsKey("batting") &&
              player["stats"]["batting"].containsKey("summary")) {
            
            String name = cleanName(player["person"]["fullName"].as<String>());
            String stats = player["stats"]["batting"]["summary"].as<String>();
            
            // Indent logic (pinch hitters and defensive replacements)
            bool shouldIndent = false;
            if (player.containsKey("battingOrder")) {
              String battingOrder = player["battingOrder"].as<String>();
              if (battingOrder.length() >= 3 && battingOrder[2] >= '1' && battingOrder[2] <= '9') {
                shouldIndent = true;
              }
            }
            
            int indent = shouldIndent ? 10 : 0;
            
            gfx->setCursor(indent, y);
            gfx->println(name);
            y += 20;
            
            gfx->setCursor(indent, y);
            gfx->println(stats);
            y += 20;
            

            if (i < endBatter - 1) {
              for (int x = 0; x < gfx->width(); x++) {
                gfx->drawPixel(x, y-2, WHITE);
              }
              y += 10;
            }
          }
        }
      }
      

      if (totalPages > 1) {
        gfx->setCursor(10, 300);
        gfx->print("Page ");
        gfx->print(batterPage + 1);
        gfx->print("/");
        gfx->print(totalPages);
      } else {
        gfx->setCursor(10, 300);
        gfx->println("Touch to change mode");
      }
    }
  }
  http.end();
}

void fetchAndDisplayAstrosPitcherStats() {
  if (lastGamePk == 0) return;
  
  String boxscoreUrl = "https://statsapi.mlb.com/api/v1/game/" + String(lastGamePk) + 
                       "/boxscore?fields=teams,name,abbreviation,pitching,batting,stats,person,fullName,summary";
  
  HTTPClient http;
  http.begin(boxscoreUrl);
  
  int httpResponseCode = http.GET();
  
  if (httpResponseCode > 0) {
    String payload = http.getString();
    DynamicJsonDocument doc(16000);
    DeserializationError error = deserializeJson(doc, payload);
    
    if (!error) {
      gfx->setTextColor(WHITE);

      // Find Astros (could be home or away)
      JsonObject astrosTeam;
      if (doc["teams"]["home"]["team"]["name"] == "Houston Astros") {
        astrosTeam = doc["teams"]["home"];
      } else {
        astrosTeam = doc["teams"]["away"];
      }
      
      // Get the pitchers array for order
      JsonArray pitchersOrder = astrosTeam["pitchers"];
      JsonObject players = astrosTeam["players"];
      
      // Display each pitcher's stats in game order
      int y = 0;
      for (int pitcherId : pitchersOrder) {
        String playerKey = "ID" + String(pitcherId); // Add "ID" prefix
        if (players.containsKey(playerKey)) {
          JsonObject player = players[playerKey];
          
          // Check if this player is a pitcher with stats
          if (player.containsKey("allPositions") && 
              player["allPositions"].size() > 0 &&
              player["allPositions"][0]["name"] == "Pitcher" && 
              player.containsKey("stats") && 
              player["stats"].containsKey("pitching") &&
              player["stats"]["pitching"].containsKey("summary")) {
            
            String name = player["person"]["fullName"];
            String stats = player["stats"]["pitching"]["summary"];
            
            if (stats.length() > 0) {
              gfx->setCursor(0, y);
              gfx->println(name);
              y += 20;
              gfx->println(stats);
              
              
              y += 17;
              for (int x = 0; x < gfx->width(); x++) {
                gfx->drawPixel(x, y, WHITE);
              }
              y += 3;
            }
          }
        }
      }
      
      // Instructions for returning
      gfx->setCursor(10, 300);
      gfx->println("Touch to change mode");
    } else {
      gfx->setTextSize(2);
      gfx->setCursor(10, 100);
      gfx->println("Error parsing data");
    }
  } else {
    gfx->setTextSize(2);
    gfx->setCursor(10, 100);
    gfx->println("Error fetching data");
  }
  
  http.end();
}

void fetchAndDisplayNextAstrosGame() {
  if (currentMode != STANDINGS_MODE) return;
  
  HTTPClient http;
  http.begin(astrosScheduleUrl);
  
  int httpResponseCode = http.GET();
  
  if (httpResponseCode > 0) {
    String payload = http.getString();
    DynamicJsonDocument doc(8000);
    DeserializationError error = deserializeJson(doc, payload);
    
    if (!error && doc["totalGames"] > 0) {
      JsonObject game = doc["dates"][0]["games"][0];
      String gameDate = game["gameDate"].as<String>();
      String opponent = game["teams"]["away"]["team"]["name"].as<String>();
      String ballpark = game["venue"]["name"].as<String>();
      String astrospitcher = game["teams"]["home"]["probablePitcher"]["fullName"].as<String>();
      String otherpitcher = game["teams"]["away"]["probablePitcher"]["fullName"].as<String>();
      
      if (opponent == "Houston Astros") {
        opponent = game["teams"]["home"]["team"]["name"].as<String>();
        astrospitcher = game["teams"]["away"]["probablePitcher"]["fullName"].as<String>();
        otherpitcher = game["teams"]["home"]["probablePitcher"]["fullName"].as<String>();
      }
      
      int spaceIndex = opponent.indexOf(' ');
      if (spaceIndex != -1) {
        opponent = opponent.substring(spaceIndex + 1);
      }

      String timeStr = String((gameDate.substring(11, 13).toInt() - 5 + 24) % 24) + ":" + gameDate.substring(14, 16);
      
      gfx->setTextSize(2);
      gfx->setTextColor(WHITE);
      gfx->setCursor(10, 160);
      gfx->println(timeStr + " CST " + ballpark);
      
      gfx->setCursor(10, 180);
      gfx->println("Astros - "+astrospitcher);
      gfx->setCursor(10, 200);
      gfx->println(opponent+ " - " +otherpitcher);
    }
  }
  http.end();
}

void fetchAndDisplayMLBStandings() {
  if (currentMode != STANDINGS_MODE) return;
  
  HTTPClient http;
  http.begin(mlbStandingsUrl);
  
  int httpResponseCode = http.GET();
  
  if (httpResponseCode > 0) {
    String payload = http.getString();
    DynamicJsonDocument doc(48000);
    DeserializationError error = deserializeJson(doc, payload);
    
    if (!error) {
      gfx->setTextSize(2);
      gfx->setTextColor(WHITE);
      gfx->setCursor(10, 10);
      gfx->println("AL WEST STANDINGS");
      for (int x = 0; x < gfx->width(); x++) {
          gfx->drawPixel(x, 30, WHITE);  // Top line
          gfx->drawPixel(x, 140, WHITE); // Bottom line
      }
      
      JsonArray records = doc["records"];
      for (JsonObject record : records) {
        if (record["division"]["id"] == AL_WEST_DIVISION_ID) {
          JsonArray teamRecords = record["teamRecords"];
          int y = 40;
          for (JsonObject team : teamRecords) {
            String teamName = team["team"]["name"].as<String>();
            if (teamName.length() > 16) teamName = teamName.substring(0, 16);
            
            gfx->setCursor(10, y);
            gfx->print(teamName);
            
            gfx->setCursor(210, y);
            gfx->print(String(team["wins"].as<int>()) + "-" + String(team["losses"].as<int>()));
            
            gfx->setCursor(280, y);
            gfx->print(team["gamesBack"].as<String>());
            
            y += 20;
          }
          break;
        }
      }
    }
  }
  http.end();
}
