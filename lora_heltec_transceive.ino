/*********
  stupid sx1276 transceiver (xbee series 1 style) by spiritplumber
  based on https://randomnerdtutorials.com/ttgo-lora32-sx1276-arduino-ide/ by  Rui Santos
*********/

//Libraries for LoRa
#include <SPI.h>
#include <LoRa.h>

//Libraries for OLED Display
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

//define the pins used by the LoRa transceiver module
#define SCK 5
#define MISO 19
#define MOSI 27
#define SS 18
#define RST 14
#define DIO0 26

//433E6 for Asia
//866E6 for Europe
//915E6 for North America
#define BAND 915E6

//OLED pins
#define OLED_SDA 4
#define OLED_SCL 15
#define OLED_RST 16
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RST);

//packet counter
int txcounter = 0;
int rxcounter = 0;
byte charcounter = 0;
boolean readytosend = false;
boolean dodisplay = false;
boolean dodisplaybuf = false;
boolean displayexists = false;

String LoRaData;


#define MAXPKTSIZE 200
char receivedChars[MAXPKTSIZE]; // an array to store the received data
int rssi; // last packat received rssi

void setup() {

  //reset OLED display via software
  pinMode(OLED_RST, OUTPUT);
  digitalWrite(OLED_RST, LOW);
  delay(20);
  digitalWrite(OLED_RST, HIGH);

  //initialize OLED
  Wire.begin(OLED_SDA, OLED_SCL);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3c, false, false)) { // Address 0x3C for 128x32
    Serial.println(F("SSD1306 allocation failed"));
    displayexists = false;
  }
  else
  {
    display.clearDisplay();
    display.setTextColor(WHITE);
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.print("Deus Nolens Exitus ");
    display.display();
    displayexists = true;
    dodisplay = true;
  }

  //initialize Serial Monitor
  Serial.begin(9600); // default slow baud rate. i'd like to lower it to 2400 if it was up to me

  //SPI LoRa pins
  SPI.begin(SCK, MISO, MOSI, SS);
  //setup LoRa transceiver module
  LoRa.setPins(SS, RST, DIO0);

  // stuff broke
  if (!LoRa.begin(BAND)) {
    Serial.println("Starting LoRa failed!");
    if (displayexists)
    {
      display.setCursor(0, 10);
      display.println("Starting LoRa failed!");
      display.display();
    }
    while (1); // we ded
  }
}

void loop() {

  //see if there's anything on the radio, and if there is, be ready to send it
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    //received a packet
    rssi = LoRa.packetRssi();

    //read packet
    while (LoRa.available()) {
      LoRaData = LoRa.readString();
      Serial.print(LoRaData);
    }
    dodisplay = true;
  }



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



  // either update the entire display, or just the buffer indicator (faster)
  if (dodisplay) // update entire display
  {
    if (displayexists)
    {
      display.clearDisplay();
      display.setCursor(0, 0);
      display.print("RTX RSSI:");
      display.setCursor(55, 0);
      display.print(rssi);
      display.setCursor(85, 0);
      display.print("BUF:");
      display.setCursor(110, 0);
      display.print(charcounter);
      display.setCursor(0, 20);
      display.print(LoRaData);
      display.display();
    }
    dodisplaybuf = false;
    dodisplay = false;
  }
  else if (dodisplaybuf) // update only buffer count
  {
    if (displayexists)
    {
      display.fillRect(110, 0, 45, 11, BLACK); // upperleftx, upperlefty, width, height, color
      display.setCursor(110, 0);
      display.print(charcounter);
      display.display();
    }
    dodisplaybuf = false;
  }



  // do actual sending; stay in send mode for as little as possible; this should be followed by the receive function
  if (readytosend)
  {
    //Send LoRa packet to receiver
    //  Serial.print("TX:");
    //  Serial.println(receivedChars);
    LoRa.beginPacket();
    LoRa.print(receivedChars);
    LoRa.endPacket();
    dodisplay = true;
    readytosend = false;
    charcounter = 0;
    for (int i = 0; i < MAXPKTSIZE; i++)
    {
      receivedChars[i] = 0;
    }
  }
}
