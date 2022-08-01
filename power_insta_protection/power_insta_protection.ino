#include "ZMPT101B.h"
#include "ACS712.h"
#include <Wire.h> 
#include <LiquidCrystal_I2C.h>
#include <SoftwareSerial.h>   
#define sensitivity 0.0093
#define baseUrl "http://citse2022-001-site1.gtempurl.com/"
#define testUrl "https://api.thingspeak.com/channels/119922/feeds/last.txt/"

LiquidCrystal_I2C lcd(0x27,16,2);  // set the LCD address to 0x27 for a 16 chars and 2 line display
SoftwareSerial SIM900A(9,10); // SoftSerial arduino pins( RX , TX );   


// Variable to store text message
String textMessage;

// We have ZMPT101B sensor connected to A0 pin of arduino
// Replace with your version if necessary
ZMPT101B voltageSensor(A0);

// We have 5 amps version sensor connected to A1 pin of arduino
// Replace with your version if necessary
ACS712 currentSensor(ACS712_05B, A1);

//RELAY SETUP
int relayPin = 7;

// Phone number
char *phoneNumbers[] = {"+255712111936"};
// char *phoneNumbers[] = {"+255653018568","+255713317205"};

int phoneNumbersLength;

// readings
float V;
float I;
int state = 1;

void setup()
{
  Serial.begin(9600);
  
  //LCD INITIALIZATION CODE
  lcd.init();
  lcd.backlight();
  lcd.setCursor(2,0);
  lcd.print("CITSE CS610");
  lcd.setCursor(2,1);
  lcd.print("PROJECT 2022");
  delay(3000);
  
  // calibrate() method calibrates zero point of sensor,
  // It is not necessary, but may positively affect the accuracy
  // Ensure that no current flows through the sensor at this moment
  // If you are not sure that the current through the sensor will not leak during calibration - 
  // comment out this method
  Serial.println("Calibrating... Ensure that no current flows through the sensor at this moment");
  delay(100);
  voltageSensor.setSensitivity(sensitivity);
  voltageSensor.calibrate();
  currentSensor.calibrate();
  Serial.println("Done!");

  //RELAY CONTROL
  pinMode(relayPin, OUTPUT);

  // By default the relay is on
  digitalWrite(relayPin, LOW);

  // Initializing GSM serial commmunication
  SIM900A.begin(9600);

  // Give time to your GSM shield log on to network
  delay(2000);
  
  Serial.print("SIM900A ready...");

  // calculate the length of number array
  phoneNumbersLength = sizeof(phoneNumbers) / sizeof(phoneNumbers[0]);
  Serial.println("Found "+ String(phoneNumbersLength) +" phone numbers");
  sendSMS("testing ...");
  // gprsSendData();  
}

// main program loop
void loop()
{
  if (state == 0){
    switchOFF();
  } else if (state == 1){
    switchON();
  }

  // To measure voltage/current we need to know the frequency of voltage/current
  // By default 50Hz is used, but you can specify desired frequency
  // as first argument to getVoltageAC and getCurrentAC() method, if necessary

  V = voltageSensor.getVoltageAC();
  I = currentSensor.getCurrentAC();

  Serial.println(String("V = ") + V + " V");
  Serial.println(String("I = ") + I + " A");

  //LCD POWER DISPLAY

  //Normal voltage
  lcd.clear();
  lcd.display();
  lcd.setCursor(0,0);
  lcd.print(String("Voltage = ") + V + " V");
  lcd.setCursor(0,1);
  lcd.print(String("Current = ") + I + " A");
  digitalWrite(relayPin, LOW);
  delay(1000);
  
  if(V >= 250){
    // Over voltage
    switchOFF();
    state = 0;
    lcd.clear();
    lcd.display();
    lcd.setCursor(0,0);
    lcd.print("OVER VOLTAGE");
    delay(5000);
    String message = "OVER VOLTAGE";
    sendSMS(message);  // send sms for overvoltage
    //gprsSendData();
  }
  else if(V <= 200){
    //under voltage
    switchOFF();
    state = 0;
    lcd.clear();
    lcd.display();
    lcd.setCursor(0,0);
    lcd.print("UNDER VOLTAGE");
    delay(5000);
    String message = "UNDER VOLTAGE";
    sendSMS(message);  // send sms for undervoltage
    //gprsSendData();
  }
  else{
    switchON();
    state = 1;
    //normal voltage
    // switchOFF();
    // lcd.clear();
    // lcd.display();
    // lcd.setCursor(0,0);
    // lcd.print("NORMAL VOLTAGE");
    // String message = "NORMAL VOLTAGE";
    // sendSMS(message);
   }
 }

void switchON(){
  digitalWrite(relayPin, LOW);
}


void switchOFF(){
  digitalWrite(relayPin, HIGH);
}


// gsm send sms
void sendSMS(String message)
{
  for (int i=0;i<phoneNumbersLength;i++) {
    Serial.println ("Sending Message");
    SIM900A.println("AT+CMGF=1");    //Sets the GSM Module in Text Mode
    delay(1000);
    //ShowSerialData();
    Serial.println ("Set SMS Number");
    SIM900A.println("AT+CMGS=\""+ String(phoneNumbers[i]) +"\"\r"); //Mobile phone number to send message
    delay(1000);
    //ShowSerialData();
    Serial.println ("Set SMS Content");
    SIM900A.println(String(message));// Messsage content
    delay(100);
    Serial.println ("Finish");
    SIM900A.println((char)26);// ASCII code of CTRL+Z
    delay(1000);
    //ShowSerialData();
    Serial.println ("Message has been sent to: " + String(phoneNumbers[i]));
  }
}



// gprs 
// ----------------

// initialize gprs
void initHTTP() 
{
  Serial.println(F("HTTP get method :"));
  /* Configure bearer profile 1 */
  // Serial.println(F("AT+SAPBR=3,1,\"CONTYPE\",\"GPRS\"\\r\\n"));    
  SIM900A.println(F("AT+SAPBR=3,1,\"CONTYPE\",\"GPRS\""));  /* Connection type GPRS */
  delay(1000);
  ShowSerialData();
  SIM900A.println(F("AT+SAPBR=3,1,\"APN\",\"m.vodacom.co.tz\""));  /* APN of the provider */
  delay(10000);
  ShowSerialData();
  connectGSM("AT+SAPBR=1,1", "OK"); /* Open GPRS context */
  ShowSerialData();
  connectGSM("AT+SAPBR=2,1", "OK"); /* Query the GPRS context */
  ShowSerialData();
  connectGSM("AT+HTTPINIT","OK"); /* Initialize HTTP service */
  delay(1000); 
  ShowSerialData();
  SIM900A.println("AT+HTTPPARA=\"CID\",1");  /* Set parameters for HTTP session */
  delay(1000);
  ShowSerialData();
}

// send a get request
void gprsSendData()
{
  // initialize http
  initHTTP();

  // sample line
  // Serial.println("AT+HTTPPARA=\"URL\",\"" + String(baseUrl) + "api/endpoint/?" +"par1=" + String(par1) + "&" + "par2=" + String(par2) + "/\"\\r\\n");
  
  // SIM900A.println("AT+HTTPPARA=\"URL\",\"" + String(baseUrl) + "api/apiFaults/getAll/" + "\"");  // Set parameters for HTTP session !uncomment this if not testing 
  SIM900A.println("AT+HTTPPARA=\"URL\",\"" + String(testUrl) + "\"");  // test url ( get request ) !comment this if not testing
  ShowSerialData();
  delay(1000);
  
  SIM900A.println(F("AT+HTTPACTION=0"));  /* Start GET session */
  ShowSerialData();
  delay(3000);
  // rSerial();
  
  SIM900A.println(F("AT+HTTPREAD"));  /* Read data from HTTP server */
  delay(1000);
  // rSerial();
  
  Serial.println(F("AT+HTTPTERM\\r\\n"));  
  SIM900A.println(F("AT+HTTPTERM"));  /* Terminate HTTP service */
  ShowSerialData();
  delay(500);
  
  SIM900A.println(F("AT+SAPBR=0,1")); /* Close GPRS context */
  ShowSerialData();
  delay(100);
}


// show serial
void ShowSerialData()
{
  while(SIM900A.available()!=0)
  Serial.write(SIM900A.read());
  delay(5000);
}

// connect at - cmd to gsm
void connectGSM (String cmd, char *res)
{
  while (1)
  {
    SIM900A.println(cmd);
    delay(500);
    while (SIM900A.available() > 0)
    {
      if (SIM900A.find(res))
      {
        delay(1000);
        return;
      }
    }
    delay(1000);
  }
} 
