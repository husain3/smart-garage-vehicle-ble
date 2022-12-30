#include <Arduino.h>
#include <NimBLEDevice.h>

static NimBLEServer* pServer;

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

	NimBLEService* pAuthorizedVehicleService = pServer->createService("pAVS");
	NimBLECharacteristic* pDateTimeCharacteristic = pAuthorizedVehicleService->createCharacteristic(
												"DTC",
												NIMBLE_PROPERTY::READ |
												NIMBLE_PROPERTY::WRITE |
												/** Require a secure connection for read and write access */
												NIMBLE_PROPERTY::READ_ENC |  // only allow reading if paired / encrypted
												NIMBLE_PROPERTY::WRITE_ENC   // only allow writing if paired / encrypted
												);

	NimBLECharacteristic* pHashCharacteristic = pAuthorizedVehicleService->createCharacteristic(
											"H4SH",
											NIMBLE_PROPERTY::NOTIFY
											);

	NimBLECharacteristic* pAliveTimeCharacteristic = pAuthorizedVehicleService->createCharacteristic(
												"AliveTime",
												NIMBLE_PROPERTY::READ |
												/** Require a secure connection for read and write access */
												NIMBLE_PROPERTY::READ_ENC // only allow reading if paired / encrypted
												);

	/** Start the services when finished creating all Characteristics and Descriptors */
	pAuthorizedVehicleService->start();

	NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
	/** Add the services to the advertisment data **/
	pAdvertising->addServiceUUID(pAuthorizedVehicleService->getUUID());
	/** If your device is battery powered you may consider setting scan response
	 *  to false as it will extend battery life at the expense of less data sent.
	*/
	pAdvertising->setScanResponse(true);
	pAdvertising->start();

	Serial.println("Advertising Started");
}

void loop()
{
	// put your main code here, to run repeatedly:
}