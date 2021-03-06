/* mbed Microcontroller Library
 * Copyright (c) 2006-2013 ARM Limited
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "mbed.h"
#include "BLEDevice.h"
#include "ble_hrs.h"

BLEDevice  ble;
DigitalOut led1(LED1);

#define NEED_CONSOLE_OUTPUT 0 /* Set this if you need debug messages on the console;
                               * it will have an impact on code-size and power consumption. */

#if NEED_CONSOLE_OUTPUT
Serial  pc(USBTX, USBRX);
#define DEBUG(...) { pc.printf(__VA_ARGS__); }
#else
#define DEBUG(...) /* nothing */
#endif /* #if NEED_CONSOLE_OUTPUT */

const static char  DEVICE_NAME[] = "Nordic_HRM";

/* Heart Rate Service */
/* Service:  https://developer.bluetooth.org/gatt/services/Pages/ServiceViewer.aspx?u=org.bluetooth.service.heart_rate.xml */
/* HRM Char: https://developer.bluetooth.org/gatt/characteristics/Pages/CharacteristicViewer.aspx?u=org.bluetooth.characteristic.heart_rate_measurement.xml */
/* Location: https://developer.bluetooth.org/gatt/characteristics/Pages/CharacteristicViewer.aspx?u=org.bluetooth.characteristic.body_sensor_location.xml */
static uint8_t hrmCounter = 100;
static uint8_t bpm[2] = {0x00, hrmCounter};
GattCharacteristic hrmRate(GattCharacteristic::UUID_HEART_RATE_MEASUREMENT_CHAR, bpm, sizeof(bpm), sizeof(bpm),
                           GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_NOTIFY);
static uint8_t location = BLE_HRS_BODY_SENSOR_LOCATION_FINGER;
GattCharacteristic hrmLocation(GattCharacteristic::UUID_BODY_SENSOR_LOCATION_CHAR,
                               (uint8_t *)&location, sizeof(location), sizeof(location),
                               GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_READ);
GattCharacteristic *hrmChars[] = {&hrmRate, &hrmLocation, };
GattService        hrmService(GattService::UUID_HEART_RATE_SERVICE, hrmChars, sizeof(hrmChars) / sizeof(GattCharacteristic *));

static const uint16_t uuid16_list[] = {GattService::UUID_HEART_RATE_SERVICE};

static volatile bool triggerSensorPolling = false; /* set to high periodically to indicate to the main thread that
                                                    * polling is necessary. */
static Gap::ConnectionParams_t connectionParams;

void disconnectionCallback(Gap::Handle_t handle)
{
    DEBUG("Disconnected handle %u!\n\r", handle);
    DEBUG("Restarting the advertising process\n\r");
    ble.startAdvertising();
}

void onConnectionCallback(Gap::Handle_t handle)
{
    DEBUG("connected. Got handle %u\r\n", handle);

    connectionParams.slaveLatency = 1;
    if (ble.updateConnectionParams(handle, &connectionParams) != BLE_ERROR_NONE) {
        DEBUG("failed to update connection paramter\r\n");
    }
}

/**
 * Triggered periodically by the 'ticker' interrupt.
 */
void periodicCallback(void)
{
    led1 = !led1; /* Do blinky on LED1 while we're waiting for BLE events */
    triggerSensorPolling = true; /* Note that the periodicCallback() executes in
                                  * interrupt context, so it is safer to do
                                  * heavy-weight sensor polling from the main
                                  * thread.*/
}

int main(void)
{
    led1 = 1;
    Ticker ticker;
    ticker.attach(periodicCallback, 1);

    DEBUG("Initialising the nRF51822\n\r");
    ble.init();
    ble.onDisconnection(disconnectionCallback);
    ble.onConnection(onConnectionCallback);

    ble.getPreferredConnectionParams(&connectionParams);

    /* setup advertising */
    ble.accumulateAdvertisingPayload(GapAdvertisingData::BREDR_NOT_SUPPORTED | GapAdvertisingData::LE_GENERAL_DISCOVERABLE);
    ble.accumulateAdvertisingPayload(GapAdvertisingData::COMPLETE_LIST_16BIT_SERVICE_IDS, (uint8_t*)uuid16_list, sizeof(uuid16_list));
    ble.accumulateAdvertisingPayload(GapAdvertisingData::HEART_RATE_SENSOR_HEART_RATE_BELT);
    ble.accumulateAdvertisingPayload(GapAdvertisingData::COMPLETE_LOCAL_NAME, (uint8_t *)DEVICE_NAME, sizeof(DEVICE_NAME));
    ble.setAdvertisingType(GapAdvertisingParams::ADV_CONNECTABLE_UNDIRECTED);
    ble.setAdvertisingInterval(160); /* 100ms; in multiples of 0.625ms. */
    ble.startAdvertising();

    ble.addService(hrmService);

    while (true) {
        if (triggerSensorPolling) {
            triggerSensorPolling = false;

            /* Do blocking calls or whatever is necessary for sensor polling. */
            /* In our case, we simply update the dummy HRM measurement. */
            hrmCounter++;
            if (hrmCounter == 175) {
                hrmCounter = 100;
            }

            if (ble.getGapState().connected) {
                /* First byte = 8-bit values, no extra info, Second byte = uint8_t HRM value */
                /* See --> https://developer.bluetooth.org/gatt/characteristics/Pages/CharacteristicViewer.aspx?u=org.bluetooth.characteristic.heart_rate_measurement.xml */
                bpm[1] = hrmCounter;
                ble.updateCharacteristicValue(hrmRate.getHandle(), bpm, sizeof(bpm));
            }
        } else {
            ble.waitForEvent();
        }
    }
}
