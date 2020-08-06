# Device Drivers for HC-SR04

* This project revolves around working on HC-SR04 ultrasonic distance sensors. 
* After sending a “ping” signal on its trigger pin, the HC-SR04 sensor sends 8 40khz square wave pulses and automatically detect whether it receives any echo from a distance object. 
* So the pulse width on the echo pin tells the time of sound traveled from the sensor to measured distance and back. 
* The distance to the object is computed as
distance = (pulse width * ultrasonic spreading velocity in air) / 2
* To connect each HC-SR04 sensor to Galileo Gen2 board, two digital IO pins of Arduino connector are used for trigger and echo signals. They are configured as Linux GPIO pins.
* The edges on echo pin triggers interrupts and the differences of TSC’s (time-stamp counter) of x86 processor at the positive and negative edges can give a precise measure of pulse width.
