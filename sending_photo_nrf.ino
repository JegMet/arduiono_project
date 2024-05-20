#include <ArduCAM.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <RH_NRF24.h>
// #include "memorysaver.h"
//This demo can only work on OV2640_MINI_2MP or OV5642_MINI_5MP or OV5642_MINI_5MP_BIT_ROTATION_FIXED platform.
#if !(defined OV5642_MINI_5MP || defined OV5642_MINI_5MP_BIT_ROTATION_FIXED || defined OV2640_MINI_2MP || defined OV3640_MINI_3MP)
  #error Please select the hardware platform and camera module in the ../libraries/ArduCAM/memorysaver.h file
#endif
#define SD_CS 4
const int SPI_CS = 7;

RH_NRF24 nrf24;


int sensor = 2;              // the pin that the sensor is atteched to
int state = LOW;             // by default, no motion detected
int val = 0;                 // variable to store the sensor status (value)





#if defined (OV2640_MINI_2MP)
  ArduCAM myCAM( OV2640, SPI_CS );
#elif defined (OV3640_MINI_3MP)
  ArduCAM myCAM( OV3640, SPI_CS );
#else
  ArduCAM myCAM( OV5642, SPI_CS );
#endif

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






void sendFromSDCard(int jpg_number) {
    uint8_t ack_good_checksum;
    uint8_t ack_id = 0;
    bool next_packet = true;
    uint8_t packet_id = 0;
    uint8_t check_sum = 0;

    Serial.println(F("IN send from sd card."));
    char str[12];
    itoa(jpg_number, str, 10);
    strcat(str, ".jpg");
    File file = SD.open(str, FILE_READ); // Open the image file from SD card

    if (!file) {
        Serial.println(F("Failed to open file."));
        return;
    }

    int count = 0;
    const int PACKET_SIZE = 28; // Adjust packet size as needed
    uint8_t buffer[PACKET_SIZE];
    int bytesRead;

    ack_good_checksum = 2;

    while (file.available() || (ack_good_checksum != 2)) {
        ack_good_checksum = 0;
        Serial.print("ack id: ");
        Serial.print(ack_id);
        Serial.println();

        Serial.print("packet_id id: ");
        Serial.print(packet_id);
        Serial.println();

        if(next_packet) { // will be true if we need to go to next packet or else leave it the same and resend it
          bytesRead = file.read(buffer, (PACKET_SIZE - 2)); // 2 bytes off, one for checksum one for packet id
          buffer[26] = packet_id;
          buffer[27] = calculateChecksum(buffer, (PACKET_SIZE-2));
          next_packet = false;

        }

        if (bytesRead <= 0) {
            Serial.println(F("Failed to read from file."));
            break;
        }

        if (!send_with_retry(buffer, bytesRead + 2)) {
            Serial.println(F("Failed to send packet."));
            break;
        }

        Serial.print("Packet id in sender is: ");
        Serial.print(packet_id);
        Serial.println();

        nrf24.waitPacketSent();
        // Now wait for a reply
        uint8_t buf[RH_NRF24_MAX_MESSAGE_LEN];
        uint8_t len = sizeof(buf);

        if (nrf24.waitAvailableTimeout(2000)) {
            // Should be a reply message for us now
            if (nrf24.recv(buf, &len)) {
                ack_id = buf[0];
                ack_good_checksum = buf[1];
                Serial.print("ack id from reply: ");
                Serial.print(ack_id);
                Serial.println();

                Serial.print("checksum: ");
                Serial.print(ack_good_checksum);
                Serial.println();

                if ((ack_id == ((packet_id + 1) % 256)) && (ack_good_checksum == 0)) {  // CHANGED CODE
                    packet_id = (packet_id + 1) % 256; // Wrap around packet ID
                    next_packet = true;
                    count++;
                } 
        } else {
            Serial.println("No reply, is nrf24_server running?");
        }
        delay(400);
        }
    }

    ack_id = 0;

    ack_good_checksum = 0;

    file.close();

    Serial.print(F("Total packets sent: "));
    Serial.println(count);
}



int myCAMSaveToSDFile(){
char str[12];
byte buf[256];
static int i = 0;
int k = 0;
uint8_t temp = 0,temp_last=0;
uint32_t length = 0;
bool is_header = false;
File outFile;
//Flush the FIFO
myCAM.flush_fifo();
//Clear the capture done flag
myCAM.clear_fifo_flag();
//Start capture
myCAM.start_capture();
Serial.println(F("start Capture"));
while(!myCAM.get_bit(ARDUCHIP_TRIG , CAP_DONE_MASK));
Serial.println(F("Capture Done."));  
length = myCAM.read_fifo_length();
Serial.print(F("The fifo length is :"));
Serial.println(length, DEC);
if (length >= MAX_FIFO_SIZE) //384K
{
  Serial.println(F("Over size."));
  return ;
}
if (length == 0 ) //0 kb
{
  Serial.println(F("Size is 0."));
  return ;
} /*
//Construct a file name
k = k + 1;
itoa(k, str, 10);
strcat(str, ".jpg");
//Open the new file
outFile = SD.open(str, O_WRITE | O_CREAT | O_TRUNC);
if(!outFile){
  Serial.println(F("File open faild"));
  return;
} */

bool fileOpened = false;

for (k = 0; k < 1000; k++) {
    // Construct a file name
    itoa(k, str, 10);
    strcat(str, ".jpg");

    // Open the new file
    outFile = SD.open(str, O_WRITE | O_CREAT | O_TRUNC);

    // Check if the file was successfully opened
    if (outFile) {
        Serial.println(F("File opened successfully. number is : "));
        Serial.println(k);
        fileOpened = true;
        break; // Exit the loop since the file is successfully created
    } else {
        Serial.println(F("File open failed. Trying again..."));
    }
}

if (!fileOpened) {
    Serial.println(F("Could not open a file up to the max integer value."));
}


myCAM.CS_LOW();
myCAM.set_fifo_burst();
while ( length-- )
{
  temp_last = temp;
  temp =  SPI.transfer(0x00);
  //Read JPEG data from FIFO
  if ( (temp == 0xD9) && (temp_last == 0xFF) ) //If find the end ,break while,
  {
    buf[i++] = temp;  //save the last  0XD9     
    //Write the remain bytes in the buffer
    myCAM.CS_HIGH();
    outFile.write(buf, i);    
    //Close the file
    outFile.close();
    Serial.println(F("Image save OK."));
    is_header = false;
    i = 0;
  }  
  if (is_header == true)
  { 
    //Write image data to buffer if not full
    if (i < 256)
    buf[i++] = temp;
    else
    {
      //Write 256 bytes image data to file
      myCAM.CS_HIGH();
      outFile.write(buf, 256);
      i = 0;
      buf[i++] = temp;
      myCAM.CS_LOW();
      myCAM.set_fifo_burst();
    }        
  }
  else if ((temp == 0xD8) & (temp_last == 0xFF))
  {
    is_header = true;
    buf[i++] = temp_last;
    buf[i++] = temp;   
  } 
} 

return k;

}

// SETUP                          !!!!!!!!!!

void setup(){
uint8_t vid, pid;
uint8_t temp;
Wire.begin();
Serial.begin(115200);
Serial.println(F("ArduCAM Start!"));

  if (!nrf24.init())
    Serial.println("init failed");
  // Defaults after init are 2.402 GHz (channel 2), 2Mbps, 0dBm
  if (!nrf24.setChannel(1))
    Serial.println("setChannel failed");
  if (!nrf24.setRF(RH_NRF24::DataRate2Mbps, RH_NRF24::TransmitPower0dBm))
    Serial.println("setRF failed");    
//set the CS as an output:
pinMode(SPI_CS,OUTPUT);
digitalWrite(SPI_CS, HIGH);
// initialize SPI:
SPI.begin();


pinMode(sensor, INPUT);    // initialize sensor as an input
  
//Reset the CPLD
myCAM.write_reg(0x07, 0x80);
delay(100);
myCAM.write_reg(0x07, 0x00);
delay(100);
  
while(1){
  //Check if the ArduCAM SPI bus is OK
  myCAM.write_reg(ARDUCHIP_TEST1, 0x55);
  temp = myCAM.read_reg(ARDUCHIP_TEST1);
  
  if (temp != 0x55){
    Serial.println(F("SPI interface Error!"));
    delay(1000);continue;
  }else{
    Serial.println(F("SPI interface OK."));break;
  }
}
//Initialize SD Card
while(!SD.begin(SD_CS)){
  Serial.println(F("SD Card Error!"));delay(1000);
}
Serial.println(F("SD Card detected."));

#if defined (OV2640_MINI_2MP)
  while(1){
    //Check if the camera module type is OV2640
    myCAM.wrSensorReg8_8(0xff, 0x01);
    myCAM.rdSensorReg8_8(OV2640_CHIPID_HIGH, &vid);
    myCAM.rdSensorReg8_8(OV2640_CHIPID_LOW, &pid);
    if ((vid != 0x26 ) && (( pid != 0x41 ) || ( pid != 0x42 ))){
      Serial.println(F("Can't find OV2640 module!"));
      delay(1000);continue;
    }
    else{
      Serial.println(F("OV2640 detected."));break;
    } 
  }
#elif defined (OV3640_MINI_3MP)
  while(1){
    //Check if the camera module type is OV3640
    myCAM.rdSensorReg16_8(OV3640_CHIPID_HIGH, &vid);
    myCAM.rdSensorReg16_8(OV3640_CHIPID_LOW, &pid);
    if ((vid != 0x36) || (pid != 0x4C)){
      Serial.println(F("Can't find OV3640 module!"));
      delay(1000);continue; 
    }else{
      Serial.println(F("OV3640 detected."));break;    
    }
 } 
#else
  while(1){
    //Check if the camera module type is OV5642
    myCAM.wrSensorReg16_8(0xff, 0x01);
    myCAM.rdSensorReg16_8(OV5642_CHIPID_HIGH, &vid);
    myCAM.rdSensorReg16_8(OV5642_CHIPID_LOW, &pid);
    if((vid != 0x56) || (pid != 0x42)){
      Serial.println(F("Can't find OV5642 module!"));
      delay(1000);continue;
    }
    else{
      Serial.println(F("OV5642 detected."));break;
    } 
  }
#endif
myCAM.set_format(JPEG);
myCAM.InitCAM();
#if defined (OV2640_MINI_2MP)
  myCAM.OV2640_set_JPEG_size(OV2640_320x240);
#elif defined (OV3640_MINI_3MP)
  myCAM.OV3640_set_JPEG_size(OV3640_320x240);
#else
  myCAM.write_reg(ARDUCHIP_TIM, VSYNC_LEVEL_MASK);   //VSYNC is active HIGH
  myCAM.OV5642_set_JPEG_size(OV5642_320x240);
#endif
delay(1000);
}


// LOOP                          !!!!!!!!!!

void loop(){
/*
  val = digitalRead(sensor);   // read sensor value
  if (val == HIGH) {           // check if the sensor is HIGH
    
    if (state == LOW) {
      Serial.println(F("Motion detected!")); 
      state = HIGH;       // update variable state to HIGH


    }
  } 
  else {
      //digitalWrite(led, LOW); // turn LED OFF
      delay(200);             // delay 200 milliseconds 
      
      if (state == HIGH){
        Serial.println(F("Motion stopped!"));
        state = LOW;       // update variable state to LOW
    }
  } */
  delay(5000);
//myCAMSaveToSDFile();
sendFromSDCard(1);
delay(5000);
}

bool send_with_retry(uint8_t *buffer, size_t len) {
    int retryCount = 0;
    while (retryCount < 3) {
        if (nrf24.send(buffer, len)) {
            return true;
        }
        Serial.println(F("Retry sending packet..."));
        delay(100);
        retryCount++;
    }
    return false;
}