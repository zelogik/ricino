//#include <AsyncJson.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ESPAsyncWiFiManager.h>
#include <SoftwareSerial.h> 

#include <FS.h>

/* AsyncWebServer Stuff */
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
DNSServer dns;

/* SoftwareSerial Stuff */
#define D5 (14)
#define D6 (12)
SoftwareSerial ricinoSerial;

#define D1 (5) 
#define D2 (4)
#define D3 (0)

/* =============== config section start =============== */
enum State {
  DISCONNECTED, // Disconnected, only allow connection
  CONNECTED, // connected ( start to test alive ping )
  SET,      // Set RACE init.
  START, // When START add 20s timeout for RACE
  RACE, // RACE, update dataRace()...
  END,  // RACE finish, wait 20sec all players
  RESET  // RACE finished auto/manual, reset every parameters
};

enum State raceStateENUM;

// proto
void updateDataLoop(bool endingRaceState);

//bool raceState = false;
bool lightState = false;
//bool connectState = false;
//uint8_t numberTotalLaps = 10;
uint32_t startRaceMillis = 0;
uint32_t currentRaceMillis = 0;

//uint32_t standbyHeartbeat = millis();

#define JSON_BUFFER 300 //Keep it low, make a nice JSON output
#define NUM_BUFFER 4 // Number of racer by race (need HTML/JavaScript adaptation if modified)
#define TOO_LATE_FOR_INIT_RACER 20000 // Time for first pass, at each new race
#define CHAR_LENGTH 32 // keep it low for memory (used for SoftwareSerial)
char receivedChars[CHAR_LENGTH]; // an array to store the received data

//#define LOG(f_, ...)  { Serial.printf((f_), ##__VA_ARGS__); }


// Struct to keep every Race laps time and some calculation.
struct racerMemory{
  public:
      uint8_t id;
      uint8_t color; // 1 to 4 for the moment...
      uint8_t numberLap;
      uint32_t offsetLap;
      uint32_t lastLap;
      uint32_t bestLap;
      uint32_t meanLap;
      uint32_t totalLap;
      uint32_t _tmpLast;

      void reset(){
          id = 0;
          color = 0;
          numberLap = 0;
          offsetLap = 0;
          lastLap = 0;
          bestLap = 0;
          meanLap = 0;
          totalLap = 0;
          _tmpLast = 0;
      }

      void init(uint8_t id, uint32_t currentTime){
          offsetLap = currentTime; //it's a feature! not a bug.
          _tmpLast = currentTime;
//          lastSeenLap = currentTime;
          this->id = id;
      }
      
      void setCurrentLap(uint32_t currentTime){
          numberLap++;
          totalLap = currentTime - offsetLap;
          meanLap = totalLap / numberLap;
          lastLap = currentTime - _tmpLast;
          _tmpLast = currentTime;
          if (lastLap < bestLap || bestLap == 0){
              bestLap = lastLap;
          }
//          _state = true;
      }
      
//      uint32_t getTimeLap(uint32_t _startRaceMillis){
//          uint32_t tmp = 0;
//          if (totalLap > 0){
//              tmp = millis() - _startRaceMillis - _tmpLast;
//          }
//          return tmp;
//      }
      
//      bool operator> (const racerBuffer &rankorder)
//      {
//          return (numberLap > rankorder.numberLap);
//          //((numberLap << 8) + meanLap) > ((rankorder.numberLap << 8) + rankorder.meanLap);
//      }
  private:
};

struct racerMemory racerBuffer[NUM_BUFFER];

class Race {
  public:
      bool raceState = false;
      bool connectState = false;
      uint8_t numberTotalLaps = 10;
      uint32_t startRaceMillis = 0;
      uint32_t currentRaceMillis = 0;
      uint32_t endRaceMillis = 0;
      uint32_t standbyHeartbeat;
      uint32_t jsonSendMillis = 0;
      const uint32_t endRaceDelay = 20 * 1000; // 20sec
      uint8_t lastRaceState;
      uint8_t biggerLap = 0;
      
      Race() // add number player?
      {
      }

      void loop(){
          bool trig = false;
          StaticJsonDocument<300> doc;
          
          switch (raceStateENUM) {
              case DISCONNECTED:
                  // only enable Connected button
                  // disable every others button/progress bar/cursor
                  if (connectState) {
                      connectState = false;
                      doc["message"] = "Disconnected!";
                      doc["connect"] = 0;
                      doc["startLock"] = 1;
                      doc["lightLock"] = 0;
                      doc["connectLock"] = 0;
                      doc["sliderLock"] = 1;
                  }
                  break;
                  
              case CONNECTED:
                  // when connected, disable connect button, enable everything
                  // go back to disconnected if no %A& ping.
                  // waiting USER to start a new Race. goto INIT
                  if (!connectState) { // Add sanety check... ie: 20x %A&
                      ricinoSerial.println("%C&");
                      setPing();
                      connectState = true;
                      doc["connect"] = 1;
                      doc["startLock"] = 0;
                      doc["lightLock"] = 0;
                      doc["connectLock"] = 0;
                      doc["sliderLock"] = 0;
                      doc["message"] = "Connected!";
                  }

                  if (connectState){
                      if (millis() - standbyHeartbeat > 5000){
                          raceStateENUM = DISCONNECTED;
                      }
                  }
                  break;

              case SET:
                  for (uint8_t i = 0; i < NUM_BUFFER; i++){
                      racerBuffer[i].reset();
                  }
                  ricinoSerial.println("%I&");
                  startRaceMillis = millis();
                  raceStateENUM = RACE;
                  doc["connectLock"] = 1;
                  doc["sliderLock"] = 1;
                  doc["stopwatch"] = 1;
                  doc["message"] = "RUN RUN RUN";
                  break;

              case RACE:
                  // update dataRacerLoop
                  // the first player have finished, goto END
                  // CHECK if numberTotalLaps have been passed

                  updateDataLoop(false);
                  if (biggerLap == numberTotalLaps - 1){
                      doc["message"] = "Last Lap";
                  }
                  if (biggerLap == numberTotalLaps){
                      endRaceMillis = millis();
                      raceStateENUM = END;
                      doc["message"] = "Hurry Up !";
                  }
                  break;

              case END:
                  // let others finish with a 20sec delay. (don't count if lap > totalLap.)
                  // goto RESET (permit a manual user STOP);
                  updateDataLoop(true); //Special Mode!!
                  if (millis() - endRaceMillis > endRaceDelay){
                      raceStateENUM = RESET;
                      doc["message"] = "Finished !";
                  }
                  break;

              case RESET:
                  // Stop everything, reset all counter...
                  // goto CONNECTED
                  ricinoSerial.println("%F&");
                  raceStateENUM = CONNECTED;
                  doc["connectLock"] = 0;
                  doc["sliderLock"] = 0;
                  doc["stopwatch"] = 0;
                  doc["race"] = 0;
                  zeroing();
                  break;

              default:
                  break;
          }

          if (lastRaceState != raceStateENUM) {
              lastRaceState = raceStateENUM;
              trig = true;
          }

          if (raceStateENUM > 1) {
              if (millis() - jsonSendMillis > 1000) {
                  jsonSendMillis = millis();
                  doc["percentLap"] = (uint8_t)getBiggestLap() / getLaps() * 100;
                  doc["lapCounter"] = getBiggestLap();
                 trig = true;
              }
          }
          
          if (trig) {
              if (!doc.isNull()) {
                  char json[300];
                  serializeJsonPretty(doc, json);
                  Serial.print("Sending :");
                  Serial.println(json);
                  ws.textAll(json);
              }
          }
      }

      void setLaps(uint8_t laps) {
          numberTotalLaps = laps;
      }

      uint8_t getLaps() {
          return numberTotalLaps;
      }

      void setBiggestLap(uint8_t laps) {
          biggerLap = laps;
      }

      uint8_t getBiggestLap() {
          return biggerLap;
      }
      

      void setPing() {
          standbyHeartbeat = millis();
      }
   private:
      const uint16_t cleanerDelay = 1000;
      uint32_t cleanerTimer = millis();

      void zeroing(){
          raceState = false;
          startRaceMillis = 0;
          currentRaceMillis = 0;
          endRaceMillis = 0;
          standbyHeartbeat = millis();
          jsonSendMillis = 0;
          biggerLap = 0;
      }
      void sendJson(){
          
      }
};
Race raceLoop = Race();


void notFound(AsyncWebServerRequest* request) {
  request->send(404, "text/plain", "Not found");
}

// callback for websocket event.
void onWSEvent(AsyncWebSocket* server,
               AsyncWebSocketClient* client,
               AwsEventType type,
               void* arg,
               uint8_t* data,
               size_t length) {
  switch (type) {
    case WS_EVT_CONNECT: 
      {
        Serial.println("WS client connect");
        DynamicJsonDocument doc(JSON_BUFFER);
        doc["light"] = lightState ? 1 : 0;
        doc["connect"] = (raceStateENUM > 1) ? 1 : 0;
        doc["race"] = (raceStateENUM > 3) ? 1 : 0;
        doc["setlaps"] = raceLoop.getLaps();
        char json[JSON_BUFFER];
        serializeJsonPretty(doc, json);
        client->text(json);
      }
      break;
    case WS_EVT_DISCONNECT:
      Serial.println("WS client disconnect");
      break;
    case WS_EVT_DATA:
      {
        AwsFrameInfo* info = (AwsFrameInfo*)arg;
        if (info->final && (info->index == 0) && (info->len == length)) {
          if (info->opcode == WS_TEXT) {
            bool trigger = false;
            data[length] = 0;
            Serial.print("data is ");
            Serial.println((char*)data);
            DynamicJsonDocument doc(JSON_BUFFER);
            deserializeJson(doc, (char*)data);
            if (doc.containsKey("race")) {
              raceStateENUM = (uint8_t)doc["race"] ? SET : RESET;
//              raceState = doc["race"];
              trigger = true;
            }
            if (doc.containsKey("light")) {
              lightState = doc["light"];
              trigger = true;
            }
            if (doc.containsKey("setlaps")) {
              raceLoop.setLaps((uint8_t)doc["setlaps"]);
              trigger = true;
            }
            if (doc.containsKey("connect")) {
              raceStateENUM = (uint8_t)doc["connect"] ? CONNECTED : DISCONNECTED;
//              connectState = doc["connect"];
              trigger = true;
            }
            if (trigger){ //why...?
                char json[JSON_BUFFER];
                serializeJsonPretty(doc, json);
                Serial.print("Sending ");
                Serial.println(json);
                ws.textAll(json);
            }
          } else {
            Serial.println("Received a ws message, but it isn't text");
          }
        } else {
          Serial.println("Received a ws message, but it didn't fit into one frame");
        }
      }
      break;
  }
}


void setup() {
  Serial.begin(9600);
  ricinoSerial.begin(9600, SWSERIAL_8N1, D5, D6, false, 32); // RX, TX
//  ricinoSerial.enableTx(true);

  if(!SPIFFS.begin()){
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }
  
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  pinMode(D1, OUTPUT);
  digitalWrite(D1, LOW);
  // WiFiManager
  // Local intialization. Once its business is done, there is no need to keep it around
  AsyncWiFiManager wifiManager(&server,&dns);
  
  // Uncomment and run it once, if you want to erase all the stored information
  //wifiManager.resetSettings();

  wifiManager.autoConnect("AP_Ricino");

  // Route to index.html + favicon.ico
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/index.html", "text/html");
  });
//  server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request){
//    request->send(SPIFFS, "/favicon.png", "image/png");
//  });
  
  // Route to load css files
  server.on("/css/style.css", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/css/style.css", "text/css");
  });
  server.on("/css/bootstrap.min.css", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/css/bootstrap.min.css", "text/css");
  });
  server.on("/css/bootstrap.min.css.map", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/css/bootstrap.min.css.map", "text/css");
  });

  // Route to load javascript files
  server.on("/js/jquery.min.js", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/js/jquery.min.js", "text/javascript");
  });
  server.on("/js/bootstrap.bundle.min.js", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/js/bootstrap.bundle.min.js", "text/javascript");
  });
  server.on("/js/bootstrap.bundle.min.js.map", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/js/bootstrap.bundle.min.js.map", "text/javascript");
  });
  server.on("/js/scripts.js", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/js/scripts.js", "text/javascript");
  });

//  server.on("/pressure", HTTP_GET, [](AsyncWebServerRequest *request){
//    request->send_P(200, "text/plain", getPressure().c_str());
//  });

//  server.on("/",HTTP_GET, handleRoot);
  server.begin();

  ws.onEvent(onWSEvent);
  server.addHandler(&ws);

}

String millisToMSMs(uint32_t tmpMillis){
    uint32_t millisec;
    uint32_t total_seconds;
    uint32_t total_minutes;
    uint32_t seconds;
    String DisplayTime;

    total_seconds = tmpMillis / 1000;
    millisec  = tmpMillis % 1000;
    seconds = total_seconds % 60;
    total_minutes = total_seconds / 60;

    String tmpStringMillis;
    if (millisec < 100)
      tmpStringMillis += '0';
    if (millisec < 10)
      tmpStringMillis += '0';
    tmpStringMillis += millisec;

    String tmpSpringSeconds;
    if (seconds < 10)
      tmpSpringSeconds += '0';
    tmpSpringSeconds += seconds;

    String tmpSpringMinutes;
    if (total_minutes < 10)
      tmpSpringMinutes += '0';
    tmpSpringMinutes += total_minutes;
    
    DisplayTime = tmpSpringMinutes + ":" + tmpSpringSeconds + "." + tmpStringMillis;

    return DisplayTime;
}

void loop(){
    static bool lightStateLast = lightState;
    static uint32_t cleanerTimer = millis();
    const uint32_t cleanerDelay = 1000;
    
    // Only connected, and waiting...
    if (raceStateENUM > 0) {
        if (receiveSerial()) { //&ricinoSerial)){
            processingData(); //receivedChars, receivedCharsSize);
        }

        if (lightState != lightStateLast){
            lightStateLast = lightState;
            digitalWrite(LED_BUILTIN, !lightState);
            digitalWrite(D1, lightState);
        }
    }

    // Everytime Loop
    if (millis() - cleanerTimer > cleanerDelay){
        ws.cleanupClients();
        cleanerTimer = millis();
//        Serial.printf("[RAM: %d]\r\n", esp_get_free_heap_size());
    }
    raceLoop.loop();
}


void updateRacer(uint8_t idDec, uint32_t timeDec){
// That function update the Struct, or init it.
  // static bool fullBuffer = false;
  bool updated = false;
  uint8_t color = 0;
  // test if idDec is already memorized,
  // if not check the first available racerBuffer. ie: check if .id == 0;
  // if all full (4slots for now), Serial.print(error);

  // First pass try to update Racer
  for (uint8_t i = 0; i < NUM_BUFFER; i++) {
      if (racerBuffer[i].id == idDec) {
          if (racerBuffer[i].numberLap < raceLoop.getLaps()) {
              racerBuffer[i].setCurrentLap(timeDec);
              updated = true;
              debugPrintRacer(i);
          }
          break; // speedup 
      }
  }
  
  // Set and init racer if buffer available
  if (!updated && timeDec < TOO_LATE_FOR_INIT_RACER){ // 20sec after that not possible to get new driver!
      for (uint8_t i = 0; i < NUM_BUFFER; i++){
          if (racerBuffer[i].id == 0){
              racerBuffer[i].color = i + 1;
              racerBuffer[i].init(idDec, timeDec);
//               debugPrintRacer(i);
              break; //finish registration
          }
          else
          {
            //  fullBuffer = true;
             Serial.println("Maximum memorized ID");
          }
      }
  }
}

void updateDataLoop(bool endingRaceState){ //run at each interval OR at each event (serialInput/updateRacer), that is the question...
    bool trigger = false;
    static uint8_t posUpdate = 0;
    static bool needJsonUpdate = false;
    static bool needLapUpdate = false;
    static uint32_t websockCount = 0;
    const uint16_t websockDelay = 200;
    static uint32_t websockTimer = millis();
    static uint32_t websockTimerFast = millis();
    static uint16_t lastTotalLap = 0;
    static uint8_t biggestTotalLap = 0;
//    struct racerMemory tmpRacerBuffer;

    uint32_t tmpRacerBuffer[9] = {0}; // id, numberLap, offsetLap, lastLap, bestLap, meanLap, totalLap

    const String orderString[] = {"one", "two", "three", "four"}; //change by char...

    uint16_t tmpTotalLap = 0;

    for (uint8_t i = 0; i < NUM_BUFFER; i++){
        tmpTotalLap += racerBuffer[i].numberLap;
        if (racerBuffer[i].numberLap > raceLoop.getBiggestLap()){
            raceLoop.setBiggestLap(racerBuffer[i].numberLap);
        }

       if ( i < NUM_BUFFER - 1){
            if (racerBuffer[i + 1].numberLap > racerBuffer[i].numberLap)
            { // My worst code ever, but working code... a way to copy a complete struct without got an esp exception...
                tmpRacerBuffer[0] = racerBuffer[i].id;
                tmpRacerBuffer[1] = racerBuffer[i].numberLap; 
                tmpRacerBuffer[2] = racerBuffer[i].offsetLap;
                tmpRacerBuffer[3] = racerBuffer[i].lastLap;
                tmpRacerBuffer[4] = racerBuffer[i].bestLap;
                tmpRacerBuffer[5] = racerBuffer[i].meanLap;
                tmpRacerBuffer[6] = racerBuffer[i].totalLap;
                tmpRacerBuffer[7] = racerBuffer[i]._tmpLast;
                tmpRacerBuffer[8] = racerBuffer[i].color;
                
                racerBuffer[i].id = racerBuffer[i + 1].id;
                racerBuffer[i].numberLap = racerBuffer[i + 1].numberLap;
                racerBuffer[i].offsetLap = racerBuffer[i + 1].offsetLap;
                racerBuffer[i].lastLap = racerBuffer[i + 1].lastLap;
                racerBuffer[i].bestLap = racerBuffer[i + 1].bestLap;
                racerBuffer[i].meanLap= racerBuffer[i + 1].meanLap;
                racerBuffer[i].totalLap = racerBuffer[i + 1].totalLap;
                racerBuffer[i]._tmpLast = racerBuffer[i + 1]._tmpLast;
                racerBuffer[i].color = racerBuffer[i + 1].color;
                
                racerBuffer[i + 1].id = tmpRacerBuffer[0];
                racerBuffer[i + 1].numberLap = tmpRacerBuffer[1];
                racerBuffer[i + 1].offsetLap = tmpRacerBuffer[2];
                racerBuffer[i + 1].lastLap = tmpRacerBuffer[3];
                racerBuffer[i + 1].bestLap = tmpRacerBuffer[4];
                racerBuffer[i + 1].meanLap = tmpRacerBuffer[5];
                racerBuffer[i + 1].totalLap = tmpRacerBuffer[6];
                racerBuffer[i + 1]._tmpLast = tmpRacerBuffer[7];
                racerBuffer[i + 1].color = tmpRacerBuffer[8];
                break;
            }
       }
    }
    
    if (tmpTotalLap != lastTotalLap || tmpTotalLap == 0){
        lastTotalLap = tmpTotalLap;
        needJsonUpdate = true;
    }
    
    DynamicJsonDocument doc(JSON_BUFFER);

    if( millis() - websockTimerFast > websockDelay){
        currentRaceMillis = millis() - startRaceMillis;
        websockTimerFast = millis();
        doc["websockTimer"] = websockTimer;

        JsonObject data = doc.createNestedObject("data");

        if (millis() - websockTimer > websockDelay / 2 && needJsonUpdate){ //change condition by time to totalLap changed

            websockTimer = millis();
            data[orderString[posUpdate] + "_class"] = racerBuffer[posUpdate].color;
            data[orderString[posUpdate] + "_id"] = racerBuffer[posUpdate].id;
            data[orderString[posUpdate] + "_lap"] = racerBuffer[posUpdate].numberLap;
            data[orderString[posUpdate] + "_last"] = millisToMSMs(racerBuffer[posUpdate].lastLap);
            data[orderString[posUpdate] + "_best"] = millisToMSMs(racerBuffer[posUpdate].bestLap);
            data[orderString[posUpdate] + "_mean"] = millisToMSMs(racerBuffer[posUpdate].meanLap);
            data[orderString[posUpdate] + "_total"] = millisToMSMs(racerBuffer[posUpdate].totalLap);
            data[orderString[posUpdate] + "_current"] = racerBuffer[posUpdate].totalLap; //yes i know...

            posUpdate++; 
            if (posUpdate == NUM_BUFFER){
              posUpdate = 0;
              needJsonUpdate = false;
            }
        }
 
        trigger = true;
    }

    char json[JSON_BUFFER];
    serializeJsonPretty(doc, json);

    if (trigger){
        ws.textAll(json);
    }
}


void debugPrintRacer(uint8_t id){
//  Serial.print("Position: ");
//  Serial.print(racerBuffer[id].rank);
  Serial.print(" | ID: ");
  Serial.print(racerBuffer[id].id);
  Serial.print(" | #lap: ");
  Serial.print(racerBuffer[id].numberLap);
  Serial.print(" | Last: ");
  Serial.print(racerBuffer[id].lastLap);
  Serial.print(" | Best: ");
  Serial.print(racerBuffer[id].bestLap);
  Serial.print(" | Mean: ");
  Serial.print(racerBuffer[id].meanLap);
  Serial.print(" | Total: ");
  Serial.print(racerBuffer[id].totalLap);
  Serial.println();
}

void processingData(){ // char* data, uint8_t dataSize){
// That function take global var 'receivedChars' (change to reference or pointer later...), processing it, and null it.
  uint8_t ampersand = 1;
  uint8_t lapComma;
  uint8_t idxTmp;
  char idHex[8] = {0}; // 0xFFFFFFFF is enough? normally 0xFF is enough
  char timeHex[8]= {0}; // 0xFFFFFFFF is enough?
  
  for (uint8_t i = 1; i < 32; i++) {
    if (receivedChars[i] == '&'){
      ampersand = i;
      break;
    }
  }

  if (receivedChars[0] == '%' && receivedChars[ampersand] == '&'){ // check data integrity
      uint32_t timeDec = 0;
      uint8_t idDec = 0;
      bool trigger = false;
      
      switch (receivedChars[1]){
          case 'A': // sort of ping, pointer or reference should be better than global var.
              raceLoop.setPing();
              break;
          case 'T': // Time
              // %T TIMEinHEX &
              idxTmp = 0;
              for (uint8_t i = 2; i < ampersand; i++){
                  timeHex[idxTmp] = receivedChars[i];
                  idxTmp++;
              }

              timeDec = strtol(timeHex, NULL, 16);
              trigger = true;
//              Serial.print("PASS Time: ");
//              Serial.println(timeDec, 10);
              break;
          case 'L': // User Lap
               // %L XX , TIMEinHEX &
              for (uint8_t i = 1; i < 32; i++) {
                if (receivedChars[i] == ','){
                  lapComma = i;
                  break;
                }
              }
              // ID part
              idxTmp = 0;
              for (uint8_t i = 2; i < lapComma; i++){
                  idHex[idxTmp] = receivedChars[i];
                  idxTmp++;
              }
              idDec = strtol(idHex, NULL, 16);
//              Serial.print("RACER ID: ");
//              Serial.println(idHex);
              // TIME part
              idxTmp = 0;
              for (uint8_t i = lapComma + 1; i < ampersand; i++){
                  timeHex[idxTmp] = receivedChars[i];
                  idxTmp++;
              }
              timeDec = strtol(timeHex, NULL, 16);
              trigger = true;
              updateRacer(idDec, timeDec);
//              Serial.print("RACER Time: ");
//              Serial.println(timeDec, DEC);
              break;
          default:
              Serial.print("Received unknown data: ");
              Serial.println(receivedChars);
              break;
      }
      
      DynamicJsonDocument doc(JSON_BUFFER);
      doc["websockConsole"] = receivedChars;
      if (trigger){
          if (idDec > 0){
              doc["racerId"] = idDec;
              doc["racerTime"] = millisToMSMs(timeDec);
          }
          else
          {
              doc["raceTime"] = millisToMSMs(timeDec);
          }
      }
      char json[JSON_BUFFER];
      serializeJsonPretty(doc, json);
      ws.textAll(json);
      memset(&receivedChars[0], 0, sizeof(receivedChars));
  }
  else
  {
      Serial.print("Received bad data: ");
      Serial.println(receivedChars);
  }
}

// This function could be more "async" and without while loop, as we have start/end caracter '%'/'&'
bool receiveSerial(){ //SoftwareSerial* ss) {
  static byte ndx = 0;
//  char endMarker = '\n';
  char rc;
  bool newData = false;
  uint8_t idx = 0;

  if (ricinoSerial.available()) {
//      Serial.print("Receiving: ");
      while (ricinoSerial.available() > 0) {
         char tmp = ricinoSerial.read();
         receivedChars[idx] = tmp;
         idx++;
         delayMicroseconds(1500); //wait incomming data, at 9600kbs we have 960bytes/s (parity), maybe wait until &, or break if too long...
         newData = true;
         // Futur bug
         // add if ( char == '\r' '\n' or... &) -> break;
         // so we have only ONE message each time and no (%T__&  %L__,___&)
      }
//      Serial.println(receivedChars);
  }
  yield();

  return newData;
}