// Protean Blaster Fire Control Driver
// Variant: Full-Software Control (FSC)

// TODO: Implement debounce otherwise the blaster literally will binary trigger
// This firmware variant for Protean assumes a Seeed XIAO RP2040 (running Arduino), and that
// the user wants to yield control over the flywheel motors and solenoid pusher fully
// to the microcontroller unit (MCU). This means that upon pulling the trigger, the flywheel
// motors will receive power while a pre-determined feed-delay will delay firing until the
// flywheels are sufficiently spun up. When a shot was recently fired and the flywheels are still spinning,
// a time-dependent reduced feed delay is invoked to allow for quicker follow-up shots.

// See other variants: Pusher Control Only (PCO)
// by: Flygonial
// Licensed under the Creative Commons Zero (CC0) license
// I make no warranties and disclaim all liabilities associated (and endorsements unless solicited and given consent)
// Redistribute, modify, or sell with or without my permission
// I won't complain unless you try to claim it as your own lol

// PINOUT:
int fMode1 = 0; // Signal output to select-fire switch (firemode 1)
int fMode2 = 1; // Pullup input from D0 to signal firemode 2
int fMode3 = 2; // Pullup input from D0 to signal firemode 3
int trigLine = 3; // Signal output to the trigger
int fWheel = 4; // Signal for flywheel gate driver
int fireNoid = 5; // Signal for solenoid gate driver
int trigger = 6; // Pullup input signal showing the trigger has been depressed

// Defining parameters
// Feed delay must be optimized on a motor/flywheel set-up basis, this may be too short or long for your set-up
unsigned long feedDelay = 200; // Dead-stop feed delay in milliseconds
// Note that on 3S, a Neutron is not capable of greater than 600 RPM, on 4S this is 1080 RPM. With fast-decay, 
// 2400 RPM but you will likely have reliability issues far sooner.
unsigned long fireRate = 750; // Determines the fully-automatic firerate in rounds per minute
unsigned long burstRate = 900; // Determines the firerate for burst-fire
int burstNum = 2; // Number of rounds per burst to be fire
bool hyperBurst = false; // Flag for hyper burst mode
unsigned long hyperRate = 1500; // Firerate for hyper bursts, 1500 is the max recommended but you can go faster.
unsigned long spinDownTime = 4000; // Time in milliseconds for how long the motors take to spin down fully
unsigned long lastShot = spinDownTime + 1; // Counter of the milliseconds since the flywheels have been allowed to coast
double dutyCycle = 0.3; // Set the duty cycle of the solenoid 
bool boot = true;
int state;
attachInterrupt(digitalPinToInterrupt(trigger), shotDetection,RISING);

void autoFire(unsigned long ROF, double dutyCycle) {
  digitalWrite(fireNoid, HIGH);
  delay(round((1000*dutyCycle)/(ROF/60))); // Convert ROF to duty-cycle dependent delays (ms per round)
  digitalWrite(fireNoid, LOW);
  delay(round((1000*(1-dutyCycle))/(ROF/60)));
}

void shotDetection(){
  if (( millis() - lastShot) > 25 ){ // 20 ms debounce
    if (trigger == HIGH) {
      trigger = LOW;
      lastShot = millis();
    }
  }
}

void spinUpDelay(unsigned long timeElapsed) {
  if (timeElapsed > spinDownTime) {
    delay(feedDelay);
  }
  // Square Ratio: Based on the assumption that since kinetic energy is a square function of angular velocity,
  // that a time-based feed delay is better modeled this way than purely linearly
  double sqRatio = roundf(pow((timeElapsed/feedDelay), 2) * 100) / 100;
  // 10% spin-up safety margin:
  if (sqRatio - 0.1 > < 0) {
    sqRatio = 0;
  }
  else {
    sqRatio -= 0.1; // Add a 10% safety margin
  }
  else
  int castedDelay = feedDelay - (sqRatio * feedDelay);
  delay(castedDelay);
}

void setup() {
  // Configure I/O Pins, identify select-fire switch position, wait for MCU to boot
  pinMode(0, OUTPUT);
  pinMode(1, INPUT);
  pinMode(2, INPUT);
  pinMode(3, OUTPUT);
  pinMode(4, OUTPUT);
  pinMode(5, OUTPUT);
  pinMode(6, INPUT);
  digitalWrite(trigLine, HIGH); // This will just always be high\
  // Run a wait upon boot
  delay(1000); // Wait for the RP2040 to fully boot
  // Check for which position the fire selector switch is in to configure FSM
  if (!(digitalRead(fMode2) == 0) && !(digitalRead(fMode3) == 0)) {
    state = 0;
  } 
  else if (digitalRead(fMode2) == 0) {
    state = 1;
  }
  else if (digitalRead(fMode3) == 0) {
    state = 2;
  }
}

void loop() {
  // Want to run the flywheels for a little bit after the last shot. This technically messes with the
  // timing of what's needed for the next spin-up but having safety margins isn't necessarily bad.
  if (millis() - lastShot > 100) {
    digitalWrite(fWheel, LOW);
  }
  switch(state) {
    // Semi-Auto
    case 0:
      if (digitalRead(trigger) == 0) {
        digitalWrite(fWheel, HIGH);        
        spinUpDelay(millis() - lastShot);
        // By default this is limited to 600 RPM
        // To do: If there is a good way of allowing for shots to be cancelled, I'm all ears but one isn't implemented here
        // as it would make semi-auto feel awful if short trigger presses on follow up shots were filtered out
        if (digitalRead(trigger) == 0) {
          digitalWrite(fireNoid, HIGH);
        delay(round(100*dutyCycle)); // You can lower this value if you're either running on 4S and/or have a fast decay driver
        digitalWrite(fireNoid, LOW);
        delay(round(100*(1-dutyCycle)));
        // digitalWrite(fWheel, LOW); // Uncomment if you want to stop the flywheels the second the solenoid returns
        }
        lastShot = millis();        
      }
      else if (digitalRead(fMode2) == 0) {
        state = 1;
      }
      else if (digitalRead(fMode3) == 0) {
        state = 2;
      }

    // This mode is either burst-fire or hyper-burst into auto
    case 1:
      if (hyperBurst == false) {
        if (digitalRead(trigger) == 0) {
          digitalWrite(fWheel, HIGH);        
          spinUpDelay(lastShot);
          for (int i = 0; i < burstNum; i++) {
            autoFire(burstRate, dutyCycle);
          }
          // digitalWrite(fWheel, LOW);
          lastShot = millis();               
        }
      }
      if (hyperBurst == true) {
        if (digitalRead(trigger) == 0) {
          digitalWrite(fWheel, HIGH);        
          spinUpDelay(lastShot);
          for (int i = 0; i < burstNum; i++) {
            autoFire(hyperRate, dutyCycle);
          }
          while (digitalRead(trigger) == 0) { // While trigger is still being held
            autoFire(fireRate, dutyCycle);
          }
          digitalWrite(fWheel, LOW);
          // lastShot = millis();
        }
      }
      else if (!(digitalRead(fMode2) == 0) && !(digitalRead(fMode3) == 0)) {
        state = 0;
      }
      else if (digitalRead(fMode3) == 0) {
        state = 2;
      }
    
    // Full auto mode (no burst)
    case 2:
      if (digitalRead(trigger) == 0) {
        digitalWrite(fWheel, HIGH);        
        spinUpDelay(lastShot);
        while (digitalRead(trigger) == 0) { // While trigger is still being held
          autoFire(fireRate, dutyCycle);
        }
        // Start counting up from the time since the last trigger pull
        digitalWrite(fWheel, LOW);
        // lastShot = millis();
      }
      else if (!(digitalRead(fMode2) == 0) && !(digitalRead(fMode3) == 0)) {
        state = 0;
      }
  }
}