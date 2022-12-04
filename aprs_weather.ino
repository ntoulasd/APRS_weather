#include <WiFi.h>
#include <WiFiMulti.h>
#include <WebServer.h>
#include <DHT.h>
#include <BME280I2C.h>
#include <Wire.h>

/* Network Settings */
char SVR_NAME[] = "euro.aprs2.net";
#define SVR_PORT 14580

/* Define your APRS-IS callsign, passcode*/
#define callsign "SV2RCK"
#define passcode "xxxxxxxxxx"

/* Define your location */
#define location "XXXX.XXN/XXXX.XXE"

/*
    Define your station symbol
    "-" Home
    "$" Phone
    "'" Plane
    "`" Antenna
    ">" Car
    "Z" Windows 95
    "," Scout
    "?" Desktop Computer
    Can you find others symbols on APRS documentation
*/
#define sta_symbol "_" //Weather station

/* Define your comment */
#define comment "Arduino APRS-IS - Weather staion TEST"

/* Update interval in minutes */
int REPORT_INTERVAL = 10;

#define VER "1.0"
#define SVR_VERIFIED "verified"

#define TO_LINE  10000

BME280I2C bme;    // Default : forced mode, standby time = 1000 ms
// Oversampling = pressure ×1, temperature ×1, humidity ×1, filter off,

#define DHTTYPE DHT22   // DHT 22  (AM2302), AM2321


// DHT Sensor
uint8_t DHTPin = 4;

// Initialize DHT sensor.
DHT dht(DHTPin, DHTTYPE);

WebServer server(80);

float Temperature;
float Humidity;
float temp(NAN), hum(NAN), pres(NAN);

void setup() {
  Serial.begin(115200);
  delay(100);

  Wire.begin();

  switch (bme.chipModel())
  {
    case BME280::ChipModel_BME280:
      Serial.println("Found BME280 sensor! Success.");
      break;
    case BME280::ChipModel_BMP280:
      Serial.println("Found BMP280 sensor! No Humidity available.");
      break;
    default:
      Serial.println("Found UNKNOWN sensor! Error!");
  }

  pinMode(DHTPin, INPUT);

  dht.begin();

  WiFiMulti wifiMulti;

// May use many wifi networks
  wifiMulti.addAP("SSID1", "password1");
  wifiMulti.addAP("SSID2", "password2");
  wifiMulti.addAP("SSID3", "password3");

  Serial.println("Connecting Wifi...");

  while (wifiMulti.run() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected to " + WiFi.SSID());
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  Serial.println("");
  Serial.println("WiFi connected..!") + WiFi.SSID();
  Serial.print("Got IP: ");  Serial.println(WiFi.localIP());

  server.on("/", handle_OnConnect);
  server.onNotFound(handle_NotFound);

  server.begin();
  Serial.println("HTTP server started");

}
void loop() {

  server.handleClient();

  boolean sent = false;

  // Use WiFiClient class to create TCP connections
  WiFiClient client;

  if ( client.connect(SVR_NAME, SVR_PORT) )
  {
    Serial.println("APRS Server connected");
    client.print("user ");
    client.print(callsign);
    client.print(" pass ");
    client.print(passcode);
    client.print(" vers APRSduino ");
    client.println(VER);
    delay(2000);
    if ( wait4content(&client, SVR_VERIFIED, 8) ) { // If connected, send APRS data
      Serial.println("Login OK");
      client.print(callsign);
      client.print(">APE001,TCPIP*,qAC,WIDE1-1,WIDE2-1,GREECE:!");
      client.print(location);
      client.print(sta_symbol);
      client.print(comment);
      Serial.println("Data sent OK");

      unsigned long timeout = millis();
      while (client.available() == 0) {
        if (millis() - timeout > 5000) {
          Serial.println(">>> Client Timeout !");
          client.stop();
          return;
        }
      }

      // Read all the lines of the reply from server and print them to Serial
      while (client.available()) {
        String line = client.readStringUntil('\r');
        Serial.print(line);
      }


      client.stop();
      Serial.println("APRS Server disconnected\n");
      delay((long)REPORT_INTERVAL * 60L * 1000L);

      sent = true;
    }
    else
    {
      Serial.println("Login failed.");
    }
  }
  else
  {
    Serial.println("Can not connect to the aprs server.");
  }


  delay(5000);

}
////////////////////////////////////////////////////////////////////////////////////////////// End Loop ///////////////////////////////////////////

boolean wait4content(Stream* stream, char *target, int targetLen)
{
  size_t index = 0;  // maximum target string length is 64k bytes!
  int c;
  boolean ret = false;
  unsigned long timeBegin;
  delay(50);
  timeBegin = millis();

  while ( true )
  {
    //  wait and read one byte
    while ( !stream->available() )
    {
      if ( millis() - timeBegin > TO_LINE )
      {
        break;
      }
      delay(2);
    }
    if ( stream->available() ) {
      c = stream->read();
      //  judge the byte
      if ( c == target[index] )
      {
        index ++;
        if ( !target[index] )
          // return true if all chars in the target match
        {
          ret = true;
          break;
        }
      }
      else if ( c >= 0 )
      {
        index = 0;  // reset index if any char does not match
      } else //  timed-out for one byte
      {
        break;
      }
    }
    else  //  timed-out
    {
      break;
    }
  }
  return ret;
}


double heatIndex(double temp, double humidity)
{
  double c1 = -42.38, c2 = 2.049, c3 = 10.14, c4 = -0.2248, c5 = -6.838e-3, c6 = -5.482e-2, c7 = 1.228e-3, c8 = 8.528e-4, c9 = -1.99e-6 ;
  double T = tempCtoF(temp);
  double R = humidity;
  double T2 = T * T;
  double R2 = R * R;
  double TR = T * R;

  double rv = c1 + c2 * T + c3 * R + c4 * T * R + c5 * T2 + c6 * R2 + c7 * T * TR + c8 * TR * R + c9 * T2 * R2;
  return tempFtoC(rv);
}

float tempCtoF (double tempC) {
  return (tempC * 1.8) + 32;
}

float tempFtoC (double tempF) {
  return (tempF - 32) * .5556;
}

void handle_OnConnect() {

  do {
    delay(100);
    Temperature = dht.readTemperature(); // Gets the values of the temperature
    Humidity = dht.readHumidity(); // Gets the values of the humidity

    BME280::TempUnit tempUnit(BME280::TempUnit_Celsius);
    BME280::PresUnit presUnit(BME280::PresUnit_Pa);

    bme.read(pres, temp, hum, tempUnit, presUnit);

  } while (Humidity < 0 and Humidity > 100);

  server.send(200, "text/html", SendHTML(Temperature, Humidity, temp, pres));
}

void handle_NotFound() {
  server.send(404, "text/plain", "Not found");
}

String SendHTML(float TempCstat, float Humiditystat, float temp2, float pressure) {
  String ptr = "<!DOCTYPE html> <html>\n";
  ptr += "<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\n";
  //ptr += "<link href=\"https://fonts.googleapis.com/css?family=Open+Sans:300,400,600\" rel=\"stylesheet\">\n";
  ptr += "<title>ESP32 Weather Report</title>\n";
  ptr += "<style>html { font-family: 'Open Sans', sans-serif; display: block; margin: 0px auto; text-align: center;color: #333333;}\n";
  ptr += "body{margin-top: 50px;}\n";
  ptr += "#webpage{width: 450px;margin: auto;text-align: inherit;}\n";
  ptr += "h1 {margin: 50px auto 30px;}\n";
  ptr += ".side-by-side{display: inline-block;vertical-align: middle;position: relative;}\n";
  ptr += ".humidity-icon{background-color: #3498db;width: 30px;height: 30px;border-radius: 50%;line-height: 36px;}\n";
  ptr += ".humidity-text{font-weight: 600;padding-left: 15px;font-size: 19px;width: 160px;text-align: left;}\n";
  ptr += ".humidity{font-weight: 300;font-size: 60px;color: #3498db;}\n";
  ptr += ".temperature-icon{background-color: #f39c12;width: 30px;height: 30px;border-radius: 50%;line-height: 40px;}\n";
  ptr += ".temperature-text{font-weight: 600;padding-left: 15px;font-size: 19px;width: 160px;text-align: left;}\n";
  ptr += ".temperature{font-weight: 300;font-size: 60px;color: #f39c12;}\n";
  ptr += ".superscript{font-size: 17px;font-weight: 600;position: absolute;right: -20px;top: 15px;}\n";
  ptr += ".data{padding: 10px;}\n";
  ptr += "</style>\n";

  ptr += "<script>\n";
  ptr += "setInterval(loadDoc,5000);\n";
  ptr += "function loadDoc() {\n";
  ptr += "var xhttp = new XMLHttpRequest();\n";
  ptr += "xhttp.onreadystatechange = function() {\n";
  ptr += "if (this.readyState == 4 && this.status == 200) {\n";
  ptr += "document.getElementById(\"webpage\").innerHTML =this.responseText}\n";
  ptr += "};\n";
  ptr += "xhttp.open(\"GET\", \"/\", true);\n";
  ptr += "xhttp.send();\n";
  ptr += "}\n";
  ptr += "</script>\n";

  ptr += "</head>\n";
  ptr += "<body>\n";

  ptr += "<div id=\"webpage\">\n";

  ptr += "<h1>ESP32 Weather Report</h1>\n";

  ptr += "<div class=\"data\">\n";
  ptr += "<div class=\"side-by-side temperature-icon\">\n";
  ptr += "<svg version=\"1.1\" id=\"Layer_1\" xmlns=\"http://www.w3.org/2000/svg\" xmlns:xlink=\"http://www.w3.org/1999/xlink\" x=\"0px\" y=\"0px\"\n";
  ptr += "width=\"9.915px\" height=\"22px\" viewBox=\"0 0 9.915 22\" enable-background=\"new 0 0 9.915 22\" xml:space=\"preserve\">\n";
  ptr += "<path fill=\"#FFFFFF\" d=\"M3.498,0.53c0.377-0.331,0.877-0.501,1.374-0.527C5.697-0.04,6.522,0.421,6.924,1.142\n";
  ptr += "c0.237,0.399,0.315,0.871,0.311,1.33C7.229,5.856,7.245,9.24,7.227,12.625c1.019,0.539,1.855,1.424,2.301,2.491\n";
  ptr += "c0.491,1.163,0.518,2.514,0.062,3.693c-0.414,1.102-1.24,2.038-2.276,2.594c-1.056,0.583-2.331,0.743-3.501,0.463\n";
  ptr += "c-1.417-0.323-2.659-1.314-3.3-2.617C0.014,18.26-0.115,17.104,0.1,16.022c0.296-1.443,1.274-2.717,2.58-3.394\n";
  ptr += "c0.013-3.44,0-6.881,0.007-10.322C2.674,1.634,2.974,0.955,3.498,0.53z\"/>\n";
  ptr += "</svg>\n";
  ptr += "</div>\n";
  ptr += "<div class=\"side-by-side temperature-text\">Humidex</div>\n";
  ptr += "<div class=\"side-by-side temperature\">";
  ptr +=   heatIndex (TempCstat, Humiditystat);
  ptr += "<span class=\"superscript\">&degC</span></div>\n";
  ptr += "</div>\n";

  ptr += "<div class=\"data\">\n";
  ptr += "<div class=\"side-by-side temperature-icon\">\n";
  ptr += "<svg version=\"1.1\" id=\"Layer_1\" xmlns=\"http://www.w3.org/2000/svg\" xmlns:xlink=\"http://www.w3.org/1999/xlink\" x=\"0px\" y=\"0px\"\n";
  ptr += "width=\"9.915px\" height=\"22px\" viewBox=\"0 0 9.915 22\" enable-background=\"new 0 0 9.915 22\" xml:space=\"preserve\">\n";
  ptr += "<path fill=\"#FFFFFF\" d=\"M3.498,0.53c0.377-0.331,0.877-0.501,1.374-0.527C5.697-0.04,6.522,0.421,6.924,1.142\n";
  ptr += "c0.237,0.399,0.315,0.871,0.311,1.33C7.229,5.856,7.245,9.24,7.227,12.625c1.019,0.539,1.855,1.424,2.301,2.491\n";
  ptr += "c0.491,1.163,0.518,2.514,0.062,3.693c-0.414,1.102-1.24,2.038-2.276,2.594c-1.056,0.583-2.331,0.743-3.501,0.463\n";
  ptr += "c-1.417-0.323-2.659-1.314-3.3-2.617C0.014,18.26-0.115,17.104,0.1,16.022c0.296-1.443,1.274-2.717,2.58-3.394\n";
  ptr += "c0.013-3.44,0-6.881,0.007-10.322C2.674,1.634,2.974,0.955,3.498,0.53z\"/>\n";
  ptr += "</svg>\n";
  ptr += "</div>\n";
  ptr += "<div class=\"side-by-side temperature-text\">Temperature</div>\n";
  ptr += "<div class=\"side-by-side temperature\">";
  ptr += TempCstat;
  ptr += "<span class=\"superscript\">&degC</span></div>\n";
  ptr += "</div>\n";

  ptr += "<div class=\"data\">\n";
  ptr += "<div class=\"side-by-side temperature-icon\">\n";
  ptr += "<svg version=\"1.1\" id=\"Layer_1\" xmlns=\"http://www.w3.org/2000/svg\" xmlns:xlink=\"http://www.w3.org/1999/xlink\" x=\"0px\" y=\"0px\"\n";
  ptr += "width=\"9.915px\" height=\"22px\" viewBox=\"0 0 9.915 22\" enable-background=\"new 0 0 9.915 22\" xml:space=\"preserve\">\n";
  ptr += "<path fill=\"#FFFFFF\" d=\"M3.498,0.53c0.377-0.331,0.877-0.501,1.374-0.527C5.697-0.04,6.522,0.421,6.924,1.142\n";
  ptr += "c0.237,0.399,0.315,0.871,0.311,1.33C7.229,5.856,7.245,9.24,7.227,12.625c1.019,0.539,1.855,1.424,2.301,2.491\n";
  ptr += "c0.491,1.163,0.518,2.514,0.062,3.693c-0.414,1.102-1.24,2.038-2.276,2.594c-1.056,0.583-2.331,0.743-3.501,0.463\n";
  ptr += "c-1.417-0.323-2.659-1.314-3.3-2.617C0.014,18.26-0.115,17.104,0.1,16.022c0.296-1.443,1.274-2.717,2.58-3.394\n";
  ptr += "c0.013-3.44,0-6.881,0.007-10.322C2.674,1.634,2.974,0.955,3.498,0.53z\"/>\n";
  ptr += "</svg>\n";
  ptr += "</div>\n";
  ptr += "<div class=\"side-by-side temperature-text\">Temperature 2</div>\n";
  ptr += "<div class=\"side-by-side temperature\">";
  ptr += temp2;
  ptr += "<span class=\"superscript\">&degC</span></div>\n";
  ptr += "</div>\n";

  ptr += "<div class=\"data\">\n";
  ptr += "<div class=\"side-by-side humidity-icon\">\n";
  ptr += "<svg version=\"1.1\" id=\"Layer_2\" xmlns=\"http://www.w3.org/2000/svg\" xmlns:xlink=\"http://www.w3.org/1999/xlink\" x=\"0px\" y=\"0px\"\n\"; width=\"12px\" height=\"17.955px\" viewBox=\"0 0 13 17.955\" enable-background=\"new 0 0 13 17.955\" xml:space=\"preserve\">\n";
  ptr += "<path fill=\"#FFFFFF\" d=\"M1.819,6.217C3.139,4.064,6.5,0,6.5,0s3.363,4.064,4.681,6.217c1.793,2.926,2.133,5.05,1.571,7.057\n";
  ptr += "c-0.438,1.574-2.264,4.681-6.252,4.681c-3.988,0-5.813-3.107-6.252-4.681C-0.313,11.267,0.026,9.143,1.819,6.217\"></path>\n";
  ptr += "</svg>\n";
  ptr += "</div>\n";
  ptr += "<div class=\"side-by-side humidity-text\">Humidity</div>\n";
  ptr += "<div class=\"side-by-side humidity\">";
  ptr += Humiditystat;
  ptr += "<span class=\"superscript\">%</span></div>\n";
  ptr += "</div>\n";

  ptr += "<div class=\"data\">\n";
  ptr += "<div class=\"side-by-side humidity-icon\">\n";
  ptr += "<svg version=\"1.1\" width='18px' height='18px' viewBox='0 0 30 30'>\n";
  ptr += "<path fill='#FFFFFF' d='M7.628,20.148v-0.234H7.454C7.507,19.996,7.572,20.068,7.628,20.148z M17.676,10.102\n";
  ptr += "c-0.426-0.121-0.864,0.146-0.979,0.598l-1.925,7.599c-1.248,0.116-2.23,1.154-2.23,2.435c0,1.357,1.1,2.457,2.458,2.457\n";
  ptr += "s2.457-1.1,2.457-2.457c0-0.865-0.449-1.622-1.125-2.06l1.909-7.537C18.355,10.687,18.103,10.223,17.676,10.102z M15.818,21.552\n";
  ptr += "h-1.638v-1.638h1.638V21.552z M15,1.895C7.762,1.895,1.895,7.762,1.895,15c0,7.237,5.867,13.104,13.105,13.104\n";
  ptr += "c7.237,0,13.104-5.867,13.104-13.104C28.104,7.762,22.237,1.895,15,1.895z M22.371,23.777v0.231h-0.294\n";
  ptr += "c-1.949,1.534-4.403,2.458-7.077,2.458c-2.674,0-5.128-0.924-7.078-2.458H7.628v-0.231C5.126,21.674,3.533,18.523,3.533,15\n";
  ptr += "C3.533,8.667,8.667,3.533,15,3.533c6.332,0,11.467,5.133,11.467,11.467C26.467,18.523,24.873,21.674,22.371,23.777z M22.371,20.148\n";
  ptr += "c0.056-0.08,0.121-0.152,0.175-0.234h-0.175V20.148z M15,5.99c-4.976,0-9.01,4.034-9.01,9.01h1.638c0-4.071,3.3-7.372,7.372-7.372\n";
  ptr += "s7.371,3.3,7.371,7.372h1.638C24.009,10.024,19.976,5.99,15,5.99z'/>\n";
  ptr += "</svg>\n";
  ptr += "</div>\n";
  ptr += "<div class=\"side-by-side humidity-text\">Pressure</div>\n";
  ptr += "<div class=\"side-by-side humidity\">";
  ptr += pressure;
  ptr += "<span class=\"superscript\">Pa</span></div>\n";
  ptr += "</div>\n";

  ptr += "</div>\n";
  ptr += "</body>\n";
  ptr += "</html>\n";
  return ptr;
}
