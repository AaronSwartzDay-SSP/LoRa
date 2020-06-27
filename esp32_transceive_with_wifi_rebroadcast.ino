/*********
  simple/stupid gateway between lora, wifi and com port(s) by spiritplumber
  (c) 2020 Robots Everywhere, LLC until we are ready to release it under copyleft
  Based on Rui Santos tutorials
*********/


// this uses the Heltec Lora+Wifi+ESP32+OLED module. absolutely 0 soldering required!

// a basic repeater will have just lora+uart and can be done on an arduino pro mini with external radio, or a cubecell
// a web gateway should probably use different firmware but can use this same hardware
// a repeater hub for frequency changes will use a parallax propeller (use serial hub code alreay done)
// the uart code can be also used for bluetooth, just copy/paste it

// there's no repeater logic here yet: this is just doing a single hop
// repeater logic to be added when someone buys me more than 2 of these things

// Load Wi-Fi library
#include <WiFi.h>

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
#define BAND 9151E5


//OLED pins
#define OLED_SDA 4
#define OLED_SCL 15
#define OLED_RST 16
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels




// Replace with your network credentials
String ssid     = "CellSol ";
String password = "derpderp";

// Set web server port number to 80
WiFiServer server(80);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RST);

//packet counter
int txcounter = 0;
int rxcounter = 0;
byte charcounter = 0;
boolean readytosend = false;
boolean dodisplay = false;
boolean dodisplaybuf = false;
boolean displayexists = false;
boolean rebroadcast = true;

String LoRaData;


// Variable to store the HTTP request
String header;

// Auxiliar variables to store the current output state
String output_pin_AState = "off";
String output_pin_BState = "off";

String string_rx_9 = "";
String string_rx_8 = "";
String string_rx_7 = "";
String string_rx_6 = "";
String string_rx_5 = "";
String string_rx_4 = "";
String string_rx_3 = "";
String string_rx_2 = "";
String string_rx_1 = "";
String string_rx_0 = "";
String LastThingISentViaLora_1=""; // when repeating, to prevent a broadcast storm
String LastThingISentViaLora_0=""; // when repeating, to prevent a broadcast storm
String gotstring = "";



IPAddress IP;
byte derpme; // third number of ip address


// Assign output variables to GPIO pins
#define output_pin_A 34
#define output_pin_B 35

#define MAXPKTSIZE 200 // lora packet is 255 bytes and we will need some for the header
char receivedChars[MAXPKTSIZE]; // an array to store the received data
int rssi; // last packat received rssi





char* string2char(String command) {
  if (command.length() != 0) {
    char *p = const_cast<char*>(command.c_str());
    return p;
  }
}



void setup() {


  // use the chip id to generate a (hopefully) unique identifier
  uint64_t chipid = ESP.getEfuseMac();
  uint16_t uptwo = (uint16_t)(chipid >> 32);
  uint32_t dnfor = (uint32_t)(chipid);
  dnfor = dnfor + uptwo;
  derpme = dnfor & 255;
  if (derpme == 255) // allows using 192.168.x.1 for the AP which saves a lot of config headaches
    derpme = 0;
  ssid = ssid + "192.168." + derpme + ".1"; // should we even try to do ip over lora? nah. too slow
  IP = IPAddress(192, 168, derpme, 1);
  WiFi.softAP(string2char(ssid));

  // we are using the hardware UART as an input option because if all else fails...
  Serial.begin(9600);  // default slow baud rate. i'd like to lower it to 2400 if it was up to me
  rebroadcast=true;
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

  //SPI LoRa pins
  SPI.begin(SCK, MISO, MOSI, SS);
  //setup LoRa transceiver module
  LoRa.setPins(SS, RST, DIO0);

  // stuff broke. for now just quit since we have nothing to do without lora, but allow local-only point later.
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


  // Initialize the output variables as outputs
  pinMode(output_pin_A, OUTPUT);
  pinMode(output_pin_B, OUTPUT);
  // Set outputs to LOW
  digitalWrite(output_pin_A, LOW);
  digitalWrite(output_pin_B, LOW);

  // Connect to Wi-Fi network with SSID and password
  //Serial.print("Setting AP (Access Point)…");
  // Remove the password parameter, if you want the AP (Access Point) to be open
  //    WiFi.softAP(string2char(ssid), string2char(password));

  delay(50); // needed for softapconfig to stick, see bug 985 for esp32

  WiFi.softAPConfig(IP, IP, IPAddress(255, 255, 255, 0));


  // get chip ID for ssid names

  //Serial.print("AP IP address: ");
  //Serial.println(IP);

  server.begin();
}


// moves old strings out
void cyclestrings(String newone)
{
  string_rx_9 = string_rx_8;
  string_rx_8 = string_rx_7;
  string_rx_7 = string_rx_6;
  string_rx_6 = string_rx_5;
  string_rx_5 = string_rx_4;
  string_rx_4 = string_rx_3;
  string_rx_3 = string_rx_2;
  string_rx_2 = string_rx_1;
  string_rx_1 = string_rx_0;
  string_rx_0 = newone;
  dodisplay = true;
  DoDisplayIfItExists();
}

void DoDisplayIfItExists()
{
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
      display.setCursor(0, 10);
      display.println(IP);
      display.setCursor(0, 20);
      display.println(string_rx_2);
      display.setCursor(0, 30);
      display.println(string_rx_1);
      display.setCursor(0, 40);
      display.println(string_rx_0);
      //display.print(LoRaData);
      display.display();
    }
    dodisplaybuf = false;
    dodisplay = false;
  }
}

void UpdateCharCounterIfDisplayExists()
{
  if (dodisplay == false and displayexists and dodisplaybuf)
  {
    display.fillRect(110, 0, 45, 11, BLACK); // upperleftx, upperlefty, width, height, color
    display.setCursor(110, 0);
    display.print(charcounter);
    display.display();
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


  // Do wifi things here
  WiFiClient client = server.available();   // Listen for incoming clients
  if (client) {                             // If a new client connects,
    //Serial.println("New Client.");          // print a message out in the serial port
    String currentLine = "";                // make a String to hold incoming data from the client
    while (client.connected()) {            // loop while the client's connected
      if (client.available() == false)
      {
        SeeIfAnythingOnRadio();
        ReadFromUart0();
        DoDisplayIfItExists();
        UpdateCharCounterIfDisplayExists();
        SendSerialIfReady();
      }

      if (client.available()) {             // if there's bytes to read from the client,



        SeeIfAnythingOnRadio();
        ReadFromUart0();
        DoDisplayIfItExists();
        UpdateCharCounterIfDisplayExists();
        SendSerialIfReady();


        char c = client.read();             // read a byte, then
        //Serial.write(c);                    // print it out the serial monitor
        header += c;
        if (c == '\n') {                    // if the byte is a newline character
          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0) {
            // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
            // and a content-type so the client knows what's coming, then a blank line:
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println("Connection: close");
            client.println();

            //Serial.println("Header:"+header);

            // Display the HTML web page
            // General purpose html header
            client.println("<!DOCTYPE html><html><head><title>CellSol Bridge 0.01</title><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"><link rel=\"icon\" href=\"data:,\">");
            // CSS to style the on/off buttons
            client.println("<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}.button { background-color: #4CAF50; border: none; color: white; padding: 16px 40px; text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}.button2 {background-color: #555555;}</style></head>");

            // title
            client.println("<body><h2>CellSol Bridge 0.01</h2>192.168.");
            client.print(derpme, DEC);
            client.println(".1<p>");


            client.println("<form action=\"/get\"> Your message: <input type=\"text\" name=\"input1\"><input type=\"submit\" value=\"Submit\"><form><br>");

            client.println("Received(9):" + string_rx_9 + "<br>");
            client.println("Received(8):" + string_rx_8 + "<br>");
            client.println("Received(7):" + string_rx_7 + "<br>");
            client.println("Received(6):" + string_rx_6 + "<br>");
            client.println("Received(5):" + string_rx_5 + "<br>");
            client.println("Received(4):" + string_rx_4 + "<br>");
            client.println("Received(3):" + string_rx_3 + "<br>");
            client.println("Received(2):" + string_rx_2 + "<br>");
            client.println("Received(1):" + string_rx_1 + "<br>");
            client.println("Received(0):" + string_rx_0 + "<br>");

            // fake form element for refreshing the page. anyone good with html wants to make this autorefresh?

            client.println("<form action=\"/get\">  <input type=\"text\" size=\"0\" maxlength=\"0\" name=\"refresh\"><input type=\"submit\" value=\"Refresh this page without sending a message\"><form><br>");


            client.println("</body></html>");


            // do external outputs
            gotstring = header.substring(header.indexOf("input1="));
            gotstring = gotstring.substring(7, gotstring.indexOf("&refresh="));//HTTP/1.1")); //input1= is 7 characters
            if (gotstring.length() > 2)
            {

              // add these as we need to for dealing with html formatting
              gotstring.replace('+', ' ');
              gotstring.replace("%22", "\"");
              gotstring.replace("%27", "'");
              gotstring.replace("%2B", "+");

              Serial.println("RXWEB:" + gotstring);

              //Send LoRa packet to receiver. Will need to make sure we don't send duplicates.
              LoraSendAndUpdate(gotstring);
              cyclestrings("WEB:" + gotstring);
            }

            // The HTTP response ends with another blank line
            client.println();
            // Break out of the while loop
            break;
          } else { // if you got a newline, then clear currentLine
            currentLine = "";
          }
        } else if (c != '\r') {  // if you got anything else but a carriage return character,
          currentLine += c;      // add it to the end of the currentLine
        }
      }
    }
    // Clear the header variable
    header = "";
    // Close the connection
    client.stop();
    //Serial.println("Client disconnected.");
    //Serial.println("");
  }
}
