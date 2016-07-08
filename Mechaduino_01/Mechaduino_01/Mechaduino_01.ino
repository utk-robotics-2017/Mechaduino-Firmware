// multi file
/*


  SAM21D18 (Arduino Zero compatible), AS5047 encoder, A4954 driver

  Controlled via a SerialUSB terminal at 115200 baud.


  ____
    |
  0005|-> LED
  0007|-> pulse
  0009|-> IN_1
  0010|-> IN_2
  0012|-> IN_3
  0011|-> IN_4
    |->                   \
  0008|-> VREF_1             \___A4954
  0013|-> VREF_2            _/
   4|->
  0006|-> chipSelectPin
   2|
    |
  SCK|-> clockPin
  MOSI|-> MISO
  MISO|-> MOSI
  ____|


  Implemented commands are:

  p  -  print [step count] , [assumed angle] , [encoder reading]

  c  -  clear step count & assumed angle

  s  -  step

  d  -  dir toggle

  z  -  seek zero position

  g  -  Go! steps around 400 times

  w  -  Same as go, but stores encoder angles to EEPROM

  r  -  returns EEPROM contents

  a  -  prompts user to enter angle

  y  -  sine sweep



*/



//----Current Parameters-----

volatile float Ts = 0.0003333333;

volatile float pKp = 19.75;
volatile float pKi = 0.50;
volatile float pKd = 0.10;

volatile float vKp = 0.05;
volatile float vKi = 200.00 * Ts;
volatile float vKd = 0.00 / Ts;


const PROGMEM float force_lookup[] = {
};



#define WAIT_TC16_REGS_SYNC(x) while(x->COUNT16.STATUS.bit.SYNCBUSY);


#include <math.h>
#include <avr/pgmspace.h>
#include <SPI.h>
#include <Wire.h>


#include "Controller.h"
#include "Parameters.h"
#include "Utils.h"
#include "State.h"

const int spr = 200; //  200 steps per revolution
const float aps = 360.0 / spr; // angle per step
int cpr = 16384; //counts per rev


int dir = 1;		//initialize stepping mode variables
int step_state = 1;		



long angle = 0; //holds processed angle value

float anglefloat = 0;

int a = 0;  // raw encoder value in closed loop and print_angle routine (should fix the latter to include LUT)


volatile long step_count = 0;  //For step/dir interrupt

volatile int interrupted = 0;

int stepNumber = 0; // step index for cal routine


//___________________________________

const float pi = 3.14159;
const int  half = 134;//128;

float new_angle = 0.0; //input angle
float current_angle = 0.0; //current angle
float diff_angle = 0.0;
int val1 = 0;
int val2 = 0;

//////////////////////////////////////
//////////////////PINS////////////////
//////////////////////////////////////
int IN_4 = 6;//11 - 1;
int IN_3 = 5;//12 - 1;
int VREF_2 = 4;//13 - 1;
int VREF_1 = 9;//3;//8-1;
int IN_2 = 7;//10 - 1;
int IN_1 = 8;//9 - 1;
int pulse = 13;//5;
const int ledPin = 13;//5; //LED connected to digital pin 13
const int chipSelectPin = A2;//5;//6; //output to chip select

int step_pin  = 1;
int dir_pin = 0;//2;

//int sine_out = //3;				// pins for debugging waveforms
//int encoder_out = //4;


//interrupt vars

volatile float ei = 0.0;
volatile int U = 0;  //control effort (abs)
volatile float r = 0.0;  //setpoint
volatile float y = 0.0;  // measured angle
volatile float yw = 0.0;
volatile float yw_1 = 0.0;
volatile float e = 0.0;  // e = r-y (error)
volatile float p = 0.0;  // proportional effort
volatile float i = 0.0;  // integral effort
volatile float PA = 1.8
;  //

volatile float u = 0.0;  //real control effort (not abs)
volatile float u_1 = 0.0;
volatile float e_1 = 0.0;
volatile float u_2 = 0.0;
volatile float e_2 = 0.0;
volatile float u_3 = 0.0;
volatile float e_3 = 0.0;
volatile long counter = 0;

volatile long wrap_count = 0;
volatile float y_1 = 0;


volatile int uMAX = 150;

volatile float ITerm;

volatile char mode;







const PROGMEM float sine_lookup[] = {
  0.0, 0.01, 0.02, 0.03, 0.04, 0.05, 0.06, 0.0699, 0.0799, 0.0899, 0.0998, 0.1098, 0.1197, 0.1296, 0.1395, 0.1494, 0.1593, 0.1692, 0.179, 0.1889, 0.1987, 0.2085, 0.2182, 0.228, 0.2377, 0.2474, 0.2571, 0.2667, 0.2764, 0.286, 0.2955, 0.3051, 0.3146, 0.324, 0.3335, 0.3429, 0.3523, 0.3616, 0.3709, 0.3802, 0.3894, 0.3986, 0.4078, 0.4169, 0.4259, 0.435, 0.4439, 0.4529, 0.4618, 0.4706, 0.4794, 0.4882, 0.4969, 0.5055, 0.5141, 0.5227, 0.5312, 0.5396, 0.548, 0.5564, 0.5646, 0.5729, 0.581, 0.5891, 0.5972, 0.6052, 0.6131, 0.621, 0.6288, 0.6365, 0.6442, 0.6518, 0.6594, 0.6669, 0.6743, 0.6816, 0.6889, 0.6961, 0.7033, 0.7104, 0.7174, 0.7243, 0.7311, 0.7379, 0.7446, 0.7513, 0.7578, 0.7643, 0.7707, 0.7771, 0.7833, 0.7895, 0.7956, 0.8016, 0.8076, 0.8134, 0.8192, 0.8249, 0.8305, 0.836, 0.8415, 0.8468, 0.8521, 0.8573, 0.8624, 0.8674, 0.8724, 0.8772, 0.882, 0.8866, 0.8912, 0.8957, 0.9001, 0.9044, 0.9086, 0.9128, 0.9168, 0.9208, 0.9246, 0.9284, 0.932, 0.9356, 0.9391, 0.9425, 0.9458, 0.949, 0.9521, 0.9551, 0.958, 0.9608, 0.9636, 0.9662, 0.9687, 0.9711, 0.9735, 0.9757, 0.9779, 0.9799, 0.9819, 0.9837, 0.9854, 0.9871, 0.9887, 0.9901, 0.9915, 0.9927, 0.9939, 0.9949, 0.9959, 0.9967, 0.9975, 0.9982, 0.9987, 0.9992, 0.9995, 0.9998, 0.9999, 1.0, 1.0, 0.9998, 0.9996, 0.9992, 0.9988, 0.9982, 0.9976, 0.9969, 0.996, 0.9951, 0.994, 0.9929, 0.9917, 0.9903, 0.9889, 0.9874, 0.9857, 0.984, 0.9822, 0.9802, 0.9782, 0.9761, 0.9738, 0.9715, 0.9691, 0.9666, 0.964, 0.9613, 0.9585, 0.9556, 0.9526, 0.9495, 0.9463, 0.943, 0.9396, 0.9362, 0.9326, 0.929, 0.9252, 0.9214, 0.9174, 0.9134, 0.9093, 0.9051, 0.9008, 0.8964, 0.8919, 0.8874, 0.8827, 0.878, 0.8731, 0.8682, 0.8632, 0.8581, 0.8529, 0.8477, 0.8423, 0.8369, 0.8314, 0.8258, 0.8201, 0.8143, 0.8085, 0.8026, 0.7966, 0.7905, 0.7843, 0.7781, 0.7718, 0.7654, 0.7589, 0.7523, 0.7457, 0.739, 0.7322, 0.7254, 0.7185, 0.7115, 0.7044, 0.6973, 0.6901, 0.6828, 0.6755, 0.6681, 0.6606, 0.653, 0.6454, 0.6378, 0.63, 0.6222, 0.6144, 0.6065, 0.5985, 0.5904, 0.5823, 0.5742, 0.566, 0.5577, 0.5494, 0.541, 0.5325, 0.524, 0.5155, 0.5069, 0.4983, 0.4896, 0.4808, 0.472, 0.4632, 0.4543, 0.4454, 0.4364, 0.4274, 0.4183, 0.4092, 0.4001, 0.3909, 0.3817, 0.3724, 0.3631, 0.3538, 0.3444, 0.335, 0.3255, 0.3161, 0.3066, 0.297, 0.2875, 0.2779, 0.2683, 0.2586, 0.2489, 0.2392, 0.2295, 0.2198, 0.21, 0.2002, 0.1904, 0.1806, 0.1708, 0.1609, 0.151, 0.1411, 0.1312, 0.1213, 0.1114, 0.1014, 0.0915, 0.0815, 0.0715, 0.0616, 0.0516, 0.0416, 0.0316, 0.0216, 0.0116, 0.0016
};





int analogPin = 1;
int val = 0;

int aout = 0;





//////////////////////////////////////
/////////////////SETUP/////////////////////
//////////////////////////////////////













void setup() {


  setupPins();
  setupSPI();
  setupTCInterrupts();

  SerialUSB.begin(115200);

  // while (!SerialUSB) {};     //wait for serial

  delay(500);


   // enableTCInterrupts();     //start in closed loop mode
  //  mode = 'x';
  //
  //  Wire.begin(4);                // join i2c bus with address #8
  //  Wire.onReceive(receiveEvent); // register event


  //tp

  //pinMode(10, OUTPUT);
  //pinMode(11, OUTPUT);
  //pinMode(12, OUTPUT);
  SerialUSB.println("Mechaduino 0.1 begin...");

}



//////////////////////////////////////
/////////////////LOOP/////////////////////
//////////////////////////////////////



void loop()
{
  //mode = 'x';


  serialCheck();

  // r=0.1125*step_count;

  

  // electronic gearing with analog
  //  val = 0;
  //  for (int i = 0; i < 10; i++){
  //    val += analogRead(analogPin);    // read the input pin
  //   // delay(1);
  //  }
  //
  //  SerialUSB.println(0.01*((float)val));             // debug value
  //  r = 0.01*((float)val);
  //
  //  aout = 10.0*yw;
  //
  //  if (aout > 1023){
  //    aout = 1023;
  //  }
  //  else if (aout < 0){
  //    aout = 0;
  //  }
  //
  //  analogWriteResolution(10);p
  //  analogWrite(A0,aout);
  //


  //  for (int i = 0; i < 1024; i++){
  //    analogWrite(A0,i);
  //    delay(2);
  //  }



  //  // tp dispenser:
  //  mode = 'x';
  //
  //  digitalWrite(10, LOW);
  //  digitalWrite(11, LOW);
  //  digitalWrite(12, HIGH);
  //
  //  if (u > 70) {
  //    digitalWrite(10, LOW);
  //    digitalWrite(11, HIGH);
  //    digitalWrite(12, LOW);
  //    for (int i = 0; i < 2700; i++) {
  //      r -= 0.2;
  //      delay(1);
  //    }
  //      digitalWrite(10, HIGH);
  //      digitalWrite(11, LOW);
  //      digitalWrite(12, LOW);
  //    delay(750);
  //    r -= 45;
  //    delay(50);
  //    r += 135;
  //    delay(250);
  //    for (int i = 0; i < 1200; i++) {
  //      r -= 0.2;
  //      delay(1);
  //    }
  //  }



  //    mode = 'x';















  //  r = 90;
  //  delay(2000);
  //  r = 0;
  //  delay(500);
  //  r = -90;
  //  delay (1000);

  //    if (u>50){
  //      r = -45;
  //      delay(500);
  //    }
  //    else if (u< -50){
  //      r = 45;
  //      delay(500);
  //    }
  //




}



//////////////////////////////////////
/////////////////FUNCTIONS/////////////////////
//////////////////////////////////////


void TC5_Handler()
{


  //unedited/old:
  /*  // TcCount16* TC = (TcCount16*) TC3; // get timer struct
    if (TC5->COUNT16.INTFLAG.bit.OVF == 1) {  // A overflow caused the interrupt
        interrupted = 1;

      TC5->COUNT16.INTFLAG.bit.OVF = 1;    // writing a one clears the flag ovf flag
    //  irq_ovf_count++;                 // for debug leds
    }

    // if (TC->INTFLAG.bit.MC0 == 1) {  // A compare to cc0 caused the interrupt
    //  digitalWrite(pin_mc0_led, LOW);  // for debug leds
     // TC->INTFLAG.bit.MC0 = 1;    // writing a one clears the flag ovf flag
    // } */



  ///new
  // TcCount16* TC = (TcCount16*) TC3; // get timer struct
  if (TC5->COUNT16.INTFLAG.bit.OVF == 1) {  // A overflow caused the interrupt




    a = readEncoder();
    y = lookup_angle(a);


    if ((y - y_1) < -180.0) {
      wrap_count += 1;
    }
    else if ((y - y_1) > 180.0) {
      wrap_count -= 1;
    }
    //y_1 = y;  pushed lower

    yw = (y + (360.0 * wrap_count));






    switch (mode) {

      case 'x':
        e = (r - yw);

        ITerm += (pKi * e);
        if (ITerm > 150) ITerm = 150;
        else if (ITerm < -150) ITerm = -150;


        u = ((pKp * e) + ITerm - (pKd * (yw - yw_1))); //ARDUINO library style
        //u = u+lookup_force(a)-20;
        //   u = u_1 + cA*e + cB*e_1 + cC*e_2;     //ppt linked in octave script

        //  u = 20*e;//



        break;

      case 'v':


        e = (r - ((yw - yw_1) * 500));//416.66667)); degrees per Tc to rpm

        ITerm += (vKi * e);
        if (ITerm > 200) ITerm = 200;
        else if (ITerm < -200) ITerm = -200;


        u = ((vKp * e) + ITerm - (vKd * (yw - yw_1)));//+ lookup_force(a)-20; //ARDUINO library style

        break;

      case 't':
        u = 1.0 * r ;//+ 1.7*(lookup_force(a)-20);
        break;

      default:
        u = 0;
        break;
    }





//
//    if (u > 0) {
//      PA = 1.8;
//    }
//    else {
//      PA = -1.8;
//    }
//
//    y += PA;



    if (u > 0) {
      y+=PA;
    }
    else {
      y -=PA;
    }





    if (u > uMAX) {                          //saturation limits max current command
      u = uMAX;
    }
    else if (u < -uMAX) {
      u = -uMAX;
    }



    U = abs(u);       //+lookup_force((((a-4213)%16384)+16384)%16384)-6); ///p);//+i);


    if (abs(e) < 0.1) {
      digitalWrite(pulse, HIGH);
      //  SerialUSB.println(r);
    }
    else  {
      digitalWrite(pulse, LOW);
    }

    output(-y, U);  //-y

    e_3 = e_2;
    e_2 = e_1;
    e_1 = e;
    u_3 = u_2;
    u_2 = u_1;
    u_1 = u;
    yw_1 = yw;
    y_1 = y;


    TC5->COUNT16.INTFLAG.bit.OVF = 1;    // writing a one clears the flag ovf flag
  }


}



void print_angle()                ///////////////////////////////////       PRINT_ANGLE   /////////////////////////////////
{
  a = 0;
  delay(100);
  a += readEncoder();
  delay(10);
  a += readEncoder();
  delay(10);
  a += readEncoder();
  delay(10);
  a += readEncoder();
  delay(10);
  a += readEncoder();
  delay(10);
  a += readEncoder();
  delay(10);
  a += readEncoder();
  delay(10);
  a += readEncoder();
  delay(10);
  a += readEncoder();
  delay(10);
  a += readEncoder();
  delay(10);
  a = a / 10;

  anglefloat = a * 0.02197265625;
  SerialUSB.print(stepNumber, DEC);
  SerialUSB.print(" , ");
  SerialUSB.print(stepNumber * aps, DEC);
  SerialUSB.print(" , ");
  SerialUSB.print(a, DEC);
  SerialUSB.print(" , ");
  SerialUSB.println(anglefloat, DEC);
}




void output(float theta, int effort) {                    //////////////////////////////////////////   OUTPUT   ///////////////////
  static int start = 0;
  static int finish = 0;
  static int intangle;
  static float floatangle;
  static int modangle;











  floatangle = (10000 * ( theta * 0.87266 + 2.3562) );//0.7854) );// 2.3562) );       //changed to 2.3 for NEMA23,NEMA17 dual..... opposite below
  //floatangle = (10000 * ( theta * 0.87266 + 0.7854) );

  intangle = (int)floatangle;
  //  modangle = (((intangle % 628) + 628) % 628);
  val1 = effort * lookup_sine(intangle);

  analogWrite(VREF_2, abs(val1));

  if (val1 >= 0)  {
    digitalWrite(IN_4, HIGH);
    //     PORTB |= (B00000001);
    digitalWrite(IN_3, LOW);
    //    PORTB &= ~(B00000010);

  }
  else  {
    digitalWrite(IN_4, LOW);
    //  PORTB &= ~(B00000001);
    digitalWrite(IN_3, HIGH);
    //    PORTB |= (B00000010);

  }





  floatangle = (10000 * (  theta * 0.8726646 + 0.7854) );//2.3562) );//0.7854) );
  //floatangle = (10000 * ( theta * 0.87266 + 2.3562) );

  intangle = (int)floatangle;
  // modangle = (((intangle % 628) + 628) % 628);
  val2 = effort * lookup_sine(intangle);

  analogWrite(VREF_1, abs(val2));

  if (val2 >= 0)  {
    digitalWrite(IN_2, HIGH);
    //     PORTB |= (B00000100);
    digitalWrite(IN_1, LOW);
    //     PORTB &= ~(B00001000);

  }
  else  {
    digitalWrite(IN_2, LOW);
    //   PORTB &= ~(B00000100);
    digitalWrite(IN_1, HIGH);
    //   PORTB |= (B00001000);

  }





}




int readEncoder()           //////////////////////////////////////////////////////   READENCODER   ////////////////////////////
{
  long angleTemp;
  digitalWrite(chipSelectPin, LOW);

  //angle = SPI.transfer(0xFF);
  byte b1 = SPI.transfer(0xFF);
  byte b2 = SPI.transfer(0xFF);


  angleTemp = (((b1 << 8) | b2) & 0B0011111111111111);
  //  SerialUSB.println((angle & 0B0011111111111111)*0.02197265625);

  digitalWrite(chipSelectPin, HIGH);
  return angleTemp;




}



void readEncoderDiagnostics()           //////////////////////////////////////////////////////   READENCODERDIAGNOSTICS   ////////////////////////////
{
  long angleTemp;
  digitalWrite(chipSelectPin, LOW);



  ///////////////////////////////////////////////READ DIAAGC (0x3FFC)
  SerialUSB.print("DIAAGC (0x3FFC)   ");

  SPI.transfer(0xFF);
  SPI.transfer(0xFC);
  digitalWrite(chipSelectPin, HIGH);

  delay(1);
  digitalWrite(chipSelectPin, LOW);

  byte b1 = SPI.transfer(0xC0);
  byte b2 = SPI.transfer(0x00);


  angleTemp = (((b1 << 8) | b2) & 0B1111111111111111);
  SerialUSB.print((angleTemp | 0B1110000000000000000 ), BIN);

  if (angleTemp & (1 << 14)) {
    SerialUSB.print("  Error occurred  ");
  }
  if (angleTemp & (1 << 11)) {
    SerialUSB.print("  MAGH  ");
  }
  if (angleTemp & (1 << 10)) {
    SerialUSB.print("  MAGL  ");
  }
  if (angleTemp & (1 << 9)) {
    SerialUSB.print("  COF  ");
  }
  if (angleTemp & (1 << 8)) {
    SerialUSB.print("  LF  ");
  }
  SerialUSB.println(" ");

  digitalWrite(chipSelectPin, HIGH);


  delay(1);

  digitalWrite(chipSelectPin, LOW);
  ///////////////////////////////////////////////READ ERRFL (0x0001)
  SerialUSB.print("ERRFL (0x0001)   ");

  SPI.transfer(0x40);
  SPI.transfer(0x01);
  digitalWrite(chipSelectPin, HIGH);

  delay(1);
  digitalWrite(chipSelectPin, LOW);

  b1 = SPI.transfer(0xC0);
  b2 = SPI.transfer(0x00);


  angleTemp = (((b1 << 8) | b2) & 0B1111111111111111);
  SerialUSB.print((angleTemp | 0B1110000000000000000 ), BIN);

  if (angleTemp & (1 << 14)) {
    SerialUSB.print("  Error occurred  ");
  }
  if (angleTemp & (1 << 2)) {
    SerialUSB.print("  parity error ");
  }
  if (angleTemp & (1 << 1)) {
    SerialUSB.print("  invalid register  ");
  }
  if (angleTemp & (1 << 0)) {
    SerialUSB.print("  framing error  ");
  }

  SerialUSB.println(" ");

  digitalWrite(chipSelectPin, HIGH);


  delay(1);







}


float lookup_sine(int m)        /////////////////////////////////////////////////  LOOKUP_SINE   /////////////////////////////
{
  float b_out;

  m = (0.01 * (((m % 62832) + 62832) % 62832)) + 0.5; //+0.5 for rounding

  //SerialUSB.println(m);

  if (m > 314) {
    m = m - 314;
    b_out = -pgm_read_float_near(sine_lookup + m);

  }
  else
  {
    b_out = pgm_read_float_near(sine_lookup + m);
  }

  return b_out;
}



float lookup_force(int m)        /////////////////////////////////////////////////  LOOKUP_force   /////////////////////////////
{
  float b_out;
  //
  //  m = (0.01*(((m % 62832) + 62832) % 62832))+0.5;  //+0.5 for rounding
  //
  //  //SerialUSB.println(m);
  //
  //  if (m > 314) {
  //    m = m - 314;
  //    b_out = -pgm_read_float_near(force_lookup + m);
  //
  //  }
  //  else
  //  {
  b_out = pgm_read_float_near(force_lookup + m);
  //  }

  return b_out;
}



void stepInterrupt() {
  if (digitalRead(dir_pin))
  {
    step_count += 1;
  }
  else
  {
    step_count -= 1;
  }


}


void setupPins() {

  pinMode(VREF_2, OUTPUT);
  pinMode(VREF_1, OUTPUT);
  pinMode(IN_4, OUTPUT);
  pinMode(IN_3, OUTPUT);
  pinMode(IN_2, OUTPUT);
  pinMode(IN_1, OUTPUT);
  pinMode(pulse, OUTPUT);
  pinMode(step_pin, INPUT);
  pinMode(dir_pin, INPUT);

  pinMode(3, OUTPUT);
  pinMode(4, OUTPUT);

  attachInterrupt(1, stepInterrupt, RISING);



  analogWrite(VREF_2, 64);
  analogWrite(VREF_1, 64);

  digitalWrite(IN_4, HIGH);
  digitalWrite(IN_3, LOW);
  digitalWrite(IN_2, HIGH);
  digitalWrite(IN_1, LOW);

  pinMode(ledPin, OUTPUT); // visual signal of I/O to chip
  // pinMode(clockPin, OUTPUT); // SCK
  pinMode(chipSelectPin, OUTPUT); // CSn -- has to toggle high and low to signal chip to start data transfer
  //  pinMode(inputPin, INPUT); // SDA



}


void setupSPI() {

  SPISettings settingsA(400000, MSBFIRST, SPI_MODE1);             ///400000, MSBFIRST, SPI_MODE1);

  SPI.begin();    //AS5047D SPI uses mode=1 (CPOL=0, CPHA=1)
  SerialUSB.println("Begin...");
  delay(1000);
  SPI.beginTransaction(settingsA);

}


void setupTCInterrupts() {



  // Enable GCLK for TC4 and TC5 (timer counter input clock)
  GCLK->CLKCTRL.reg = (uint16_t) (GCLK_CLKCTRL_CLKEN | GCLK_CLKCTRL_GEN_GCLK0 | GCLK_CLKCTRL_ID(GCM_TC4_TC5));
  while (GCLK->STATUS.bit.SYNCBUSY);

  TC5->COUNT16.CTRLA.reg &= ~TC_CTRLA_ENABLE;   // Disable TCx
  WAIT_TC16_REGS_SYNC(TC5)                      // wait for sync

  TC5->COUNT16.CTRLA.reg |= TC_CTRLA_MODE_COUNT16;   // Set Timer counter Mode to 16 bits
  WAIT_TC16_REGS_SYNC(TC5)

  TC5->COUNT16.CTRLA.reg |= TC_CTRLA_WAVEGEN_MFRQ; // Set TC as normal Normal Frq
  WAIT_TC16_REGS_SYNC(TC5)

  TC5->COUNT16.CTRLA.reg |= TC_CTRLA_PRESCALER_DIV1;   // Set perscaler
  WAIT_TC16_REGS_SYNC(TC5)


  TC5->COUNT16.CC[0].reg = 0x3E72; //0x4AF0;
  WAIT_TC16_REGS_SYNC(TC5)


  TC5->COUNT16.INTENSET.reg = 0;              // disable all interrupts
  TC5->COUNT16.INTENSET.bit.OVF = 1;          // enable overfollow
  TC5->COUNT16.INTENSET.bit.MC0 = 1;         // enable compare match to CC0

  // Enable InterruptVector
  NVIC_EnableIRQ(TC5_IRQn);

  // Enable TC
  //  TC5->COUNT16.CTRLA.reg |= TC_CTRLA_ENABLE;
  //  WAIT_TC16_REGS_SYNC(TC5)




}


void enableTCInterrupts() {

  TC5->COUNT16.CTRLA.reg |= TC_CTRLA_ENABLE;    //Enable TC5
  WAIT_TC16_REGS_SYNC(TC5)                      //wait for sync
}


void disableTCInterrupts() {


  TC5->COUNT16.CTRLA.reg &= ~TC_CTRLA_ENABLE;   // Disable TC5
  WAIT_TC16_REGS_SYNC(TC5)                      // wait for sync
}



void commandW() {

  int encoderReading = 0;     //or float?  not sure if we can average for more res?
  int lastencoderReading = 0;
  int avg = 10;         //how many readings to average

  int iStart = 0;
  int jStart = 0;
  int stepNo = 0;

  int fullStepReadings[spr];
  int fullStep = 0;
  //  float newLookup[cpr];
  int ticks = 0;

  float lookupAngle = 0.0;

  encoderReading = readEncoder();
  dir = 1;
  oneStep();
  delay(500);

  if ((readEncoder() - encoderReading) < 0)
  {
    //dir = 0;
    SerialUSB.println("Wired backwards");
    return;
  }

  while (stepNumber != 0) {
    if (stepNumber > 0) {
      dir = 1;
    }
    else
    {
      dir = 0;
    }
    oneStep();
    delay(100);
  }
  dir = 1;
  for (int x = 0; x < spr; x++) {

    encoderReading = 0;
    delay(100);

    for (int reading = 0; reading < avg; reading++) {
      encoderReading += readEncoder();
      delay(10);
    }

    encoderReading = encoderReading / avg;

    anglefloat = encoderReading * 0.02197265625;
    fullStepReadings[x] = encoderReading;
    SerialUSB.println(fullStepReadings[x], DEC);
    oneStep();
  }
  SerialUSB.println(" ");
  SerialUSB.println("ticks:");
  SerialUSB.println(" ");
  for (int i = 0; i < spr; i++) {
    ticks = fullStepReadings[mod((i + 1), spr)] - fullStepReadings[mod((i), spr)];
    if (ticks < -15000) {
      ticks += cpr;

    }
    else if (ticks > 15000) {
      ticks -= cpr;
    }
    SerialUSB.println(ticks);

    if (ticks > 1) {
      for (int j = 0; j < ticks; j++) {
        stepNo = (mod(fullStepReadings[i] + j, cpr));
        // SerialUSB.println(stepNo);
        if (stepNo == 0) {
          iStart = i;
          jStart = j;
        }

      }
    }

    if (ticks < 1) {
      for (int j = -ticks; j > 0; j--) {
        stepNo = (mod(fullStepReadings[199 - i] + j, cpr));
        // SerialUSB.println(stepNo);
        if (stepNo == 0) {
          iStart = i;
          jStart = j;
        }

      }
    }



  }




  SerialUSB.println(" ");
  SerialUSB.println("newLookup:");
  SerialUSB.println(" ");

  for (int i = iStart; i < (iStart + spr); i++) {
    ticks = fullStepReadings[mod((i + 1), spr)] - fullStepReadings[mod((i), spr)];

    if (ticks < -15000) {
      ticks += cpr;

    }
    else if (ticks > 15000) {
      ticks -= cpr;
    }
    //SerialUSB.println(ticks);

    if (ticks > 1) {
      for (int j = jStart; j < (jStart + ticks); j++) {
        lookupAngle = 0.01 * mod(100 * (aps * i + (aps * j / ticks)), 36000.0);
        SerialUSB.print(lookupAngle);
        SerialUSB.print(" , ");
      }
    }
    else if (ticks < 1) {
      for (int j = jStart - ticks; j > (jStart); j--) {
        lookupAngle = 0.01 * mod(100 * (aps * (i) + (aps * (ticks + j) / ticks)), 36000.0);
        SerialUSB.print(lookupAngle);
        SerialUSB.print(" , ");
      }
    }

  }
  SerialUSB.println(" ");







}




void serialCheck() {

  if (SerialUSB.available()) {

    char inChar = (char)SerialUSB.read();

    switch (inChar) {


      case 'p':             //print
        print_angle();
        break;

      case 's':             //step
        oneStep();
        print_angle();
        break;

      case 'd':             //dir
        if (dir == 1) {
          dir = 0;
        }
        else {
          dir = 1;
        }
        break;

      case 'w':
        commandW();           //cal routine
        break;

      case 'e':
        readEncoderDiagnostics();   //encoder error?
        break;

      case 'y':
        enableTCInterrupts();      //enable closed loop
        break;

      case 'n':
        disableTCInterrupts();      //disable closed loop
        break;

      case 'r':             //new setpoint
        SerialUSB.println("Enter setpoint:");
        while (SerialUSB.available() == 0)  {}
        r = SerialUSB.parseFloat();
        break;

      case 'x':
        mode = 'x';           //position loop
        break;

      case 'v':
        mode = 'v';           //velocity loop
        break;

      case 't':
        mode = 't';           //torque loop
        break;

      case 'c':
        mode = 'c';           //custom loop
        break;

      case 'q':
        parameterQuery();     // prints copy-able parameters
        break;

      case 'a':             //anticogging
        antiCoggingCal();
        break;

      case 'k':
        { 
          parameterEditmain();
          
          break;
        }

      default:
        break;
    }
  }

}


void parameterQuery() {
  SerialUSB.println(' ');
  SerialUSB.println("----Current Parameters-----");
  SerialUSB.println(' ');
  SerialUSB.println(' ');

  SerialUSB.print("volatile float Ts = ");
  SerialUSB.print(Ts, DEC);
  SerialUSB.println(";");
  SerialUSB.println(' ');

  SerialUSB.print("volatile float pKp = ");
  SerialUSB.print(pKp);
  SerialUSB.println(";");

  SerialUSB.print("volatile float pKi = ");
  SerialUSB.print(pKi);
  SerialUSB.println(";");

  SerialUSB.print("volatile float pKd = ");
  SerialUSB.print(pKd);
  SerialUSB.println(";");

  SerialUSB.println(' ');

  SerialUSB.print("cvolatile float vKp = ");
  SerialUSB.print(vKp);
  SerialUSB.println(";");

  SerialUSB.print("volatile float vKi = ");
  SerialUSB.print(vKi / Ts);
  SerialUSB.println(" * Ts;");

  SerialUSB.print("volatile float vKd = ");
  SerialUSB.print(vKd * Ts);
  SerialUSB.println(" / Ts;");

  SerialUSB.println(' ');

  SerialUSB.println("const PROGMEM float lookup[] = {");
  for (int i = 0; i < 16384; i++) {
    SerialUSB.print(lookup_angle(i));
    SerialUSB.print(", ");
  }
  SerialUSB.println("");
  SerialUSB.println("};");



}


int mod(int xMod, int mMod) {
  return (xMod % mMod + mMod) % mMod;
}



void antiCoggingCal() {
  SerialUSB.println(" -----------------BEGIN ANTICOGGING CALIBRATION!----------------");
  mode = 'x';
  r = lookup_angle(1);
  enableTCInterrupts();
  delay(1000);


  for (int i = 1; i < 657; i++) {
    r = lookup_angle(i);
    SerialUSB.print(r, DEC);
    SerialUSB.print(" , ");
    delay(100);
    SerialUSB.println(u, DEC);
  }
  SerialUSB.println(" -----------------REVERSE!----------------");

  for (int i = 656; i > 0; i--) {
    r = lookup_angle(i);
    SerialUSB.print(r, DEC);
    SerialUSB.print(" , ");
    delay(100);
    SerialUSB.println(u, DEC);
  }
  SerialUSB.println(" -----------------DONE!----------------");
  disableTCInterrupts();
}





void parameterEditmain() {

    SerialUSB.println();
    SerialUSB.println("Edit parameters:");
    SerialUSB.println();
    SerialUSB.println("p ----- proportional loop");
    SerialUSB.println("v ----- velocity loop");
    SerialUSB.println("o ----- other");
    SerialUSB.println("q ----- quit");
    SerialUSB.println();

    while (SerialUSB.available() == 0)  {}
    char inChar2 = (char)SerialUSB.read();

    switch (inChar2) {
      case 'p':
      {
         parameterEditp();
      }
        break;

      case 'v':
      {    
       parameterEditv();     
      }      
        break;

      case 'o':
      {
        parameterEdito();
      }
        break;
      default:
      {}
        break;
    
   

          }
}


void parameterEditp(){


  SerialUSB.println("Edit position loop gains:");
        SerialUSB.println();
        SerialUSB.print("p ----- pKp = ");
        SerialUSB.println(pKp,DEC);
        SerialUSB.print("i ----- pKi = ");
        SerialUSB.println(pKi,DEC);
        SerialUSB.print("d ----- pKd = ");
        SerialUSB.println(pKd,DEC);
        SerialUSB.println("q ----- quit");
        SerialUSB.println();
        
        while (SerialUSB.available() == 0)  {}
        char inChar3 = (char)SerialUSB.read();
        
        switch (inChar3) {
            case 'p':
              {
              SerialUSB.println("pKp = ?");
              while (SerialUSB.available() == 0)  {}
              pKp = SerialUSB.parseFloat();
              }
              break;
            case 'i':
              {
              SerialUSB.println("pKi = ?");
              while (SerialUSB.available() == 0)  {}
              pKi = SerialUSB.parseFloat();
              }
              break;
            case 'd':  
              {
              SerialUSB.println("pKd = ?");
              while (SerialUSB.available() == 0)  {}
              pKd = SerialUSB.parseFloat();
              }
              break;
            default:
            {}
              break;
        }
}


void parameterEditv(){
  SerialUSB.println("Edit velocity loop gains:");
  SerialUSB.println();
  SerialUSB.print("p ----- vKp = ");
  SerialUSB.println(vKp,DEC);
  SerialUSB.print("i ----- vKi = ");
  SerialUSB.println(vKi,DEC);
  SerialUSB.print("d ----- vKd = ");
  SerialUSB.println(vKd,DEC);
  SerialUSB.println("q ----- quit");
  SerialUSB.println();
  
  while (SerialUSB.available() == 0)  {}
  char inChar4 = (char)SerialUSB.read();
  
  switch (inChar4) {
      case 'p':
        {
        SerialUSB.println("vKp = ?");
        while (SerialUSB.available() == 0)  {}
        vKp = SerialUSB.parseFloat();
        }
        break;
      case 'i':
        {
        SerialUSB.println("vKi = ?");
        while (SerialUSB.available() == 0)  {}
        vKi = SerialUSB.parseFloat();
        }
        break;
      case 'd':  
        {
        SerialUSB.println("vKd = ?");
        while (SerialUSB.available() == 0)  {}
        vKd = SerialUSB.parseFloat();
        }
        break;
      default:
      {}
        break;             
  }
}

void parameterEdito(){


  SerialUSB.println("Edit other parameters:");
        SerialUSB.println();
        SerialUSB.print("p ----- PA = ");
        SerialUSB.println(PA,DEC);
        SerialUSB.println();

        
        while (SerialUSB.available() == 0)  {}
        char inChar3 = (char)SerialUSB.read();
        
        switch (inChar3) {
            case 'p':
              {
              SerialUSB.println("PA = ?");
              while (SerialUSB.available() == 0)  {}
              PA = SerialUSB.parseFloat();
              }
       
              break;
            default:
            {}
              break;
        }
}
