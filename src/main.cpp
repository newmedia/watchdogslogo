#include <Adafruit_NeoPixel.h>
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <EEPROM.h>

#define OUTERRING_PIN 4
#define OUTERLOGO_PIN 5
#define INNERLOGO_PIN 2
#define NUMPIXELS_OUTERRING 111
#define NUMPIXELS_OUTERLOGO 80
#define NUMPIXELS_INNERLOGO 20
#define DELAYVAL 500

// Effects
#define RAINBOWCYCLE 0
#define THEATHERCASE 1
#define THEATHERCASERAINBOW 2
#define FIRE 3


WiFiServer server(80);
Adafruit_NeoPixel pixel_outerring(NUMPIXELS_OUTERRING, OUTERRING_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel pixel_outerlogo(NUMPIXELS_OUTERLOGO, OUTERLOGO_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel pixel_innerlogo(NUMPIXELS_INNERLOGO, INNERLOGO_PIN, NEO_GRB + NEO_KHZ800);
WiFiManager wifiManager;
String header;

// Timings
unsigned long currentMillis = 0;
unsigned long previousWebServerMillis = 0;
int timeOut = 3000;

const int webServerInterval = 300;


// Default effect
byte selectedEffect=RAINBOWCYCLE;
Adafruit_NeoPixel selectedPixel; 

void setup() {
  Serial.begin(9600);
  Serial.println("Booting WatchDogsLogo...");

  Serial.println("Seeding randomness..");
  randomSeed(analogRead(0));

  // Setup eyes
  pinMode(OUTERRING_PIN, OUTPUT);
  pinMode(OUTERLOGO_PIN, OUTPUT);
  pinMode(INNERLOGO_PIN, OUTPUT);
  pixel_outerring.begin();
  pixel_outerlogo.begin();
  pixel_innerlogo.begin();
  pixel_outerring.show();
  pixel_outerlogo.show();
  pixel_innerlogo.show();

  // WiFi Manager
  wifiManager.setAPStaticIPConfig(IPAddress(10,0,1,1), IPAddress(10,0,1,1), IPAddress(255,255,255,0));
  wifiManager.autoConnect("WatchDogsLogo");

  Serial.println("Connected to wifi.");
  server.begin();
}

// GENERIC FUNCTIONS

// Apply LED color changes
void showStrip(Adafruit_NeoPixel* pixel) {
   pixel->show();
}

// Set a LED color (not yet visible)
void setPixel(Adafruit_NeoPixel* pixel, int pixel_id, byte red, byte green, byte blue) {
   pixel->setPixelColor(pixel_id, pixel->Color(red, green, blue));
}

// Set all LEDs to a given color and apply it (visible)
void setAll(Adafruit_NeoPixel* pixel, byte red, byte green, byte blue) {
  for(int i = 0; i < pixel->numPixels(); i++ ) {
    setPixel(pixel, i, red, green, blue); 
  }
  showStrip(pixel);
}



// used by rainbowCycle and theaterChaseRainbow
byte * Wheel(byte WheelPos) {
  static byte c[3];
  
  if(WheelPos < 85) {
   c[0]=WheelPos * 3;
   c[1]=255 - WheelPos * 3;
   c[2]=0;
  } else if(WheelPos < 170) {
   WheelPos -= 85;
   c[0]=255 - WheelPos * 3;
   c[1]=0;
   c[2]=WheelPos * 3;
  } else {
   WheelPos -= 170;
   c[0]=0;
   c[1]=WheelPos * 3;
   c[2]=255 - WheelPos * 3;
  }

  return c;
}

// RAINBOW
void rainbowCycle(Adafruit_NeoPixel* pixel, int SpeedDelay) {
  byte *c;
  uint16_t i, j;

  EEPROM.put(0, RAINBOWCYCLE);

  for(j=0; j<256*5; j++) { // 5 cycles of all colors on wheel
    for(i=0; i< pixel->numPixels(); i++) {
      c=Wheel(((i * 256 / pixel->numPixels()) + j) & 255);
      setPixel(pixel, i, *c, *(c+1), *(c+2));
    }
    showStrip(pixel);
    delay(SpeedDelay);
  }
}


void theaterChase(Adafruit_NeoPixel* pixel,byte red, byte green, byte blue, int SpeedDelay) {
  for (int j=0; j<10; j++) {  //do 10 cycles of chasing
    for (int q=0; q < 3; q++) {
      for (int i=0; i < pixel->numPixels(); i=i+3) {
        setPixel(pixel, i+q, red, green, blue);    //turn every third pixel on
      }
      showStrip(pixel);
     
      delay(SpeedDelay);
     
      for (int i=0; i < pixel->numPixels(); i=i+3) {
        setPixel(pixel, i+q, 0,0,0);        //turn every third pixel off
      }
    }
  }
}

void theaterChaseRainbow(Adafruit_NeoPixel* pixel,int SpeedDelay) {
  byte *c;
  
  for (int j=0; j < 256; j++) {     // cycle all 256 colors in the wheel
    for (int q=0; q < 3; q++) {
        for (int i=0; i < pixel->numPixels(); i=i+3) {
          c = Wheel((i+j) % 255);
          setPixel(pixel, i+q, *c, *(c+1), *(c+2));    //turn every third pixel on
        }
        showStrip(pixel);
       
        delay(SpeedDelay);
       
        for (int i=0; i < pixel->numPixels(); i=i+3) {
          setPixel(pixel, i+q, 0,0,0);        //turn every third pixel off
        }
    }
  }
}


void setPixelHeatColor (Adafruit_NeoPixel* pixel, int pixel_id, byte temperature) {
  // Scale 'heat' down from 0-255 to 0-191
  byte t192 = round((temperature/255.0)*191);
 
  // calculate ramp up from
  byte heatramp = t192 & 0x3F; // 0..63
  heatramp <<= 2; // scale up to 0..252
 
  // figure out which third of the spectrum we're in:
  if( t192 > 0x80) {                     // hottest
    setPixel(pixel, pixel_id, 255, 255, heatramp);
  } else if( t192 > 0x40 ) {             // middle
    setPixel(pixel, pixel_id, 255, heatramp, 0);
  } else {                               // coolest
    setPixel(pixel, pixel_id, heatramp, 0, 0);
  }
}

void Fire(Adafruit_NeoPixel* pixel, int Cooling, int Sparking, int SpeedDelay) {
  int NUMPIXELS = pixel->numPixels();
  byte heat[NUMPIXELS];
  int cooldown;
  
  // Step 1.  Cool down every cell a little
  for( int i = 0; i < pixel->numPixels(); i++) {
    cooldown = random(0, ((Cooling * 10) / pixel->numPixels()) + 2);
    
    if(cooldown>heat[i]) {
      heat[i]=0;
    } else {
      heat[i]=heat[i]-cooldown;
    }
  }
  
  // Step 2.  Heat from each cell drifts 'up' and diffuses a little
  for( int k= pixel->numPixels() - 1; k >= 2; k--) {
    heat[k] = (heat[k - 1] + heat[k - 2] + heat[k - 2]) / 3;
  }
    
  // Step 3.  Randomly ignite new 'sparks' near the bottom
  if( random(255) < Sparking ) {
    int y = random(7);
    heat[y] = heat[y] + random(160,255);
    //heat[y] = random(160,255);
  }

  // Step 4.  Convert heat to LED colors
  for( int j = 0; j < pixel->numPixels(); j++) {
    setPixelHeatColor(pixel, j, heat[j] );
  }

  showStrip(pixel);
  delay(SpeedDelay);
}

void webserver()
{
  WiFiClient client = server.available();

  if (millis() - previousWebServerMillis >= webServerInterval) {
    if (client) {
      Serial.println("New Client.");          // print a message out in the serial port
      String currentLine = "";                // make a String to hold incoming data from the client
      while (client.connected()) {            // loop while the client's connected
        if (client.available()) {             // if there's bytes to read from the client,
          char c = client.read();             // read a byte, then
          Serial.write(c);                    // print it out the serial monitor
          header += c;
          if (c == '\n') {                    // if the byte is a newline character
            // if the current line is blank, you got two newline characters in a row.
            // that's the end of the client HTTP request, so send a response:
            if (currentLine.length() == 0) {
              client.println("HTTP/1.1 200 OK");
              client.println("Content-type:text/html");
              client.println("Connection: close");
              client.println();

              if (header.indexOf("GET /outerring/rainbow") >= 0) {
                selectedEffect = RAINBOWCYCLE;
                selectedPixel = pixel_outerring;
              } else if (header.indexOf("GET /outerlogo/rainbow") >= 0) {
                rainbowCycle(&pixel_outerlogo, 20);
              } else if (header.indexOf("GET /innerlogo/rainbow") >= 0) {
                rainbowCycle(&pixel_innerlogo, 20);
              } else if (header.indexOf("GET /outerring/theaterchase") >= 0) {
                theaterChase(&pixel_outerring, 0xff,0,0,50);
              } else if (header.indexOf("GET /outerlogo/theaterchase") >= 0) {
                theaterChase(&pixel_outerlogo, 0xff,0,0,50);
              } else if (header.indexOf("GET /innerlogo/theaterchase") >= 0) {
                theaterChase(&pixel_innerlogo, 0xff,0,0,50);
              } else if (header.indexOf("GET /outerring/theaterrainbow") >= 0) {
                theaterChaseRainbow(&pixel_outerring, 50);
              } else if (header.indexOf("GET /outerlogo/theaterrainbow") >= 0) {
                theaterChaseRainbow(&pixel_outerlogo, 50);
              } else if (header.indexOf("GET /innerlogo/theaterrainbow") >= 0) {
                theaterChaseRainbow(&pixel_innerlogo, 50);
              } else if (header.indexOf("GET /outerring/fire") >= 0) {
                Fire(&pixel_outerring, 55,120,15);
              } else if (header.indexOf("GET /outerlogo/fire") >= 0) {
                Fire(&pixel_outerlogo, 55,120,15);
              } else if (header.indexOf("GET /innerlogo/fire") >= 0) {
                Fire(&pixel_innerlogo, 55,120,15);
              } else if (header.indexOf("GET /reset/network") >= 0) {
                wifiManager.resetSettings();
              }

              // Display the HTML web page
              client.println("<!DOCTYPE html><html>");
              client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
              client.println("<link rel=\"icon\" href=\"data:,\">");
              // CSS to style the on/off buttons 
              // Feel free to change the background-color and font-size attributes to fit your preferences
              client.println("<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}");
              client.println(".button { background-color: #195B6A; border: none; color: white; padding: 16px 40px;");
              client.println("text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}");
              client.println(".button2 {background-color: #77878A;}</style></head>");
              
              // Web Page Heading
              client.println("<body><h1>WatchDogs logo server</h1>");

              client.println("<h2>Rainbow<h2>");
              client.println("<p>");
              client.println("<a href=\"/outerring/rainbow\"><button class=\"button button\">Outerring</button></a>");
              client.println("<a href=\"/outerlogo/rainbow\"><button class=\"button button\">Outerlogo</button></a>");
              client.println("<a href=\"/innerlogo/rainbow\"><button class=\"button button\">Innerlogo</button></a>");
              client.println("</p>");

              client.println("<h2>Theaterchase<h2>");
              client.println("<p>");
              client.println("<a href=\"/outerring/theaterchase\"><button class=\"button button\">Outerring</button></a>");
              client.println("<a href=\"/outerlogo/theaterchase\"><button class=\"button button\">Outerlogo</button></a>");
              client.println("<a href=\"/innerlogo/theaterchase\"><button class=\"button button\">Innerlogo</button></a>");
              client.println("</p>");

              client.println("<h2>Theaterchase rainbow<h2>");
              client.println("<p>");
              client.println("<a href=\"/outerring/theaterrainbow\"><button class=\"button button\">Outerring</button></a>");
              client.println("<a href=\"/outerlogo/theaterrainbow\"><button class=\"button button\">Outerlogo</button></a>");
              client.println("<a href=\"/innerlogo/theaterrainbow\"><button class=\"button button\">Innerlogo</button></a>");
              client.println("</p>");

              client.println("<h2>Fire<h2>");
              client.println("<p>");
              client.println("<a href=\"/outerring/fire\"><button class=\"button button\">Outerring</button></a>");
              client.println("<a href=\"/outerlogo/fire\"><button class=\"button button\">Outerlogo</button></a>");
              client.println("<a href=\"/innerlogo/fire\"><button class=\"button button\">Innerlogo</button></a>");
              client.println("</p>");

              client.println("<h2>network settings<h2>");
              client.println("<p><a href=\"/network/reset\"><button class=\"button button\">RESET</button></a></p>");
              client.println("</body></html>");
              
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
      Serial.println("Client disconnected.");
      Serial.println("");
    }
    previousWebServerMillis += webServerInterval;
  }
}


void loop() {
  EEPROM.get(0,selectedEffect);

  currentMillis = millis();
  webserver();

  switch (selectedEffect)
  {
    case RAINBOWCYCLE:
      rainbowCycle(&selectedPixel, 20);
      break;
    default:
      break;
  }
}