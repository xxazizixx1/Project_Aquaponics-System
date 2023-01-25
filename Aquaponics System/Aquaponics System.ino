#include <ESP8266WiFi.h>
#include <FirebaseESP8266.h>
#include <Adafruit_ADS1X15.h>

//Firebase Auth Tilapia
#define FIREBASE_HOST "tilapia-e9b97-default-rtdb.asia-southeast1.firebasedatabase.app"                    //Your Firebase Project URL goes here without "http:" , "\" and "/"
#define FIREBASE_AUTH "q0vPD4nBAE4rfyaMZZUYheyRkgnYNawKcZBk8Ma8" //Your Firebase Database Secret goes here
#define WIFI_SSID "Mineral Water"                                               //WiFi SSID to which you want NodeMCU to connect
#define WIFI_PASSWORD "1234azizi1234"

// Declare the Firebase Data object in the global scope
FirebaseData firebaseData;

//Pin number
const int solTank = 15; //Solenoid to tank
const int solRiv = 13; //solenoid to riber
const int motPla = 12; //motor to plant
const int motFis = 14; //motor frm fish
const int motRiv = 10; //motor frm river

//For Interrupt
const unsigned long eventSprinkler = 10000; //every 12 hr
const unsigned long eventCleaning = eventSprinkler*8; //every 4 day
unsigned long previousTime = 0;
unsigned long currentTime = millis();

//use 12 bit ADC
Adafruit_ADS1015 ads;

void interruptSprinkler();
void maintenance();

void setup(void)
{
  Serial.begin(115200);
  ads.setGain(GAIN_ONE);        // 1x gain   +/- 4.096V  1 bit = 2mV      0.125mV
  
  //If ADC fails
  if (!ads.begin()) {
    Serial.println("Failed to initialize ADS.");
    while (1);
  }
  
  //Initializing Wifi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);                                     //try to connect with wifi
  Serial.print("Connecting to ");
  Serial.print(WIFI_SSID);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println();
  Serial.print("Connected to ");
  Serial.println(WIFI_SSID);
  Serial.print("IP Address is : ");
  Serial.println(WiFi.localIP());                                            //print local IP address
  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);
  delay(1000);  
  
  //Pin assignment
  pinMode(solTank, OUTPUT);
  pinMode(solRiv, OUTPUT);
  pinMode(motPla, OUTPUT);
  pinMode(motFis, OUTPUT);
  pinMode(motRiv, OUTPUT);
  
  //Initial state
  digitalWrite(solTank,LOW);
  digitalWrite(solRiv,LOW);
  digitalWrite(motPla,LOW);
  digitalWrite(motFis,LOW);
  digitalWrite(motRiv,LOW);

}

//Function to read water level sensor at filter tank
double waterS1()
{
  int16_t adc0;
  float volts0;

  adc0 = ads.readADC_SingleEnded(0); //Read ADC signal

  volts0 = ads.computeVolts(adc0);  //Show volts value
  
  Serial.print("AIN0: "); Serial.print(adc0); Serial.print("  "); Serial.print(volts0); Serial.println("V");
  delay(500);

  //Print Firebase Value
  Firebase.setInt(firebaseData, "/water/Filter/Value(bit)", adc0); //value of water level sensor in bit
  Firebase.setInt(firebaseData, "/water/Filter/Volt(V)", volts0); //value of water level sensor in volt
  delay(1000);
  return adc0;
}

//Function to read water level sensor at fish tank
double waterS2()
{
  int16_t adc1;
  float volts1;

  adc1 = ads.readADC_SingleEnded(1); //Read ADC signal

  volts1 = ads.computeVolts(adc1);  //Show volts value
  
  Serial.print("AIN1: "); Serial.print(adc1); Serial.print("  "); Serial.print(volts1); Serial.println("V");
  delay(500);

  Firebase.setInt(firebaseData, "/water/FishTank/Value(bit)", adc1); //value of water level sensor in bit
  Firebase.setInt(firebaseData, "/water/FishTank/Volt(V)", volts1); //value of water level sensor in volt
  delay(1000);
  return adc1;
}

//Function for ph at filter
double PH1()
{
  int16_t adc2;
  float volts2;

  adc2 = ads.readADC_SingleEnded(2);
  volts2 = ads.computeVolts(adc2);


  //no sticker sensor
  float trueVal1 = (7 + ((3.81 - volts2)/0.18))/2.55;
  Firebase.setInt(firebaseData, "/pH/Filter/Value(0-14)", trueVal1);
  delay(1000);
  return trueVal1;
}

//Ph at fish tank
double PH2()
{
  int16_t adc3;
  float volts3;

  adc3 = ads.readADC_SingleEnded(3);
  volts3 = ads.computeVolts(adc3);
  //with sticker
  float trueVal2 = (7 + ((3.87 - volts3)/0.18))/2.55;
  
  Firebase.setInt(firebaseData, "/pH/FishTank/Value(0-14)", trueVal2);
  delay(1000);
  return trueVal2;  
}

//Function to turn on relay (NO connection initially)
void relayStatOn(int pinNumber) //Will supply power if used
{
  digitalWrite(pinNumber, HIGH);
}

void relayStatOff(int pinNumber)
{
  digitalWrite(pinNumber, LOW);
}

//MAIN Function to pump water from river to filter tank
void river2filter()
{
  int waterAtFilter = waterS1();
  Serial.println("River 2 filter");
  if(waterAtFilter <= 300) //if water level low, pump water
  {
    relayStatOn(motRiv);
  }else{ //else if water level low
    relayStatOff(motRiv);    
  }  
}

//MAIN Function to open solenoid from filter tank to fish tank
void filter2fish()
{
  int waterAtFish = waterS2();
  int phAtFilter = PH1();
  Serial.println("filter 2 fishr");
  //if water level low and ph 6.5 to 9 open solenoid
  if(waterAtFish <= 200 || phAtFilter >= 6 && phAtFilter <=9)
  {
    relayStatOn(solTank);    
  }else{
    relayStatOff(solTank);
  }
}
//MAIN function to open pump from fish tank to filter
void fish2filter()
{
  int waterAtFish1 = waterS2();
  Serial.println("fish 2 filter");
  if(waterAtFish1 >= 200)
  {
    relayStatOn(motFis);    
  }else{
    relayStatOff(motFis);
  }
}
//MAIN function to open solenoid to river from filter
void filter2River()
{
  int waterAtFilter1 = waterS1();
  Serial.println("filter 2 riber");  
  if(waterAtFilter1 >= 200)
  {
    relayStatOn(solRiv);
  }else
    relayStatOff(solRiv);  
}

void overall()
{
  river2filter();
  filter2fish();
  fish2filter();
  filter2River();
}

void interruptSprinkler()
{
  if(currentTime - previousTime >= eventSprinkler)
  {
    Serial.println("interruptsprinkler");
    relayStatOn(motPla);
    delay(10000); //10 sec
    relayStatOff(motPla);    
  }  
}

void maintenance()
{
  int start = millis();
  int end = millis();

  while((end - start) <= 600000) //run for 10min
  {
    relayStatOn(solTank);relayStatOn(solRiv); //motor to fishtank on, motor to river on
    if ((end - start) <= 420000) //if reach 7 minutes
    {
      relayStatOn(motRiv);
    }   
  }
  overall();
}

void loop()
{// {
//   if(millis() - previousTime >= eventCleaning)
//   {
//     maintenance();
//   }else{
  interruptSprinkler();
  PH2();
  overall();
  maintenance();    
  
}