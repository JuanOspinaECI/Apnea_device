#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BNO055.h>
#include <utility/imumaths.h>

#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h> // only for esp_wifi_set_channel()

//#include <BluetoothSerial.h>
//BluetoothSerial SerialBT;
#define BNO055_SAMPLERATE_DELAY_MS (10)

esp_now_peer_info_t slave;
#define CHANNEL 1
#define PRINTSCANRESULTS 0
#define DELETEBEFOREPAIR 0

typedef struct struct_message {
    int id;
    float x;
    float y;
    float z;
    int readingId;
} struct_message;

struct_message myData;

// Check I2C device address and correct line below (by default address is 0x29 or 0x28)
//                                   id, address
Adafruit_BNO055 bno = Adafruit_BNO055(-1, 0x28, &Wire);

/**************************************************************************/
/*
    Arduino setup function (automatically called at startup)
*/
/**************************************************************************/
int c_ms = 0;

void InitESPNow() {
  WiFi.disconnect();
  if (esp_now_init() == ESP_OK) {
    Serial.println(F("ESPNow Init Success"));
  }
  else {
    Serial.println(F("ESPNow Init Failed"));
    // Retry InitESPNow, add a counte and then restart?
    // InitESPNow();
    // or Simply Restart
    ESP.restart();
  }
}

void ScanForSlave() {
  int16_t scanResults = WiFi.scanNetworks(false, false, false, 300, CHANNEL); // Scan only on one channel
  // reset on each scan
  bool slaveFound = 0;
  memset(&slave, 0, sizeof(slave));

  Serial.println(F(""));
  if (scanResults == 0) {
    Serial.println(F("No WiFi devices in AP Mode found"));
  } else {
    Serial.print(F("Found ")); Serial.print(scanResults); Serial.println(F(" devices "));
    for (int i = 0; i < scanResults; ++i) {
      // Print SSID and RSSI for each device found
      String SSID = WiFi.SSID(i);
      int32_t RSSI = WiFi.RSSI(i);
      String BSSIDstr = WiFi.BSSIDstr(i);

      if (PRINTSCANRESULTS) {
        Serial.print(i + 1);
        Serial.print(F(": "));
        Serial.print(SSID);
        Serial.print(F(" ("));
        Serial.print(RSSI);
        Serial.print(F(")"));
        Serial.println(F(""));
      }
      delay(10);
      // Check if the current device starts with `Slave`
      if (SSID.indexOf("Apnea_device") == 0) {
        // SSID of interest
        Serial.println(F("Found a Slave."));
        Serial.print(i + 1); Serial.print(F(": ")); Serial.print(SSID); Serial.print(F(" [")); Serial.print(BSSIDstr); Serial.print(F("]")); Serial.print(F(" (")); Serial.print(RSSI); Serial.print(F(")")); Serial.println(F(""));
        // Get BSSID => Mac Address of the Slave
        int mac[6];
        if ( 6 == sscanf(BSSIDstr.c_str(), "%x:%x:%x:%x:%x:%x",  &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5] ) ) {
          for (int ii = 0; ii < 6; ++ii ) {
            slave.peer_addr[ii] = (uint8_t) mac[ii];
          }
        }

        slave.channel = CHANNEL; // pick a channel
        slave.encrypt = 0; // no encryption

        slaveFound = 1;
        // we are planning to have only one slave in this example;
        // Hence, break after we find one, to be a bit efficient
        break;
      }
    }
  }

  if (slaveFound) {
    Serial.println(F("Slave Found, processing.."));
  } else {
    Serial.println(F("Slave Not Found, trying again."));
  }

  // clean up ram
  WiFi.scanDelete();
}

bool manageSlave() {
  if (slave.channel == CHANNEL) {
    if (DELETEBEFOREPAIR) {
      deletePeer();
    }

    Serial.print(F("Slave Status: "));
    // check if the peer exists
    bool exists = esp_now_is_peer_exist(slave.peer_addr);
    if ( exists) {
      // Slave already paired.
      Serial.println(F("Already Paired"));
      return true;
    } else {
      // Slave not paired, attempt pair
      esp_err_t addStatus = esp_now_add_peer(&slave);
      if (addStatus == ESP_OK) {
        // Pair success
        Serial.println(F("Pair success"));
        return true;
      } else if (addStatus == ESP_ERR_ESPNOW_NOT_INIT) {
        // How did we get so far!!
        Serial.println(F("ESPNOW Not Init"));
        return false;
      } else if (addStatus == ESP_ERR_ESPNOW_ARG) {
        Serial.println(F("Invalid Argument"));
        return false;
      } else if (addStatus == ESP_ERR_ESPNOW_FULL) {
        Serial.println(F("Peer list full"));
        return false;
      } else if (addStatus == ESP_ERR_ESPNOW_NO_MEM) {
        Serial.println(F("Out of memory"));
        return false;
      } else if (addStatus == ESP_ERR_ESPNOW_EXIST) {
        Serial.println(F("Peer Exists"));
        return true;
      } else {
        Serial.println(F("Not sure what happened"));
        return false;
      }
    }
  } else {
    // No slave found to process
    Serial.println(F("No Slave found to process"));
    return false;
  }
}

void deletePeer() {
  esp_err_t delStatus = esp_now_del_peer(slave.peer_addr);
  Serial.print(F("Slave Delete Status: "));
  if (delStatus == ESP_OK) {
    // Delete success
    Serial.println(F("Success"));
  } else if (delStatus == ESP_ERR_ESPNOW_NOT_INIT) {
    // How did we get so far!!
    Serial.println(F("ESPNOW Not Init"));
  } else if (delStatus == ESP_ERR_ESPNOW_ARG) {
    Serial.println(F("Invalid Argument"));
  } else if (delStatus == ESP_ERR_ESPNOW_NOT_FOUND) {
    Serial.println(F("Peer not found."));
  } else {
    Serial.println(F("Not sure what happened"));
  }
}

uint8_t data = 0;
// send data
void sendData(float raw_x, float raw_y, float raw_z) {
  data++;
  myData.id = 2;
  myData.x = raw_x;
  myData.y = raw_y;
  myData.z = raw_z;
  myData.readingId = 0;
  const uint8_t *peer_addr = slave.peer_addr;
  Serial.print(F("Sending: ")); Serial.println(data);
  esp_err_t result = esp_now_send(peer_addr, (uint8_t *) &myData, sizeof(myData));
  Serial.print(F("Send Status: "));
  if (result == ESP_OK) {
    Serial.println(F("Success"));
  } else if (result == ESP_ERR_ESPNOW_NOT_INIT) {
    // How did we get so far!!
    Serial.println(F("ESPNOW not Init."));
  } else if (result == ESP_ERR_ESPNOW_ARG) {
    Serial.println(F("Invalid Argument"));
  } else if (result == ESP_ERR_ESPNOW_INTERNAL) {
    Serial.println(F("Internal Error"));
  } else if (result == ESP_ERR_ESPNOW_NO_MEM) {
    Serial.println(F("ESP_ERR_ESPNOW_NO_MEM"));
  } else if (result == ESP_ERR_ESPNOW_NOT_FOUND) {
    Serial.println(F("Peer not found."));
  } else {
    Serial.println(F("Not sure what happened"));
  }
}

// callback when data is sent from Master to Slave
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
           mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
  Serial.print(F("Last Packet Sent to: ")); Serial.println(macStr);
  Serial.print(F("Last Packet Send Status: ")); Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}

void setup(void)
{
  Serial.begin(115200);
  //SerialBT.begin("ESP32_ACCELERACIÓN");
  while (!Serial) delay(10);  // wait for serial port to open!

  Serial.println(F("Orientation Sensor Raw Data Test")); Serial.println(F(""));

  /* Initialise the sensor */
  if(!bno.begin())
  {
    /* There was a problem detecting the BNO055 ... check your connections */
    Serial.print(F("Ooops, no BNO055 detected ... Check your wiring or I2C ADDR!"));
    while(1);
  }

  delay(1000);

  /* Display the current temperature */
  int8_t temp = bno.getTemp();
  Serial.print(F("Current Temperature: "));
  Serial.print(temp);
  Serial.println(F(" C"));
  Serial.println(F(""));

  bno.setExtCrystalUse(true);

  Serial.println(F("Calibration status values: 0=uncalibrated, 3=fully calibrated"));

  WiFi.mode(WIFI_STA);
  esp_wifi_set_channel(CHANNEL, WIFI_SECOND_CHAN_NONE);
  Serial.println(F("ESPNow/Basic/Master Example"));
  // This is the mac address of the Master in Station Mode
  Serial.print(F("STA MAC: ")); Serial.println(WiFi.macAddress());
  Serial.print(F("STA CHANNEL ")); Serial.println(WiFi.channel());
  // Init ESPNow with a fallback logic
  InitESPNow();
  // Once ESPNow is successfully Init, we will register for Send CB to
  // get the status of Trasnmitted packet
  esp_now_register_send_cb(OnDataSent);
}

/**************************************************************************/
/*
    Arduino loop function, called once 'setup' is complete (your own code
    should go here)
*/
/**************************************************************************/
void loop(void)
{
  // Possible vector values can be:
  // - VECTOR_ACCELEROMETER - m/s^2
  // - VECTOR_MAGNETOMETER  - uT
  // - VECTOR_GYROSCOPE     - rad/s
  // - VECTOR_EULER         - degrees
  // - VECTOR_LINEARACCEL   - m/s^2
  // - VECTOR_GRAVITY       - m/s^2
  imu::Vector<3> ACC = bno.getVector(Adafruit_BNO055::VECTOR_ACCELEROMETER);
  float x = ACC.x();
  float y = ACC.y();
  float z = ACC.z();

    // In the loop we scan for slave
  ScanForSlave();
  // If Slave is found, it would be populate in `slave` variable
  // We will check if `slave` is defined and then we proceed further
  if (slave.channel == CHANNEL) { // check if slave channel is defined
    // `slave` is defined
    // Add slave as peer if it has not been added already
    bool isPaired = manageSlave();
    if (isPaired) {
      // pair success or already paired
      // Send data to device
      sendData(x,y,z);
    } else {
      // slave pair failed
      Serial.println(F("Slave pair failed!"));
    }
  }
  else {
    // No slave found to process
  }

  /* Display the floating point data */
  Serial.print (c_ms);
  Serial.print(F(";X:"));
  Serial.print(x);
  Serial.print(F(";"));

  Serial.print(F(" Y:"));
  Serial.print(y);
  Serial.print(F(";"));

  
  Serial.print(F(" Z:"));
  Serial.print(z);
  Serial.print(F(";"));

  
  Serial.print(F("\t"));
  
 
  /* Display calibration status for each sensor. */
  uint8_t system, gyro, accel, mag = 0;
  bno.getCalibration(&system, &gyro, &accel, &mag);
 
  Serial.print(F(" Cal_Acc="));
  Serial.println(accel, DEC);

  //SerialBT.print("AC");
  //SerialBT.println(accel); 
   
  delay(BNO055_SAMPLERATE_DELAY_MS);
  c_ms += 1; 
}


