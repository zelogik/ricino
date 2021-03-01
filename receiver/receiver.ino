// **********************************************************************************
//
// Receiver
//
// **********************************************************************************
#define RED_PIN 4
#define GREEN_PIN 6
#define RECV_PIN 10
#define LIGHT_PIN 11
#define BUZZER_PIN 5

#include <SoftwareSerial.h>
SoftwareSerial ESPSerial(3, 2); // RX | TX

#include <IRremote.h>
IRrecv irrecv(RECV_PIN);
decode_results results;

#include <NewTone.h>

#define NUMBER_CAR_DEBOUNCE 8  //Normally we can increase that to 32 without to much trouble... who try?
uint32_t debounce_id[NUMBER_CAR_DEBOUNCE];
uint32_t debounce_last_time[NUMBER_CAR_DEBOUNCE];
uint32_t debounce_current_time[NUMBER_CAR_DEBOUNCE];
const uint32_t debounce_delay = 3000;
//unsigned long already_sent = 0;

class Buzzer
{
private:
  bool _enabled = true;
  uint16_t _buzzerDelay[8] = {0};
  uint16_t _buzzerFreq[8] = {0};
  bool _tonePending  = false;
  uint8_t _pin;
  uint8_t _currentLoop = 0;
  uint8_t _currentLoopLength = 0;
  uint32_t _lastMillis = 0;

public:
  Buzzer(uint8_t pin){
      this->_pin = pin;
  }
//  ~Buzzer();

  void init(bool state){
      _enabled = state;
  }

  bool getTonePending(){
      return _tonePending;
  }

  void setTone(uint16_t frequency[], uint16_t duration[]){   
      this->_currentLoopLength = 8;
      for (int i = 0 ; i < _currentLoopLength ; i++)
      {
          this->_buzzerDelay[i] = *(duration + i);
          this->_buzzerFreq[i] = *(frequency + i);
      }
      this->_tonePending = true;
//      this->_currentLoop = 0;
  }

  void loop(){
      if (_tonePending && _enabled)
      {
          if ((millis() - _lastMillis) >= _buzzerDelay[_currentLoop])
          {
              Serial.println(_buzzerDelay[_currentLoop]);
              Serial.println(_buzzerFreq[_currentLoop]);
              Serial.println();
              NewTone(_pin, _buzzerFreq[_currentLoop]);
              _lastMillis = millis();
              _currentLoop++;
          }
      }

      if (_currentLoop >= _currentLoopLength)
      {
          _currentLoop = 0;
          _tonePending = false;
          noNewTone(_pin);
      }
  }
};


class Led // lKeep RED and GREEN led active
{
private:
    uint8_t _ledPin;
    uint32_t OnTime = 1000;     // milliseconds of on-time
    uint32_t OffTime = 1000;    // milliseconds of off-time
    bool ledState = LOW;                 // ledState used to set the LED
    uint32_t previousMillis;   // will store last time LED was updated
 
    void setOutput(bool state_, uint32_t currentMillis_){
        ledState = state_;
        previousMillis = currentMillis_;
        digitalWrite(_ledPin, state_);
    }

public:
    Led(uint8_t _ledPin)
    {
        this->_ledPin = _ledPin;
        pinMode(_ledPin, OUTPUT);
        previousMillis = 0; 
    }

    void set(uint32_t on, uint32_t off){
        OnTime = on;
        OffTime = off;
    }

    void loop(){
        uint32_t currentMillis = millis();
            
        if((ledState == HIGH) && (currentMillis - previousMillis >= OnTime))
        {
            setOutput(LOW, currentMillis);
        }
        else if ((ledState == LOW) && (currentMillis - previousMillis >= OffTime))
        {
            setOutput(HIGH, currentMillis);
        }
    }
};

Buzzer buzzer = Buzzer(BUZZER_PIN);

Led redLed = Led(RED_PIN);
Led greenLed = Led(GREEN_PIN);

void setup() {
    Serial.begin(9600);
    // ESPSerial.begin(9600);
    pinMode(RED_PIN,OUTPUT);
    pinMode(GREEN_PIN,OUTPUT);
    pinMode(LIGHT_PIN,OUTPUT);

    irrecv.enableIRIn();
}

void loop() {
    char message[3];

    buzzer.loop();
    redLed.loop();
    greenLed.loop();

    ricinoLoop();

  // while (ESPSerial.available() > 0) {
  //   for (int i = 0; i < 3; i++) {
  //       message[i] = ESPSerial.read();
  //   }
  //   ESPSerial.flush(); //tricky bug with espsoftwareserial?
  //   break;
  // }
}

void ricinoLoop(){
    static uint32_t lastHeartbeat = 0;
    static const uint32_t delayHeartbeat = 5 * 1000; // send message every 5sec
    static uint32_t runningStart = 0;
    static uint32_t connectLastMillis = 0;
    static uint8_t count = 0;

    enum RaceState {
        off,
        standby,
        race
    };
    static RaceState raceState = off;

    char message[3] = {0};
    while (Serial.available() > 0) {
        for (int i = 0; i < 3; i++) {
            message[i] = Serial.read();
            delay(10);
        }
    }

    // Check message[] integrity/checksum
    if (message[0] == '%' && message[2] == '&')
    {
        char byte_received = message[1];

        uint16_t melody_start[] = {100, 0, 100, 0, 100, 0, 500, 650};
        uint16_t freq_start[] = { 250, 750, 250, 750, 250, 750, 250, 250};
        uint16_t melody_pass[] = {262, 100, 40, 400, 406, 0, 147, 2552 };
        uint16_t freq_pass[] = { 250, 125, 125, 250, 250, 250, 250, 250};
        uint16_t melody_stop[] = { 254, 196, 24, 220, 196, 0, 247, 24 };
        uint16_t freq_stop[] = { 250, 125, 125, 250, 250, 250, 250, 250};

        switch (byte_received) {
        case 'I': // init timer
            if (raceState = standby)
            {
                raceState = race;
                lastHeartbeat = 0;
                runningStart = millis();
                greenLed.set(700,300);
                redLed.set(0,1000);
                buzzer.setTone(melody_start, freq_start);
            }
            break;

        case 'F': // End connection
            if (raceState == race)
            {
                raceState = standby;
                greenLed.set(0,1000);
                redLed.set(900,100);
                buzzer.setTone(melody_stop, freq_stop);
            }
            break;

        case 'C': // connection
            buzzer.setTone(melody_pass, freq_pass);
            greenLed.set(35,35);
            redLed.set(50,50);
            while (count <= 20)
            { // Message: Confirmation "%A&"
                if (millis() - connectLastMillis >= 50)
                {
                    Serial.print("%A&");
                    Serial.println();
                    connectLastMillis = millis();
                    count++;
                }
                greenLed.loop();
                redLed.loop();
                buzzer.loop();
            }
            raceState = standby;
            greenLed.set(0,1000);
            redLed.set(900,100);
            break;

        default:
            break;
        }
    }

    if (raceState == race)
    {
        if (millis() - lastHeartbeat >= delayHeartbeat)
        {
            uint32_t tmpMillis = (millis() - runningStart);
            Serial.print("%T");
            Serial.print(tmpMillis,HEX);
            Serial.print("&");
            Serial.print("\r\n");
            lastHeartbeat = millis();
        }

        if (irrecv.decode(&results))
        {
          if (results.decode_type == NEC)
          {
//            lap_counter(results.value);
            irrecv.resume();
          }
        }
    }
}

// **********************************************************************************
//
//void lap_counter(byte _raceStatus) { //print Racer Lap Time
//  
//    int tx_row = check_transponder(_raceStatus);
//
//    if (tx_row < NUMBER_CAR_DEBOUNCE && _raceStatus == 1) { //no more than 8 car registration possible or need to change this number everywhere...
//      digitalWrite(RED_PIN, HIGH);
////      if (chooseSerial){
//        ESPSerial.print("%L");
//        ESPSerial.print(debounce_id[tx_row], HEX);
//        ESPSerial.print(",");
//        ESPSerial.print(debounce_last_time[tx_row], HEX);
//        ESPSerial.print("&");
//        ESPSerial.println();
////        sound_send( 100, 250);
//
////      }else if(fromSerial){     
////        Serial.print("%L");
////        Serial.print(debounce_id[tx_row], HEX);
////        Serial.print(",");
////        Serial.print(debounce_current_time[tx_row], HEX);
////        Serial.print("&");
////        Serial.println();
////      }       
//      digitalWrite(RED_PIN, LOW);
//      light_array_status = 5;
//    }
//    else if(_raceStatus == 2){
//        ESPSerial.print("%L");
//        ESPSerial.print(debounce_id[tx_row], HEX);
//        ESPSerial.print("&");
//        ESPSerial.println();
//    }
//    
//}
//
//
//// **********************************************************************************
//
//int check_transponder(byte _raceStatus) {  //With free DEBOUNCING included ( could toggle on/off for pure compatibility with zround ?)
//    static const uint32_t MAGIC_NUMBER = 42; // not proud about this but i haven't found another way....
//
//    uint32_t tx = 0; // magic number because 0 == NULL = ""...
//    int debounce_tx_row = NUMBER_CAR_DEBOUNCE;
////    unsigned long time_counter;
//
//    if (results.value >= 0x00000000 && results.value <= 0xFFFFFFFF ) { // unsigned long vs uint32_t on attiny85..?
//        switch (results.value) {
//            case 0x287EAF36: tx=1; break; // R8 LMS 2015
//            case 0xB7CF8A27: tx=2; break; // McLaren P1 GTR
//            case 0xD097C1CC: tx=3; break; // Toyota lemans
//            case 0xDDB60357: tx=4; break; // Nissan Fairlady Official car
//            case 0x3d834070: tx=5; break; // Toyota Vitz RS 
//            case 0xC86EE88D: tx=6; break; // LaFerrari
//            case 0x584DC384: tx=7; break; // Porsche GTS
//            case 0x656C050F: tx=8; break; // Fiat 500 Abarth
//            case 0x8DA34F8A: tx=9; break; // Honda Civic Type R
//            case 0x1CF42A7B: tx=10; break; // Subaru Impreza STI 2010
//            case 0xCD3118BA: tx=11; break; // Citroen DS3 
//            case 0x5C81F3AB: tx=12; break; // Honda HSV-010 
//            case 0x754A2B50: tx=13; break; // 
//            case 0x82686CDB: tx=14; break; // 
//            case 0xE235A9F4: tx=15; break; // 
//            case 0x6D215211: tx=16; break; //
//            case 0xFD002D08: tx=17; break; // 
//            case 0xA1E6E93:  tx=18; break; // 
//            case 0x74E73AF0: tx=19; break; //
//            case 0xFFD2E30D: tx=20; break; // 
//            case 0x7646E32E: tx=21; break; // 
//            default:  
//              if (_raceStatus == 2){
//                tx=results.value;
//              }else{
//                tx=42; break;
//              }
//        }
//    }
//
//    if (tx > 0 && tx < MAGIC_NUMBER && _raceStatus == 1){ // why .. dont' ask... sort of filtering 
//      time_counter = millis() - timeStart;
//      for (uint8_t i = 0; i < NUMBER_CAR_DEBOUNCE; i++) { //Debounce Loop max, 8 car on each race. and memorise it!
//        if (debounce_id[i] == tx){
//          debounce_current_time[i] = time_counter;
//          debounce_tx_row = i;
//          break;
//        }
//        else if (debounce_id[i] == 0 ){ //init ID debounce
//          debounce_id[i] = tx;
//          debounce_current_time[i] = time_counter;
////          debounce_last_time[i] = time_counter;  //maybe....
//          debounce_tx_row = i;
//          break;
//        }
//      }
//      if ( debounce_tx_row < NUMBER_CAR_DEBOUNCE){
//        if ( debounce_current_time[debounce_tx_row] < debounce_last_time[debounce_tx_row] + debounce_delay){
//          debounce_last_time[debounce_tx_row] = time_counter;
//          debounce_tx_row = NUMBER_CAR_DEBOUNCE;
//        }else{
//          debounce_last_time[debounce_tx_row] = time_counter;
//          return debounce_tx_row;
//        }
//      }
//    }else if (_raceStatus == 2){
//      debounce_id[0] = tx;
//      debounce_tx_row = 0;
//    }
//    return debounce_tx_row;
//}
