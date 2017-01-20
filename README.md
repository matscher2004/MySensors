# MySensors
MySensors - Sketches

Sketches are based on MySensors.org example sketches, especially the PulsePowerMeter sketch.
Both sketches are build with the MySensors library version 1.4.1, which are compatible with newer library of MySensors.

1. WaterMeterPulseSensor
  * only analog input pin a0 is used -> additional hardware: inductive proximity sensor
  * with a fix debouncer and threshold
    - 145
  
2. EnergyMeterPulseSenor - contains two sensors
  * only analog input are used
  * pin a0 - energy     -> ir reflex sensor
  * with a fix debouncer and threshold which can be easy modified:
    - define TRIGGERLEVELEHEIGH 303  // highest level -> greater than this, red mark detected
    - define TRIGGERLEVELLOW 285     // lowet level -> lower than this, red mark is gone
    
  * pni a1 - gas meter  -> reed contact
    - 200

Please visit www.mysensors.org for more information