#include "MicroBit.h"
MicroBit uBit;
uint8_t tx_power_level = 6;

#define MANUFACTURER_TESTING_LO (0xFF)
#define MANUFACTURER_TESTING_HI (0xFF)

#define SIGNATURE_LO   (0x39)
#define SIGNATURE_HI   (0xA8)

#define RELEASE_NUMBER (1)  // limit 255
#define API_ID         (3)  // limit 255

#define FLAGS_INTENDED_RECEIVER_ASSIGNED          (0x08)
#define FLAGS_TOGGLE_COUNTER_HAS_WRAPPED          (0x04)
#define FLAGS_BUTTON_A                            (0x02)
#define FLAGS_BUTTON_B                            (0x01)
#define FLAGS_NONE                                (0x00)

struct
{
    uint8_t manufacturer[2];
    uint8_t signature[2];
    uint8_t apiId;
    uint8_t releaseNumber;
    uint8_t intendedReceiverBluetoothMac[6];
    uint8_t flags;
    uint8_t lowerEightBitsToggleCounter;
    uint8_t upperEightBitsToggleCounter;
    uint8_t toggleHistory[11];
} BleAdvPacketApi = {MANUFACTURER_TESTING_LO, MANUFACTURER_TESTING_HI,
                     SIGNATURE_LO, SIGNATURE_HI,
                     API_ID,
                     RELEASE_NUMBER,
                     0x00, 0x00, 0x00, 0x00, 0x00, 0x00,     // intended receiver bluetooth mac
                     FLAGS_NONE,
                     0,                                      // toggle counter
                     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};       // toggle history

uint8_t buttonsState = FLAGS_NONE;  // uses FLAGS_BUTTON_A and FLAGS_BUTTON_B
uint16_t toggleCounter = 0;
uint8_t toggleCounterHasWrapped = 0;

char digit(uint8_t n)
{
    return '0' + n;
}

void fail(ManagedString message)
{
    uBit.display.scroll("FAIL " + message, 200);
}

void startAdvertising()
{
    int connectable = 0;
    int interval = 160;
    uBit.bleManager.setTransmitPower(tx_power_level);
    uBit.bleManager.ble->setAdvertisingType(connectable ? GapAdvertisingParams::ADV_CONNECTABLE_UNDIRECTED : GapAdvertisingParams::ADV_NON_CONNECTABLE_UNDIRECTED);
    uBit.bleManager.ble->setAdvertisingInterval(interval);
    uBit.bleManager.ble->clearAdvertisingPayload();
    uBit.bleManager.ble->accumulateAdvertisingPayload(GapAdvertisingData::BREDR_NOT_SUPPORTED | GapAdvertisingData::LE_GENERAL_DISCOVERABLE);
    uBit.bleManager.ble->accumulateAdvertisingPayload(GapAdvertisingData::MANUFACTURER_SPECIFIC_DATA, (uint8_t*) &BleAdvPacketApi, sizeof(BleAdvPacketApi));
    uBit.bleManager.ble->startAdvertising();
    if (sizeof(BleAdvPacketApi) == 26)
        uBit.display.scrollAsync("ULTIBO BUTTONS V1", 90);
    else
        fail("ULTIBO PACK");
}

char text[] = "?";

void onButton(MicroBitEvent e)
{
    uint8_t toggleHistoryWhichButton = 0;
    uint8_t mask = 0;
    uint8_t previousButtonsState = buttonsState;
    if (e.source == MICROBIT_ID_BUTTON_A)
    {
        mask = FLAGS_BUTTON_A;
        toggleHistoryWhichButton = 0;
    }
    else if (e.source == MICROBIT_ID_BUTTON_B)
    {
        mask = FLAGS_BUTTON_B;
        toggleHistoryWhichButton = 1;
    }

    if (e.value == MICROBIT_BUTTON_EVT_DOWN)
        buttonsState |= mask;
    else if (e.value == MICROBIT_BUTTON_EVT_UP)
        buttonsState &= ~mask;

    if (buttonsState != previousButtonsState)
    {
        uBit.bleManager.ble->clearAdvertisingPayload();
        uBit.bleManager.ble->accumulateAdvertisingPayload(GapAdvertisingData::BREDR_NOT_SUPPORTED | GapAdvertisingData::LE_GENERAL_DISCOVERABLE);

        toggleCounter = toggleCounter + 1;
        toggleCounterHasWrapped |= toggleCounter == 0;

        BleAdvPacketApi.flags = buttonsState;
        BleAdvPacketApi.lowerEightBitsToggleCounter = toggleCounter & 0xFF;
        BleAdvPacketApi.upperEightBitsToggleCounter = (toggleCounter >> 8) & 0xFF;
        if (toggleCounterHasWrapped)
        {
            BleAdvPacketApi.flags |= FLAGS_TOGGLE_COUNTER_HAS_WRAPPED;
        }

        for (int i = sizeof(BleAdvPacketApi.toggleHistory); i >= 1; i--)
        {
            BleAdvPacketApi.toggleHistory[i] = ((BleAdvPacketApi.toggleHistory[i] >> 1) & 0x7f) | ((BleAdvPacketApi.toggleHistory[i - 1] & 0x01) << 7);
        }
        BleAdvPacketApi.toggleHistory[0] = ((BleAdvPacketApi.toggleHistory[0] >> 1) & 0x7f) | (toggleHistoryWhichButton << 7);

        uBit.bleManager.ble->accumulateAdvertisingPayload(GapAdvertisingData::MANUFACTURER_SPECIFIC_DATA, (uint8_t*) &BleAdvPacketApi, sizeof(BleAdvPacketApi));
	if (buttonsState == FLAGS_NONE)
	    text[0] = ' ';
	else if (buttonsState == FLAGS_BUTTON_A)
	    text[0] = 'A';
	else if (buttonsState == FLAGS_BUTTON_B)
	    text[0] = 'B';
	else if (buttonsState == (FLAGS_BUTTON_A | FLAGS_BUTTON_B))
	    text[0] = '2';
        else
            text[0] = '?';
        uBit.display.printAsync(text);
    }
}

int main()
{
    uBit.init();
    uBit.messageBus.listen(MICROBIT_ID_BUTTON_A, MICROBIT_EVT_ANY, onButton);
    uBit.messageBus.listen(MICROBIT_ID_BUTTON_B, MICROBIT_EVT_ANY, onButton);
    startAdvertising();
    release_fiber();
}
