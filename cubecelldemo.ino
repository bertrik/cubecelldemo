/*******************************************************************************
 * Copyright (c) 2015 Thomas Telkamp and Matthijs Kooijman
 *
 * Permission is hereby granted, free of charge, to anyone
 * obtaining a copy of this document and accompanying files,
 * to do whatever they want with them without any restriction,
 * including, but not limited to, copying, modification and redistribution.
 * NO WARRANTY OF ANY KIND IS PROVIDED.
 *
 * This example sends a valid LoRaWAN packet with payload "Hello,
 * world!", using frequency and encryption settings matching those of
 * the The Things Network.
 *
 * This uses OTAA (Over-the-air activation), where where a DevEUI and
 * application key is configured, which are used in an over-the-air
 * activation procedure where a DevAddr and session keys are
 * assigned/generated for use with all further communication.
 *
 * Note: LoRaWAN per sub-band duty-cycle limitation is enforced (1% in
 * g1, 0.1% in g2), but not the TTN fair usage policy (which is probably
 * violated by this sketch when left running for longer)!

 * To use this sketch, first register your application and device with
 * the things network, to set or generate an AppEUI, DevEUI and AppKey.
 * Multiple devices can use the same AppEUI, but each device has its own
 * DevEUI and AppKey.
 *
 * Do not forget to define the radio type correctly in config.h.
 *
 *******************************************************************************/

#include <basicmac.h>
#include <hal/hal.h>
#include <SPI.h>

#include "board-config.h"

static const u1_t PROGMEM APPEUI[8]= { 0x70, 0xB3, 0xD5, 0x7E, 0xD0, 0x04, 0x19, 0x80 };
void os_getJoinEui (u1_t* buf)
{
    for (int i = 0; i < 8; i++) {
        buf[i] = APPEUI[7 - i];
    }
}

void os_getDevEui (u1_t* buf)
{
    uint64_t chipid = getID();
    for (int i = 0; i < 8; i++) {
        buf[i] = chipid & 0xFF;
        chipid >>= 8;
    }
}

// This key should be in big endian format (or, since it is not really a
// number but a block of memory, endianness does not really apply). In
// practice, a key taken from ttnctl can be copied as-is.
// The key shown here is the semtech default key.
static const u1_t PROGMEM APPKEY[16] =
    { 0x47, 0x02, 0xC1, 0x4F, 0x19, 0xE1, 0x7D, 0x67, 0xE2, 0xC5, 0xD7, 0x87, 0xA9, 0x7D, 0xF0, 0xE6 };
void os_getNwkKey (u1_t* buf)
{
    memcpy_P(buf, APPKEY, 16);
}

// The region to use, this just uses the first one (can be changed if
// multiple regions are enabled).
u1_t os_getRegion (void) { return LMIC_regionCode(0); }

// Schedule TX every this many milliseconds (might become longer due to duty
// cycle limitations).
static const unsigned TX_INTERVAL = 60000;

// Timestamp of last packet sent
static uint32_t last_packet = 0;

const lmic_pinmap lmic_pins = {
    .nss = RADIO_NSS,
    .tx = LMIC_CONTROLLED_BY_DIO2,
    .rx = LMIC_UNUSED_PIN,
    .rst = RADIO_RESET,
    .dio = {LMIC_UNUSED_PIN, RADIO_DIO_1, LMIC_UNUSED_PIN},
    .busy = RADIO_BUSY,
    .tcxo = LMIC_CONTROLLED_BY_DIO3,
};

void onLmicEvent (ev_t ev) {
    Serial.print(os_getTime());
    Serial.print(": ");
    switch(ev) {
        case EV_SCAN_TIMEOUT:
            Serial.println(F("EV_SCAN_TIMEOUT"));
            break;
        case EV_BEACON_FOUND:
            Serial.println(F("EV_BEACON_FOUND"));
            break;
        case EV_BEACON_MISSED:
            Serial.println(F("EV_BEACON_MISSED"));
            break;
        case EV_BEACON_TRACKED:
            Serial.println(F("EV_BEACON_TRACKED"));
            break;
        case EV_JOINING:
            Serial.println(F("EV_JOINING"));
            break;
        case EV_JOINED:
            Serial.println(F("EV_JOINED"));
            break;
        case EV_RFU1:
            Serial.println(F("EV_RFU1"));
            break;
        case EV_JOIN_FAILED:
            Serial.println(F("EV_JOIN_FAILED"));
            break;
        case EV_REJOIN_FAILED:
            Serial.println(F("EV_REJOIN_FAILED"));
            break;
            break;
        case EV_TXCOMPLETE:
            Serial.println(F("EV_TXCOMPLETE (includes waiting for RX windows)"));
            if (LMIC.txrxFlags & TXRX_ACK)
              Serial.println(F("Received ack"));
            if (LMIC.dataLen) {
              Serial.print(F("Received "));
              Serial.print(LMIC.dataLen);
              Serial.println(F(" bytes of payload"));
            }
            break;
        case EV_LOST_TSYNC:
            Serial.println(F("EV_LOST_TSYNC"));
            break;
        case EV_RESET:
            Serial.println(F("EV_RESET"));
            break;
        case EV_RXCOMPLETE:
            // data received in ping slot
            Serial.println(F("EV_RXCOMPLETE"));
            break;
        case EV_LINK_DEAD:
            Serial.println(F("EV_LINK_DEAD"));
            break;
        case EV_LINK_ALIVE:
            Serial.println(F("EV_LINK_ALIVE"));
            break;
        case EV_SCAN_FOUND:
            Serial.println(F("EV_SCAN_FOUND"));
            break;
        case EV_TXSTART:
            Serial.println(F("EV_TXSTART"));
            break;
        case EV_TXDONE:
            Serial.println(F("EV_TXDONE"));
            break;
        case EV_DATARATE:
            Serial.println(F("EV_DATARATE"));
            break;
        case EV_START_SCAN:
            Serial.println(F("EV_START_SCAN"));
            break;
        case EV_ADR_BACKOFF:
            Serial.println(F("EV_ADR_BACKOFF"));
            break;

         default:
            Serial.print(F("Unknown event: "));
            Serial.println(ev);
            break;
    }
}

static void send_packet(){
    // Prepare upstream data transmission at the next possible time.
    uint8_t mydata[] = "Hello, world!";
    LMIC_setTxData2(1, mydata, sizeof(mydata)-1, 0);
    Serial.println(F("Packet queued"));

    last_packet = millis();
}

void setup() {
    Serial.begin(115200);

    // Wait up to 5 seconds for serial to be opened, to allow catching
    // startup messages on native USB boards (that do not reset when
    // serial is opened).
    unsigned long start = millis();
    while (millis() - start < 5000 && !Serial);

    Serial.println();
    Serial.println();
    Serial.println(F("Starting"));
    Serial.println();

    // LMIC init
    os_init(nullptr);
    LMIC_reset();

    // Start join
    LMIC_startJoining();

    // Make sure the first packet is scheduled ASAP after join completes
    last_packet = millis() - TX_INTERVAL;
}

void loop() {
    // Let LMIC handle background tasks
    os_runstep();

    // If TX_INTERVAL passed, *and* our previous packet is not still
    // pending (which can happen due to duty cycle limitations), send
    // the next packet.
    if (millis() - last_packet > TX_INTERVAL && !(LMIC.opmode & (OP_JOINING|OP_TXRXPEND)))
        send_packet();
}

