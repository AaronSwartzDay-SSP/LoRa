/*********
  simple/stupid lora transceiver with serial port between lora com port(s) by spiritplumber
  (c) 2020 Robots Everywhere, LLC until we are ready to release it under copyleft
  Based on Rui Santos tutorials

  Note that this is doing a bunch of stuff that we don't need, and it's not
  optimized at all. it's just for me to stay sane because I want to keep the basic
  functions the same between this and the esp32 board. for example we really
  don't care about holding old messages on this guy. optimized version to follow.

*********/


//Libraries for LoRa
#include <SPI.h>
#include <LoRa.h>

//define the pins used by the LoRa transceiver module
// it's pretty much how you wire the whole thing, too
#define SCK 13
#define MISO 12
#define MOSI 11
#define SS 10
#define RST 9 // move me someday, i want to save a pwm pin
//#define DIO0 8 // not used right now?

//433E6 for Asia
//866E6 for Europe
//915E6 for North America
#define BAND 9151E5

//packet counter
int txcounter = 0;
int rxcounter = 0;
byte charcounter = 0;
boolean readytosend = false;
boolean dodisplay = false;
boolean dodisplaybuf = false;
boolean displayexists = false;
boolean rebroadcast = true;

String LoRaData = "";
//String string_rx_9 = "";
//String string_rx_8 = "";
//String string_rx_7 = "";
//String string_rx_6 = "";
//String string_rx_5 = "";
//String string_rx_4 = "";
//String string_rx_3 = "";
//String string_rx_2 = "";
//String string_rx_1 = "";
String string_rx_0 = "";
String LastThingISentViaLora_1 = ""; // when repeating, to prevent a broadcast storm
String LastThingISentViaLora_0 = ""; // when repeating, to prevent a broadcast storm


#define MAXPKTSIZE 200
char receivedChars[MAXPKTSIZE]; // an array to store the received data
int rssi; // last packat received rssi

void setup() {
  //initialize Serial Monitor
  Serial.begin(9600); // default slow baud rate. i'd like to lower it to 2400 if it was up to me
  rebroadcast=true;
  //SPI LoRa pins
  SPI.begin();//SCK, MISO, MOSI, SS);
  //setup LoRa transceiver module
  LoRa.setPins(SS, RST);//, DIO0);

  // stuff broke
  if (!LoRa.begin(BAND)) {
    Serial.println("Starting LoRa failed!");
    while (1); // we ded
  }
}

// moves old strings out
void cyclestrings(String newone)
{
//  string_rx_9 = string_rx_8;
//  string_rx_8 = string_rx_7;
//  string_rx_7 = string_rx_6;
//  string_rx_6 = string_rx_5;
//  string_rx_5 = string_rx_4;
//  string_rx_4 = string_rx_3;
//  string_rx_3 = string_rx_2;
//  string_rx_2 = string_rx_1;
//  string_rx_1 = string_rx_0;
  string_rx_0 = newone;
  dodisplay = true;
  DoDisplayIfItExists();
}


// alas, display does not exist
void DoDisplayIfItExists()
{
  if (dodisplay)
  {
    if (displayexists)
    {
    }
    dodisplaybuf = false;
    dodisplay = false;
  }
}

// alas, display does not exist
void UpdateCharCounterIfDisplayExists()
{
  if (dodisplay == false and displayexists and dodisplaybuf)
  {
    dodisplaybuf = false;
  }
}


void ReadFromUart0() {
  // the function below can be copied and pasted for additional UARTs, bluetooth, soft serial (with flag character), audioserial etc.
  // see if there's anything up the UART buffer and transfer it to the message buffer, then decide whether to send it
  if (Serial.available() > 0)
  {
    receivedChars[charcounter] = Serial.read();
    //Serial.print(receivedChars[charcounter]);
    //Serial.print(":");
    //Serial.print(charcounter);
    //Serial.print(":");
    //Serial.print( (uint8_t) receivedChars[charcounter]);
    //Serial.print(" ");
    dodisplaybuf = true;
    UpdateCharCounterIfDisplayExists();
    if ((++charcounter >= MAXPKTSIZE) or (charcounter > 1 and receivedChars[charcounter - 1] == 13))
    {
      dodisplay = true;
      readytosend = true;
    }
    if (receivedChars[0] == 10 or receivedChars[0] == 13 or receivedChars[1] == 10 or receivedChars[1] == 13) // eliminate stray RFs in case we get a CRLF, and don't send empty packets
    {
      charcounter = 0;
      receivedChars[0] = 0;
      receivedChars[1] = 0;
      readytosend = false;
      dodisplay = false;
    }
  }
}

void SeeIfAnythingOnRadio() {
  //see if there's anything on the radio, and if there is, be ready to send it
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    //received a packet
    rssi = LoRa.packetRssi();
    //read packet
    while (LoRa.available()) {
      LoRaData = LoRa.readString();
      if (LoRaData.equals(LastThingISentViaLora_0)==false)
      {
      Serial.println("RXWAN:" + LoRaData);
      cyclestrings("WAN:" + LoRaData);
      MaybeDoRebroadcast(LoRaData);
      }
    }
    dodisplay = true;
  }
}

void MaybeDoRebroadcast(String whattosend)
{
  if (rebroadcast)
    if (LastThingISentViaLora_0.equals(whattosend) == false)
      if (LastThingISentViaLora_1.equals(whattosend) == false)
        LoraSendAndUpdate(whattosend);
}

void LoraSendAndUpdate(String whattosend)
{
  LoRa.beginPacket();
  LoRa.print(whattosend);
  LoRa.endPacket();
  LastThingISentViaLora_1 = LastThingISentViaLora_0; // hopefully prevent broadcast storms; needs some sort of TTL
  LastThingISentViaLora_0 = whattosend; // hopefully prevent broadcast storms; needs some sort of TTL

}

void SendSerialIfReady()
{
  // do actual sending; stay in send mode for as little as possible; this should be followed by the receive function
  if (readytosend)
  {
    //Send LoRa packet to receiver
    //  Serial.print("TX:");
    //  Serial.println(receivedChars);
    LoraSendAndUpdate(receivedChars);
    dodisplay = true;
    readytosend = false;
    charcounter = 0;
    cyclestrings("COM:" + String(receivedChars));
    for (int i = 0; i < MAXPKTSIZE; i++)
    {
      receivedChars[i] = 0;
    }
  }
}






void loop() {

  SeeIfAnythingOnRadio();
  ReadFromUart0();
  DoDisplayIfItExists();
  UpdateCharCounterIfDisplayExists();
  SendSerialIfReady();
}
