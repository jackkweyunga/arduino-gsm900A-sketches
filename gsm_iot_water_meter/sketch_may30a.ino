/* PRE-PAID WATER METERING SYSTEM */

/* LIBRARIES */
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <SoftwareSerial.h>
#include <String.h>
#include <EEPROM.h>

/* DEBUG */
#define DEBUG 0
#if DEBUG == 1
#define debug(x) Serial.print(x)
#define debugln(x) Serial.println(x)
#define debugln_dec(x, y) Serial.println(x, y)
#else
#define debug(x)
#define debugln(x)
#define debugln_dec(x, y)
#endif

/* EEPROM */
#define store(x,y) EEPROM.write(x, y)
#define retrieve(x) EEPROM.read(x)

/* GLOBAL CONSTANTS */
#define baseUrl "http://ip-address:PORT/"
#define flowsensor 2
#define valve 13
#define gsm_tx 7
#define gsm_rx 8
#define flow_input 3

/* LIBRARIES INITIALIZATIONS */
SoftwareSerial gprsSerial(gsm_tx,gsm_rx);
LiquidCrystal_I2C lcd(0x27,16,2);

/* GLOBAL VARIABLES */
struct flowSense {
  unsigned long currentTime;
  unsigned long cloopTime;
  volatile byte flow_frequency;
} flow_sense;

/* Memory addreses */
struct memAddr {
  unsigned char is_registered_addr = 0;
  unsigned char last_units_addr = 1;
  unsigned char last_reading_addr = 2;
  unsigned char units_addr = 3;
  unsigned char customer_phone_addr = 6;
} mem_addr;


// Define a datastore
struct DataStore {
   int is_registered;
   int has_units;
   int last_units;
   float vol;
   float l_minute;
   float last_reading;
   int units;
} data;

char customer_phone[13];
char token[11]; 
const char meter_phone[13] = "255XXXXXXXX";
int sms_flag=0;
bool gsm_connected=0;
String number="";
String msg="";
String instr="";
int i=0,temp=0;
int rec_read=0;
int temp1=0;

void setup()
{
  gprsSerial.begin(9600); 
  Serial.begin(9600);
  lcd.init();
  lcd.backlight();

  // initialize gsm
  // initGSM();
  
  // initialize values
  initializeValues();

  // load initial values
  data.vol = retrieve(mem_addr.last_reading_addr);
  read_customer_phone();

  if (String(customer_phone)=="") {
    data.is_registered = 0;
  }

  if (String(customer_phone)!="") {
    data.is_registered = 1;
  }

  debug("Customer phone: ");
  debugln(customer_phone);
  
//  data.is_registered = retrieve(mem_addr.is_registered_addr);
  data.units = retrieve(mem_addr.units_addr);
  data.last_units = retrieve(mem_addr.last_units_addr);

  if (data.units > 0) {
    data.has_units = 1;
  }
  
  pinMode(flowsensor, OUTPUT);
  pinMode(flow_input, INPUT);
  pinMode(valve, OUTPUT);
  digitalWrite(flow_input, HIGH); // Optional Internal Pull-Up
  attachInterrupt(digitalPinToInterrupt(flow_input), flow, RISING); // Setup Interrupt
  sei(); // Enable interrupts

  if (data.has_units == 1 && data.is_registered == 1) {
    lcd.setCursor(0,0);
    lcd.print("Opening valve");
    open_valve();

    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Water FlowMeter");
    lcd.setCursor(0,1);
    lcd.print("   sensing...  ");
    delay(500);
  } else if (data.has_units == 0) {
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Low unit balance");
    lcd.setCursor(0,1);
    lcd.print("   ...  ");
    delay(1000);
  } else if (data.is_registered == 0) {
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("No owner found");
    lcd.setCursor(0,1);
    lcd.print("  ... ");
    delay(1000);
  }
    
  debugln(F("Prepaid meter is now setup"));
  delay(500);
  
  flow_sense.currentTime = millis();
  flow_sense.cloopTime = flow_sense.currentTime;

}
 
void loop()
{

  delay(1000);

  if(gprsSerial.available())
  {
    instr = gprsSerial.readString();
    
    if(instr.indexOf("\n+CMT: \"")>=0)
    {
      sms_flag=1;
    }
  
    if(sms_flag==1) // received sms
    {
      
      debugln(instr);
      
      number = instr.substring(10, 22);
      debugln(number);
      
      String sms = instr.substring(51);
  
      // debug
      debug(F("from: "));
      debugln(number);
      debug(F("sms: "));
      debugln(sms);
  
//      number.toCharArray(customer_phone, 13);
//      debug(F("length: "));
//      debugln(sms.length());

      // check if number is registered
      if (String(customer_phone).indexOf(number)>=0) {
        if ( sms.length() > 10) {  
          SendMessage(F("Token ya malipo imepokelewa. Utarejewa hivi punde."));
          sms.toCharArray(token, 11);
          confirmPayment();
        } else {
    
          // switch actions
          switch(sms[0]) {
            case '0': // check action menu
            SendMessage(F("1 - Hakiki usajili \n2 - unit \n3 - ujazo \n5 - akaunti \n6 - kiwango \n7 - majaribio \n8 - funga mfereji \n9 - fungua mfereji"));
            delay(2000);
            break;
            
            case '1': // registration code
            {
              rSendMessage(number, F("Ombi la kuhakiki usajili wa huduma limepokelewa."));
              registerMeter(number);
              break;
            }
          
            case '2':
            {
              debug(F("units: "));
              debugln(data.units);
              SendMessage("units: "+String(data.units));
              break;
            }
        
            case '3':
            {
              debug(F("water volume used: "));
              debugln(data.vol);
              SendMessage("kiasi kilichotumika (Lita): "+String(data.vol));
              break;
            }
        
            case '5':
            {
              debugln(F("client phone: "));
              debug(customer_phone);
              SendMessage("Mteja aliyesajiliwa na bomba: " + String(customer_phone));
              break;
            }
        
            case '6':
            {
              debug(F("flow rate L/hour: "));
              debugln_dec(data.l_minute, DEC);
              SendMessage("Kiwango cha maji kinachopita (Lita/saa): " + String(data.l_minute, DEC));
              break;
            }
        
            case '7': // test sms
            {
              SendMessage(F("Ukiona ujumbe huu, huduma za sms za mita yako zinafanya kazi kwa usahihi."));
              delay(2000);
              break;
            }

            case '9': // test sms
            {
              open_valve();
              SendMessage(F("Mfereji umefunguliwa."));
              delay(2000);
              break;
            }

            case '8': // test sms
            {
              close_valve();
              SendMessage(F("Mfereji umefungwa."));
              delay(2000);
              break;
            }
        
            default:
            {
              debugln("listening ...");
              break;
            }
          }
        }
        token[0] = 0;
//        customer_phone[0] = 0;
        sms_flag=0;
        instr="";
        rec_read=1;
        temp1=1;
        i=0;
      } else {
        switch(sms[0]) {
            case '1': // check action menu
            {
              rSendMessage(number, F("Ombi la kuhakiki usajili wa huduma limepokelewa. Tutakurejea hivi punde."));
              registerMeter(number);
              break;
            }
            default: 
            {
              if (data.is_registered == 1) {
               SendMessage("Namba " + String(number) +" ilijaribu kuwasiliana na bomba lako wakati haijasajiliwa."); 
              }
              rSendMessage(number, F("Namba hii haijasajiliwa na huduma ya maji. Tafadhali tembelea ofisi zetu kujiunga na huduma hii."));
              sms_flag=0;
              break;
            }
        }
      }
    }
  }
  else if (sms_flag==0  && data.is_registered == 1) // adjust flow
  {
    open_valve();
    if (data.has_units == 1) {
      
      flow_sense.currentTime = millis();
      // Every second, calculate and print litres/hour
      if(flow_sense.currentTime >= (flow_sense.cloopTime + 1000))
      {
        flow_sense.cloopTime = flow_sense.currentTime; // Updates cloopTime
        if(flow_sense.flow_frequency != 0)
        {   
          data.l_minute = (flow_sense.flow_frequency * 60 / 7.5) / 3600; // (Pulse frequency x 60 min) / 7.5Q = flowrate in L/hour
          data.vol = data.vol + data.l_minute;
          if (data.vol >= 10){ // assuming 1unit is 10 L
            if (data.units >= 0){
             
              if (data.units == (int)(data.last_units - (0.75 * data.last_units))) {
                // alert low unit balance
                SendMessage(F("Salio lako la maji limekaribia kuisha."));
              } 
              
              if (data.units == 0) {
                // alert no units left
                // change units state to low.
                SendMessage(F("Salio lako la maji limeisha."));
                data.has_units = 0;
              } else {
                 data.units -= 1; 
                 update_units(data.units);
                 data.vol = 0.00;
              }
            }
          }
         
          // update vol in memory
          update_last_reading(data.vol);
          lcd.clear();
          lcd.setCursor(0,0);
          lcd.print("Units: ");
          lcd.print(data.units);
          lcd.setCursor(0,1);   
          lcd.print("Vol:");
          lcd.print(data.vol);
          lcd.print(" L");
          flow_sense.flow_frequency = 0; // Reset Counter
  //        Serial.println(data.l_minute, DEC); // Print litres/hour
  //        debugln(" L/Sec");
        }
        else {
          lcd.clear();
          lcd.setCursor(0,0);
          lcd.print(F("Units: "));
          lcd.print(data.units);
          lcd.setCursor(0,1);
          lcd.print(F("Vol:"));
          lcd.print(data.vol);
          lcd.print(" L");
        }
     }
    } else if (data.has_units == 0) {
      close_valve();
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print(F(" NO UNITS "));
      lcd.setCursor(0,1);
      lcd.print(F(" RECHARGE! "));
    }
  }

  sms_flag = 0;
}    

void open_valve(){
  digitalWrite(valve, HIGH);
  digitalWrite(flowsensor, HIGH);
}

void close_valve(){
  digitalWrite(valve, LOW);
  digitalWrite(flowsensor, LOW);
}

void initGSM(){
  lcd.clear();
  lcd.print("Finding Module..");
  boolean at_flag=1;
  while(at_flag)
  {
    gprsSerial.println("AT");
    while(gprsSerial.available()>0)
    {
      if(gprsSerial.find("OK"))
      at_flag=0;
    }
    delay(1000);
  }
  lcd.clear();
  lcd.print("Module Connected..");
  delay(1000);
//  lcd.clear();
//  lcd.print("Disabling ECHO");
  boolean echo_flag=1;
  while(echo_flag)
  {
    gprsSerial.println("ATE1");
    while(gprsSerial.available()>0)
    {
      if(gprsSerial.find("OK"))
      echo_flag=0;
    }
    delay(1000);
  }
//  lcd.clear();
//  lcd.print("Echo OFF");
//  delay(1000);
  lcd.clear();
  lcd.print("Finding Network..");
  boolean net_flag=1;
  while(net_flag)
  {
    gprsSerial.println("AT+CPIN?");
    while(gprsSerial.available()>0)
    {
      if(gprsSerial.find("+CPIN: READY"))
      net_flag=0;
    }
    delay(1000);
  }
  lcd.clear();
  lcd.print("Network Found..");
  delay(1000);
  lcd.clear();
}

void connectGSM (String cmd, char *res)
{
  while (1)
  {
    gprsSerial.println(cmd);
    delay(500);
    while (gprsSerial.available() > 0)
    {
      if (gprsSerial.find(res))
      {
        delay(1000);
        return;
      }
    }
    delay(1000);
  }
}


// SMS
// functions
// ---------------------------------------------------

void SendMessage(String msg)
{
  connectGSM("AT+CMGF=1", "OK");    //Sets the GSM Module in Text Mode
  ShowSerialData();
  gprsSerial.println("AT+CMGS=\""+ String(customer_phone) +"\"\r"); // Replace x with mobile number
  delay(1000);
  ShowSerialData();
  gprsSerial.println(msg);// The SMS text you want to send
  delay(5000);
  ShowSerialData();
  gprsSerial.println((char)26);// ASCII code of CTRL+Z
  delay(1000);
  ShowSerialData();
  debug(F("sms sent to: "));
  debugln(customer_phone);
}

void rSendMessage(String n, String msg)
{
  connectGSM("AT+CMGF=1", "OK");    //Sets the GSM Module in Text Mode
  ShowSerialData();
  gprsSerial.println("AT+CMGS=\""+ String(n) +"\"\r"); // Replace x with mobile number
  delay(1000);
  ShowSerialData();
  gprsSerial.println(msg);// The SMS text you want to send
  delay(5000);
  ShowSerialData();
  gprsSerial.println((char)26);// ASCII code of CTRL+Z
  delay(1000);
  ShowSerialData();
  debug(F("sms sent to: "));
  debugln(customer_phone);
}


// internet
// functions
// ---------------------------------------------------

void initHTTP() 
{
  debugln(F("HTTP get method :"));
  /* Configure bearer profile 1 */
  debugln(F("AT+SAPBR=3,1,\"CONTYPE\",\"GPRS\"\\r\\n"));    
  gprsSerial.println(F("AT+SAPBR=3,1,\"CONTYPE\",\"GPRS\""));  /* Connection type GPRS */
  delay(1000);
  ShowSerialData();
  gprsSerial.println(F("AT+SAPBR=3,1,\"APN\",\"m.vodacom.co.tz\""));  /* APN of the provider */
  delay(10000);
  ShowSerialData();
  connectGSM("AT+SAPBR=1,1", "OK"); /* Open GPRS context */
  ShowSerialData();
  connectGSM("AT+SAPBR=2,1", "OK"); /* Query the GPRS context */
  ShowSerialData();
  connectGSM("AT+HTTPINIT","OK"); /* Initialize HTTP service */
  delay(1000); 
  ShowSerialData();
  gprsSerial.println("AT+HTTPPARA=\"CID\",1");  /* Set parameters for HTTP session */
  delay(1000);
  ShowSerialData();
}


void registerMeter(String n) // registration for meter
{
  String sid;

  if (n != ""){
    sid = n;
  } else {
    sid = customer_phone;
  }
  
  // initialize http
  initHTTP();
  
  // debugln("AT+HTTPPARA=\"URL\",\"" + String(baseUrl) + "customers/watermeter/register/?" +"sid=" + String(customer_phone) + "&" + "mid=" + String(meter_phone) + "/\"\\r\\n");
  gprsSerial.println("AT+HTTPPARA=\"URL\",\"" + String(baseUrl) + "customers/watermeter/register/?" +"sid=" + String(sid) + "&" + "mid=" + String(meter_phone) + "\"");  /* Set parameters for HTTP session */
  ShowSerialData();
  delay(1000);
  
  gprsSerial.println(F("AT+HTTPACTION=0"));  /* Start GET session */
  ShowSerialData();
  delay(3000);
  rSerial();
  
  gprsSerial.println(F("AT+HTTPREAD"));  /* Read data from HTTP server */
  delay(1000);
  rSerial();
  
  debugln(F("AT+HTTPTERM\\r\\n"));  
  gprsSerial.println(F("AT+HTTPTERM"));  /* Terminate HTTP service */
  ShowSerialData();
  delay(500);
  
  gprsSerial.println(F("AT+SAPBR=0,1")); /* Close GPRS context */
  ShowSerialData();
  delay(100);
  
}


void confirmPayment() // registration for meter
{
  // initialize http
  initHTTP();
  
  //debugln("AT+HTTPPARA=\"URL\",\"" + String(baseUrl) + "payments/confirm-payment/?" +"code=" + String(token) + "&" + "cid=" + String(customer_phone) + "\"\\r\\n");
  gprsSerial.println("AT+HTTPPARA=\"URL\",\"" + String(baseUrl) + "payments/confirm-payment/?" +"code=" + String(token) + "&" + "cid=" + String(customer_phone) + "\"");  /* Set parameters for HTTP session */
  ShowSerialData();
  delay(1000);
  
  debugln(F("AT+HTTPACTION=0\\r\\n"));
  gprsSerial.println(F("AT+HTTPACTION=0"));  /* Start GET ession */
  ShowSerialData();
  delay(3000);
  
  gprsSerial.println(F("AT+HTTPREAD"));  /* Read data from HTTP server */
  ShowSerialData();
  delay(1000);
  pSerial();
  
  debugln(F("AT+HTTPTERM\\r\\n"));  
  gprsSerial.println(F("AT+HTTPTERM"));  /* Terminate HTTP service */
  delay(500);
  ShowSerialData();
  gprsSerial.println(F("AT+SAPBR=0,1")); /* Close GPRS context */
  delay(100);
  ShowSerialData();
}

void pSerial()
{
  while(gprsSerial.available())
  {
    instr = gprsSerial.readString();
    int ind = instr.indexOf("\n+HTTPREAD:");
    if(ind >= 0) {
      debugln("got it: ");
      debugln(instr);
      int start = 28;
      String bin = "";
      for (int i=28;i< instr.length(); i++){
        if (instr[i]=='\n') {
          break;
        }
        bin += instr[i];
      };
      debugln(bin);

      switch (bin[0]){
        case '1':
        {
          debugln(F("payment confirmed"));
          int binLength = bin.length();
          String units = bin.substring(1, binLength);
          debug("Units: ");
          debugln(units);
          SendMessage("Unit "+units+" zimeongezwa kwenye salio lako la maji.");
          // add the numbers to permanent memory.
          update_units(units.toInt());
          update_last_units(units.toInt());
          data.has_units = 1;
          break;
        }
        case '2':
        {
          debugln(F("Already used token"));
          SendMessage(F("Token hii ya malipo imekwisha tumika"));
          break;
        }
        case '0':
        {
          debugln(F("Cannot find this token"));
          SendMessage(F("Tokeni hii ya malipo haipo."));
          break;
        }
        default:
        {
          debugln(F("Network error"));
          SendMessage(F("Kuna shida ya mtandao. Tafadhali jaribu tena."));
          break;
        }
      }     
    }
    instr = "";
  }
}

void rSerial()
{
  while(gprsSerial.available())
  {
    instr = gprsSerial.readString();
    int ind = instr.indexOf("\n+HTTPREAD:");
    if(ind >= 0) {
      debugln("got it: ");
      debugln(instr.length());
      String bin = instr.substring(28,29);
      debugln(bin);

      switch (bin[0]){
        case '1':
        {
          debugln(F("registration complete"));
          debug("number: ");
          debugln(number);
          rSendMessage(number, F("Uhakiki wa usajili umekamilika. Huduma zimeunganishwa na namba hii."));
          // add the numbers to permanent memory.
          update_customer_phone();
          break;
        }
        case '2':
        {
          debugln(F("Registration exists"));
          SendMessage(F("Usajili ulikwisha fanyika"));
          update_customer_phone();
          break;
        }
        case '0':
        {
          debugln(F("Cannot find this registration"));
          rSendMessage(number, F("Usajili wako haupo"));
          break;
        }
        default:
        {
          debugln(F("Network error"));
          if (data.is_registered == 0)
            rSendMessage(number, F("Kuna shida ya mtandao. Tafadhali jaribu tena."));
          else if (data.is_registered == 1)
            SendMessage(F("Kuna shida ya mtandao. Tafadhali jaribu tena."));
          break;
        }
      }     
    }
    instr = "";
  }
}


void ShowSerialData()
{
  while(gprsSerial.available()!=0)
  Serial.write(gprsSerial.read());
  delay(5000); 
}


// Auxiliary functions
//
// ---------------------------------------------------------------------

void flow () // Interrupt function to increment flow
{
   flow_sense.flow_frequency++;
}


void update_last_reading (float new_reading) // Whenever last reading needs updating
{
  store(mem_addr.last_reading_addr, new_reading);
  data.last_reading = new_reading;
}

void update_customer_phone () // Whenever customer needs updating
{
  int k = 0;
  for (int i=(int)mem_addr.customer_phone_addr;i<13+(int)mem_addr.customer_phone_addr;i++){
    store(i, number[k]);
    k++;
  }  
  data.is_registered == 1;
}

void read_customer_phone () // Whenever customer needs updating
{
  int k = 0;
  for (int i=(int)mem_addr.customer_phone_addr;i<13+(int)mem_addr.customer_phone_addr;i++){
    debugln(retrieve(i));
    customer_phone[k] = retrieve(i);
    k++;
  }  
}

void update_last_units (int new_units) // whenever a new limit is set
{
  store(mem_addr.last_units_addr, new_units);
  data.last_units = new_units;
}

void update_units (int new_units) // whenever new units are addded
{
   store(mem_addr.units_addr, new_units);
   data.units = new_units;
}

void initializeValues() {
//    store initial values to memory
//    update_units(10);
//    update_last_units(10);
//    update_water_limit_vol(0.00);
//    update_last_reading(0.00);
//    update_customer_phone();
      read_customer_phone();
      debug("number: ");
      debugln(number);
}
