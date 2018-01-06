//
// Use this sensor to measure volume and flow of your house watermeter.
// You need to set the correct pulsefactor of your meter (pulses per m3).
// The sensor starts by fetching current volume reading from gateway (VAR 1).
// Reports both volume and flow back to gateway.
//
// Unfortunately millis() won't increment when the Arduino is in 
// sleepmode. So we cannot make this sensor sleep if we also want  
// to calculate/report flow.
//

#include <SPI.h>
#include <MySensor.h>  

#define SENSOR_ANALOG_PIN_WATER 0               // analog pin for the sensor -> for water meter

#define PULSE_FACTOR 2000                       // Nummber of blinks per m3 of your meter (One rotation/liter)

#define SLEEP_MODE false                        // flowvalue can only be reported when sleep mode is false.

#define MAX_FLOW 40                             // Max flow (l/min) value to report. This filters outliers.

#define CHILD_ID 3                              // Id of the sensor child

unsigned long SEND_FREQUENCY = 20000;           // Minimum time between send (in milliseconds). We don't want to spam the gateway.

boolean triggerState = false;                   // false = not detected / true = detected

MySensor gw;
MyMessage flowMsg(CHILD_ID,V_FLOW);
MyMessage volumeMsg(CHILD_ID,V_VOLUME);
MyMessage lastCounterMsg(CHILD_ID,V_VAR1);

double ppl = ((double)PULSE_FACTOR)/1000;       // Pulses per liter

volatile unsigned long pulseCount = 0;   
volatile unsigned long lastBlink = 0;
volatile unsigned long lastBlinkReal = 0;
volatile double flow = 0;  
boolean pcReceived = false;
unsigned long oldPulseCount = 0;
unsigned long newBlink = 0;   
double oldflow = 0;
double volume =0;                     
double oldvolume =0;
unsigned long lastSend =0;
unsigned long lastPulse =0;

#define DEBOUNCEARRAY_COUNT 200
int debounceArray[DEBOUNCEARRAY_COUNT];
byte debounceArrayIndex = 0;

void setup()  
{
  digitalWrite(SENSOR_ANALOG_PIN_WATER, INPUT_PULLUP); // pullUp
  
  // init debouncer array
  for(int i = 0; i < DEBOUNCEARRAY_COUNT; i++) {
    debounceArray[i] = 255;
  }
  
  gw.begin(incomingMessage); 

  // Send the sketch version information to the gateway and Controller
  gw.sendSketchInfo("Water Meter", "1.2");

  // Register this device as Waterflow sensor
  gw.present(CHILD_ID, S_WATER);       

  pulseCount = oldPulseCount = 0;

  // Fetch last known pulse count value from gw
  gw.request(CHILD_ID, V_VAR1);

  lastSend = lastPulse = millis();
}

void loop()     
{ 
  gw.process();
  unsigned long currentTime = millis();

  // Only send values at a maximum frequency or woken up from sleep
  if (SLEEP_MODE || (currentTime - lastSend > SEND_FREQUENCY))
  {
    lastSend=currentTime;
    
    if (!pcReceived) {
      //Last Pulsecount not yet received from controller, request it again
      gw.request(CHILD_ID, V_VAR1);
      return;
    }

    if (!SLEEP_MODE && flow != oldflow) {
      oldflow = flow;

      Serial.print("l/min:");
      Serial.println(flow);

      // Check that we dont get unresonable large flow value. 
      // could hapen when long wraps or false interrupt triggered
      if (flow<((unsigned long)MAX_FLOW)) {
        gw.send(flowMsg.set(flow, 2));                   // Send flow value to gw
      }  
    }
  
    // No Pulse count received in 2min 
    if(currentTime - lastPulse > 120000){
      flow = 0;
    } 

    // Pulse count has changed
    if (pulseCount != oldPulseCount) {
      oldPulseCount = pulseCount;

      Serial.print("pulsecount:");
      Serial.println(pulseCount);

      gw.send(lastCounterMsg.set(pulseCount));                  // Send  pulsecount value to gw in VAR1

      double volume = ((double)pulseCount/((double)PULSE_FACTOR));     
      if (volume != oldvolume) {
        oldvolume = volume;

        Serial.print("volume:");
        Serial.println(volume, 3);
        
        gw.send(volumeMsg.set(volume, 3));               // Send volume value to gw
      } 
    }
  }
  if (SLEEP_MODE) {
    gw.sleep(SEND_FREQUENCY);
  }
  
  CheckSensor();
}

void incomingMessage(const MyMessage &message) {
  if (message.type == V_VAR1) {
    pulseCount = oldPulseCount = message.getLong();
    Serial.print("Received last pulse count from gw:");
    Serial.println(pulseCount);
    pcReceived = true;
  }
}

void CheckSensor()     
{
  unsigned long newBlink = micros();   
  unsigned long interval = newBlink-lastBlink;
  unsigned long intervalReal = newBlink-lastBlinkReal;
  
  boolean nextState = triggerState;
  int val = analogRead(SENSOR_ANALOG_PIN_WATER);
  
  int debounceSum = 0;

  debounceArray[debounceArrayIndex] = val;
  for(int i = 0; i < DEBOUNCEARRAY_COUNT; i++) {
    debounceSum += debounceArray[i];
  }

  if(debounceArrayIndex == DEBOUNCEARRAY_COUNT-1)
    debounceArrayIndex = 0;
  else
    debounceArrayIndex++;
  
  // average of DEBOUNCEARRAY_COUNT
  val = debounceSum / DEBOUNCEARRAY_COUNT;
  
  Serial.print("Sensor value: ");
  Serial.println(val);
  
  if (val > 145) {
    nextState = false;
  } else if (val == 0) {
    nextState = true;
  }
  
  if (nextState != triggerState) {
    triggerState = nextState;
    //Serial.println(triggerState? 1 : 0);

    if(triggerState) {
      if (interval!=0) {
        if (intervalReal < 1100000L) {
          // Sometimes we get interrupt on RISING,  1100000 = 1,1 sek debounce ( max 60 l/min)
          lastBlinkReal = newBlink;
          return;   
        }
        
        lastPulse = millis();
        
        flow = (60000000.0 /interval) / ppl;
      
        lastBlink = newBlink;

        pulseCount++;
      }
    }
  }
}

