#include <SPI.h>
#include <RH_NRF24.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Base64.h>
#include <WiFiMulti.h>
#define USE_SERIAL Serial
// Singleton instance of the radio driver
RH_NRF24 nrf24(22, 21); // Use GPIO 4 as CE, and GPIO 5 as CSN
 
WiFiMulti wifiMulti;
 
const int maxBufferSize = 12000; // Maximum buffer size for data accumulation   
uint8_t buffer[maxBufferSize];   // Array to hold all the incoming data
int currentIndex = 0;            // Current index in the buffer
bool jpegComplete = false;       // Flag indicating if we've received the full JPEG file
 
//Https POST Variables
const char* ssid = "IoT-D210";
const char* password = "ssepf3x9";
 
// Example hardcoded JPEG hex data (shortened for example purposes)
uint8_t buffer_2[] = {
    0xFF, 0xD8, 0xFF, 0xE0, 0x00, 0x10, 0x4A, 0x46, 0x49, 0x46, 0x00, 0x01, 0x01, 0x01,
    0x00, 0x60, 0x00, 0x60, 0x00, 0x00, 0xFF, 0xDB, 0x00, 0x43, 0x00, 0x03, 0x02, 0x02,
    0xFF, 0xD9 // JPEG end marker
};
 
void setup() {
  USE_SERIAL.begin(115200);
  while (!USE_SERIAL) ; // Necessary for Leonardo only
 
  if (!nrf24.init()) {
    USE_SERIAL.println("init failed");
  }
 
    WiFi.begin(ssid, password);
  if(WiFi.isConnected()){
    USE_SERIAL.println("Connecting");
 
    USE_SERIAL.print("Connected to WiFi network with IP Address: ");
    USE_SERIAL.println(WiFi.localIP());
  }
 
  // Set radio channel and RF settings
  if (!nrf24.setChannel(1)) {
    USE_SERIAL.println("setChannel failed");
  }
  if (!nrf24.setRF(RH_NRF24::DataRate2Mbps, RH_NRF24::TransmitPower0dBm)) {
    USE_SERIAL.println("setRF failed");
  }
 
  //Wifi Connection
  for(uint8_t t = 4; t > 0; t--) {
        USE_SERIAL.printf("[SETUP] WAIT %d...\n", t);
        USE_SERIAL.flush();
        delay(1000);
    }
  wifiMulti.addAP("IoT-D210", "ssepf3x9");
 
}


// Function to calculate checksum
uint16_t calculateChecksum(const uint8_t* data, size_t length) {
    uint32_t sum = 0;

    // Process every 16-bit chunk
    for (size_t i = 0; i < length / 2; ++i) {
        uint16_t word = (data[2 * i] << 8) + data[2 * i + 1];
        sum += word;

        // Fold any carry bits
        if (sum >> 16) {
            sum = (sum & 0xFFFF) + (sum >> 16);
        }
    }

    // If there's a byte left
    if (length % 2 != 0) {
        uint16_t word = data[length - 1] << 8;
        sum += word;

        // Fold any carry bits
        if (sum >> 16) {
            sum = (sum & 0xFFFF) + (sum >> 16);
        }
    }

    // One's complement of sum
    uint16_t checksum = ~sum;

    return checksum;
}

uint8_t expected_id = 0;

// 0 means no data, 1 means it processed the start, 2 means its processed the end but its unstored, 3 means end is processed and stored
int ready_to_post = 0;
 
void loop() {
 
  if (nrf24.available()) {
        const int packetSize = 28; // Example packet size
        uint8_t packet[packetSize];
        uint8_t packetLength = sizeof(packet);
 
        // Check if the incoming data fits into the remaining buffer space
        if (currentIndex + packetSize > maxBufferSize) {
            USE_SERIAL.println("Buffer overflow. Clearing buffer.");
            currentIndex = 0; // Reset buffer to prevent overflow
        }
 
        // Receive data
        if (nrf24.recv(packet, &packetLength)) {
          Serial.print("ready to post value: ");
          Serial.println(ready_to_post);
                //PROCESS THE DATA
                USE_SERIAL.print("Current packet id: ");
                USE_SERIAL.print(packet[26]);
                USE_SERIAL.print(" expected_id: ");
                USE_SERIAL.print(expected_id);
                USE_SERIAL.println();

                  // Check for JPEG end marker
                  for (int i = 1; i < packetLength; i++) {
                      if (packet[i] == 0xD9 && packet[i - 1] == 0xFF) {
                          USE_SERIAL.println("\nJPEG End Found");

                          if(ready_to_post == 1) {
                            ready_to_post = 2;
                          }

                              Serial.print("ready to post value: ");
                              Serial.println(ready_to_post);


                          jpegComplete = true;
                          //if checksum good

                      }

                       if (packet[i] == 0xD8 && packet[i - 1] == 0xFF) {
                          USE_SERIAL.println("\nJPEG Start Found");

                          if(ready_to_post == 0 ) { // will be equal to zero on first try
                            ready_to_post = 1;
                          } 

                          Serial.print("ready to post value: ");
                          Serial.println(ready_to_post);
                          
                          jpegComplete = false;
                          expected_id = 0;
                          currentIndex = 0;
                      }
                    
                  }

                // last packet will be equal to 1 to save last packet once
                if(expected_id == packet[26] || (ready_to_post==2)) {
                

                  if(ready_to_post == 2) { // only allow special condition to happen once
                    ready_to_post = 3;
                  }
    
                  USE_SERIAL.print("In Good Number");
                  USE_SERIAL.println();
                  USE_SERIAL.println();

                    // SAVES THE packet into buffer
                  memcpy(buffer + currentIndex, packet, packetLength - 2);
                  USE_SERIAL.print(currentIndex);
                  USE_SERIAL.print(" ");
                  currentIndex += packetLength - 2;
                  expected_id = (expected_id + 1) % 256; // Wrap around packet ID / CHANGED CODE!

                }

                if(!jpegComplete) {


                uint8_t data[] = {expected_id, 0};

                nrf24.send(data, sizeof(data));
                nrf24.waitPacketSent();
                Serial.println("Sent a reply"); 
                
                } else {

                    uint8_t data[] = {expected_id, 2};

                    
                    jpegComplete = true;
                    //if checksum good

                    nrf24.send(data, sizeof(data));
                    nrf24.waitPacketSent();
                    Serial.println("Sent Final reply"); 

                }

        } else {
            USE_SERIAL.println("Receive failed");
        } 
    }



    //Check WiFi connection status
    if(WiFi.status()== WL_CONNECTED && jpegComplete && (ready_to_post == 3)){ 

       ready_to_post = 0;
      
 
       if (wifiMulti.run() == WL_CONNECTED) {
        String base64Image = base64::encode(buffer, currentIndex);
        HTTPClient http;
        USE_SERIAL.println("[HTTP] begin...");
 
        // Define the URL for POST request
        String url = "https://birdwatchers.azurewebsites.net/Json/PostJson";
        http.begin(url);
 
        // Set headers indicating that we are sending JSON
        http.addHeader("Content-Type", "application/json");
 
        // Define a simple JSON object to send
        String jsonPayload = "{\"image\":\"" + base64Image + "\"}";
 
        USE_SERIAL.printf("[HTTP] POST %s...\n", url.c_str());
        USE_SERIAL.printf("Payload: %s\n", jsonPayload.c_str());
 
        // Send the POST request and get the response code
        int httpCode = http.POST(jsonPayload);
 
        // Check if the POST request was successful
        if (httpCode > 0) {
            USE_SERIAL.printf("[HTTP] POST... code: %d\n", httpCode);
 
            // Check for HTTP 200 OK
            if (httpCode == HTTP_CODE_OK) {
                String payload = http.getString();
                USE_SERIAL.println("Response:");
                USE_SERIAL.println(payload);
            }
        } else {
            USE_SERIAL.printf("[HTTP] POST... failed, error: %s\n", http.errorToString(httpCode).c_str());
        }
        
      // Free resources
      http.end();
    } else {
      USE_SERIAL.print("No wifi");
      jpegComplete = false;
    }
 
    // Print the accumulated data if a JPEG is completed
    if (jpegComplete) {
        USE_SERIAL.println("Final buffer contents:");
        for (int i = 0; i < currentIndex; i++) {
            if (buffer[i] < 0x10) USE_SERIAL.print("0");
            USE_SERIAL.print(buffer[i], HEX);
        }
        USE_SERIAL.println();
 
        // Reset buffer index and flag for the next file
        currentIndex = 0;
        jpegComplete = false;
    }

    if(currentIndex != 0){
      currentIndex = 0;
    }

    expected_id = 0;
 
    // delay(5000); // Wait 5 seconds before trying again
  }
 
    
}