#include <SoftwareSerial.h>
#include <EEPROM.h>

#include "NewPing.h"
#include "SmartThings.h"

#define DEBUG_ENABLED      1
#define DEBUG_RAW_ENABLED  0
#define SUBMIT_TO_ST       1

#define PIN_THING_RX       3
#define PIN_THING_TX       2
#define PIN_SENSOR_TRIGGER 8
#define PIN_SENSOR_ECHO    7
#define PIN_WORKING_LED    13

#define MAX_DISTANCE       200

#define MESSAGE_BUF        32

#define CONFIG_VERSION    "WL1"
#define CONFIG_START      32
 
NewPing sonar(PIN_SENSOR_TRIGGER, PIN_SENSOR_ECHO, MAX_DISTANCE);

SmartThingsCallout_t messageCallout;
SmartThings smartthing(PIN_THING_RX, PIN_THING_TX, messageCallout);

struct Settings {
  char version[4];
  int floorLevel;
  int safeDepth;
  int warnDepth;
} settings = {
  CONFIG_VERSION,
  0,
  0,
  0
};

int compare_int(const void *a, const void *b) 
{
  return *(const int *)a - *(const int *)b;
}

template <int T> class Queue
{
public:
  int _size;
  int _v[T];
  int _average;

public:
  Queue() 
  {
    _size = 0;
    _average = 0;
    
    memset(&_v, 0, sizeof(_v));
  }

  inline int size() 
  {
    return _size;  
  }

  inline void invalidate()
  {
    _size = 0;
  }

  inline bool valid() 
  {
    return size() == T;
  }
  
  void push(int v) 
  {
    if (v > 0) 
    {
      for (int i = 1; i < T; ++i)     
      {
        _v[i - 1] = _v[i];
      }
  
      _v[T - 1] = v;

      ++_size;
      if (_size > T) 
      {
        _size = T;
      }
    }

    update_average();
  }

  inline int* values() 
  {
    return _v;  
  } 

  void update_average() 
  {
    float v = 0;
    for (int i = 0; i < T; ++i) 
    {
      v += _v[i];
    }

    _average = (int)(v / T);
  }

  inline int average() 
  {
    return _average;
  }
};

Queue<40> measurements;
float lastDistance = 0;

int lastLedState = 0;
unsigned long lastCheck = 0;
unsigned long lastUpdate = 0;

char message[MESSAGE_BUF];

void loadConfiguration() {
  if (EEPROM.read(CONFIG_START + 0) == CONFIG_VERSION[0] &&
      EEPROM.read(CONFIG_START + 1) == CONFIG_VERSION[1] &&
      EEPROM.read(CONFIG_START + 2) == CONFIG_VERSION[2]) {
    for (unsigned int t = 0; t < sizeof(settings); t++)
      *((char*)&settings + t) = EEPROM.read(CONFIG_START + t);
  }
}

void saveConfiguration() {
  for (unsigned int t = 0; t < sizeof(settings); t++)
    EEPROM.write(CONFIG_START + t, *((char*)&settings + t));
}

void setup() {
  Serial.begin(9600);

  pinMode(PIN_WORKING_LED, OUTPUT);

  smartthing.shieldSetLED(0, 0, 2);

  loadConfiguration();

  lastCheck = millis();
}

void loop() {
  unsigned long time = millis();

  if (time - lastCheck > 250) {
    lastLedState = !lastLedState;    
    digitalWrite(PIN_WORKING_LED, lastLedState ? HIGH : LOW);

    measurements.push(sonar.ping());    

    if (measurements.valid()) 
    {
      float cm = (float)measurements.average() / US_ROUNDTRIP_CM;
      
#if DEBUG_RAW_ENABLED
      Serial.print("Raw: ");
      Serial.print(cm);

      float diff = lastDistance - cm;

      if (diff >= 0) 
      {
        Serial.print(" +");
      } 
      else
      {
        Serial.print(" ");
      }

      Serial.print(diff);
      
      Serial.print("  :: Last 10 :: ");
      
      for (int i = max(0, measurements.size() - 10); i < measurements.size(); ++i) 
      {
        Serial.print((float)measurements.values()[i] / US_ROUNDTRIP_CM);
        Serial.print(" ");
      }
      
      Serial.println("");
#endif  

      if (abs(lastDistance - cm) > 0) {
        lastDistance = cm;

#if DEBUG_ENABLED       
        Serial.print("Ping: ");
        Serial.print(cm);
        Serial.println("cm");
#endif        

        if (settings.floorLevel > 0 && settings.safeDepth > 0) {
          int depth = settings.floorLevel - (int)cm;

          if (depth <= settings.warnDepth) {
            smartthing.shieldSetLED(0, 4, 0);
          } else {
            if (depth <= settings.safeDepth) {
              smartthing.shieldSetLED(6, 4, 0); 
            } else {
              smartthing.shieldSetLED(8, 0, 0);               
            }
          }
        }

#if SUBMIT_TO_ST
#if DEBUG_ENABLED
        Serial.print("Update ST...");
#endif
        
        snprintf(message, sizeof(message), "level:%d", (int)cm);
        smartthing.send(message);
#endif        
      }        
    }
    
    lastCheck = time;
  }
  
  // Run smartthing logic.
  smartthing.run();
}

void configure(String& message) {
  sscanf(message.c_str() + 5, "%d %d %d", &settings.floorLevel, &settings.safeDepth, &settings.warnDepth);

#if DEBUG_ENABLED       
  Serial.print("Configure floor level ");
  Serial.print(settings.floorLevel);
  Serial.print(" and safe depth ");
  Serial.print(settings.safeDepth);
  Serial.print(" and warn depth ");
  Serial.print(settings.warnDepth);
#endif        

  saveConfiguration();
}

void messageCallout(String message)
{
#if DEBUG_ENABLED
    Serial.print("Received message: '");
    Serial.print(message);
    Serial.println("' ");
#endif

  if (message.length() > 3) {
    if (message[0] == 'c' && message[1] == 'n' && message[2] == 'f' && message[3] == 'g') {
      configure(message);
    }
  }
}
