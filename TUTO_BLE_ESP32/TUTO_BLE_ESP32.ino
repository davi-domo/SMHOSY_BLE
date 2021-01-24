/*
    Video: https://www.youtube.com/watch?v=oCMOYS71NIU
    Based on Neil Kolban example for IDF: https://github.com/nkolban/esp32-snippets/blob/master/cpp_utils/tests/BLE%20Tests/SampleNotify.cpp
    Ported to Arduino ESP32 by Evandro Copercini

   adaptation par SMHOSY DAVID https://smhosy.blogspot.com/
   petit tuto permettant le pilotage de 8 relais via une aplication android

   Librairie PCF8574 https://github.com/skywodd/pcf8574_arduino_library
*/

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>


#include "Arduino.h"
#include "PCF8574.h"
#include "config.h"

BLEServer *pServer = NULL;
BLECharacteristic * pTxCharacteristic;
bool deviceConnected = false;
bool oldDeviceConnected = false;
uint8_t txValue = 0;

// See the following for generating UUIDs:
// https://www.uuidgenerator.net/

#define SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E" // UART service UUID
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

//initialisation de l'I2C dans notre exemple @0x38
PCF8574 I2COutput(0x38);

// variable pour l'I2C
const int out_array[8] = {0,1,2,3,4,5,6,7};
int etat_out_array[8] = {1,1,1,1,1,1,1,1};

//fonction explode les donnée recu pour extraire l'API_KEY et la commande
String explode(String data, char separator, int index){
  int found = 0;
  int strIndex[] = { 0, -1 };
  int maxIndex = data.length() - 1;

  for (int i = 0; i <= maxIndex && found <= index; i++) {
    if (data.charAt(i) == separator || i == maxIndex) {
      found++;
      strIndex[0] = strIndex[1] + 1;
      strIndex[1] = (i == maxIndex) ? i+1 : i;
     }
   }
  return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
    }
};

class MyCallbacks: public BLECharacteristicCallbacks {
  String api_key_uart = "";
  int btn = -1;
  
  void onWrite(BLECharacteristic *pCharacteristic) {
    std::string rxValue = pCharacteristic->getValue();
    String recv ="";
    if (rxValue.length() > 0) {
      for (int i = 0; i < rxValue.length(); i++){
        recv += rxValue[i];
      }
    }
    // on eclate la varible de reception
    api_key_uart = explode(recv, ':', 0);
    btn = (explode(recv, ':', 1)).toInt();
    // si l'API_KEY correspond
    if(API_KEY == api_key_uart){
      // on inverse l'etat de la pin
      etat_out_array[btn-1] = 1 - etat_out_array[btn-1];
      // on envoi la commande
      I2COutput.digitalWrite(btn-1, etat_out_array[btn-1]);
      Serial.print("[SUCCES] -> COMMANDE BTN : ");Serial.println(btn);
    }
    else
    {
      Serial.println("[ERROR] -> API_KEY NO CORRECT");
    }
  }  
};


void setup() {
  Serial.begin(115200);
  
  /*Déclaration des I/O I2C */
  for (int i=0; i<8; i++) {
    I2COutput.pinMode (out_array[i], OUTPUT);
    I2COutput.digitalWrite(out_array[i], etat_out_array[out_array[i]]);
  }
  
  I2COutput.begin();
  // Create the BLE Device
  BLEDevice::init("ESP32_BLE_smhosy");

  // Create the BLE Server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create the BLE Service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create a BLE Characteristic
  pTxCharacteristic = pService->createCharacteristic(
										CHARACTERISTIC_UUID_TX,
										BLECharacteristic::PROPERTY_NOTIFY
									);
                      
  pTxCharacteristic->addDescriptor(new BLE2902());

  BLECharacteristic * pRxCharacteristic = pService->createCharacteristic(
											 CHARACTERISTIC_UUID_RX,
											BLECharacteristic::PROPERTY_WRITE
										);

  pRxCharacteristic->setCallbacks(new MyCallbacks());

  // Start the service
  pService->start();

  // Start advertising
  pServer->getAdvertising()->start();
  Serial.println("Waiting a client connection to notify...");
}



void loop() {

    if (deviceConnected) {
        pTxCharacteristic->setValue(&txValue, 1);
        pTxCharacteristic->notify();
        txValue++;
		    delay(10); // bluetooth stack will go into congestion, if too many packets are sent
	  }

    // disconnecting
    if (!deviceConnected && oldDeviceConnected) {
        delay(500); // give the bluetooth stack the chance to get things ready
        pServer->startAdvertising(); // restart advertising
        Serial.println("start advertising");
        oldDeviceConnected = deviceConnected;
    }
    // connecting
    if (deviceConnected && !oldDeviceConnected) {
		// do stuff here on connecting
        oldDeviceConnected = deviceConnected;
    }
}
