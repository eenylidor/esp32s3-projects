#include <Arduino.h>
#include <TFT_eSPI.h>

#define BUTTON_LEFT  0
#define BUTTON_RIGHT 14
#define TFT_BACKLIGHT 38
#define TFT_POWER_ON 15

TFT_eSPI tft;

constexpr int SCREEN_W = 170;
constexpr int SCREEN_H = 320;

constexpr int GRID_W = 10;
constexpr int GRID_H = 16;
constexpr int CELL = 14;

constexpr int BOARD_W = GRID_W * CELL;
constexpr int BOARD_H = GRID_H * CELL;

constexpr int HEADER_Y = 16;
constexpr int HEADER_LINE_GAP = 14;
constexpr int HEADER_TEXT_SIZE = 1;
constexpr int BOARD_BOTTOM_MARGIN = 6;


constexpr int BOARD_X = (SCREEN_W - BOARD_W) / 2;
constexpr int BOARD_Y = SCREEN_H - BOARD_H - BOARD_BOTTOM_MARGIN;

bool board[GRID_H][GRID_W];

int blockX = GRID_W / 2;
int blockY = 0;

unsigned long lastFall = 0;
constexpr unsigned long FALL_DELAY = 500;

unsigned long lastInput = 0;
constexpr unsigned long INPUT_DELAY = 60;

unsigned long lastFrame = 0;
constexpr unsigned long FRAME_DELAY = 250;
constexpr unsigned long DEBOUNCE_DELAY = 35;

bool lastLeftReading = HIGH;
bool lastRightReading = HIGH;

bool stableLeftState = HIGH;
bool stableRightState = HIGH;

bool previousLeftPressed = false;
bool previousRightPressed = false;

unsigned long lastLeftDebounceTime = 0;
unsigned long lastRightDebounceTime = 0;

bool readDebouncedButton(
    int buttonPin,
    bool &lastReading,
    bool &stableState,
    unsigned long &lastDebounceTime
)
{
    bool currentReading = digitalRead(buttonPin);

    if (currentReading != lastReading)
    {
        lastDebounceTime = millis();
        lastReading = currentReading;
    }

    if (millis() - lastDebounceTime >= DEBOUNCE_DELAY)
    {
        stableState = currentReading;
    }

    return stableState == LOW;
}


void clearBoard()
{
    for (int y = 0; y < GRID_H; y++)
    {
        for (int x = 0; x < GRID_W; x++)
        {
            board[y][x] = false;
        }
    }
}

int randomSpawnColumn()
{
    return random(0, GRID_W);
}

void spawnBlock()
{
    blockX = randomSpawnColumn();
    blockY = 0;

    if (board[blockY][blockX])
    {
        clearBoard();
    }
}

void lockBlock()
{
    if (blockY >= 0 && blockY < GRID_H && blockX >= 0 && blockX < GRID_W)
    {
        board[blockY][blockX] = true;
    }
}

void clearFullLines()
{
    for (int y = GRID_H - 1; y >= 0; y--)
    {
        bool full = true;

        for (int x = 0; x < GRID_W; x++)
        {
            if (!board[y][x])
            {
                full = false;
                break;
            }
        }

        if (full)
        {
            for (int row = y; row > 0; row--)
            {
                for (int x = 0; x < GRID_W; x++)
                {
                    board[row][x] = board[row - 1][x];
                }
            }

            for (int x = 0; x < GRID_W; x++)
            {
                board[0][x] = false;
            }

            y++;
        }
    }
}

bool canMoveTo(int x, int y)
{
    if (x < 0 || x >= GRID_W)
    {
        return false;
    }

    if (y >= GRID_H)
    {
        return false;
    }

    if (y >= 0 && board[y][x])
    {
        return false;
    }

    return true;
}

void drawCell(int x, int y, uint16_t color)
{
    int px = BOARD_X + x * CELL;
    int py = BOARD_Y + y * CELL;

    tft.fillRect(px + 1, py + 1, CELL - 2, CELL - 2, color);
}

void drawGame()
{
    tft.fillScreen(TFT_BLACK);

    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(HEADER_TEXT_SIZE);
    tft.setTextDatum(TC_DATUM);

    tft.drawString("LilyGo T-Display S3", SCREEN_W / 2, HEADER_Y);
    tft.drawString("Mini Tetris", SCREEN_W / 2, HEADER_Y + HEADER_LINE_GAP);
    tft.drawString("eenylidor ( Github )", SCREEN_W / 2, HEADER_Y + HEADER_LINE_GAP * 2);
    tft.drawString("framework : arduino", SCREEN_W / 2, HEADER_Y + HEADER_LINE_GAP * 3);
    tft.drawString("Platformio c++", SCREEN_W / 2, HEADER_Y + HEADER_LINE_GAP * 4);
    tft.setTextDatum(TL_DATUM);

    tft.drawRect(
        BOARD_X - 1,
        BOARD_Y - 1,
        BOARD_W + 2,
        BOARD_H + 2,
        TFT_WHITE
    );

    for (int y = 0; y < GRID_H; y++)
    {
        for (int x = 0; x < GRID_W; x++)
        {
            if (board[y][x])
            {
                drawCell(x, y, TFT_CYAN);
            }
        }
    }

    drawCell(blockX, blockY, TFT_RED);
}

void handleInput()
{
    bool leftPressed = readDebouncedButton(
        BUTTON_LEFT,
        lastLeftReading,
        stableLeftState,
        lastLeftDebounceTime
    );

    bool rightPressed = readDebouncedButton(
        BUTTON_RIGHT,
        lastRightReading,
        stableRightState,
        lastRightDebounceTime
    );

    if (leftPressed && !previousLeftPressed && canMoveTo(blockX - 1, blockY))
    {
        blockX--;
    }

    if (rightPressed && !previousRightPressed && canMoveTo(blockX + 1, blockY))
    {
        blockX++;
    }

    previousLeftPressed = leftPressed;
    previousRightPressed = rightPressed;
}

void updateGame()
{
    if (millis() - lastFall < FALL_DELAY)
    {
        return;
    }

    lastFall = millis();

    if (canMoveTo(blockX, blockY + 1))
    {
        blockY++;
    }
    else
    {
        lockBlock();
        clearFullLines();
        spawnBlock();
    }
}

void setup()
{
    Serial.begin(115200);
    delay(1000);

    pinMode(BUTTON_LEFT, INPUT_PULLUP);
    pinMode(BUTTON_RIGHT, INPUT_PULLUP);

    pinMode(TFT_POWER_ON, OUTPUT);
    digitalWrite(TFT_POWER_ON, HIGH);

    pinMode(TFT_BACKLIGHT, OUTPUT);
    digitalWrite(TFT_BACKLIGHT, HIGH);

    randomSeed(esp_random());

    tft.init();
    tft.setRotation(0);
    tft.fillScreen(TFT_RED);

    delay(500);

    tft.fillScreen(TFT_BLACK);

    clearBoard();
    spawnBlock();

    Serial.println("Display init done");
}

void loop()
{
    Serial.print("LEFT=");
    Serial.print(digitalRead(BUTTON_LEFT));
    Serial.print(" RIGHT=");
    Serial.println(digitalRead(BUTTON_RIGHT));

    handleInput();
    updateGame();

    if (millis() - lastFrame >= FRAME_DELAY)
    {
        lastFrame = millis();
        drawGame();
    }
}