#include <attiny85_ir_send.h>
#include <math.h>

// Transponder Code
// The transponder code is a 2 byte HEX code which
// can range from 0x1000 to 0xFFFF

// 0x00001010  R8 LMS 2015 @287EAF36
// 0x00001011  McLaren P1 GTR @B7CF8A27
// 0x00001012  Toyota lemans @D097C1CC
// 0x00001013  Nissan Fairlady Official car @DDB60357
// 0x00001014  Toyota Vitz RS @3d834070
// 0x00001015  LaFerrari @C86EE88D
// 0x00001016  Porsche GTS @584DC384
// 0x00001017  Fiat 500 Abarth @656C050F
// 0x00001018  Honda Civic Type R @8DA34F8A
// 0x00001019  Subaru Impreza STI 2010 @1CF42A7B
// 0x00001020  Citroen DS3 @CD3118BA
// 0x00001021  Honda HSV-010 @5C81F3AB
//1022 @754A2B50
//1023 @82686CDB
//1024 @E235A9F4
//1025 @6D215211
//1026 @FD002D08
//1027 @A1E6E93
//1028 @74E73AF0
//1029 @FFD2E30D
//1030 @7646E32E

// 0x00006666 LC15E146E
unsigned const long tx_id = 0x1022 ;
//#define F_CPU  8000000


IRsend irsend;
const int ledPin = 0;

void setup() {
    irsend.enableIROut(38);
    pinMode(ledPin, OUTPUT);
//    OSCCAL = 40;
}

void loop() {
    static const byte states[] = {HIGH, LOW};
    const uint16_t intervals[] = {2000, 2000, 60000, 2000};
    static byte state;
    static uint32_t lastBlinkMillis = 0;
    static bool ledState = LOW;

    static uint32_t lastSendMillis = 0;
    static const uint8_t delaySend= 10;

    if (millis() - lastSendMillis >= delaySend)
    {
        irsend.sendNEC(tx_id, 16);
        lastSendMillis = millis();
    }

    if (millis() - lastBlinkMillis >= intervals[state % 4])
    {
      digitalWrite(ledPin, states[state++ % 2]);
      lastBlinkMillis = millis();
    }

}

void lightBreath(int breathe_delay){
  unsigned long loop_led=0;
//  int breathe_delay = 50;
  unsigned long breathe_time = millis();
  if( (breathe_time + breathe_delay) < millis() ){
    breathe_time = millis();
    float val = (exp(sin(loop_led/50.0*PI*10)) - 0.36787944)*108.0; 
    // this is the math function recreating the effect
//    val = map(val,0, 255, 5, 255);
    analogWrite(ledPin, val);  // PWM
    loop_led=loop_led+1;
  }
}
