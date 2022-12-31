#include <Arduino.h>
#include <NimBLEDevice.h>

static NimBLEServer *pServer;

/**  None of these are required as they will be handled by the library with defaults. **
 **                       Remove as you see fit for your needs                        */
class ServerCallbacks : public NimBLEServerCallbacks
{
	void onConnect(NimBLEServer *pServer)
	{
		Serial.println("Client connected");
		Serial.println("Multi-connect support: start advertising");
		// NimBLEDevice::startAdvertising();
	};
	/** Alternative onConnect() method to extract details of the connection.
	 *  See: src/ble_gap.h for the details of the ble_gap_conn_desc struct.
	 */
	void onConnect(NimBLEServer *pServer, ble_gap_conn_desc *desc)
	{
		Serial.print("Client address: ");
		Serial.println(NimBLEAddress(desc->peer_ota_addr).toString().c_str());
		/** We can use the connection handle here to ask for different connection parameters.
		 *  Args: connection handle, min connection interval, max connection interval
		 *  latency, supervision timeout.
		 *  Units; Min/Max Intervals: 1.25 millisecond increments.
		 *  Latency: number of intervals allowed to skip.
		 *  Timeout: 10 millisecond increments, try for 5x interval time for best results.
		 */
		pServer->updateConnParams(desc->conn_handle, 24, 48, 0, 60);
	};
	void onDisconnect(NimBLEServer *pServer)
	{
		Serial.println("Client disconnected - start advertising");
		NimBLEDevice::startAdvertising();
	};
	void onMTUChange(uint16_t MTU, ble_gap_conn_desc *desc)
	{
		Serial.printf("MTU updated: %u for connection ID: %u\n", MTU, desc->conn_handle);
	};

	/********************* Security handled here **********************
	****** Note: these are the same return values as defaults ********/
	uint32_t onPassKeyRequest()
	{
		Serial.println("Server Passkey Request");
		/** This should return a random 6 digit number for security
		 *  or make your own static passkey as done here.
		 */
		return 123456;
	};

	bool onConfirmPIN(uint32_t pass_key)
	{
		Serial.print("The passkey YES/NO number: ");
		Serial.println(pass_key);
		/** Return false if passkeys don't match. */
		return true;
	};

	void onAuthenticationComplete(ble_gap_conn_desc *desc)
	{
		/** Check that encryption was successful, if not we disconnect the client */
		if (!desc->sec_state.encrypted)
		{
			NimBLEDevice::getServer()->disconnect(desc->conn_handle);
			Serial.println("Encrypt connection failed - disconnecting client");
			return;
		}
		Serial.println("Starting BLE work!");
	};
};

/** Handler class for characteristic actions */
class DateTimeCharacteristicCallbacks : public NimBLECharacteristicCallbacks
{
	void onWrite(NimBLECharacteristic *pCharacteristic)
	{
		Serial.print(pCharacteristic->getUUID().toString().c_str());
		Serial.print(": onWrite(), value: ");
		Serial.println(pCharacteristic->getValue<long>());

		/*Logic to calculate rolling code to send back to BLE Client*/
		NimBLEService *pSvc = pServer->getServiceByUUID("AAAA");
		if (pSvc)
		{
			NimBLECharacteristic *pChr = pSvc->getCharacteristic("CCCC");
			if (pChr)
			{
				pChr->setValue(millis());
				pChr->notify(true);
			}
		}
	};
};

class AliveTimeCharacteristicCallbacks : public NimBLECharacteristicCallbacks
{
	void onRead(NimBLECharacteristic *pCharacteristic)
	{
		Serial.print(pCharacteristic->getUUID().toString().c_str());
		Serial.print(": onRead(), value: ");
		Serial.println(pCharacteristic->getValue().c_str());
	};
};

class HashNotificationCharacteristicCallbacks : public NimBLECharacteristicCallbacks
{
	/** Called before notification or indication is sent,
	 *  the value can be changed here before sending if desired.
	 */
	void onNotify(NimBLECharacteristic *pCharacteristic)
	{
		Serial.println("Sending notification to clients");
	};

	/** The status returned in status is defined in NimBLECharacteristic.h.
	 *  The value returned in code is the NimBLE host return code.
	 */
	void onStatus(NimBLECharacteristic *pCharacteristic, Status status, int code)
	{
		String str = ("Notification/Indication status code: ");
		str += status;
		str += ", return code: ";
		str += code;
		str += ", ";
		str += NimBLEUtils::returnCodeToString(code);
		Serial.println(str);
	};

	void onSubscribe(NimBLECharacteristic *pCharacteristic, ble_gap_conn_desc *desc, uint16_t subValue)
	{
		String str = "Client ID: ";
		str += desc->conn_handle;
		str += " Address: ";
		str += std::string(NimBLEAddress(desc->peer_ota_addr)).c_str();
		if (subValue == 0)
		{
			str += " Unsubscribed to ";
		}
		else if (subValue == 1)
		{
			str += " Subscribed to notfications for ";
		}
		else if (subValue == 2)
		{
			str += " Subscribed to indications for ";
		}
		else if (subValue == 3)
		{
			str += " Subscribed to notifications and indications for ";
		}
		str += std::string(pCharacteristic->getUUID()).c_str();

		Serial.println(str);
	};
};

static DateTimeCharacteristicCallbacks dateTimeChrCallbacks;
static HashNotificationCharacteristicCallbacks hashNotifyChrCallbacks;
static AliveTimeCharacteristicCallbacks aliveTimeChrCallbacks;

void setup()
{
	Serial.begin(115200);
	Serial.println("Starting NimBLE Server");

	/** sets device name */
	NimBLEDevice::init("RAV4-Opener");

	/** Optional: set the transmit power, default is 3db */
#ifdef ESP_PLATFORM
	NimBLEDevice::setPower(ESP_PWR_LVL_P9); /** +9db */
#else
	NimBLEDevice::setPower(9); /** +9db */
#endif

	NimBLEDevice::setSecurityAuth(false, false, false);

	pServer = NimBLEDevice::createServer();
	pServer->setCallbacks(new ServerCallbacks());

	NimBLEService *pAuthorizedVehicleService = pServer->createService("AAAA");
	NimBLECharacteristic *pDateTimeCharacteristic = pAuthorizedVehicleService->createCharacteristic(
		"BBBB",
		NIMBLE_PROPERTY::READ |
			NIMBLE_PROPERTY::WRITE |
			/** Require a secure connection for read and write access */
			NIMBLE_PROPERTY::READ_ENC | // only allow reading if paired / encrypted
			NIMBLE_PROPERTY::WRITE_ENC	// only allow writing if paired / encrypted
	);
	pDateTimeCharacteristic->setValue("hello");
	pDateTimeCharacteristic->setCallbacks(&dateTimeChrCallbacks);

	NimBLECharacteristic *pHashCharacteristic = pAuthorizedVehicleService->createCharacteristic(
		"CCCC",
		NIMBLE_PROPERTY::NOTIFY);

	pHashCharacteristic->setCallbacks(&hashNotifyChrCallbacks);

	NimBLECharacteristic *pAliveTimeCharacteristic = pAuthorizedVehicleService->createCharacteristic(
		"DDDD",
		NIMBLE_PROPERTY::READ |
			NIMBLE_PROPERTY::WRITE |
			/** Require a secure connection for read and write access */
			NIMBLE_PROPERTY::READ_ENC | // only allow reading if paired / encrypted
			NIMBLE_PROPERTY::WRITE_ENC	// only allow writing if paired / encrypted
	);

	pAliveTimeCharacteristic->setCallbacks(&aliveTimeChrCallbacks);

	/** Start the services when finished creating all Characteristics and Descriptors */
	pAuthorizedVehicleService->start();

	NimBLEAdvertising *pAdvertising = NimBLEDevice::getAdvertising();
	/** Add the services to the advertisment data **/
	pAdvertising->addServiceUUID(pAuthorizedVehicleService->getUUID());
	/** If your device is battery powered you may consider setting scan response
	 *  to false as it will extend battery life at the expense of less data sent.
	 */
	pAdvertising->setScanResponse(true);
	pAdvertising->start();

	Serial.println("Advertising Started");
}

long lastWrittenTime = 0;
void loop()
{
	long currentTime = millis();

	if (currentTime - lastWrittenTime > 1000)
	{
		NimBLEService *pSvc = pServer->getServiceByUUID("AAAA");
		if (pSvc)
		{
			NimBLECharacteristic *pChr = pSvc->getCharacteristic("DDDD");
			if (pChr)
			{
				Serial.println(currentTime);
				pChr->setValue(currentTime);
				lastWrittenTime = currentTime;
			}

			NimBLECharacteristic *pChr2 = pSvc->getCharacteristic("BBBB");
			if (pChr)
			{
				Serial.println(currentTime);
				Serial.println(pChr2->getValue());
				lastWrittenTime = currentTime;
			}
		}
	}
	delay(2000);
}