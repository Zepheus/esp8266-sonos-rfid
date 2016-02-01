#include <ESP8266WiFi.h>
#include <WiFiUDP.h>
#include <stdint.h>
#include "MFRC522.h"

extern "C" {
    #include "yxml.h"
}

//Wireshark filter: (tcp.dstport == 3400 || tcp.srcport == 3400 || tcp.srcport == 1400 || tcp.dstport == 1400) && xml

#define RST_PIN 4
#define SS_PIN 5
MFRC522 mfrc522(SS_PIN, RST_PIN);  // Create MFRC522 instance
unsigned long lastpress = 0;

const char* ssid = "access_point_ssid";
const char* password = "wpa2_pass";

WiFiServer server(3400);
WiFiUDP Udp;
unsigned int portUni = 1901;

void setup() {
    Serial.begin(9600);
    delay(250);
    
    Serial.println("Initializing SPI Bus and chip");
    SPI.begin(); // Init SPI bus
    mfrc522.PCD_Init(); // Init MFRC522
    
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
    sprintf(s, "%d.%d.%d.%d", localIP[0], localIP[1], localIP[2], localIP[3]);
    
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
        String line = client.readStringUntil('r');
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
        String line = client.readStringUntil('r');
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
        String line = client.readStringUntil('r');
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
    bool had_header = false;
    char escaped_xml[1024*8]; //accept max 8k (if bigger then f-you)
    char recv_buffer[256];
    size_t read;
    
    size_t max_len = sizeof(escaped_xml) - 1;
    char sequence[8];
    size_t seq_i = 0;
    size_t read_len;
    size_t dest_i = 0;
    
    bool reading_escaped = false;
    bool len_exceeded = false;
    
    while(!len_exceeded && (read = client.readBytes(recv_buffer, sizeof(recv_buffer) - 1)) > 0){
        char* read_buffer = recv_buffer;
        if(!had_header){
            recv_buffer[read] = 0; //NULL-terminate for safety while searching for header
            char* end_of_header = strstr(recv_buffer, "\r\n\r\n");
            if(end_of_header != NULL) {
                *end_of_header = '\0'; //null terminate the header
                Serial.printf("%s", recv_buffer);
                correct = strncmp(recv_buffer, "NOTIFY", 6) == 0;
                if(!correct)
                  break;
                read_buffer = end_of_header + 4;
                had_header = true;
            } else {
                Serial.println("Header is too big!");
                break;
            }
        }
        
        // Process data
        char* recv_buffer_end = recv_buffer + read;
        for (;read_buffer < recv_buffer_end && *read_buffer != 0; ++read_buffer) {
            len_exceeded = dest_i >= max_len;
            if (len_exceeded)
              break; //abort before we overwrite invalid memory
            
            char c = *read_buffer;
            if (c != '&') {
                // normal character, apend
                if (reading_escaped) { //finalize sequence
                    if (c == ';') {
                        // unescape sequence
                        if (seq_i == 2 && sequence[0] == 'l' && sequence[1] == 't') {
                            escaped_xml[dest_i++] = '<';
                        }
                        else if (seq_i == 2 && sequence[0] == 'g' && sequence[1] == 't') {
                            escaped_xml[dest_i++] = '>';
                        }
                        else if (seq_i == 3 && sequence[0] == 'a' && sequence[1] == 'm' && sequence[2] == 'p') {
                            escaped_xml[dest_i++] = '&';
                        }
                        else if (seq_i == 4 && sequence[0] == 'q' && sequence[1] == 'u' && sequence[2] == 'o' && sequence[3] == 't') {
                            escaped_xml[dest_i++] = '"';
                        }
                        else {
                            sequence[seq_i] = 0; //null-terminate for print
                            Serial.printf("Unknown escape sequence: %sn", sequence);
                        }
                        reading_escaped = false;
                    }
                    else {
                        if (seq_i < 7) {
                            sequence[seq_i++] = c;
                        }
                        else {
                            sequence[7] = 0;
                            Serial.printf("Invalid escape sequence: %s", sequence);
                            len_exceeded = true;
                        }
                    }
                }
                else {
                    escaped_xml[dest_i++] = c;
                }
            }
            else {
                if (reading_escaped) {
                    Serial.println("Invalid ampersand while already in escaped mode.n");
                }
                else {
                    reading_escaped = true;
                    seq_i = 0;
                }
            }
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
            Serial.printf("Tag detected, seconds since: %d, size: %dn", diff/1000, mfrc522.uid.size);
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
