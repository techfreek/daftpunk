/*********************************************************************
 This is an example for our nRF51822 based Bluefruit LE modules

 Pick one up today in the adafruit shop!

 Adafruit invests time and resources providing this open source code,
 please support Adafruit and open-source hardware by purchasing
 products from Adafruit!

 MIT license, check LICENSE for more information
 All text above, and the splash screen below must be included in
 any redistribution
*********************************************************************/
#include <string.h>
#include <Arduino.h>
#include <SPI.h>
#if not defined (_VARIANT_ARDUINO_DUE_X_) && not defined (_VARIANT_ARDUINO_ZERO_)
  #include <SoftwareSerial.h>
#endif

#include "Adafruit_BLE.h"
#include "Adafruit_BluefruitLE_SPI.h"
#include "Adafruit_BluefruitLE_UART.h"

#include "BluefruitConfig.h"

#include <Adafruit_NeoPixel.h>
#include <Adafruit_GFX.h>
#include <Adafruit_NeoMatrix.h>

// Adafruit config
/*
FACTORYRESET_ENABLE       Perform a factory reset when running this sketch
   
                              Enabling this will put your Bluefruit LE module
                          in a 'known good' state and clear any config
                          data set in previous sketches or projects, so
                              running this at least once is a good idea.
   
                              When deploying your project, however, you will
                          want to disable factory reset by setting this
                          value to 0.  If you are making changes to your
                              Bluefruit LE device via AT commands, and those
                          changes aren't persisting across resets, this
                          is the reason why.  Factory reset will erase
                          the non-volatile memory where config data is
                          stored, setting it back to factory default
                          values.
       
                              Some sketches that require you to bond to a
                          central device (HID mouse, keyboard, etc.)
                          won't work at all with this feature enabled
                          since the factory reset will clear all of the
                          bonding data stored on the chip, meaning the
                          central device won't be able to reconnect.
-----------------------------------------------------------------------*/
#define FACTORYRESET_ENABLE    0
#define PIN                    6
#define WIDTH                  15
#define HEIGHT                 8
#define PIXELS                 (WIDTH * HEIGHT)

// General speeds
#define BRIGHTNESS              10
#define UINT16_MAX              65535

// other settings
#define SCANNER_SPEED           60
#define TEXT_SPEED             200
#define EYE_SPEED              250

// order of these matches the order returned through the app
enum ble_key {
    KEY_NONE = 0,
    KEY_1,
    KEY_2,
    KEY_3,
    KEY_4,
    KEY_UP,
    KEY_DOWN,
    KEY_LEFT,
    KEY_RIGHT,
};

Adafruit_NeoMatrix matrix = Adafruit_NeoMatrix(WIDTH, HEIGHT, PIN,
                            NEO_MATRIX_TOP + NEO_MATRIX_LEFT +
                            NEO_MATRIX_ROWS + NEO_MATRIX_ZIGZAG,
                            NEO_GRB            + NEO_KHZ800);

// predefined colors
int RED = matrix.Color(255, 0, 0);
int GREEN = matrix.Color(0, 255, 0);
int BLUE = matrix.Color(0, 0, 255);
int WHITE = matrix.Color(255, 255, 255);
int OFF = matrix.Color(0, 0, 0);

/* ...hardware SPI, using SCK/MOSI/MISO hardware SPI pins and then user selected CS/IRQ/RST */
Adafruit_BluefruitLE_SPI ble(BLUEFRUIT_SPI_CS, BLUEFRUIT_SPI_IRQ, BLUEFRUIT_SPI_RST);

// function prototypes over in packetparser.cpp
uint8_t readPacket(Adafruit_BLE *ble, uint16_t timeout);
float parsefloat(uint8_t *buffer);
void printHex(const uint8_t * data, const uint32_t numBytes);

// the packet buffer
extern uint8_t packetbuffer[];

// last pressed key
ble_key gkey = KEY_NONE;
ble_key gdir = KEY_NONE;

// A small helper
void error(String err) {
    Serial.println(err);
    while (1);
}

String daftPunkStrs[] = {
    "technologic",
    "harder",
    "faster",
    "stronger",
    "better",
    "Around the World",
    "One More Time",
    "Daft Punk",
    "We're up all night to get lucky",
    "Drag and drop it, zip, unzip it",
    "Don't stop the dancing",
    "Television, rules the nation",
    "Celebration"
};

String partyStrs[] = {
    "beer me",
    "hey",
    "spooky",
    "spooky scary skeletons",
    "boo!",
    "Lose yourself to dance",
    "Music's got me feeling so free"
    "Go cougs!"
};

enum daft_string {
    DAFT_PUNK,
    PARTY
};

// Input a value 0 to 255 to get a color value.
// The colours are a transition r - g - b - back to r.
uint32_t Wheel(byte WheelPos) {
    WheelPos = 255 - WheelPos;
    if(WheelPos < 85) {
        return matrix.Color(255 - WheelPos * 3, 0, WheelPos * 3);
    }
    if(WheelPos < 170) {
        WheelPos -= 85;
        return matrix.Color(0, WheelPos * 3, 255 - WheelPos * 3);
    }
    WheelPos -= 170;
    return matrix.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
}

void rainbow() {
    uint16_t i, j, pixel = 0;

    matrix.fillScreen(OFF);
    matrix.show();

    for(j=1; j<120; j+=5) {
        for(i=0; i<PIXELS; i++, pixel++) {
            matrix.drawPixel((pixel % WIDTH) , (pixel / WIDTH) % HEIGHT, Wheel(((i * 2) + j) % 255));
            matrix.show();
        }
    }
}

void scanner() {
    int pos = 0;
    matrix.fillScreen(0);
    for(pos = 0; pos < matrix.width(); pos++) {
        matrix.fillScreen(OFF);
        matrix.drawLine(pos, 0, pos, HEIGHT, RED);
        matrix.show();
        delay(SCANNER_SPEED);
    }
    for(pos = (matrix.width() - 1); pos >= 0; pos--) {
        matrix.fillScreen(OFF);
        matrix.drawLine(pos, 0, pos, HEIGHT, RED);
        matrix.show();
        delay(SCANNER_SPEED);
    }
    matrix.fillScreen(0);
    matrix.show();
}

void scrollText(String text) {
    int x = matrix.width();

    // Account for 6 pixel wide characters plus a space
    int length = (text.length() * 6) + matrix.width();

    matrix.setCursor(x, 0);
    matrix.print(text);
    matrix.show();

    while (x > (matrix.width() - length)) {
        matrix.fillScreen(OFF);
        matrix.setCursor(--x, 0);
        matrix.print(text);
        matrix.show();
        delay(TEXT_SPEED);
    }
}

void showStrings(int daft) {
    uint8_t length = 0;
    uint8_t index = 0;
    String text;

    switch(daft) {
        case DAFT_PUNK:
            length = sizeof(daftPunkStrs) / sizeof(daftPunkStrs[0]);
            index = random(0, length);
            text = daftPunkStrs[index];
            break;
        case PARTY:
            length = sizeof(partyStrs) / sizeof(partyStrs[0]);
            index = random(0, length);
            text = partyStrs[index];
            break;
    }

    scrollText(text);
    delay(2000);
}

void eye(int x, bool wink) {
    int height = 5;
    int y = 0;

    if (!wink) {
        // top line
        matrix.drawLine(x + 2, y, x + 4, y, RED);
        // left line
        matrix.drawLine(x + 1, y + 1, x + 1 , y + 5, RED);
        // right line
        matrix.drawLine(x + 5, y + 1, x + 5 , y + 5, RED);
        // cornea
        matrix.drawPixel(x + 3, y + 3, RED);
    } else {
        // left line
        matrix.drawLine(x + 1, y + 4, x + 1 , y + 5, RED);
        // right line
        matrix.drawLine(x + 5, y + 4, x + 5 , y + 5, RED);
    }
    // bottom line
    matrix.drawLine(x + 2, y + 6, x + 4, y + 6, RED);
}

void eyes(int dir) {
    int i = 0;
    int pos = 0;
    static int x_offset = 0;
    int blink = 0;

    matrix.fillScreen(0);

    switch(dir) {
        case KEY_UP:
            blink = 1;
            break;
        case KEY_DOWN:
            blink = 2;
            break;
        case KEY_LEFT:
            x_offset = max(x_offset - 1, -1);
            break;
        case KEY_RIGHT:
            x_offset = min(x_offset + 1, 1);
            break;
        case KEY_NONE:
        default:
            break;
    }

    if (blink == 0) {
        eye(0 + x_offset, false);
        eye((matrix.width() / 2) + x_offset + 1, false);
    } else if (blink == 1) {
        // wink
        eye(0 + x_offset, false);
        eye((matrix.width() / 2) + x_offset + 1, true);
    } else {
        // blink
        eye(0 + x_offset, true);
        eye((matrix.width() / 2) + x_offset + 1, true);
    }
    // clear the direction
    gdir = KEY_NONE;
    matrix.show();
    delay(random(1, 4) * EYE_SPEED);
}

void setup() {
    randomSeed(analogRead(0));

    matrix.begin();
    matrix.setBrightness(BRIGHTNESS);
    matrix.setTextColor(RED);
    matrix.setTextWrap(false);
    matrix.setCursor(0, 0);
    scanner();

    Serial.begin(115200);
    Serial.println("Initializing");

    if(!ble.begin(VERBOSE_MODE)) {
        error(F("Couldn't find Bluefruit"));
    }

    if (FACTORYRESET_ENABLE) {
        /* Perform a factory reset to make sure everything is in a known state */
        Serial.println("Performing a factory reset: ");
        if(!ble.factoryReset()) {
            error("Couldn't factory reset");
        }
    }

    /* Disable command echo from Bluefruit */
    ble.echo(false);

    /* Wait for connection */
    while(!ble.isConnected()) {
        scanner();
    }

    ble.setMode(BLUEFRUIT_MODE_DATA);
}

void loop() {
    uint8_t buttnum = 0;
    boolean pressed = false;

    if (ble.available()) {
        uint8_t len = readPacket(&ble, BLE_READPACKET_TIMEOUT);
        if (len == 0) return;

        switch(packetbuffer[1]) {
            case 'B':
                //buttons
                buttnum = packetbuffer[2] - '0';
                pressed = packetbuffer[3] - '0';

                if (buttnum >= KEY_UP) {
                  gdir = (int)buttnum;
                } else {
                  gkey = (int)buttnum;
                  gdir = KEY_NONE;
                }
                break;
            case 'C':
                // color, fallthrough
            default:
                return;
        }
    }

    Serial.println(String("Running command ") + String(gkey) + String(", dir ") + String(gdir));
    switch (gkey) {
        case KEY_1:
            eyes(gdir);
            break;
        case KEY_2:
            showStrings(DAFT_PUNK);
            break;
        case KEY_3:
            showStrings(PARTY);
            break;
        case KEY_4:
            //colorWipe(WHITE, 10);
            rainbow();
            delay(500);
            break;
        default:
            scanner();
            break;
    }
}
