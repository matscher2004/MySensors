// Use this sensor to measure KWH and Watt of your house meeter
// You need to set the correct pulsefactor of your meeter (blinks per KWH).
// The sensor starts by fetching current KWH value from gateway.
// Reports both KWH and Watt back to gateway.
//
// Unfortunately millis() won't increment when the Arduino is in 
// sleepmode. So we cannot make this sensor sleep if we also want 
// to calculate/report watt-number.

#include <SPI.h>
#include <MySensor.h>  

#define SENSOR_ANALOG_PIN_ENERGY 0  // analog pin for the sensor -> for energy meter
#define SENSOR_ANALOG_PIN_GAS 1		// gas sensor at pin 1

#define PULSE_FACTOR 75         // Nummber of blinks per KWH of your meter
#define PULSE_FACTOR_GAS 100    // Nummber of blinks per m3 of your meter (One rotation/liter)

#define MAX_WATT 10000          // Max watt value to report. This filetrs outliers.
#define MAX_FLOW 40             // Max flow (l/min) value to report. This filters outliers.

#define CHILD_ID 1              // Id of the sensor child ENERGY
#define CHILD_ID_GAS 2          // Id of the sensor child GAS

#define TRIGGERLEVELEHEIGH 303  // highest level -> greater than this, red mark detected
#define TRIGGERLEVELLOW 285     // lowet level -> lower than this, red mark is gone

boolean triggerState = false;   // false = not detected / true = detected
boolean triggerStateGas = false;// false = not detected / true = detected

unsigned long SEND_FREQUENCY = 20000; // Minimum time between send (in milliseconds). We don't wnat to spam the gateway.

MySensor gw;
double ppwh = ((double)PULSE_FACTOR)/1000;  // Pulses per watt hour
boolean pcReceived = false;     // energy value received from gateway
boolean pcGasReceived = false;  // gas value received from gateway

volatile unsigned long pulseCount = 0;
unsigned long oldPulseCount = 0;

double ppl = ((double)PULSE_FACTOR_GAS)/1000;       // Pulses per liter
volatile unsigned long pulseCountGas = 0;
unsigned long oldPulseCountGas = 0;
unsigned long lastPulseGas = 0;
unsigned long lastSendGas = 0;
volatile double flow = 0;
double oldflow = 0;
double volume = 0;                     
double oldvolume = 0;

volatile unsigned long lastBlinkEnergy = 0; // energy last detection of red mark
volatile unsigned long lastBlinkGas = 0;    // gas last detection of red mark

volatile unsigned long watt = 0;
  
unsigned long oldWatt = 0;
double oldKwh;
unsigned long lastSend;

#define DEBOUNCERARRAYENERGY 15
int debounceArray[DEBOUNCERARRAYENERGY] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
byte debounceArrayIndex = 0;

#define DEBOUNCERARRAYGAS 10
int debounceGasArray[DEBOUNCERARRAYGAS] = {0,0,0,0,0,0,0,0,0,0};
byte debounceGasArrayIndex = 0;

MyMessage wattMsg(CHILD_ID,V_WATT);
MyMessage kwhMsg(CHILD_ID,V_KWH);
MyMessage pcMsg(CHILD_ID,V_VAR1);

MyMessage flowMsg(CHILD_ID_GAS,V_FLOW);
MyMessage volumeMsg(CHILD_ID_GAS,V_VOLUME);
MyMessage gasValueMsg(CHILD_ID_GAS,V_VAR1);

void setup()  
{
  gw.begin(incomingMessage);

  // Send the sketch version information to the gateway and Controller
  gw.sendSketchInfo("Energy and Gas Meter", "1.0");

  // Register power sensor
  gw.present(CHILD_ID, S_POWER);

  // register gas meter
  gw.present(CHILD_ID_GAS, S_GAS);

  pulseCountGas = oldPulseCountGas = 0;

  // Fetch last known pulse count value from gw
  gw.request(CHILD_ID, V_VAR1);
  gw.request(CHILD_ID_GAS, V_VAR1);
  
  lastSend = millis();
  
  lastSendGas = lastPulseGas = millis();
}


void loop()     
{ 
  gw.process();
  unsigned long now = millis();
  // Only send values at a maximum frequency
  bool sendTime = now - lastSend > SEND_FREQUENCY;
  if (pcReceived && sendTime) {
    // New watt value has been calculated  
    if (watt != oldWatt) {
      // Check that we dont get unresonable large watt value. 
      // could hapen when long wraps or false interrupt triggered
      if (watt<((unsigned long)MAX_WATT)) {
        gw.send(wattMsg.set(watt));  // Send watt value to gw 
      }  
      Serial.print("Watt:");
      Serial.println(watt);
      oldWatt = watt;
    }
  
    // Pulse cout has changed
    if (pulseCount != oldPulseCount) {
      gw.send(pcMsg.set(pulseCount));  // Send pulse count value to gw 
      double kwh = ((double)pulseCount/((double)PULSE_FACTOR));     
      oldPulseCount = pulseCount;
      if (kwh != oldKwh) {
        gw.send(kwhMsg.set(kwh, 4));  // Send kwh value to gw 
        oldKwh = kwh;
      }
    }    
    lastSend = now;
  } else if (sendTime && !pcReceived) {
    // No count received. Try requesting it again
    gw.request(CHILD_ID, V_VAR1);

    lastSend=now;
  }

  if (pcGasReceived && sendTime) {
    if (flow != oldflow) {
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
    if(now - lastPulseGas > 120000){
      flow = 0;
    }
    
    // Pulse count has changed
    if (pulseCountGas != oldPulseCountGas) {
      oldPulseCountGas = pulseCountGas;

      Serial.print("pulsecount:");
      Serial.println(pulseCountGas);

      gw.send(gasValueMsg.set(pulseCountGas));                  // Send  pulsecount value to gw in VAR1

      double volume = ((double)pulseCountGas/((double)PULSE_FACTOR_GAS));     
      if (volume != oldvolume) {
        oldvolume = volume;

        Serial.print("volume:");
        Serial.println(volume, 3);
        
        gw.send(volumeMsg.set(volume, 3));               // Send volume value to gw
      } 
    }
    
    lastSendGas = now;
  } else if(sendTime && !pcGasReceived) {
  // No count received. Try requesting it again
    gw.request(CHILD_ID_GAS, V_VAR1);
  }
  
  // detect red mark and increment count
  CheckEnergyAnalogValueToDetect();

  // detect magnet at gas meter
  CheckGasAnalogValueToDetect();
}

void incomingMessage(const MyMessage &message) {
  if (message.type == V_VAR1 && message.sensor == CHILD_ID) { // energy meter

    pulseCount = oldPulseCount = message.getLong();
    Serial.print("Received last energy pulse count from gw: ");
    Serial.println(pulseCount);
    pcReceived = true;

  } else if (message.type == V_VAR1 && message.sensor == CHILD_ID_GAS) {  // gas meter

    pulseCountGas += message.getLong();
    Serial.print("Received last gas pulse count from gw: ");
    Serial.println(pulseCountGas);
    pcGasReceived = true;
  }
}

/**
 * Detect the red mark at the energy meter
 */
void CheckEnergyAnalogValueToDetect() {

  unsigned long newBlink = micros();  
  unsigned long interval = newBlink-lastBlinkEnergy;

  boolean nextState = triggerState;
  int val = analogRead(SENSOR_ANALOG_PIN_ENERGY);
  int debounceSum = 0;

  debounceArray[debounceArrayIndex] = val;
  for(int i = 0; i < DEBOUNCERARRAYENERGY; i++)
  {
    debounceSum += debounceArray[i];
  }

  if(debounceArrayIndex == DEBOUNCERARRAYENERGY-1)
    debounceArrayIndex = 0;
  else
    debounceArrayIndex++;
  
  // average of DEBOUNCERARRAYENERGY
  val = debounceSum / DEBOUNCERARRAYENERGY;

  if (val > TRIGGERLEVELEHEIGH) {
    nextState = true;
  } else if (val < TRIGGERLEVELLOW) {
    nextState = false;
  }
  if (nextState != triggerState) {
    triggerState = nextState;
    Serial.println(triggerState? 1 : 0);

    if(triggerState)
    {
      if (interval > 0) {
        watt = (3600000000.0 /interval) / ppwh;
      }
      
      lastBlinkEnergy = newBlink;

      pulseCount++;
            
      Serial.print("Pulse detected: ");
      Serial.println(pulseCount);
    }
  }
}

/**
 * Detect the magnet at the gas meter
 */
void CheckGasAnalogValueToDetect() {

  unsigned long newBlink = micros();  
  unsigned long interval = newBlink-lastBlinkGas;

  boolean nextState = triggerStateGas;

  int value = analogRead(SENSOR_ANALOG_PIN_GAS);
  Serial.print("GasSensor value: ");
  Serial.println(value);

  int debounceSum = 0;

  debounceGasArray[debounceGasArrayIndex] = value;
  for(int i = 0; i < DEBOUNCERARRAYGAS; i++)
  {
    debounceSum += debounceGasArray[i];
  }

  if(debounceGasArrayIndex == DEBOUNCERARRAYGAS-1)
    debounceGasArrayIndex = 0;
  else
    debounceGasArrayIndex++;

  // average of DEBOUNCERARRAYGAS
  value = debounceSum / DEBOUNCERARRAYGAS;

  if (value > 145) {
    nextState = false;
  } else if (value == 0) {
    nextState = true;
  }
    
  if(nextState != triggerStateGas)
  {
    triggerStateGas = nextState;

    if(triggerStateGas) {

      if (interval!=0)
      {
        lastPulseGas = millis();
        if (interval<500000L) {
          // Sometimes we get interrupt on RISING,  500000 = 0.5sek debounce ( max 120 l/min)
          return;   
        }
        flow = (60000000.0 /interval) / ppl;
      }
      lastBlinkGas = newBlink;
      
      pulseCountGas++;

      Serial.print("Pulse gas detected: ");
      Serial.println(pulseCountGas);
    }
  }
}