/*
  WiFi Web Server LED

  A simple web server that lets you blink an LED via the web.
  This sketch will create a new access point (with no password).
  It will then launch a new server and print out the IP address
  to the //PORTSERIE Monitor. From there, you can open that address in a web browser
  to turn on and off the LED on pin 13.

*/

//#include <SPI.h>
#include <SoftwareSerial.h>
#include <WiFiNINA.h>
// #include <utility/wifi_drv.h>
#include "arduino_secrets.h"
// #include "MenuMoteur.h"
#include "CRC.h"
#include "CRC16.h"
#include <Thread.h>
#include <ThreadController.h>
#include "EEPROM_24.h"


#define MOTORSTEPS 1600
#define MAXMENUITEMS  500
#define NBITEMSTRAME 3
#define ZBLENGTHMAX 65

///////please enter your sensitive data in the Secret tab/arduino_secrets.h
char ssid[] = SECRET_SSID;  // your network SSID (name)
char pass[] = SECRET_PASS;  // your network password (use for WPA, or use as key for WEP)
int keyIndex = 0;           // your network key index number (needed only for WEP)

int status = WL_IDLE_STATUS;
WiFiServer server(80);
SoftwareSerial XBee(2, 3);  // RX, TX
EEPROM_24 Eeprom;  // Using default address in EEPROM.h

//String lastZBData = "";
char lastZBData[65];
int lastZBLength;
char receivedZBData[40];
char webCmd[70];
//String webCmd = "";
char currentCharLine[2000];
WiFiClient client;
int32_t   m1Steps = -1;
int32_t   m2Steps = -1;
int32_t   m3Steps = -1;


// MenuM menuM;
uint16_t nbStepsMenu = 0;
uint16_t sendStepsMenu = 0;
uint8_t motorToSend = 0;
uint8_t retrySending = 0;
uint16_t clientCounter = 0;
uint16_t startElementMenu = 0;
uint16_t stopElementMenu = 0;
boolean readyToSend = true;
boolean readyToAnswer = false;
boolean answerExpected = false;
boolean distanceRequest = false;
boolean initContinue = false;

// ThreadController that will controll all threads
ThreadController controll = ThreadController();

//His Thread (not pointer)
Thread webThread = Thread();
Thread readZBThread = Thread();
// Thread writeZBThread = Thread();
Thread sendMenuThread = Thread();
// Thread sendInitThread = Thread();

void setup() {
  
  Serial.begin(115200);
 
  Serial.println("Creating access point...");
    WiFi.setLEDs(0, 0, 50); //GRB

  // check for the WiFi module:
  if (WiFi.status() == WL_NO_MODULE) {
    
      while (true)
      ;
  }else{
  //WiFi.initLEDs();
  //WiFi.setLEDs(120, 0, 120); //GRB
  }

  String fv = WiFi.firmwareVersion();
  if (fv < WIFI_FIRMWARE_LATEST_VERSION) {
    //PORTSERIE.println("Please upgrade the firmware");
  }

  // by default the local IP address will be 192.168.4.1
   WiFi.config(IPAddress(192, 168, 3, 1));

  // print the network name (SSID);
  //PORTSERIE.print("Creating access point named: ");
  //PORTSERIE.println(ssid);

  // Create open network. Change this line if you want to create an WEP network:
  status = WiFi.beginAP(ssid, pass);
    delay(500);
  //WiFi.initLEDs();
  if (status != WL_AP_LISTENING) {
    Serial.println("Creating access point failed");
    Serial.println(status);
    delay(2000);
    WiFi.config(IPAddress(192, 168, 3, 1));
    WiFi.setLEDs(200, 0, 0); //GRB
    status = WiFi.beginAP(ssid, pass);
    delay(500);
  if (status != WL_AP_LISTENING) {  
    Serial.println("Creating access point failed again");
    Serial.println(status);
    WiFi.setLEDs(200, 0, 0); //GRB
    while(1){

    }
  }
   
  }

  XBee.begin(38400);

  // wait 10 seconds for connection:
  for(int i = 0; i<10;i++){
    WiFi.setLEDs(0, 0, 255); //GRB
    delay(500);
    WiFi.setLEDs(0, 0, 0); //GRB
    delay(500);
  }
  // delay(10000);

  // start the web server on port 80
  server.begin();

  // Configure myThread
  webThread.onRun(webCallback);
  webThread.setInterval(5);
  readZBThread.onRun(readZBCallback);
  readZBThread.setInterval(5);
  sendMenuThread.onRun(writeZBCallback);
  sendMenuThread.setInterval(200);
  // sendInitThread.onRun(sendInitZBCallback);
  // sendInitThread.setInterval(100);

  controll.add(&webThread);
  controll.add(&readZBThread);


  Serial.println("Access point created");
  WiFi.setLEDs(0, 255, 0); //GRBn
  Eeprom = EEPROM_24();
 
}

void loop() {

  controll.run();
 
}

//************************************
//*** Threads : Call backs         ***
//************************************
void writeZBCallback() {

  if((sendStepsMenu < nbStepsMenu) && readyToSend){   
    retrySending = 0;
    int dizaine = (nbStepsMenu-sendStepsMenu-1)/NBITEMSTRAME;
    int i;
    long copyInt = dizaine;
  
      // Serial.print(" | dizaine = ");
      // Serial.print(dizaine);
    char zigbeeData[ZBLENGTHMAX] = "Menu=M";
    char cToStr[2];
    cToStr[1] = '\0';
      cToStr[0] = (char) (motorToSend+0x30);
      strcat(zigbeeData,cToStr);
    strcat(zigbeeData,"'");
    for(i = 1; dizaine/i > 9; i *=10){ }
    for(i; i >= 1; i /= 10){
      cToStr[0] = (char) ((copyInt/i)+0x30);
      strcat(zigbeeData,cToStr);
      copyInt = copyInt%i;
    }
    strcat(zigbeeData,":");
    int max;
    if(dizaine==(nbStepsMenu)/NBITEMSTRAME){
      max = (nbStepsMenu)%NBITEMSTRAME;
    }else{
      max = NBITEMSTRAME;
    }
    // Serial.print(" | Max = ");
    // Serial.println(max);

    for(int j = 0; j< max;j++){
      MenuM MenuM = Eeprom.getMenuEEPROM((dizaine*NBITEMSTRAME)+j);
      copyInt = MenuM.startTime;
      for(i = 1; copyInt/i > 9; i *=10){ }
      for(i; i >= 1; i /= 10){
        cToStr[0] = (char) ((copyInt/i)+0x30);
        strcat(zigbeeData,cToStr);
        copyInt = copyInt%i;
      }
      strcat(zigbeeData,"_");
      if(MenuM.steps >= 0){
         strcat(zigbeeData,"+");
      }else{
         strcat(zigbeeData,"-");
      }
      copyInt = abs(MenuM.steps);
      for(i = 1; copyInt/i > 9; i *=10){ }
      for(i; i >= 1; i /= 10){
        cToStr[0] = (char) ((copyInt/i)+0x30);
        strcat(zigbeeData,cToStr);
         copyInt = copyInt%i;
      }
       strcat(zigbeeData,"_");
      copyInt = abs(MenuM.speedM);
      for(i = 1; copyInt/i > 9; i *=10){ }
      for(i; i >= 1; i /= 10){
        cToStr[0] = (char) ((copyInt/i)+0x30);
        strcat(zigbeeData,cToStr);
        copyInt = copyInt%i;
      }
      
       strcat(zigbeeData,";");
    }
    sendStepsMenu= sendStepsMenu+max;
    if(max>0){
      readyToSend = false;
      sendDataToMotor(zigbeeData,strlen(zigbeeData));
    }
    if(sendStepsMenu==nbStepsMenu){
      Serial.print("All steps sent : ");
      Serial.println(sendStepsMenu);
      strcpy(webCmd,"Record:M");
      strcat(webCmd,Eeprom.numberToChar(motorToSend));
      strcat(webCmd,"=ALL");
      readyToAnswer = true;
      //motorToSend = 0;
      sendStepsMenu = 0;
      readyToSend = true;
      controll.remove(&sendMenuThread);
    }
    
  }
  retrySending ++;
  if(retrySending == 40 || retrySending == 80){
      Serial.print("Send back to M");
      Serial.print(motorToSend);
      Serial.print(" = ");
      Serial.println(lastZBData);
      sendDataToMotor(lastZBData,lastZBLength);
  }
  if(retrySending > 120){
      Serial.print("Too many Trys : ");
      Serial.println(motorToSend);
      strcpy(webCmd,"Error Menu M");
      strcat(webCmd,motorToSend);
      readyToAnswer = true;
      controll.remove(&sendMenuThread);
      readyToSend = true;
      motorToSend = 0;
      sendStepsMenu = 0;
      retrySending = 0;
  }
}

void readZBCallback() {
  //Serial.println("ReadCB");
  int setPointChar = 0;
  int nbInt = 0;
  unsigned long params = 0;
  char *p = receivedZBData;
  if (XBee.available()) {
    while(XBee.available()){
       p[0] = XBee.read();
       p++;
    }
    p[0]= NULL;
    Serial.print("!!! Coord Zigbee read : ");
    Serial.println(receivedZBData);
    
    p = strstr(receivedZBData,"CRC Error");
    if(p >0){
        // Serial.println("!!! Coord CRC Error *****");
        p = strstr(receivedZBData,"M")+1;
        if(p[0]-0x30 == motorToSend){
          p++;
          Serial.print("!!! Coord try again CRC! to M");
          Serial.println(motorToSend);
          strcpy(webCmd,"ZB:CRC Error:M");
          strcat(webCmd,Eeprom.numberToChar(motorToSend));
          readyToAnswer = true;
          
          sendDataToMotor(lastZBData,lastZBLength);
          return;
        }
    }
    else{
      p = strstr(receivedZBData,"Start");
      if(p >0){
        p = strstr(receivedZBData,"M")+1;
        Serial.print("Start M");
        Serial.print(p[0]);
         char *q = strstr(p,"=");
         nbInt = (strstr(q,";") - q);
            
        switch(p[0]){
          case 1:
            m1Steps = 0;
            for(int i = 0; i < nbInt; i++){
                m1Steps += (q[0] - '0') * round(pow(10.0,(nbInt-1.0-i)));
                q++;
            }
            Serial.print(" => ");
           Serial.println(m1Steps);
            break;
          case 2:
            m2Steps = 0;
            for(int i = 0; i < nbInt; i++){
                m2Steps += (q[0] - '0') * round(pow(10.0,(nbInt-1.0-i)));
                q++;
            }
            Serial.print(" => ");
           Serial.println(m2Steps);
          break;
          case 3:
            m3Steps = 0;
            for(int i = 0; i < nbInt; i++){
                m3Steps += (q[0] - '0') * round(pow(10.0,(nbInt-1.0-i)));
                q++;
            }
            Serial.print(" => ");
           Serial.println(m3Steps);
          break;
        }
      }

      p = strstr(receivedZBData,"M")+1;
      if(p[0]-0x30 == motorToSend){
        
      // Serial.println("!!! Answer from requested motor");
      strcpy(webCmd,"");
        
        readyToSend = true;

        // Answer from requested motor!!

        if(distanceRequest){
          Serial.println("Distance = ");
          p = strstr(receivedZBData,"distance");
          if(p >0){
            p = strstr(p,"=") + 1;
            nbInt = (strstr(p,";") - p);
            for(int i = 0; i < nbInt; i++){
                params += (p[0] - '0') * round(pow(10.0,(nbInt-1.0-i)));
                p++;
            }
            Serial.print(params);
            Serial.println("mm");
            strcpy(webCmd,"ZB:Distance:M");
          
            strcat(webCmd,Eeprom.numberToChar(motorToSend));
            strcat(webCmd,"=");
            strcat(webCmd,Eeprom.numberToChar(params));
            strcat(webCmd,";");
            readyToAnswer = true;

            distanceRequest = false;
            return;
          }
        }
        else{
          // Menu received
          p = strstr(receivedZBData,"menu");
          if(p >0){
            strcpy(webCmd,"ZB:Record:M");
            strcat(webCmd,Eeprom.numberToChar(motorToSend));
            
            p = strstr(p,"=ALL");
            if(p >0){
              strcat(webCmd,"=ALL;");
            }else{
              p = strstr(receivedZBData,"=") +1;
              nbInt = (strstr(p,";") - p);
              for(int i = 0; i < nbInt; i++){
                  params += (p[0] - '0') * round(pow(10.0,(nbInt-1.0-i)));
                  p++;
              }
              strcat(webCmd,"=");
              strcat(webCmd,Eeprom.numberToChar(params));
              strcat(webCmd,";");
            }
            readyToAnswer = true;
            Serial.println("Menu received:");
            // Serial.println(webCmd);
            return;
          }

          //Movements
          p = strstr(receivedZBData,"move");
          if(p >0){
            strcpy(webCmd,"ZB:Move:M");
            strcat(webCmd,Eeprom.numberToChar(motorToSend));
            
            p = strstr(p,"=END");
            if(p >0){
              strcat(webCmd,"=END;");
            }else{
              p = strstr(receivedZBData,"=") +1;
              nbInt = (strstr(p,";") - p);
              for(int i = 0; i < nbInt; i++){
                  params += (p[0] - '0') * round(pow(10.0,(nbInt-1.0-i)));
                  p++;
              }
              strcat(webCmd,"=");
              strcat(webCmd,Eeprom.numberToChar(params));
              strcat(webCmd,";");
            }
            readyToAnswer = true;
            Serial.println("Movement received:");
            // Serial.println(webCmd);
            return;
          }

          //Reset
          p = strstr(receivedZBData,"reset");
          if(p >0){
            strcpy(webCmd,"ZB:Reset:M");
            strcat(webCmd,Eeprom.numberToChar(motorToSend));
            strcat(webCmd,";");
            readyToAnswer = true;
            Serial.println("Reset received:");
            // Serial.println(webCmd);
            return;
          }

          //Init
          p = strstr(receivedZBData,"init");
          if(p >0){
            
            if(initContinue == true){
              strcpy(webCmd,"ZB:Init:M");
              strcat(webCmd,Eeprom.numberToChar(motorToSend));
              strcat(webCmd," end");
              readyToAnswer = true;
              Serial.print("Init continue received: ");
              // Serial.println(webCmd);
              sendDataToMotor(lastZBData,lastZBLength);
            }else{
              strcpy(webCmd,"ZB:Step:M");
              strcat(webCmd,Eeprom.numberToChar(motorToSend));
              strcat(webCmd," end");
              readyToAnswer = true;
              // Serial.print("1 Step received: ");
              // Serial.println(webCmd);
            }
          }

          //End Commands
        }
        
      }
      else{
        //Not requested motor answer...
        Serial.println("Answer from other motor");
      }
    }
  }else{
  }
}

void webCallback(){
  int cmd = 0;

  if (client) {
    if(answerExpected && readyToAnswer){
      Serial.print("Answering Web : ");
      Serial.println(webCmd);
      client.println("HTTP/1.1 200 OK");
      client.println("Content-type:text/html");
      client.println();
      client.println(webCmd);
      client.println();

      answerExpected = false;
      delay(5);
      client.stop();
    } 
    else if (answerExpected){
      clientCounter ++;
      if(clientCounter > 1500){
        Serial.println("Answer skipped!");
        strcpy(webCmd,"ERR:");
        strcat(webCmd,lastZBData);
        client.println("HTTP/1.1 200 OK");
        client.println("Content-type:text/html");
        client.println();
        client.println(webCmd);
        client.println();
        delay(5);
        client.stop();
        answerExpected = false;
        clientCounter = 0;
      }
    }

  }else{

    client = server.available();   // listen for incoming clients

    char *p = currentCharLine;
    char *test;
    while (client.connected()) {            // loop while the client's connected
    
      delayMicroseconds(10);                // This is required for the Arduino Nano RP2040 Connect - otherwise it will loop so fast that SPI will never be served.

      if (client.available()) {             // if there's bytes to read from the client,
        char c = client.read();             // read a byte, then
        if (c == '\n') {                    // if the byte is a newline character
        // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if ((p-currentCharLine) == 0) {
            answerExpected = true;
            // webCmd = "";
            // Serial.println("Answering Web :");
            // Serial.println(webCmd);
            // Serial.println("******");
            // client.println("HTTP/1.1 200 OK");
            // client.println("Content-type:text/html");
            // client.println();
            // client.println(webCmd);
            // client.println();
            break;
          }
          else {      // if you got a newline, then clear currentLine:
              if(cmd > 0){
                  
                  p = strstr(currentCharLine," HTTP/1.1");
                  p[0] = NULL;      
                  //webCmd = "";
                  if((p-currentCharLine)>0){
                    webCmd[0]= NULL;
                    readyToAnswer = false;
                    treatCmd(cmd,currentCharLine,(p-currentCharLine));
                  }else{
                    readyToAnswer = true;
                  }
                  cmd= 0;
                  p=currentCharLine;
                  //break;
              }else{
                
                  p=currentCharLine;
                  //currentLine = "";
              }
          }
        }
        else if (c != '\r') {    // if you got anything else but a carriage return character,
          p[0] = c;      // add it to the end of the currentLine
          //Serial.print(p[0]);
          p++;
        }
        test = strstr(currentCharLine,"GET /M1=");
        if (test != NULL) {
          //Serial.println("Commande Get M1");
              p=currentCharLine;
              //currentLine = "";
              cmd = 1;
               
              motorToSend = cmd;
        } 
        test = strstr(currentCharLine,"GET /M2=");
        if (test != NULL) {
          //Serial.println("Commande Get M2");
              p=currentCharLine;
              //currentLine = "";
              cmd = 2;
                
              motorToSend = cmd;
        }
        test = strstr(currentCharLine,"GET /M3=");
        if(test != NULL) {
          //Serial.println("Commande Get M3");
              p=currentCharLine;
              //currentLine = "";
              cmd = 3;
                
              motorToSend = cmd;
        }
        test = strstr(currentCharLine,"GET /ALL=");
        if(test != NULL) {
          //Serial.println("Commande Get ALL");
              p=currentCharLine;
              //currentLine = "";
              cmd = 10;
        }
      }
    }
  }
  
}

//**********************************************
//******    Commands                       *****
//**********************************************

void treatCmd(int cmd, char *commandToTreat,int length) {
  //int length = currentLine.length();
  //char *commandToTreat =  (char*)malloc(length+1);
  // //currentLine.toCharArray(commandToTreat,length+1);
  // Serial.println("********");
  // Serial.print("Char command : ");
  // Serial.print(length);
  // Serial.print(" = ");
  // Serial.println(commandToTreat);
  char *p;
  motorToSend = cmd;

  p = strstr (commandToTreat,"Menu:");
  if(p != NULL){
    Serial.println("Cmd MENU save!");
    commandToTreat = strstr(commandToTreat,":")+1;
    length = length -5;
    // Serial.println(commandToTreat);
    decodeMenu(cmd,commandToTreat,length);
  }

  p = strstr (commandToTreat,"Send:");
  if(p != NULL){
    Serial.println("Cmd MENU send!");
    //printMenu(motorToSend);
    controll.add(&sendMenuThread); 
  }

  //CMD 1 STEP
  p = strstr(commandToTreat, "Step:");
  if (p != NULL) {
    Serial.println("Cmd 1 Step : ");
    p = strstr(commandToTreat, ":") + 1;
    sendInit(false,cmd, p);
  }

  p = strstr (commandToTreat,"Init:");
  if(p != NULL){
    Serial.println("Cmd INIT : ");
    p = strstr (commandToTreat,":")+1;
    sendInit(true, cmd, p);
  } 

  p = strstr (commandToTreat,"Desinit");
  if(p != NULL){
    Serial.println("Cmd DESINIT!");
    //initContinu
    sendInit(false, 0, "");
  }


  p = strstr (commandToTreat,"Distance");
  if(p != NULL){
    Serial.println("Cmd Distance : ");
    sendDistance(cmd);
  }
  
  p = strstr (commandToTreat,"Reset");
  if(p != NULL){

    p = strstr (commandToTreat,":dist");
    if(p != NULL){
      Serial.println("Cmd reset distance : ");
      sendResetDist(cmd);
    }

    p = strstr (commandToTreat,":items");
    if(p != NULL){
      Serial.println("Cmd reset items : ");
      sendResetItems(cmd);
    }
  }

  p = strstr (commandToTreat,"Start");
  if(p != NULL){
    Serial.print("Cmd START!");
    strcpy(webCmd,"Start");
    sendDataToMotor("Start",strlen("Start"));

  } 
  
  p = strstr (commandToTreat,"Stop");
  if(p != NULL){
    Serial.print("Cmd STOP!");
    strcpy(webCmd,"Stop");
    sendDataToMotor("Stop",strlen("Stop"));
  }

  p = strstr (commandToTreat,"Accel:");
  if(p != NULL){
    // Serial.print("Accel!");
    strcpy(webCmd,"Accel");
    p = strstr (commandToTreat,":")+1;
    sendAcceleration(p);
  }
  
    //free(commandToTreat);
}


void sendDistance(int motor) {
    char zigbeeData[20] = "Distance=M";
    motorToSend = motor;
    strcat(zigbeeData,Eeprom.numberToChar(motor));
    distanceRequest = true;
    sendDataToMotor(zigbeeData,strlen(zigbeeData));
  
}

void sendResetDist(int motor) {
    char zigbeeData[20] = "RstDis=M";
    motorToSend = motor;
    strcat(zigbeeData,Eeprom.numberToChar(motor));
    distanceRequest = false;
    sendDataToMotor(zigbeeData,strlen(zigbeeData));
  
}

void sendResetItems(int motor) {
    char zigbeeData[20] = "RstIte=M";
    motorToSend = motor;
    strcat(zigbeeData,Eeprom.numberToChar(motor));
    distanceRequest = false;
    sendDataToMotor(zigbeeData,strlen(zigbeeData));
  
}


void sendInit(boolean initiate, int motor, char data[]) {
  // Serial.println("********");
  motorToSend = motor;
  
  initContinue = initiate;

  char zigbeeData[40] = "Init=M";
  strcat(zigbeeData,Eeprom.numberToChar(motorToSend));
  strcat(zigbeeData,":");
  strcat(zigbeeData,data);
  sendDataToMotor(zigbeeData,strlen(zigbeeData));
  
}

void sendAcceleration(char data[]){
  char zigbeeData[20] = "Accel=";
  strcat(zigbeeData,data);
  Serial.println("accel cmd to zigbee : ");
  Serial.println(zigbeeData);
  sendDataToMotor(zigbeeData,strlen(zigbeeData));
}


//**********************************************
//******    Decoding                       *****
//**********************************************

void decodeMenu(int motor, char menu_to_decode[], int length)
{

  char *p = menu_to_decode;
  char readChar;
  int setpoint = 0;
  int speedpoint = 0;
  int indexMenu = startElementMenu;
  MenuM menuM;

  int nbInt = (strstr(p,":") - p);
  for(int i = 0; i < nbInt; i++){
    indexMenu += (p[0] - '0') * round(pow(10.0,(nbInt-1.0-i)));
    p++;
  }
  Serial.print("Menu Starting @ : ");
  Serial.println(indexMenu);
    
  p = strstr(p,":")+1;
  
  while( p-menu_to_decode < length){

    //PORTSERIE.print(indexMenu);
    menuM.startTime = 0;
    menuM.steps = 0;
    menuM.speedM = 0;

    uint32_t startTime = 0;
    int32_t steps = 0;
    uint32_t speedM = 0;
    
    //int nbInt = (menu_to_decode.indexOf('_',setPointChar) - setPointChar);
    int nbInt = (strstr(p,"_") - p);
     for(int i = 0; i < nbInt; i++){
        startTime += (p[0] - '0') * round(pow(10.0,(nbInt-1.0-i)));
        p++;
     }

    //Get # steps to run, if steps > 0 motor FW, if steps < 0 motor RE
    // setPointChar = menu_to_decode.indexOf('_',setPointChar)+1;
    // nbInt = (menu_to_decode.indexOf('_',setPointChar) - setPointChar -1);

    p++;
    nbInt = (strstr(p,"_") - p -1);
    
    if(p[0] == '+'){
     // Positive # of steps
      p++;
      for(int i = 0; i < nbInt; i++){
         steps += (p[0] - '0') * round(pow(10.0,(nbInt-1.0-i)));
         p++;
      }
   }else if(p[0] == '-'){
     // Negative # of steps
      p++;
      for(int i = 0; i < nbInt; i++){ 
        ////PORTSERIE.println("Negative  : ");
        steps += (p[0] - '0') * round(pow(10.0,(nbInt-1.0-i)));
        p++;
        ////PORTSERIE.println("Negative  : ");
     }
        steps = steps*-1;
   }

    p++;
   //Get Speed in tr/min
    // setPointChar = menu_to_decode.indexOf('_',setPointChar)+1;
    // nbInt = (menu_to_decode.indexOf(';',setPointChar) - setPointChar);
    nbInt = (strstr(p,";") - p);
   
    for(int i = 0; i < nbInt; i++){
      speedM += (p[0] - '0') * round(pow(10.0,(nbInt-1.0-i)));
      p++;
    }
    //setPointChar = menu_to_decode.indexOf(';',setPointChar)+1;
    p++;

    menuM.startTime = startTime;
    menuM.steps = steps;
    menuM.speedM = speedM;
    
    Eeprom.putMenuEEPROM(indexMenu,menuM);
    indexMenu++;

    if(indexMenu>= MAXMENUITEMS ){
      break;
    }

  }

  nbStepsMenu = indexMenu;
  sendStepsMenu = 0;
  motorToSend = motor;
  strcpy(webCmd,"Record:M");
  strcat(webCmd,Eeprom.numberToChar(motorToSend));
  strcat(webCmd,"=");
  strcat(webCmd,Eeprom.numberToChar(nbStepsMenu));
  readyToAnswer = true;
  // if(nbStepsMenu != stopElementMenu){
  //   Serial.println("!!!!! ERROR NB ELEMENTS!!!!");
  // }
  //controll.add(&sendMenuThread); moved to a dedicated command
}


//**********************************************
//******       Send Zigbee                 *****
//**********************************************

void sendDataToMotor(char *data, int length) {
  strcpy(lastZBData,data);
  lastZBLength = length;
  CRC16 crc;
  // Serial.println("**** Sending ZB :");
  // Serial.println(data);
  // Serial.println("**********");
  strcpy(receivedZBData,"");

  crc.setPolynome(0x1021);
  crc.add(data, length);
  char crcText[2];
  uint16_t crcValue = crc.getCRC();
  crcText[0] = crcValue & 0xFF;
  crcText[1] = (crcValue >> 8) & 0xFF;
  
    // Serial.print( "CRC = ");
    // Serial.print( crcText[1],HEX);
    // Serial.print( " | ");
    // Serial.println( crcText[0],HEX);
    // Serial.print( " =  ");
    // Serial.println( crcValue);
  

  XBee.print(data);
  XBee.write(Eeprom.getAscii((crcText[1]>>4)&0xF));
  XBee.write(Eeprom.getAscii((crcText[1]>>0)&0xF));
  XBee.write(Eeprom.getAscii((crcText[0]>>4)&0xF));
  XBee.write(Eeprom.getAscii((crcText[0]>>0)&0xF));
  XBee.write(0x04);  // EOF
}

//**********************************************
//******       Prints                      *****
//**********************************************

void printMenu(int motor){
  Serial.println("********");

    Serial.print("Menu M");
    Serial.print(motor);
    Serial.print(" : ");
    Serial.println(nbStepsMenu);
    for(int i =0; i< nbStepsMenu; i++){
      MenuM menuM = Eeprom.getMenuEEPROM(i);
      Serial.print(i);
      Serial.print(" : ");
      Serial.print("Start Time = ");
      Serial.print(menuM.startTime);
      Serial.print(" | Steps = ");
      Serial.print(menuM.steps);
      Serial.print(" | Motor Speed = ");
      Serial.println(menuM.speedM);
    }
  
}

void printWiFiStatus() {
  // print the SSID of the network you're attached to:
  //PORTSERIE.print("SSID: ");
  //PORTSERIE.println(WiFi.SSID());

  // print your WiFi shield's IP address:
  IPAddress ip = WiFi.localIP();
  //PORTSERIE.print("IP Address: ");
  //PORTSERIE.println(ip);

  // print where to go in a browser:
  // //PORTSERIE.print("To see this page in action, open a browser to http://");
  // //PORTSERIE.println(ip);

}
