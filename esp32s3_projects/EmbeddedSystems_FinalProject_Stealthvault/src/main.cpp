#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <esp_task_wdt.h>
#include "mbedtls/aes.h"
#include "mbedtls/md.h"
#include "mbedtls/pkcs5.h"

// --- הגדרות חומרה ---
#define BTN_BOTTOM_PIN 0
#define BTN_TOP_PIN 14
#define TFT_POWER_PIN 15
#define TFT_BACKLIGHT_PIN 38

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite countSpr = TFT_eSprite(&tft);

// --- מצבי המערכת ---
enum SystemState {
    STATE_CAMOUFLAGE,
    STATE_AUTH,
    STATE_VAULT,
    STATE_CLEANUP
};
SystemState currentState = STATE_CAMOUFLAGE;

// --- משתני הסוואה ---
long clickCounter = 0;
int resetHistory[4] = {0, 0, 0, 0};
uint32_t lastDrawTime = 0;
bool needsUpdate = true;

// --- מערכי הקלדה ---
const char* charsets[4] = {
    "abcdefghijklmnopqrstuvwxyz",
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ",
    "0123456789",
    "!@#$%^&*()"
};
const char* setNames[4] = { "[ a-z ]", "[ A-Z ]", "[ 0-9 ]", "[ !@# ]" };
int currentSet = 0;

// משתני סיסמא
char enteredPassword[11];
int passIndex = 0;
int charIndex = 0;

// משתני כספת
char* vault_ram_buffer = NULL;
size_t vault_buffer_size = 0;
int scrollOffset = 0;

int getSetLength(int set) {
    if (set == 0 || set == 1) return 26;
    return 10;
}

// --- מחלקת כפתורים ---
struct ButtonFSM {
    uint8_t pin;
    bool state = true;
    bool lastState = true;
    uint32_t pressTime = 0;
    uint32_t releaseTime = 0;
    uint32_t lastRepeat = 0;

    bool justPressed = false;
    bool waitingDoubleClick = false;
    bool longPressFired = false;
    uint8_t clickCount = 0;

    enum Event { NONE, SHORT_CLICK, DOUBLE_CLICK, LONG_PRESS, HOLD_REPEAT, SHORT_HOLD_REPEAT };
    Event event = NONE;

    void init(uint8_t p) {
        pin = p;
        pinMode(pin, INPUT_PULLUP);
    }

    void update() {
        event = NONE;
        justPressed = false;
        bool current = (digitalRead(pin) == LOW);
        uint32_t now = millis();

        if (current && !lastState) {
            justPressed = true;
            pressTime = now;
            lastRepeat = now;
            longPressFired = false;
        }
        else if (!current && lastState) {
            releaseTime = now;
            if (!longPressFired) {
                clickCount++;
                waitingDoubleClick = true;
            } else {
                clickCount = 0;
                waitingDoubleClick = false;
            }
        }

        if (current) {
            uint32_t holdTime = now - pressTime;
            if (holdTime > 500) {
                if (!longPressFired) {
                    event = LONG_PRESS;
                    longPressFired = true;
                    lastRepeat = now;
                } else {
                    if (now - lastRepeat > 200) {
                        if (clickCount == 1) {
                            event = SHORT_HOLD_REPEAT;
                        } else {
                            event = HOLD_REPEAT;
                        }
                        lastRepeat = now;
                    }
                }
            }
        }

        if (!current && waitingDoubleClick && (now - releaseTime > 250)) {
            if (clickCount == 1) {
                event = SHORT_CLICK;
            } else if (clickCount >= 2) {
                event = DOUBLE_CLICK;
            }
            waitingDoubleClick = false;
            clickCount = 0;
        }

        lastState = current;
        state = current;
    }
};

ButtonFSM btnBottom;
ButtonFSM btnTop;

// --- פונקציות תצוגה ---

void drawDecoyStatic() {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE);
    int centerX = 140;

    tft.setTextDatum(MC_DATUM);
    tft.drawString("Clicks-Counter", centerX, 30, 4);
    tft.drawString("Embedded systems Final Project", centerX, 60, 2);

    tft.setTextDatum(MR_DATUM);
    tft.drawString("Top: Reset", tft.width() - 5, 30, 2);
    tft.drawString("Btm: Count", tft.width() - 5, 150, 2);
}

void updateCounterDisplay() {
    countSpr.fillSprite(TFT_BLACK);
    countSpr.setTextColor(TFT_GREEN, TFT_BLACK);
    countSpr.setTextDatum(MC_DATUM);
    char countStr[20];
    sprintf(countStr, "%ld", clickCounter);
    countSpr.drawString(countStr, countSpr.width() / 2, countSpr.height() / 2, 6);
    countSpr.pushSprite(40, 80);
}

void drawAuthScreen() {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE);
    tft.setTextDatum(MC_DATUM);
    int centerX = 140;

    tft.drawString("Vault Authentication", centerX, 20, 2);

    tft.setTextColor(TFT_CYAN);
    tft.drawString(setNames[currentSet], centerX, 55, 4);

    String displayStr = "";
    for (int i = 0; i < 10; i++) {
        if (i < passIndex) {
            displayStr += "* ";
        } else if (i == passIndex) {
            displayStr += String(charsets[currentSet][charIndex]) + " ";
        } else {
            displayStr += "_ ";
        }
    }

    tft.setTextColor(TFT_WHITE);
    tft.drawString(displayStr, centerX, 100, 4);

    tft.drawString("Top: 1=Set, 2=Del, Lng=Ent", centerX, 160, 2);
    tft.drawString("Btm: 1=Nxt, 2=Prv, Hld=Fast", centerX, 140, 2);

}

void drawVaultScreen() {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_GREEN);
    tft.setTextDatum(TL_DATUM);
    tft.drawString("--- SECURE VAULT ---", 5, 5, 2);

    tft.setTextColor(TFT_WHITE);
    int y = 25;

    char* temp_ram = strdup(vault_ram_buffer);
    if (!temp_ram) return;
    char* token = strtok(temp_ram, "\n");
    int lineCount = 0;

    while (token != NULL) {
        if (lineCount >= scrollOffset && y < tft.height() - 25) {
            tft.drawString(token, 5, y, 2);
            y += 20;
        }
        token = strtok(NULL, "\n");
        lineCount++;
    }
    free(temp_ram);

    tft.setTextColor(TFT_YELLOW);
    tft.setTextDatum(MC_DATUM);

    // עדכון כיתוב ההנחיות בתחתית המסך
    tft.drawString("Btm: Dn | Top: Up | Hold Both: Wipe", 140, tft.height() - 10, 2);
}

// --- לוגיקת הצפנה ואבטחה ---
bool decryptVault() {
    fs::File f = LittleFS.open("/vault.enc", "r");
    if (!f) return false;

    size_t file_size = f.size();
    if (file_size <= 16) { f.close(); return false; }

    uint8_t* file_buffer = (uint8_t*)malloc(file_size);
    if (!file_buffer) { f.close(); return false; }

    f.read(file_buffer, file_size);
    f.close();

    unsigned char derived_aes_key[32];
    const unsigned char salt[] = "StealthVaultSalt";
    mbedtls_md_context_t md_ctx;

    mbedtls_md_init(&md_ctx);
    mbedtls_md_setup(&md_ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 1);

    mbedtls_pkcs5_pbkdf2_hmac(&md_ctx, (const unsigned char*)enteredPassword, 10,
                              salt, strlen((char*)salt), 50000, 32, derived_aes_key);
    mbedtls_md_free(&md_ctx);

    unsigned char iv[16];
    memcpy(iv, file_buffer, 16);
    size_t ciphertext_len = file_size - 16;
    unsigned char *ciphertext = file_buffer + 16;

    vault_buffer_size = ciphertext_len + 1;
    vault_ram_buffer = (char*)malloc(vault_buffer_size);
    if (!vault_ram_buffer) {
        memset(derived_aes_key, 0, sizeof(derived_aes_key));
        free(file_buffer);
        return false;
    }

    mbedtls_aes_context aes_ctx;
    mbedtls_aes_init(&aes_ctx);
    mbedtls_aes_setkey_dec(&aes_ctx, derived_aes_key, 256);
    mbedtls_aes_crypt_cbc(&aes_ctx, MBEDTLS_AES_DECRYPT, ciphertext_len, iv, ciphertext, (unsigned char*)vault_ram_buffer);
    mbedtls_aes_free(&aes_ctx);

    int pad_len = vault_ram_buffer[ciphertext_len - 1];
    if(pad_len > 0 && pad_len <= 16) {
         vault_ram_buffer[ciphertext_len - pad_len] = '\0';
    } else {
         vault_ram_buffer[ciphertext_len] = '\0';
    }

    memset(derived_aes_key, 0, sizeof(derived_aes_key));
    free(file_buffer);
    return true;
}

// --- אתחול המערכת ---
void setup() {
    Serial.begin(115200);

    disableLoopWDT();
    disableCore0WDT();
    disableCore1WDT();

    WiFi.mode(WIFI_OFF);

    pinMode(TFT_POWER_PIN, OUTPUT);
    digitalWrite(TFT_POWER_PIN, HIGH);

    pinMode(TFT_BACKLIGHT_PIN, OUTPUT);
    analogWrite(TFT_BACKLIGHT_PIN, 128);

    tft.init();
    tft.setRotation(1);
    countSpr.createSprite(200, 60);

    if(!LittleFS.begin(true)){
        Serial.println("LittleFS Mount Failed");
    } else {
        Serial.println("LittleFS Mounted Successfully");
    }

    btnBottom.init(BTN_BOTTOM_PIN);
    btnTop.init(BTN_TOP_PIN);

    drawDecoyStatic();
    needsUpdate = true;
}

// --- לולאה ראשית ---
void loop() {
    uint32_t now = millis();
    btnBottom.update();
    btnTop.update();

    switch (currentState) {
        case STATE_CAMOUFLAGE: {
            if (btnBottom.justPressed) {
                clickCounter++;
                needsUpdate = true;
            }
            if (btnTop.justPressed) {
                resetHistory[0] = resetHistory[1];
                resetHistory[1] = resetHistory[2];
                resetHistory[2] = resetHistory[3];
                resetHistory[3] = clickCounter;

                clickCounter = 0;
                needsUpdate = true;

                if (resetHistory[0] == 1 && resetHistory[1] == 3 &&
                    resetHistory[2] == 3 && resetHistory[3] == 7) {

                    memset(resetHistory, 0, sizeof(resetHistory));
                    passIndex = 0;
                    charIndex = 0;
                    currentSet = 0;
                    memset(enteredPassword, 0, sizeof(enteredPassword));

                    currentState = STATE_AUTH;
                    needsUpdate = true;
                }
            }
            break;
        }

        case STATE_AUTH: {
            if (btnBottom.event == ButtonFSM::SHORT_CLICK) {
                charIndex = (charIndex + 1) % getSetLength(currentSet);
                needsUpdate = true;
            }
            else if (btnBottom.event == ButtonFSM::DOUBLE_CLICK) {
                charIndex = (charIndex == 0) ? (getSetLength(currentSet) - 1) : (charIndex - 1);
                needsUpdate = true;
            }
            else if (btnBottom.event == ButtonFSM::HOLD_REPEAT) {
                charIndex = (charIndex + 1) % getSetLength(currentSet);
                needsUpdate = true;
            }
            else if (btnBottom.event == ButtonFSM::SHORT_HOLD_REPEAT) {
                charIndex = (charIndex == 0) ? (getSetLength(currentSet) - 1) : (charIndex - 1);
                needsUpdate = true;
            }

            if (btnTop.event == ButtonFSM::SHORT_CLICK) {
                currentSet = (currentSet + 1) % 4;
                charIndex = 0;
                needsUpdate = true;
            }
            else if (btnTop.event == ButtonFSM::DOUBLE_CLICK) {
                if (passIndex > 0) {
                    passIndex--;
                    enteredPassword[passIndex] = '\0';
                    needsUpdate = true;
                }
            }
            else if (btnTop.event == ButtonFSM::LONG_PRESS) {
                if (passIndex < 9) {
                    enteredPassword[passIndex] = charsets[currentSet][charIndex];
                    passIndex++;
                    needsUpdate = true;
                }
                else if (passIndex == 9) {
                    enteredPassword[passIndex] = charsets[currentSet][charIndex];
                    enteredPassword[10] = '\0';

                    tft.fillScreen(TFT_BLACK);
                    tft.setTextColor(TFT_YELLOW);
                    tft.setTextDatum(MC_DATUM);
                    tft.drawString("Authenticating...", 140, tft.height()/2, 4);

                    if (decryptVault()) {
                        scrollOffset = 0;
                        currentState = STATE_VAULT;
                    } else {
                        currentState = STATE_CLEANUP;
                    }
                    needsUpdate = true;
                }
            }
            break;
        }

        case STATE_VAULT: {
            // טיימר ייעודי לזיהוי לחיצה ארוכה על שני הכפתורים יחד
            static uint32_t bothHoldStart = 0;
            static bool bothPressed = false;

            if (btnBottom.state && btnTop.state) {
                if (!bothPressed) {
                    bothPressed = true;
                    bothHoldStart = millis();
                } else if (millis() - bothHoldStart > 800) {
                    bothPressed = false;
                    currentState = STATE_CLEANUP;
                    needsUpdate = true;
                }
            } else {
                bothPressed = false;

                // גלילה למטה עם הכפתור התחתון
                if (btnBottom.event == ButtonFSM::SHORT_CLICK || btnBottom.event == ButtonFSM::HOLD_REPEAT) {
                    scrollOffset++;
                    needsUpdate = true;
                }

                // גלילה למעלה עם הכפתור העליון
                if (btnTop.event == ButtonFSM::SHORT_CLICK || btnTop.event == ButtonFSM::HOLD_REPEAT) {
                    if (scrollOffset > 0) scrollOffset--;
                    needsUpdate = true;
                }
            }
            break;
        }

        case STATE_CLEANUP: {
            if (vault_ram_buffer != NULL) {
                memset(vault_ram_buffer, 0, vault_buffer_size);
                free(vault_ram_buffer);
                vault_ram_buffer = NULL;
                vault_buffer_size = 0;
            }
            memset(enteredPassword, 0, sizeof(enteredPassword));

            tft.fillScreen(TFT_BLACK);
            tft.setTextColor(TFT_RED);
            tft.setTextDatum(MC_DATUM);
            tft.drawString("RAM SECURELY WIPED", 140, tft.height()/2, 2);
            delay(1500);

            clickCounter = 0;
            currentState = STATE_CAMOUFLAGE;
            drawDecoyStatic();
            needsUpdate = true;
            break;
        }
    }

    if (needsUpdate && (now - lastDrawTime >= 200)) {
        if (currentState == STATE_CAMOUFLAGE) {
            updateCounterDisplay();
        } else if (currentState == STATE_AUTH) {
            drawAuthScreen();
        } else if (currentState == STATE_VAULT) {
            drawVaultScreen();
        }
        lastDrawTime = now;
        needsUpdate = false;
    }

    delay(10);
}