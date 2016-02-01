#include <ESP8266WiFi.h>
#include <WiFiUDP.h>
#include <stdint.h>
#include "MFRC522.h"

extern "C" {
  #include "yxml.h"
}

//Wireshark filter: (tcp.dstport == 3400 || tcp.srcport == 3400 || tcp.srcport == 1400 || tcp.dstport == 1400) && xml

#define RST_PIN 4
#define SS_PIN  5
MFRC522 mfrc522(SS_PIN, RST_PIN);   // Create MFRC522 instance
unsigned long lastpress = 0;
 
const char* ssid     = "access_point_ssid";
const char* password = "wpa2_pass";

WiFiServer server(3400);
WiFiUDP Udp;
unsigned int portUni = 1901;
 
void setup() {
  Serial.begin(9600);
  delay(250);

  Serial.println("Initializing SPI Bus and chip");
  SPI.begin();           // Init SPI bus
  mfrc522.PCD_Init();    // Init MFRC522 

  // We start by connecting to a WiFi network
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
 
  Serial.println("");
  Serial.println("WiFi connected");  
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  server.begin();
  Udp.begin(1901);
}

void subscribe(WiFiClient& client, IPAddress& sonosIP){
  Serial.println("Subscribing to Sonos AV Events");
  if (!client.connect(sonosIP, 1400)) {
    Serial.println("connection to sonos failed");
    return;
  }

  IPAddress localIP = WiFi.localIP();
  char s[16];  
  sprintf(s, "%d.%d.%d.%d",  localIP[0], localIP[1], localIP[2], localIP[3]);
  
  Serial.println("CALLBACK: <http://" + String(s) +":3400/notify>");
  client.println("SUBSCRIBE /MediaRenderer/AVTransport/Event HTTP/1.1");
  client.println("HOST: " + String(sonosIP) + ":" + String(1400));
  client.println("USER-AGENT: ESP8266/1.0");
  client.println("CALLBACK: <http://" + String(s) + ":3400/notify>");
  client.println("NT: upnp:event");
  client.println("TIMEOUT: Second-3600");
  client.println();
  delay(10);

   while(client.connected() && client.available()){
    String line = client.readStringUntil('\r');
    Serial.print(line);
  }
  Serial.println("Closed Sonos connection.");
}

void pause(WiFiClient& client, IPAddress& sonos){
   String payload = "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\"><s:Body><u:Pause xmlns:u=\"urn:schemas-upnp-org:service:AVTransport:1\"><InstanceID>0</InstanceID></u:Pause></s:Body></s:Envelope>";

  if (!client.connect(sonos, 1400)) {
    Serial.println("connection to sonos failed");
    return;
  }
  
  client.println("POST /MediaRenderer/AVTransport/Control HTTP/1.1");
  client.println("CONNECTION: close");
  client.println("Host: " + String(sonos) + ":" + String(1400));
  client.println("User-Agent: ESP8266/1.0");
  client.println("Connection: close");
  client.println("Content-type: text/xml; charset=\"utf-8\"");
  client.print("Content-Length: ");
  client.println(payload.length());
  client.println("SOAPACTION: \"urn:schemas-upnp-org:service:AVTransport:1#Pause\"");
  client.println();
  client.println(payload); 
  delay(10);

   while(client.connected() && client.available()){
    String line = client.readStringUntil('\r');
    //Serial.print(line);
  }
  Serial.println("Closed Sonos connection.");
}

void play(WiFiClient& client, IPAddress& sonos){
    String payload = "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\"><s:Body><u:Play xmlns:u=\"urn:schemas-upnp-org:service:AVTransport:1\"><InstanceID>0</InstanceID><Speed>1</Speed></u:Play></s:Body></s:Envelope>";

  if (!client.connect(sonos, 1400)) {
    Serial.println("connection to sonos failed");
    return;
  }

  client.println("POST /MediaRenderer/AVTransport/Control HTTP/1.1");
  client.println("CONNECTION: close");
  client.println("Host: " + String(sonos) + ":" + String(1400));
  client.println("User-Agent: ESP8266/1.0");
  client.println("Connection: close");
  client.println("Content-type: text/xml; charset=\"utf-8\"");
  client.print("Content-Length: ");
  client.println(payload.length());
  client.println("SOAPACTION: \"urn:schemas-upnp-org:service:AVTransport:1#Play\"");
  client.println();
  client.println(payload); 
  delay(10);

   while(client.connected() && client.available()){
    String line = client.readStringUntil('\r');
    //Serial.print(line);
  }
  Serial.println("Closed Sonos connection.");
}

void checkWebserver(){
  WiFiClient client = server.available();
  if (!client) {
    return;
  }
  Serial.println("Notification coming in!");

  while(!client.available()){
    delay(100);
  }

  bool correct = false;
  char buffer[1024*8]; //accept max 8k (if bigger then f-you)
  int read = client.readBytes(buffer, sizeof(buffer) - 1));
  if(read < sizeof(buffer) - 1)){
      buffer[read] = 0; //NULL-terminate for safety
      const char* end_of_header = strstr(buffer, "\r\n\r\n");
      if(end_of_header != NULL) {
        end_of_header[len] = 0;
        Serial.printf("%s", header_data);
        correct = strncmp(buffer, "NOTIFY", 6) == 0;
        if(correct) {
          // Parse the XML
          char xml_buffer[1024*8];
          yxml_t x;
          yxml_init(&x, xml_buffer, sizeof(xml_buffer));
          char* doc = end_of_header + 4;
          for(; *doc; doc++) {
            yxml_ret_t r = yxml_parse(x, *doc); //feed one by one
            if(r < 0){
              correct = false; //invalid token parsed
            }
          }
      } else {
        Serial.println("Header too big!");
      }
  }

  // Send result
  if(!correct){
     client.println("HTTP/1.1 400 Bad Request");
  } else {
    client.println("HTTP/1.1 200 OK");
  }
  client.println("Server: ESP8266/1.0");
  client.println("Connection: close");
  client.println();
  client.flush();
}

IPAddress discoverSonos(){
  IPAddress sonosIP;
  bool found = false;
  do {
    Serial.println("Sending M-SEARCH multicast");
    Udp.beginPacketMulticast(IPAddress(239, 255, 255, 250), 1900, WiFi.localIP());
    Udp.write("M-SEARCH * HTTP/1.1\r\n" 
      "HOST: 239.255.255.250:1900\r\n" 
      "MAN: \"ssdp:discover\"\r\n" 
      "MX: 1\r\n" 
      "ST: urn:schemas-upnp-org:device:ZonePlayer:1\r\n");
    Udp.endPacket();  
    unsigned long lastSearch = millis();
    
    while(!found && (millis() - lastSearch) < 5000){
      int packetSize = Udp.parsePacket();
      if(packetSize){
        char packetBuffer[255];
        Serial.print("Received packet of size ");
        Serial.println(packetSize);
        Serial.print("From ");
        sonosIP = Udp.remoteIP();
        found = true;
        Serial.print(sonosIP);
        Serial.print(", port ");
        Serial.println(Udp.remotePort());
    
        // read the packet into packetBufffer
        int len = Udp.read(packetBuffer, 255);
        if (len > 0) {
          packetBuffer[len] = 0;
        }
        Serial.println("Contents:");
        Serial.println(packetBuffer);
      }
      delay(50);
    }
  } while(!found);
  return sonosIP;
}

void rfidCheck(WiFiClient& client, IPAddress& sonosIP){
   if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) {
      return;
    } else {
      unsigned long now = millis();
      unsigned long diff = now - lastpress;
      if(diff > 3000){
         Serial.printf("Tag detected, seconds since: %d, size: %d\n", diff/1000, mfrc522.uid.size);    
         switch(mfrc522.uid.size){
          case 4:
              Serial.println(*((uint32_t*)mfrc522.uid.uidByte));
              switch(*((uint32_t*)mfrc522.uid.uidByte)){
                case 2240846132:
                  pause(client, sonosIP);
                break;

                case 723452371:
                   play(client, sonosIP);
                break;
              }
          break;

          case 7:
          case 10:
          default:
            break;
         }
         lastpress = now;
      }
   }
}

void loop() {
  WiFiClient client;
  IPAddress sonosIP;
  
  delay(5000);
  sonosIP = discoverSonos();
  subscribe(client, sonosIP);
  while(1){
    rfidCheck(client, sonosIP);
    checkWebserver();
    delay(50);
  }
}
