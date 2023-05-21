//**************************************************************//
//  Name    : TWKMIDI Controller                                //
//  Author  : Troy Meier                                        //
//  Date    : 2022-12-22                                        //
//  Version : 0.1                                               //
//  Notes   : Shift register code adapted from                  //
//          : http://www.arduino.cc/en/Tutorial/ShiftIn         //
//****************************************************************

#include <TaskScheduler.h>

//define where your pins are
const int latch_pin = 15;
const int data_pin = 1;
const int clock_pin = 14;

const int default_velocity = 99;

// the MIDI channel number to send messages
const int key_channel = 1;
const int drum_channel = 10;

int octave = 5;
int octave_adjustment = 0;

int board_led = LED_BUILTIN;
int board_led_state = false;

//Define variables to hold the data 
//for shift register.
//starting with a non-zero numbers can help
//troubleshoot
byte input_shift_1_byte = 72;  //01001000
byte input_shift_2_byte = 72;  //01001000
byte input_shift_3_byte = 72;  //01001000
byte input_shift_4_byte = 72;  //01001000

// Output mapping for input shift registers
// {type, value, channel}
// type can be:
// 0 = note on/off
//     Patch and note details: https://www.midi.org/specifications-old/item/gm-level-1-sound-set
//     {0, note number, channel}
// 1 = controlChange momentary
//     Control Change details: https://anotherproducer.com/online-tools-for-musicians/midi-cc-list/
//     9, 14, 15, 20-31 are undefined control change numbers that can safely be used
//     {1, control change code, channel}
// 2 = octave change
//     {2, 0, 0} = Octave up
//     {2, 1, 0} = Octave down
// 3 = controlChange toggle
//     Control Change details: https://anotherproducer.com/online-tools-for-musicians/midi-cc-list/
//     9, 14, 15, 20-31 are undefined control change numbers that can safely be used
//     The third element holds the toggle's state
//     {3, 15, false}
int input_shift_1_output[8][3] = {
  {0, 72, key_channel}, 
  {0, 73, key_channel}, 
  {0, 74, key_channel}, 
  {0, 75, key_channel},
  {0, 76, key_channel},
  {0, 77, key_channel},
  {0, 78, key_channel},
  {0, 79, key_channel}
};
int input_shift_2_output[8][3] = { 
  {0, 80, key_channel},
  {0, 81, key_channel},
  {0, 82, key_channel},
  {0, 83, key_channel},
  {0, 84, key_channel},
  {2, 0, 0}, // Octave up
  {2, 1, 0}, // Octave down
  {1, 15, key_channel} // Not used
}; 
int input_shift_3_output[8][3] = {
  {0, 36, drum_channel},
  {0, 37, drum_channel},
  {0, 38, drum_channel},
  {0, 39, drum_channel},
  {0, 40, drum_channel},
  {0, 41, drum_channel},
  {0, 42, drum_channel},
  {0, 43, drum_channel}
};  
int input_shift_4_output[8][3] = {
  {0, 44, drum_channel},
  {0, 45, drum_channel},
  {0, 46, drum_channel},
  {0, 47, drum_channel},
  {3, 15, false},
  {3, 20, false},
  {3, 21, false},
  {1, 22, key_channel} // Not used
 };

// Track the state of shift registers
int input_shift_1_state[8] = {
  false, false, false, false, false, false, false, false
};
int input_shift_2_state[8] = {
  false, false, false, false, false, false, false, false
};
int input_shift_3_state[8] = {
  false, false, false, false, false, false, false, false
};
int input_shift_4_state[8] = {
  false, false, false, false, false, false, false, false
};

void blink();
void pollInputs();

Task blinkTask(1000, TASK_FOREVER, &blink);
Task pollTask(50, TASK_FOREVER, &pollInputs);

Scheduler scheduler;

void setup() {
  //start serial
  Serial.begin(9600);

  //define pin modes
  pinMode(latch_pin, OUTPUT);
  pinMode(clock_pin, OUTPUT); 
  pinMode(data_pin, INPUT);

  pinMode(board_led, OUTPUT);

  // Set up the scheduler
  scheduler.init();
  scheduler.addTask(blinkTask);
  scheduler.addTask(pollTask);

  delay(250);

  blinkTask.enable();
  pollTask.enable();
}

void loop() {
  // Run scheduled tasks
  scheduler.execute();
  while (usbMIDI.read()) {
     // controllers must call .read() to keep the queue clear even if they are not responding to MIDI
  }
}

void blink()
{
  board_led_state ? board_led_state=false : board_led_state=true;
  digitalWrite(board_led, board_led_state);
}

void pollInputs() {
  //Pulse the latch pin:
  //set it to 1 to collect parallel data
  digitalWrite(latch_pin,1);
  //set it to 1 to collect parallel data, wait
  //delayMicroseconds(20);
  //set it to 0 to transmit data serially  
  digitalWrite(latch_pin,0);

  //while the shift register is in serial mode
  //collect each shift register into a byte
  //the register attached to the chip comes in first 
  input_shift_1_byte = shiftIn(data_pin, clock_pin);
  input_shift_2_byte = shiftIn(data_pin, clock_pin);
  input_shift_3_byte = shiftIn(data_pin, clock_pin);
  input_shift_4_byte = shiftIn(data_pin, clock_pin);

  //Print out the results.
  //leading 0's at the top of the byte 
  //(7, 6, 5, etc) will be dropped before 
  //the first pin that has a high input
  //reading  
  Serial.println(input_shift_1_byte, BIN);
  Serial.println(input_shift_2_byte, BIN);
  Serial.println(input_shift_3_byte, BIN);
  Serial.println(input_shift_4_byte, BIN);


  //This for-loop steps through the byte
  //bit by bit which holds the shift register data 
  //and if it was high (1) then it prints
  //the corresponding location in the array
  for (int n=0; n<=7; n++)
  {
    //so, when n is 3, it compares the bits
    //in switchVar1 and the binary number 00001000
    //which will only return true if there is a 
    //1 in that bit (ie that pin) from the shift
    //register.

    handleOutput(input_shift_1_byte & (1 << n), input_shift_1_output[n], input_shift_1_state, n);
    handleOutput(input_shift_2_byte & (1 << n), input_shift_2_output[n], input_shift_2_state, n);
    handleOutput(input_shift_3_byte & (1 << n), input_shift_3_output[n], input_shift_3_state, n);
    handleOutput(input_shift_4_byte & (1 << n), input_shift_4_output[n], input_shift_4_state, n);    
  }
}

void handleOutput(int triggered, int *output_array, int *state, int thisBit) {
  int adjust_by = (output_array[0] == 0 && output_array[2] != drum_channel) ? octave_adjustment : 0;
  if (triggered){ // Button is in a pressed stated
      if (state[thisBit] == false) {
        //Keyboard.print(input_shift_4_output[n]); 
        switch(output_array[0]) {
          case 0:
            // Note
            usbMIDI.sendNoteOn(output_array[1] + adjust_by, default_velocity, output_array[2]);
            break;
          case 1:
            // Control Change momentary
            usbMIDI.sendControlChange(output_array[1], 127, output_array[2]);
            break;
          case 2: 
            // Octave change
            break; 
          case 3:
            // Control Change Toggle
            break;
          default:
            break;
        }
        state[thisBit] = true;
      }        
    } else { // Button is not pressed
      if (state[thisBit] == true) { // Button was just released
        switch(output_array[0]) {
          case 0:
            usbMIDI.sendNoteOff(output_array[1] + adjust_by, default_velocity, output_array[2]);
            break;
          case 1:
            usbMIDI.sendControlChange(output_array[1], 0, output_array[2]);
            break;           
          case 2:
            usbMIDI.sendControlChange(123, 0, key_channel);
            if (output_array[1] == 0) {
              if (octave != 9) {
                ++octave;
                octave_adjustment = (octave - 5) * 12;
              }
            } else {
              if (octave != 1) {
                --octave;
                octave_adjustment = (octave - 5) * 12;
              }
            }
            break;
          case 3:
            // Always sends on the key_channel
            // array key 2 holds the state of the toggle
            switch (output_array[2]) {
              case true:
                usbMIDI.sendControlChange(output_array[1], 0, key_channel);
                break;
              case false:
                usbMIDI.sendControlChange(output_array[1], 127, key_channel);
                break;
            }
            output_array[2] = !output_array[2];
            break;
          default:
            break;
        }
        state[thisBit] = false;
      }
    }
}

////// ----------------------------------------shiftIn function
///// just needs the location of the data pin and the clock pin
///// it returns a byte with each bit in the byte corresponding
///// to a pin on the shift register. leftBit 7 = Pin 7 / Bit 0= Pin 0

byte shiftIn(int my_data_pin, int my_clock_pin) { 
  int i;
  int temp = 0;
  int pin_state;
  byte my_data_in = 0;

  pinMode(my_clock_pin, OUTPUT);
  pinMode(my_data_pin, INPUT);
//we will be holding the clock pin high 8 times (0,..,7) at the
//end of each time through the for loop

//at the begining of each loop when we set the clock low, it will
//be doing the necessary low to high drop to cause the shift
//register's data_pin to change state based on the value
//of the next bit in its serial information flow.
//The register transmits the information about the pins from pin 7 to pin 0
//so that is why our function counts down
  for (i=7; i>=0; i--)
  {
    digitalWrite(my_clock_pin, 0);
    delayMicroseconds(0.2);
    temp = digitalRead(my_data_pin);
    if (temp) {
      pin_state = 1;
      //set the bit to 0 no matter what
      my_data_in = my_data_in | (1 << i);
    }
    else {
      //turn it off -- only necessary for debuging
     //print statement since my_data_in starts as 0
      pin_state = 0;
    }

    //Debuging print statements
    //Serial.print(pin_state);
    //Serial.print("     ");
    //Serial.println (dataIn, BIN);

    digitalWrite(my_clock_pin, 1);
  }
  //debuging print statements whitespace
  //Serial.println();
  //Serial.println(my_data_in, BIN);
  return my_data_in;
}
