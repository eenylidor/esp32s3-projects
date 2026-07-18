#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <Preferences.h>
#include <WiFi.h>

/* ====================================================================
 *                       CUSTOMIZATION ZONE
 * ====================================================================
 */

// --- Power Management & Performance ---
#define TFT_BRIGHTNESS 150   // Backlight brightness: 0 to 255 (150 saves huge power, still bright)
#define TARGET_FPS     30    // Frames per second (5 to 60). Lower FPS = more battery life!

// --- Display & Grid Sizing ---
#define SCREEN_W 170
#define SCREEN_H 320
#define CELL       15
#define BOARD_COLS 10
#define BOARD_ROWS 16
#define BOARD_W    (BOARD_COLS * CELL)
#define BOARD_H    (BOARD_ROWS * CELL)
#define BOARD_X    ((SCREEN_W - BOARD_W) / 2)
#define BOARD_Y    50

// --- Gameplay & Difficulty ---
#define PTS_PER_LEVEL    1000
#define COMBO_WINDOW_MS  450
#define PAUSE_HOLD_MS    750

// --- Falling Speeds ---
#define INITIAL_FALL_MS  800
#define SPEED_DECAY_MS   65
#define MIN_FALL_MS      80

// --- Button Drag Mechanics (DAS & ARR) ---
#define DAS_MS  180
#define ARR_MS  30

// --- Colors ---
#define C_BG     0x0000
#define C_BORDER 0xFFFF
#define C_TEXT   0xFFFF
#define C_HL     0x07FF

const uint16_t LEVEL_COLORS[10] = {
    0x07FF, 0xFFE0, 0xF81F, 0x07E0, 0x001F,
    0xF800, 0xFC00, 0xFE19, 0xAFE5, 0x780F
};

/* ====================================================================
 *                        CORE ENGINE
 * ==================================================================== */

#define BTN_LEFT_PIN      0
#define BTN_RIGHT_PIN     14
#define TFT_BACKLIGHT_PIN 38
#define TFT_POWER_PIN     15

#define STATE_MENU       0
#define STATE_PLAY       1
#define STATE_SCORES     2
#define STATE_HOWTOPLAY  3
#define STATE_GAMEOVER   4
#define STATE_COUNTDOWN  5
#define STATE_CREDITS    6

#define FRAME_DELAY_MS   (1000 / TARGET_FPS) // Calculated dynamically

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite spr = TFT_eSprite(&tft);
Preferences prefs;

struct Game {
    uint8_t grid[BOARD_ROWS][BOARD_COLS];
    int piece[4][2];
    int ptype, px, py;
    int next_ptype;
    int score, level;
    int state;
    uint32_t lastFall;
    bool gameInProgress;
    bool pauseHandled;
} g;

int menuSelection = 0;
int topScores[5] = {0, 0, 0, 0, 0};
int countdownValue = 3;
uint32_t countdownTimer = 0;

static const int PIECES[7][4][2] = {
    {{0,-1}, {0,0}, {0,1}, {0,2}},
    {{0,0},  {0,1}, {1,0}, {1,1}},
    {{0,-1}, {0,0}, {0,1}, {1,0}},
    {{1,-1}, {1,0}, {0,0}, {0,1}},
    {{0,-1}, {0,0}, {1,0}, {1,1}},
    {{-1,-1},{0,-1},{0,0}, {0,1}},
    {{0,-1}, {0,0}, {0,1}, {-1,1}}
};

struct Button {
    uint8_t pin = 0;
    bool state = false, lastState = false;
    uint32_t pressTime = 0, lastRepeat = 0;
    bool justPressed = false, repeating = false;

    void init(uint8_t p) { pin = p; pinMode(pin, INPUT_PULLUP); }
    void update() {
        bool current = (digitalRead(pin) == LOW);
        justPressed = false; repeating = false;
        if (current && !lastState) {
            justPressed = true;
            pressTime = millis();
            lastRepeat = millis();
        }
        if (current && lastState) {
            if (millis() - pressTime > DAS_MS) {
                if (millis() - lastRepeat > ARR_MS) {
                    repeating = true;
                    lastRepeat = millis();
                }
            }
        }
        lastState = current;
        state = current;
    }
};

Button btnL, btnR;

void loadScores() {
    prefs.begin("tetris", true);
    for(int i=0; i<5; i++) topScores[i] = prefs.getInt(String(i).c_str(), 0);
    prefs.end();
}

void saveScore(int newScore) {
    if (newScore == 0) return;
    bool isHigh = false;
    for(int i=0; i<5; i++) {
        if (newScore > topScores[i]) {
            for(int j=4; j>i; j--) topScores[j] = topScores[j-1];
            topScores[i] = newScore;
            isHigh = true;
            break;
        }
    }
    if (isHigh) {
        prefs.begin("tetris", false);
        for(int i=0; i<5; i++) prefs.putInt(String(i).c_str(), topScores[i]);
        prefs.end();
    }
}

bool checkCollision(int cx, int cy, int temp_piece[4][2] = nullptr) {
    for(int i=0; i<4; i++) {
        int px = cx + (temp_piece ? temp_piece[i][1] : g.piece[i][1]);
        int py = cy + (temp_piece ? temp_piece[i][0] : g.piece[i][0]);
        if (px < 0 || px >= BOARD_COLS || py >= BOARD_ROWS) return true;
        if (py >= 0 && g.grid[py][px]) return true;
    }
    return false;
}

void spawnPiece() {
    g.ptype = g.next_ptype;
    g.next_ptype = random(7);
    g.px = BOARD_COLS / 2 - 1;
    g.py = 1;
    for (int i=0; i<4; i++) {
        g.piece[i][0] = PIECES[g.ptype][i][0];
        g.piece[i][1] = PIECES[g.ptype][i][1];
    }
    if (checkCollision(g.px, g.py)) {
        g.state = STATE_GAMEOVER;
        g.gameInProgress = false;
    }
}

void rotatePiece(bool clockwise) {
    if (g.ptype == 1) return;
    int temp[4][2];
    for (int i=0; i<4; i++) {
        temp[i][0] = clockwise ? g.piece[i][1] : -g.piece[i][1];
        temp[i][1] = clockwise ? -g.piece[i][0] : g.piece[i][0];
    }
    int kickX = 0;
    if (checkCollision(g.px, g.py, temp)) {
        if (!checkCollision(g.px + 1, g.py, temp)) kickX = 1;
        else if (!checkCollision(g.px - 1, g.py, temp)) kickX = -1;
        else if (g.ptype == 0 && !checkCollision(g.px + 2, g.py, temp)) kickX = 2;
        else if (g.ptype == 0 && !checkCollision(g.px - 2, g.py, temp)) kickX = -2;
        else return;
    }
    g.px += kickX;
    for (int i=0; i<4; i++) {
        g.piece[i][0] = temp[i][0];
        g.piece[i][1] = temp[i][1];
    }
}

void render() {
    spr.fillSprite(C_BG);

    if (g.state == STATE_MENU) {
        spr.setTextColor(0xAFE5);
        spr.setTextSize(3);
        spr.setCursor((int16_t)35, (int16_t)30); spr.print("VIBE");
        spr.setCursor((int16_t)25, (int16_t)65); spr.print("TETRIS");

        const char* mActive[] = {"RESUME", "RESTART", "SCORES", "CONTROLS", "CREDITS"};
        const char* mIdle[]   = {"PLAY GAME", "SCORES", "CONTROLS", "CREDITS"};
        int mCount = g.gameInProgress ? 5 : 4;
        const char** menus = g.gameInProgress ? mActive : mIdle;

        spr.setTextSize(1);
        for(int i=0; i<mCount; i++) {
            int16_t yPos = (int16_t)(120 + i * 30);
            if (menuSelection == i) {
                spr.setTextColor(C_HL);
                spr.setCursor((int16_t)15, yPos);
                spr.print(">> "); spr.print(menus[i]);
            } else {
                spr.setTextColor(C_TEXT);
                spr.setCursor((int16_t)35, yPos);
                spr.print(menus[i]);
            }
        }
        spr.setTextColor(C_TEXT);
        spr.setCursor((int16_t)15, (int16_t)290); spr.print("L: Scroll  |  R: Select");

    } else if (g.state == STATE_SCORES) {
        spr.setTextColor(LEVEL_COLORS[1]);
        spr.setTextSize(2);
        spr.setCursor((int16_t)15, (int16_t)30); spr.print("TOP SCORES");
        spr.setTextSize(1);
        spr.setTextColor(C_TEXT);
        for(int i=0; i<5; i++) {
            spr.setCursor((int16_t)30, (int16_t)(90 + i*30));
            spr.print(i+1); spr.print(". "); spr.print(topScores[i]);
        }
        spr.setTextColor(C_HL);
        spr.setCursor((int16_t)20, (int16_t)270); spr.print("Press Any Button...");

    } else if (g.state == STATE_HOWTOPLAY) {
        spr.setTextColor(C_HL);
        spr.setTextSize(2);
        spr.setCursor((int16_t)10, (int16_t)20); spr.print("CONTROLS");
        spr.setTextColor(C_TEXT);
        spr.setTextSize(1);
        spr.setCursor((int16_t)10, (int16_t)70);  spr.println("- Click L/R: Move");
        spr.setCursor((int16_t)10, (int16_t)110); spr.println("- Hold L/R: Fast Drag");
        spr.setCursor((int16_t)10, (int16_t)150); spr.println("- L then R: Rot. Left");
        spr.setCursor((int16_t)10, (int16_t)190); spr.println("- R then L: Rot. Right");
        spr.setCursor((int16_t)10, (int16_t)230); spr.println("- Hold BOTH: Pause");
        spr.setTextColor(0xAFE5);
        spr.setCursor((int16_t)10, (int16_t)280); spr.print("Press Any Button...");

    } else if (g.state == STATE_CREDITS) {
        spr.setTextColor(0xFE19);
        spr.setTextSize(2);
        spr.setCursor((int16_t)25, (int16_t)120);
        spr.print("Github:");
        spr.setCursor((int16_t)25, (int16_t)160);
        spr.print("eenylidor");


        static float bx = 10, by = 50;
        static float bvx = 1.2, bvy = 1.2;
        bx += bvx; by += bvy;
        if (bx <= 0 || bx >= SCREEN_W - 108) bvx = -bvx;
        if (by <= 0 || by >= SCREEN_H - 10) bvy = -bvy;

        spr.setTextColor(C_HL);
        spr.setTextSize(1);
        spr.setCursor((int16_t)bx, (int16_t)by);
        spr.print("Vibe coding Tetris");

        spr.setTextColor(C_TEXT);
        spr.setCursor((int16_t)20, (int16_t)280);
        spr.print("Press Any Button...");

    } else if (g.state == STATE_GAMEOVER) {
        spr.setTextColor(0xF800);
        spr.setTextSize(3);
        spr.setCursor((int16_t)10, (int16_t)100); spr.print("GAME OVER");
        spr.setTextColor(C_TEXT);
        spr.setTextSize(2);
        spr.setCursor((int16_t)20, (int16_t)160); spr.print("Score: "); spr.print(g.score);
        spr.setTextSize(1);
        spr.setCursor((int16_t)30, (int16_t)240); spr.print("Click to Return");

    } else if (g.state == STATE_PLAY || g.state == STATE_COUNTDOWN) {
        spr.drawRect((int32_t)(BOARD_X - 1), (int32_t)(BOARD_Y - 1), (int32_t)(BOARD_W + 2), (int32_t)(BOARD_H + 2), C_BORDER);

        for (int y=0; y<BOARD_ROWS; y++) {
            for (int x=0; x<BOARD_COLS; x++) {
                if (g.grid[y][x]) {
                    uint16_t c = LEVEL_COLORS[(g.grid[y][x] - 1) % 10];
                    int32_t drawX = (int32_t)(BOARD_X + x * CELL);
                    int32_t drawY = (int32_t)(BOARD_Y + y * CELL);
                    spr.fillRect(drawX, drawY, (int32_t)CELL, (int32_t)CELL, c);
                    spr.drawRect(drawX, drawY, (int32_t)CELL, (int32_t)CELL, C_BG);
                }
            }
        }

        uint16_t activeColor = LEVEL_COLORS[g.level % 10];
        for (int i=0; i<4; i++) {
            int32_t dx = (int32_t)(BOARD_X + (g.px + g.piece[i][1]) * CELL);
            int32_t dy = (int32_t)(BOARD_Y + (g.py + g.piece[i][0]) * CELL);
            if (dy >= BOARD_Y) {
                spr.fillRect(dx, dy, (int32_t)CELL, (int32_t)CELL, activeColor);
                spr.drawRect(dx, dy, (int32_t)CELL, (int32_t)CELL, C_BG);
            }
        }

        spr.setTextColor(C_TEXT);
        spr.setTextSize(1);
        spr.setCursor((int16_t)5, (int16_t)5);
        spr.print("Score: "); spr.print(g.score);
        spr.setCursor((int16_t)90, (int16_t)5);
        spr.print("Lvl: "); spr.print(g.level + 1);

        int ptsNeeded = (g.level + 1) * PTS_PER_LEVEL - g.score;
        if(ptsNeeded < 0) ptsNeeded = 0;
        spr.setTextColor(LEVEL_COLORS[1]);
        spr.setCursor((int16_t)5, (int16_t)18);
        spr.print("Next Lv: "); spr.print(ptsNeeded); spr.print(" pts");

        spr.setTextColor(C_TEXT);
        spr.setCursor((int16_t)125, (int16_t)18); spr.print("Next:");
        for (int i=0; i<4; i++) {
            int32_t nx = (int32_t)(135 + PIECES[g.next_ptype][i][1] * 7);
            int32_t ny = (int32_t)(31 + PIECES[g.next_ptype][i][0] * 7);
            spr.fillRect(nx, ny, 7, 7, activeColor);
            spr.drawRect(nx, ny, 7, 7, C_BG);
        }

        if (g.state == STATE_COUNTDOWN) {
            spr.fillRect((int32_t)BOARD_X, (int32_t)(BOARD_Y + 100), (int32_t)BOARD_W, 40, C_BG);
            spr.drawRect((int32_t)BOARD_X, (int32_t)(BOARD_Y + 100), (int32_t)BOARD_W, 40, C_BORDER);
            spr.setTextColor(LEVEL_COLORS[1]);
            spr.setTextSize(3);
            spr.setCursor((int16_t)(BOARD_X + 65), (int16_t)(BOARD_Y + 110));
            spr.print(countdownValue);
        }
    }
    spr.pushSprite(0, 0);
}

void resetGame() {
    memset(g.grid, 0, sizeof(g.grid));
    g.score = 0; g.level = 0;
    g.pauseHandled = false;
    g.gameInProgress = true;
    g.next_ptype = random(7);
    spawnPiece();
    g.lastFall = millis();
}

void setup() {
    // --- POWER OPTIMIZATIONS START ---
    WiFi.mode(WIFI_OFF);
    setCpuFrequencyMhz(80);
    pinMode(TFT_BACKLIGHT_PIN, OUTPUT);
    analogWrite(TFT_BACKLIGHT_PIN, TFT_BRIGHTNESS);
    // --- POWER OPTIMIZATIONS END ---

    pinMode(TFT_POWER_PIN, OUTPUT);
    digitalWrite(TFT_POWER_PIN, HIGH);

    tft.init(); tft.setRotation(0);
    spr.createSprite(SCREEN_W, SCREEN_H);
    btnL.init(BTN_LEFT_PIN); btnR.init(BTN_RIGHT_PIN);
    loadScores();
    g.state = STATE_MENU;
    g.gameInProgress = false;
    randomSeed(analogRead(4));
}

void loop() {
    uint32_t now = millis();
    btnL.update();
    btnR.update();

    if (g.state == STATE_MENU) {
        int mCount = g.gameInProgress ? 5 : 4;
        if (btnL.justPressed) {
            menuSelection = (menuSelection + 1) % mCount;
        } else if (btnR.justPressed) {
            if (g.gameInProgress) {
                if (menuSelection == 0) { g.state = STATE_COUNTDOWN; countdownValue = 3; countdownTimer = now; }
                else if (menuSelection == 1) { resetGame(); g.state = STATE_PLAY; }
                else if (menuSelection == 2) { g.state = STATE_SCORES; }
                else if (menuSelection == 3) { g.state = STATE_HOWTOPLAY; delay(200); }
                else if (menuSelection == 4) { g.state = STATE_CREDITS; }
            } else {
                if (menuSelection == 0) { resetGame(); g.state = STATE_PLAY; }
                else if (menuSelection == 1) { g.state = STATE_SCORES; }
                else if (menuSelection == 2) { g.state = STATE_HOWTOPLAY; delay(200); }
                else if (menuSelection == 3) { g.state = STATE_CREDITS; }
            }
        }
    }
    else if (g.state == STATE_SCORES || g.state == STATE_HOWTOPLAY || g.state == STATE_CREDITS) {
        if (btnL.justPressed || btnR.justPressed) {
            g.state = STATE_MENU;
            menuSelection = 0;
        }
    }
    else if (g.state == STATE_GAMEOVER) {
        if (btnL.justPressed || btnR.justPressed) {
            saveScore(g.score);
            loadScores();
            g.state = STATE_MENU;
            menuSelection = 0;
            delay(300);
        }
    }
    else if (g.state == STATE_COUNTDOWN) {
        if (now - countdownTimer >= 1000) {
            countdownValue--;
            countdownTimer = now;
            if (countdownValue <= 0) {
                g.state = STATE_PLAY;
                g.lastFall = now;
            }
        }
    }
    else if (g.state == STATE_PLAY) {

        // --- 1. INDEPENDENT PAUSE TIMER ---
        static uint32_t pauseHoldStart = 0;

        if (btnL.state && btnR.state) {
            if (pauseHoldStart == 0) pauseHoldStart = now; // Start counting when both are held

            if (now - pauseHoldStart >= PAUSE_HOLD_MS) {
                if (!g.pauseHandled) {
                    g.state = STATE_MENU;
                    menuSelection = 0; // Default to "Resume"
                    g.pauseHandled = true;
                    btnL.justPressed = false;
                    btnR.justPressed = false;
                }
            }
        } else {
            pauseHoldStart = 0; // Reset timer if a button is let go
            g.pauseHandled = false;
        }

        if (g.state == STATE_PLAY) { // Double check we didn't just trigger the pause

            // --- 2. MOVEMENT & ROTATION ---
            if (btnL.justPressed) {
                if (btnR.state && (now - btnR.pressTime < COMBO_WINDOW_MS)) {
                    rotatePiece(true);
                    btnR.pressTime = now; // Reset DAS delay so it doesn't wall-slam
                } else {
                    if (!checkCollision(g.px - 1, g.py)) g.px--;
                }
            } else if (btnL.repeating && !btnR.state) {
                if (!checkCollision(g.px - 1, g.py)) g.px--;
            }

            if (btnR.justPressed) {
                if (btnL.state && (now - btnL.pressTime < COMBO_WINDOW_MS)) {
                    rotatePiece(false);
                    btnL.pressTime = now; // Reset DAS delay so it doesn't wall-slam
                } else {
                    if (!checkCollision(g.px + 1, g.py)) g.px++;
                }
            } else if (btnR.repeating && !btnL.state) {
                if (!checkCollision(g.px + 1, g.py)) g.px++;
            }

            // --- 3. GRAVITY ---
            uint32_t fallSpeed = INITIAL_FALL_MS - (g.level * SPEED_DECAY_MS);
            if (fallSpeed < MIN_FALL_MS) fallSpeed = MIN_FALL_MS;

            if (now - g.lastFall > fallSpeed) {
                if (checkCollision(g.px, g.py + 1)) {
                    // Lock Piece
                    for(int i=0; i<4; i++) {
                        int ty = g.py + g.piece[i][0];
                        int tx = g.px + g.piece[i][1];
                        if (ty >= 0 && ty < BOARD_ROWS) g.grid[ty][tx] = g.level + 1;
                    }

                    // Clear Lines
                    int cleared = 0;
                    for (int y = 0; y < BOARD_ROWS; y++) {
                        bool full = true;
                        for (int x = 0; x < BOARD_COLS; x++) {
                            if (!g.grid[y][x]) { full = false; break; }
                        }
                        if (full) {
                            cleared++;
                            for (int ty = y; ty > 0; ty--) {
                                for (int tx = 0; tx < BOARD_COLS; tx++) g.grid[ty][tx] = g.grid[ty-1][tx];
                            }
                        }
                    }

                    // Score & Level Logic
                    if (cleared > 0) {
                        g.score += cleared * 100 * (g.level + 1);
                        g.level = g.score / PTS_PER_LEVEL;
                    }
                    g.score += 10;
                    g.level = g.score / PTS_PER_LEVEL;
                    spawnPiece();
                } else {
                    g.py++;
                }
                g.lastFall = now;
            }
        }
    }

    // --- Dynamic FPS Render Limiter ---
    static uint32_t lastDraw = 0;
    if (now - lastDraw >= FRAME_DELAY_MS) {
        render();
        lastDraw = now;
    }

    delay(2);
}