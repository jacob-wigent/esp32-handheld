// main.cpp
#include <Arduino.h>
#include <FastLED.h>
#include <gamer_pins.h>
#include <driver/adc.h>
#include <esp_adc_cal.h>
#include <esp_sleep.h>

// --- Hardware Config ---
#define NUM_SECTIONS 8
#define PIXELS_PER_SECTION 40
const uint8_t SECTION_PINS[NUM_SECTIONS] = { DISP1, DISP2, DISP3, DISP4, DISP5, DISP6, DISP7, DISP8 };
CRGB leds[NUM_SECTIONS][PIXELS_PER_SECTION];

// Grid Dimensions
#define GRID_ROWS (NUM_SECTIONS * 2)      // 16
#define GRID_COLS (PIXELS_PER_SECTION / 2) // 20

// --- System Globals ---
unsigned long t_battRead = 0;
uint8_t globalBrightness = 40; 
unsigned long t_inputPoll = 0;
const unsigned long INPUT_POLL_MS = 30;
bool joystickPresent = true;

enum AppState { STATE_MENU, STATE_PLAY, STATE_GAMEOVER };
AppState appState = STATE_MENU;

enum GameType { GAME_SNAKE = 0, GAME_BREAKOUT = 1, GAME_ART = 2, GAME_FLAPPY = 3, GAME_TETRIS = 4 };
GameType currentGame = GAME_SNAKE;

// --- Input State ---
struct ButtonState {
  bool curr;
  bool prev;
  bool pressed() { return curr && !prev; } 
};

ButtonState btnA, btnB, btnX, btnY, btnStart, btnSelect;
ButtonState dUp, dDown, dLeft, dRight;
bool joyUp, joyDown, joyLeft, joyRight;

// --- Menu Variables ---
const int MENU_ITEMS = 6; 
const int MENU_WIDTH = MENU_ITEMS * GRID_COLS; 
int menuCursorCol = GRID_COLS / 2; 
int menuCursorRow = GRID_ROWS / 2;
float cameraScrollX = 0.0; 

const char *menuMessage = nullptr;
unsigned long t_menuMessage = 0;
const unsigned long MENU_MESSAGE_MS = 1500;
bool powerPending = false;

// --- Snake Variables ---
const int MAX_CELLS = GRID_ROWS * GRID_COLS;
uint16_t snake[MAX_CELLS];
int snakeLen = 0;
int dirRow = 0, dirCol = 1;
int pendingDirRow = 0, pendingDirCol = 1;
uint16_t food = 0xFFFF;
unsigned long t_snakeStep = 0;
unsigned long snakeInterval = 200;

// --- Breakout Variables ---
const int BRICK_ROWS = 5;
bool bricks[BRICK_ROWS][GRID_COLS];
int paddleCol = 0;
// Paddle position/velocity (paddleCol remains for rendering/compat)
float paddleX = 0.0f;
float paddleVel = 0.0f;
const float PADDLE_MAX_SPEED = 32.0f; // columns per second
const int JOY_DEAD = 200; // ADC deadzone around center
const int PADDLE_WIDTH = 5;
int ballR = 0, ballC = 0;
int ballDirR = -1, ballDirC = 1;
unsigned long t_ballStep = 0;
unsigned long ballInterval = 150;
int breakoutScore = 0;

// --- Art Variables ---
CRGB gridColors[GRID_ROWS][GRID_COLS];
int artCursorR = 0, artCursorC = 0;
int selectedColorIndex = 2;
const CRGB ART_PALETTE[] = { CRGB::White, CRGB::Black, CRGB::Red, CRGB::Green, CRGB::Blue, CRGB::Yellow, CRGB::Purple, CRGB::Cyan, CRGB::Orange };
const int ART_PALETTE_LEN = sizeof(ART_PALETTE) / sizeof(ART_PALETTE[0]);

// --- Flappy Bird Variables ---
float flappyBirdY = 0.0f;
float flappyBirdVel = 0.0f;
const float FLAPPY_GRAVITY = 0.45f;
const float FLAPPY_FLAP_IMPULSE = -2.0f; // velocity set on flap (resets momentum)
const float FLAPPY_MAX_FALL_SPEED = 6.0f;
int flappyGapSize = 5;
const int FLAPPY_PIPE_COUNT = 2;
const int FLAPPY_PIPE_SPACING = (GRID_COLS / 2) + 2;
int flappyObstacleX[FLAPPY_PIPE_COUNT];
int flappyObstacleGapY[FLAPPY_PIPE_COUNT];
int flappyScore = 0;
unsigned long t_flappyStep = 0;
const unsigned long FLAPPY_STEP_MS = 80;
bool flappyStarted = false;

// --- Tetris Variables ---
// Playfield is 10 columns, centered inside the 20-column panel
const int TETRIS_COLS = 10;
const int TETRIS_ROWS = GRID_ROWS;
const int TETRIS_X_OFFSET = (GRID_COLS - TETRIS_COLS) / 2; // 5
// store piece type+1 (0 = empty)
uint8_t tetrisGrid[TETRIS_ROWS][TETRIS_COLS];
int tetrisCurPieceX = 0;
int tetrisCurPieceY = 0;
int tetrisCurPieceType = 0;
int tetrisCurRotation = 0;
int tetrisNextPieceType = 0;
int tetrisScore = 0;
unsigned long t_tetrisStep = 0;
const unsigned long TETRIS_STEP_MS = 400;
const CRGB TETRO_COLORS[7] = { CRGB::Cyan, CRGB::Yellow, CRGB::Purple, CRGB::Orange, CRGB::Blue, CRGB::Lime, CRGB::Red };

// --- Battery / ADC ---
esp_adc_cal_characteristics_t *adc_chars;
#define DEFAULT_VREF 1100
#define NO_OF_SAMPLES 64
#define ADC_ATTEN ADC_ATTEN_DB_12

// --- Function Prototypes ---
void setPixelXY(int row, int col, const CRGB &c);
void clearGrid();
float readBatteryVoltage();
void startNewGame();
void startBreakout();
void startArt();
void renderSnake();
void renderBreakout();
void renderArt();
void renderMenu();
void handleInput();
void startFlappyBird();
void startTetris();
void updateFlappyBird();
void updateTetris();
void renderFlappyBird();
void renderTetris();
void updateSnake();
void updateBreakout();
// Tetris helper forward declarations (used by input handler)
int getTetroWidth(int type, int rot);
int getTetroHeight(int type, int rot);
bool tetrisCanPlace(int x, int y, int type, int rot);
void tetrisPlacePiece(int x, int y, int type, int rot);
void tetrisClearLines();

// --- Core Helpers ---

void setPixelXY(int row, int col, const CRGB &c) {
  if (row < 0 || row >= GRID_ROWS) return;
  if (col < 0 || col >= GRID_COLS) return;
  int section = row / 2;
  if (row % 2 == 0) {
    leds[section][col] = c;
  } else {
    int idx = PIXELS_PER_SECTION - 1 - col;
    leds[section][idx] = c;
  }
}

void clearGrid() {
  for (int r = 0; r < GRID_ROWS; ++r)
    for (int c = 0; c < GRID_COLS; ++c)
      setPixelXY(r, c, CRGB::Black);
}

bool pointEquals(uint16_t a, uint16_t b) { return a == b; }

// --- Setup ---

void setup() {
  Serial.begin(115200);
  delay(50);
  
  pinMode(StatusLED, OUTPUT);
  digitalWrite(StatusLED, HIGH);
  pinMode(ChgStat, INPUT);

  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);
  adc_chars = (esp_adc_cal_characteristics_t *)calloc(1, sizeof(esp_adc_cal_characteristics_t));
  esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN, ADC_WIDTH_BIT_12, DEFAULT_VREF, adc_chars);

  FastLED.addLeds<WS2812, DISP1, GRB>(leds[0], PIXELS_PER_SECTION);
  FastLED.addLeds<WS2812, DISP2, GRB>(leds[1], PIXELS_PER_SECTION);
  FastLED.addLeds<WS2812, DISP3, GRB>(leds[2], PIXELS_PER_SECTION);
  FastLED.addLeds<WS2812, DISP4, GRB>(leds[3], PIXELS_PER_SECTION);
  FastLED.addLeds<WS2812, DISP5, GRB>(leds[4], PIXELS_PER_SECTION);
  FastLED.addLeds<WS2812, DISP6, GRB>(leds[5], PIXELS_PER_SECTION);
  FastLED.addLeds<WS2812, DISP7, GRB>(leds[6], PIXELS_PER_SECTION);
  FastLED.addLeds<WS2812, DISP8, GRB>(leds[7], PIXELS_PER_SECTION);

  FastLED.setBrightness(globalBrightness);
  clearGrid();
  FastLED.show();

  randomSeed(analogRead(ABAT));

  pinMode(UP, INPUT_PULLUP);
  pinMode(DOWN, INPUT_PULLUP);
  pinMode(LEFT, INPUT_PULLUP);
  pinMode(RIGHT, INPUT_PULLUP);
  pinMode(A, INPUT_PULLUP);
  pinMode(B, INPUT_PULLUP);
  pinMode(X, INPUT_PULLUP);
  pinMode(Y, INPUT_PULLUP);
  pinMode(START, INPUT_PULLUP);
  pinMode(SELECT, INPUT_PULLUP);
  pinMode(StckBtn, INPUT_PULLUP);

  int sampleAX = analogRead(AX);
  int sampleAY = analogRead(AY);
  joystickPresent = !(sampleAX < 8 && sampleAY < 8);
  
  appState = STATE_MENU;
  menuCursorCol = GRID_COLS / 2;
  menuCursorRow = GRID_ROWS / 2;
}

// --- Input Processing ---

void readInputs() {
  auto readBtn = [](int pin, ButtonState &b) {
    b.prev = b.curr;
    b.curr = (digitalRead(pin) == LOW);
  };

  readBtn(A, btnA); readBtn(B, btnB);
  readBtn(X, btnX); readBtn(Y, btnY);
  readBtn(START, btnStart); readBtn(SELECT, btnSelect);
  readBtn(UP, dUp); readBtn(DOWN, dDown);
  readBtn(LEFT, dLeft); readBtn(RIGHT, dRight);

  if (joystickPresent) {
    int ax = analogRead(AX);
    int ay = analogRead(AY);
    const int MID = 2048;
    const int DEAD = 600; 
    joyRight = (ax > MID + DEAD);
    joyLeft = (ax < MID - DEAD);
    joyDown = (ay > MID + DEAD); 
    joyUp = (ay < MID - DEAD);
  } else {
    joyUp = joyDown = joyLeft = joyRight = false;
  }
}

void getNavDirection(bool &u, bool &d, bool &l, bool &r) {
  u = dUp.curr || joyUp;
  d = dDown.curr || joyDown;
  l = dLeft.curr || joyLeft;
  r = dRight.curr || joyRight;
}

// --- Main Loop ---

void loop() {
  unsigned long now = millis();

  if (now - t_inputPoll >= INPUT_POLL_MS) {
    t_inputPoll = now;
    readInputs();
    handleInput();
  }

  if (appState == STATE_PLAY) {
    if (currentGame == GAME_SNAKE) updateSnake();
    else if (currentGame == GAME_BREAKOUT) updateBreakout();
    else if (currentGame == GAME_FLAPPY) updateFlappyBird();
    else if (currentGame == GAME_TETRIS) updateTetris();
  }
  else if (appState == STATE_MENU) {
    // Camera Logic
    float maxScroll = MENU_WIDTH - GRID_COLS;
    if (maxScroll < 0) maxScroll = 0;
    if (joystickPresent) {
      // Read analog stick to control scroll speed smoothly
      const int MID = 2048;
      const int DEAD = 300;
      int ax = analogRead(AX);
      float norm = 0.0f;
      if (ax > MID + DEAD) norm = (float)(ax - (MID + DEAD)) / (float)(MID - DEAD);
      else if (ax < MID - DEAD) norm = (float)(ax - (MID - DEAD)) / (float)(MID - DEAD);
      // clamp
      if (norm > 1.0f) norm = 1.0f;
      if (norm < -1.0f) norm = -1.0f;

      const float MAX_SCROLL_SPEED = 12.0f; // columns per second
      float dt = (float)INPUT_POLL_MS / 1000.0f;
      cameraScrollX += norm * MAX_SCROLL_SPEED * dt;
      if (cameraScrollX < 0) cameraScrollX = 0;
      if (cameraScrollX > maxScroll) cameraScrollX = maxScroll;
      // Note: do NOT modify `menuCursorCol` here — keep the cursor position
      // independent of continuous camera scrolling to avoid flicker. Selection
      // will be read from the world position under the fixed cursor when the
      // user presses START/A.
    } else {
      // Snap/lerp to nearest card based on discrete cursor
      float targetCamX = menuCursorCol - (GRID_COLS / 2.0f);
      if (targetCamX < 0) targetCamX = 0;
      if (targetCamX > maxScroll) targetCamX = maxScroll;
      cameraScrollX += (targetCamX - cameraScrollX) * 0.2f;
    }
  }

  clearGrid(); 

  if (appState == STATE_MENU) renderMenu();
  else if (appState == STATE_GAMEOVER) {
    for(int r=0; r<GRID_ROWS; r++) 
      for(int c=0; c<GRID_COLS; c++) 
        setPixelXY(r, c, CRGB(50, 0, 0));
    
    for(int i=0; i<GRID_ROWS; i++) {
        setPixelXY(i, i + 2, CRGB::Red);
        setPixelXY(i, GRID_COLS - 3 - i, CRGB::Red);
    }
  }
  else if (appState == STATE_PLAY) {
    if (currentGame == GAME_SNAKE) renderSnake();
    else if (currentGame == GAME_BREAKOUT) renderBreakout();
    else if (currentGame == GAME_ART) renderArt();
    else if (currentGame == GAME_FLAPPY) renderFlappyBird();
    else if (currentGame == GAME_TETRIS) renderTetris();
  }

  if (menuMessage && (now - t_menuMessage < MENU_MESSAGE_MS)) {
     int msgRow = GRID_ROWS/2;
     for(int c=2; c<GRID_COLS-2; c++) setPixelXY(msgRow, c, CRGB::White);
  } else {
     menuMessage = nullptr;
  }

  FastLED.setBrightness(globalBrightness);
  FastLED.show();

  if (now - t_battRead > 2000) {
    t_battRead = now;
    readBatteryVoltage();
  }
}

// --- Input Handler ---

void handleInput() {
  bool u, d, l, r;
  getNavDirection(u, d, l, r);

  // --- MENU INPUT ---
  if (appState == STATE_MENU) {
    
    // FIXED: Use Timer for Continuous Movement (Same logic as Art)
    static unsigned long t_menuMove = 0;
    if (millis() - t_menuMove > 100) { 
      // 100ms speed = smooth scroll
      if (l) menuCursorCol--;
      if (r) menuCursorCol++;
      if (u) menuCursorRow--;
      if (d) menuCursorRow++;
      
      t_menuMove = millis();
    }

    // Clamp Cursor to World Bounds
    if (menuCursorCol < 0) menuCursorCol = 0;
    if (menuCursorCol >= MENU_WIDTH - 1) menuCursorCol = MENU_WIDTH - 1;
    
    // Clamp Cursor to Screen Height
    if (menuCursorRow < 0) menuCursorRow = 0;
    if (menuCursorRow >= GRID_ROWS - 1) menuCursorRow = GRID_ROWS - 1;

    // Selection: choose the card under the fixed cursor.
    if (btnStart.pressed() || btnA.pressed()) {
      int selectedGame = 0;
      if (joystickPresent) {
        // When joystick controls smooth scrolling, use the world column
        // currently under the screen center as the selected card.
        float cam = cameraScrollX;
        int worldCol = (int)roundf(cam + (GRID_COLS / 2.0f));
        if (worldCol < 0) worldCol = 0;
        if (worldCol >= MENU_WIDTH) worldCol = MENU_WIDTH - 1;
        selectedGame = worldCol / GRID_COLS;
      } else {
        // When joystick is absent (D-pad navigation), use menuCursorCol
        selectedGame = menuCursorCol / GRID_COLS;
      }

      if (selectedGame == 0) { currentGame = GAME_SNAKE; startNewGame(); }
      else if (selectedGame == 1) { currentGame = GAME_BREAKOUT; startBreakout(); }
      else if (selectedGame == 2) { currentGame = GAME_ART; startArt(); }
      else if (selectedGame == 3) { currentGame = GAME_FLAPPY; startFlappyBird(); }
      else if (selectedGame == 4) { currentGame = GAME_TETRIS; startTetris(); }
      else if (selectedGame == 5) {
        menuMessage = "Bye";
        t_menuMessage = millis();
        powerPending = true;
      }
    }

    if (btnX.pressed()) { globalBrightness = max(5, globalBrightness - 10); }
    if (btnY.pressed()) { globalBrightness = min(255, globalBrightness + 10); }
    
    if (powerPending && !btnStart.curr && !btnA.curr) {
      clearGrid(); FastLED.show(); delay(100);
      esp_sleep_enable_ext0_wakeup((gpio_num_t)START, 0);
      esp_deep_sleep_start();
    }
  }

  // --- GAME OVER INPUT ---
  else if (appState == STATE_GAMEOVER) {
    if (btnStart.pressed()) appState = STATE_MENU;
    if (btnA.pressed()) {
      if (currentGame == GAME_SNAKE) startNewGame();
      else if (currentGame == GAME_BREAKOUT) startBreakout();
    }
  }

  // --- PLAY INPUT ---
  else if (appState == STATE_PLAY) {
    
    if (currentGame == GAME_SNAKE) {
      if (btnStart.pressed()) appState = STATE_MENU;
      
      int reqR = 0, reqC = 0;
      if (u) reqR = -1;
      else if (d) reqR = 1;
      else if (l) reqC = -1;
      else if (r) reqC = 1;

      if ((reqR != 0 || reqC != 0) && !(reqR == -dirRow && reqC == -dirCol)) {
        pendingDirRow = reqR;
        pendingDirCol = reqC;
      }
      if (btnA.pressed()) snakeInterval = max(50UL, snakeInterval - 20);
      if (btnB.pressed()) snakeInterval = min(500UL, snakeInterval + 20);
    }

    else if (currentGame == GAME_BREAKOUT) {
      if (btnStart.pressed()) appState = STATE_MENU;

      // Time delta for movement (seconds)
      float dt = INPUT_POLL_MS / 1000.0f;

      if (joystickPresent) {
        int ax = analogRead(AX);
        const int MID = 2048;
        int deflect = ax - MID;

        // Deadzone
        if (abs(deflect) < JOY_DEAD) deflect = 0;

        // Map deflection (-MID..MID) to velocity (-PADDLE_MAX_SPEED..PADDLE_MAX_SPEED)
        float norm = (float)deflect / (float)(MID - JOY_DEAD);
        if (norm > 1.0f) norm = 1.0f;
        if (norm < -1.0f) norm = -1.0f;

        // Desired velocity from joystick
        float desiredVel = norm * PADDLE_MAX_SPEED;

        // Simple smoothing towards desired velocity (higher accel = snappier)
        const float accel = 300.0f; // speed units per second^2
        float velDiff = desiredVel - paddleVel;
        float maxDelta = accel * dt;
        if (velDiff > maxDelta) velDiff = maxDelta;
        if (velDiff < -maxDelta) velDiff = -maxDelta;
        paddleVel += velDiff;
      } else {
        // Buttons: simple fixed velocity per press
        if (l) paddleVel = -PADDLE_MAX_SPEED * 0.9f;
        else if (r) paddleVel = PADDLE_MAX_SPEED * 0.9f;
        else {
          // apply damping to slow to zero
          paddleVel *= 0.5f;
          if (fabs(paddleVel) < 0.01f) paddleVel = 0.0f;
        }
      }

      // friction / damping when joystick centered
      if (joystickPresent) {
        // small damping to prevent endless drift (reduced for snappier feel)
        paddleVel *= 0.995f;
        if (fabs(paddleVel) < 0.01f) paddleVel = 0.0f;
      }

      // Update position
      paddleX += paddleVel * dt;

      // Constrain and sync integer paddleCol for rendering/collision
      int maxIdx = GRID_COLS - PADDLE_WIDTH;
      if (paddleX < 0) { paddleX = 0; paddleVel = 0; }
      if (paddleX > maxIdx) { paddleX = maxIdx; paddleVel = 0; }
      paddleCol = constrain((int)roundf(paddleX), 0, maxIdx);
    }

    else if (currentGame == GAME_ART) {
      if (btnStart.pressed()) appState = STATE_MENU;
      
      static unsigned long t_artMove = 0;
      if (millis() - t_artMove > 100) {
        if (u) artCursorR--;
        if (d) artCursorR++;
        if (l) artCursorC--;
        if (r) artCursorC++;
        t_artMove = millis();
      }
      if (artCursorR < 0) artCursorR = GRID_ROWS - 1;
      if (artCursorR >= GRID_ROWS) artCursorR = 0;
      if (artCursorC < 0) artCursorC = GRID_COLS - 1;
      if (artCursorC >= GRID_COLS) artCursorC = 0;

      if (btnA.curr) gridColors[artCursorR][artCursorC] = ART_PALETTE[selectedColorIndex];
      if (btnB.pressed()) selectedColorIndex = (selectedColorIndex + 1) % ART_PALETTE_LEN;
      if (btnX.pressed()) selectedColorIndex = 1; 
      if (btnY.pressed()) {
        for(int r=0; r<GRID_ROWS; r++)
          for(int c=0; c<GRID_COLS; c++)
            gridColors[r][c] = ART_PALETTE[selectedColorIndex];
      }
    }

    else if (currentGame == GAME_FLAPPY) {
      if (btnStart.pressed() || btnB.pressed()) appState = STATE_MENU;
      if (dUp.pressed() || btnA.pressed()) {
        // First flap starts the game; subsequent flaps reset vertical velocity
        if (!flappyStarted) {
          flappyStarted = true;
          // ensure obstacle timing starts fresh
          t_flappyStep = millis();
        }
        // reset momentum and apply consistent flap
        flappyBirdVel = FLAPPY_FLAP_IMPULSE;
        // cap upward velocity
        if (flappyBirdVel < -8.0f) flappyBirdVel = -8.0f;
      }
    }

    else if (currentGame == GAME_TETRIS) {
      // START returns to menu; B is used for rotation per request
      if (btnStart.pressed()) appState = STATE_MENU;

      // Rotation inputs: D-pad up/down and X/B rotate the piece
      if (dUp.pressed() || dDown.pressed() || btnX.pressed() || btnB.pressed()) {
        int newRot = (tetrisCurRotation + 1) % 4;
        if (tetrisCanPlace(tetrisCurPieceX, tetrisCurPieceY, tetrisCurPieceType, newRot)) {
          tetrisCurRotation = newRot;
        }
      }

      // Horizontal movement via joystick (preferred). If joystick absent, allow small repeated nudges via left/right hold.
      if (joystickPresent) {
        int ax = analogRead(AX);
        const int MID = 2048;
        const int DEAD = 300;
        if (ax > MID + DEAD) {
          if (tetrisCanPlace(tetrisCurPieceX+1, tetrisCurPieceY, tetrisCurPieceType, tetrisCurRotation)) tetrisCurPieceX++;
        } else if (ax < MID - DEAD) {
          if (tetrisCanPlace(tetrisCurPieceX-1, tetrisCurPieceY, tetrisCurPieceType, tetrisCurRotation)) tetrisCurPieceX--;
        }
      } else {
        static unsigned long t_tetroNudge = 0;
        if (millis() - t_tetroNudge > 120) {
          if (dLeft.curr) {
            if (tetrisCanPlace(tetrisCurPieceX-1, tetrisCurPieceY, tetrisCurPieceType, tetrisCurRotation)) tetrisCurPieceX--;
          }
          if (dRight.curr) {
            if (tetrisCanPlace(tetrisCurPieceX+1, tetrisCurPieceY, tetrisCurPieceType, tetrisCurRotation)) tetrisCurPieceX++;
          }
          t_tetroNudge = millis();
        }
      }

      // Hard drop (slam down)
      if (btnA.pressed()) {
        while (tetrisCanPlace(tetrisCurPieceX, tetrisCurPieceY + 1, tetrisCurPieceType, tetrisCurRotation)) {
          tetrisCurPieceY++;
        }
      }

      // Clamp piece X to playfield bounds (consider rotation)
      int maxX = TETRIS_COLS - getTetroWidth(tetrisCurPieceType, tetrisCurRotation);
      tetrisCurPieceX = constrain(tetrisCurPieceX, 0, maxX);
    }
  }
}

// --- Game Logic ---

void spawnFood() {
  if (snakeLen >= MAX_CELLS) { food = 0xFFFF; return; }
  while (true) {
    int r = random(GRID_ROWS);
    int c = random(GRID_COLS);
    uint16_t p = r * GRID_COLS + c;
    bool ok = true;
    for (int i = 0; i < snakeLen; ++i) if (snake[i] == p) { ok = false; break; }
    if (ok) { food = p; return; }
  }
}

void startNewGame() {
  snakeLen = 3;
  int sr = GRID_ROWS / 2;
  int sc = GRID_COLS / 2 - 1;
  snake[0] = sr * GRID_COLS + sc;
  snake[1] = sr * GRID_COLS + (sc - 1);
  snake[2] = sr * GRID_COLS + (sc - 2);
  dirRow = 0; dirCol = 1;
  pendingDirRow = dirRow; pendingDirCol = dirCol;
  snakeInterval = 200;
  t_snakeStep = millis();
  spawnFood();
  appState = STATE_PLAY;
  currentGame = GAME_SNAKE;
}

void updateSnake() {
  if (millis() - t_snakeStep < snakeInterval) return;
  t_snakeStep = millis();

  dirRow = pendingDirRow; dirCol = pendingDirCol;
  int hr = snake[0] / GRID_COLS;
  int hc = snake[0] % GRID_COLS;
  int nr = hr + dirRow;
  int nc = hc + dirCol;

  if (nr < 0 || nr >= GRID_ROWS || nc < 0 || nc >= GRID_COLS) {
    appState = STATE_GAMEOVER; return;
  }

  uint16_t np = nr * GRID_COLS + nc;
  bool willGrow = (np == food);
  int checkLen = snakeLen - (willGrow ? 0 : 1);
  for (int i = 0; i < checkLen; ++i) if (snake[i] == np) { appState = STATE_GAMEOVER; return; }

  for (int i = snakeLen; i > 0; --i) snake[i] = snake[i-1];
  snake[0] = np;
  
  if (willGrow) {
    snakeLen++;
    if (snakeLen > MAX_CELLS) snakeLen = MAX_CELLS;
    spawnFood();
  } 
}

void renderSnake() {
  if (food != 0xFFFF) {
    setPixelXY(food / GRID_COLS, food % GRID_COLS, CRGB::Red);
  }
  for (int i = 0; i < snakeLen; ++i) {
    int r = snake[i] / GRID_COLS;
    int c = snake[i] % GRID_COLS;
    setPixelXY(r, c, (i==0) ? CRGB::Green : CRGB(0, 100, 0));
  }
}

void startBreakout() {
  for (int r = 0; r < BRICK_ROWS; ++r)
    for (int c = 0; c < GRID_COLS; ++c)
      bricks[r][c] = true;

  paddleCol = (GRID_COLS - PADDLE_WIDTH) / 2;
  // Initialize float position/velocity for velocity-based control
  paddleX = (GRID_COLS - PADDLE_WIDTH) / 2;
  paddleVel = 0.0f;
  ballR = GRID_ROWS - 3;
  ballC = GRID_COLS / 2;
  ballDirR = -1; ballDirC = 1;
  t_ballStep = millis();
  ballInterval = 100;
  breakoutScore = 0;
  appState = STATE_PLAY;
  currentGame = GAME_BREAKOUT;
}

void updateBreakout() {
  if (millis() - t_ballStep < ballInterval) return;
  t_ballStep = millis();

  int nr = ballR + ballDirR;
  int nc = ballC + ballDirC;

  if (nc < 0) { nc = 0; ballDirC = -ballDirC; }
  if (nc >= GRID_COLS) { nc = GRID_COLS - 1; ballDirC = -ballDirC; }
  if (nr < 0) { nr = 0; ballDirR = -ballDirR; }

  // Paddle moved up one row: detect collision one row earlier
  if (nr >= GRID_ROWS - 2) {
    if (nc >= paddleCol && nc < paddleCol + PADDLE_WIDTH) {
      ballDirR = -1;
      // place ball one row above the paddle after bounce
      nr = GRID_ROWS - 3;
      int diff = nc - (paddleCol + PADDLE_WIDTH/2);
      if(diff < 0) ballDirC = -1;
      else if(diff > 0) ballDirC = 1;
    } else {
      appState = STATE_GAMEOVER;
      return;
    }
  }

  if (nr < BRICK_ROWS && nr >= 0) {
    if (bricks[nr][nc]) {
      bricks[nr][nc] = false;
      ballDirR = -ballDirR;
    }
  }

  ballR = nr; ballC = nc;
}

void renderBreakout() {
  for (int r = 0; r < BRICK_ROWS; ++r)
    for (int c = 0; c < GRID_COLS; ++c)
      if (bricks[r][c]) setPixelXY(r, c, CRGB::Orange);

  int prow = GRID_ROWS - 2; // paddle raised one row
  for (int i = 0; i < PADDLE_WIDTH; ++i)
    setPixelXY(prow, paddleCol + i, CRGB::Blue);

  setPixelXY(ballR, ballC, CRGB::White);
}

void startArt() {
  artCursorR = GRID_ROWS/2; artCursorC = GRID_COLS/2;
  appState = STATE_PLAY;
  currentGame = GAME_ART;
}

void renderArt() {
  for (int r = 0; r < GRID_ROWS; ++r)
    for (int c = 0; c < GRID_COLS; ++c)
      setPixelXY(r, c, gridColors[r][c]);

  for (int i = 0; i < ART_PALETTE_LEN; ++i) setPixelXY(0, i, ART_PALETTE[i]);
  setPixelXY(1, selectedColorIndex, CRGB::White);

  if ((millis() / 300) % 2 == 0) {
    setPixelXY(artCursorR, artCursorC, CRGB::White);
  }
}

void renderMenu() {
  int camInt = (int)cameraScrollX;

  for (int i = 0; i < MENU_ITEMS; ++i) {
    int startCol = i * GRID_COLS;
    int screenX = startCol - camInt;
    
    if (screenX > GRID_COLS || screenX + GRID_COLS < 0) continue;

    for (int c = 0; c < GRID_COLS; ++c) {
      int actualScreenCol = screenX + c;
      if (actualScreenCol < 0 || actualScreenCol >= GRID_COLS) continue;

      if (i == 0) {
         setPixelXY(1, actualScreenCol, CRGB::Grey); 
         if (c > 5 && c < 15) setPixelXY(GRID_ROWS/2, actualScreenCol, CRGB::Green);
      } 
      else if (i == 1) {
         setPixelXY(1, actualScreenCol, CRGB::Grey);
         if (c%4 != 0) setPixelXY(GRID_ROWS/2, actualScreenCol, CRGB::Orange); 
         if (c > 5 && c < 15) setPixelXY(GRID_ROWS-3, actualScreenCol, CRGB::Blue); 
      }
      else if (i == 2) {
         setPixelXY(1, actualScreenCol, CRGB::Grey);
         setPixelXY(GRID_ROWS/2, actualScreenCol, CHSV(c*10, 255, 255));
      }
      else if (i == 3) {
         setPixelXY(1, actualScreenCol, CRGB::Grey);
         if (c > 8 && c < 12) setPixelXY(GRID_ROWS/2, actualScreenCol, CRGB::Yellow);
      }
      else if (i == 4) {
         setPixelXY(1, actualScreenCol, CRGB::Grey);
         if (c > 8 && c < 12) setPixelXY(GRID_ROWS/2, actualScreenCol, CRGB::Magenta);
      }
      else if (i == 5) {
         setPixelXY(1, actualScreenCol, CRGB::Grey);
         if (c > 8 && c < 12) {
            setPixelXY(GRID_ROWS/2, actualScreenCol, CRGB::Red);
            setPixelXY(GRID_ROWS/2-1, actualScreenCol, CRGB::Red);
         }
      }
      if (c == 0 || c == GRID_COLS - 1) {
        for(int r=2; r<GRID_ROWS-1; r++) setPixelXY(r, actualScreenCol, CRGB(20,20,20));
      }
    }
  }

  int maxScroll = MENU_WIDTH - GRID_COLS;
  if (maxScroll < 0) maxScroll = 0;
  int cursorScreenCol;
  if (joystickPresent) {
    // When joystick drives smooth scrolling, keep the cursor visually fixed
    // in the center of the screen to avoid flicker and edge-jumps.
    cursorScreenCol = GRID_COLS / 2;
  } else {
    // When using D-pad navigation, show the actual cursor position at
    // the screen edge when the camera cannot scroll further.
    if (camInt <= 0) {
      cursorScreenCol = menuCursorCol;
    } else if (camInt >= maxScroll) {
      cursorScreenCol = menuCursorCol - camInt;
    } else {
      cursorScreenCol = GRID_COLS / 2;
    }
  }
  // clamp and draw
  if (cursorScreenCol < 0) cursorScreenCol = 0;
  if (cursorScreenCol >= GRID_COLS) cursorScreenCol = GRID_COLS - 1;
  setPixelXY(menuCursorRow, cursorScreenCol, CRGB::White);
}

float readBatteryVoltage() {
  uint32_t adc_reading = 0;
  for (int i = 0; i < NO_OF_SAMPLES; i++) adc_reading += analogRead(ABAT);
  adc_reading /= NO_OF_SAMPLES;
  uint32_t voltage_mv = esp_adc_cal_raw_to_voltage(adc_reading, adc_chars);
  return (voltage_mv / 1000.0) * 2.0 + 0.01;
}

// ============= FLAPPY BIRD =============

void startFlappyBird() {
  flappyBirdY = GRID_ROWS / 2;
  flappyBirdVel = 0.0f;
  // Initialize two pipes spaced apart so the player sees more challenge
  flappyObstacleX[0] = GRID_COLS - 1;
  flappyObstacleX[1] = GRID_COLS - 1 + FLAPPY_PIPE_SPACING;
  for (int i = 0; i < FLAPPY_PIPE_COUNT; ++i) {
    flappyObstacleGapY[i] = random(2, GRID_ROWS - 2 - flappyGapSize);
  }
  flappyScore = 0;
  t_flappyStep = millis();
  flappyStarted = false;
  appState = STATE_PLAY;
  currentGame = GAME_FLAPPY;
}

void updateFlappyBird() {
  if (millis() - t_flappyStep < FLAPPY_STEP_MS) return;
  t_flappyStep = millis();
  // If the game hasn't started (no first flap), don't update physics or obstacles
  if (!flappyStarted) return;

  // Gravity and velocity
  flappyBirdVel += FLAPPY_GRAVITY;
  // cap fall speed
  if (flappyBirdVel > FLAPPY_MAX_FALL_SPEED) flappyBirdVel = FLAPPY_MAX_FALL_SPEED;
  flappyBirdY += flappyBirdVel;

  // Collision: top (bounce/stop) and bottom (death)
  if (flappyBirdY < 0) {
    flappyBirdY = 0;
    flappyBirdVel = 0; // hit top, do not kill
  }
  if (flappyBirdY >= GRID_ROWS) {
    appState = STATE_GAMEOVER;
    return;
  }

  // Move obstacles left; respawn each when it passes off-screen, keeping spacing
  for (int i = 0; i < FLAPPY_PIPE_COUNT; ++i) {
    flappyObstacleX[i]--;
  }
  // Handle respawn for any pipe that moved past the left edge
  for (int i = 0; i < FLAPPY_PIPE_COUNT; ++i) {
    if (flappyObstacleX[i] < 0) {
      // Find current max X among pipes to maintain spacing
      int maxX = flappyObstacleX[0];
      for (int j = 1; j < FLAPPY_PIPE_COUNT; ++j) if (flappyObstacleX[j] > maxX) maxX = flappyObstacleX[j];
      flappyObstacleX[i] = maxX + FLAPPY_PIPE_SPACING;
      flappyObstacleGapY[i] = random(2, GRID_ROWS - 2 - flappyGapSize);
      flappyScore++;
    }
  }

  // Collision with any obstacle
  for (int i = 0; i < FLAPPY_PIPE_COUNT; ++i) {
    if (flappyObstacleX[i] == 1) { // bird position is hardcoded at column 2
      int birdR = (int)flappyBirdY;
      if (birdR < flappyObstacleGapY[i] || birdR >= flappyObstacleGapY[i] + flappyGapSize) {
        appState = STATE_GAMEOVER;
      }
    }
  }
}

void renderFlappyBird() {
  clearGrid();

  // Draw bird (at fixed column 2)
  int birdR = constrain((int)flappyBirdY, 0, GRID_ROWS - 1);
  setPixelXY(birdR, 2, CRGB::Yellow);

  // Draw obstacles (make pipes green)
  for (int i = 0; i < FLAPPY_PIPE_COUNT; ++i) {
    int ox = flappyObstacleX[i];
    int gap = flappyObstacleGapY[i];
    for (int r = 0; r < GRID_ROWS; ++r) {
      if (r < gap || r >= gap + flappyGapSize) {
        setPixelXY(r, ox, CRGB::Green);
      }
    }
  }

  // Draw score at top
  for (int i = 0; i < flappyScore && i < 5; ++i) {
    setPixelXY(0, i, CRGB::Green);
  }
}

// ============= TETRIS =============

// Tetromino shapes: simplified compact representation
// Each piece is a 4x4 grid encoded as rows of booleans
struct Tetromino {
  uint8_t w, h;
  uint16_t data; // simplified: pack shape bits
};

// Rotation-aware tetromino helpers
bool isTetroCell(int type, int row, int col, int rot) {
  static const bool SHAPES[7][4][4] = {
    // I
    {{ 1, 1, 1, 1 }, { 0, 0, 0, 0 }, { 0, 0, 0, 0 }, { 0, 0, 0, 0 }},
    // O
    {{ 1, 1, 0, 0 }, { 1, 1, 0, 0 }, { 0, 0, 0, 0 }, { 0, 0, 0, 0 }},
    // T
    {{ 0, 1, 0, 0 }, { 1, 1, 1, 0 }, { 0, 0, 0, 0 }, { 0, 0, 0, 0 }},
    // L
    {{ 1, 0, 0, 0 }, { 1, 1, 1, 0 }, { 0, 0, 0, 0 }, { 0, 0, 0, 0 }},
    // J
    {{ 0, 0, 1, 0 }, { 1, 1, 1, 0 }, { 0, 0, 0, 0 }, { 0, 0, 0, 0 }},
    // S
    {{ 0, 1, 1, 0 }, { 1, 1, 0, 0 }, { 0, 0, 0, 0 }, { 0, 0, 0, 0 }},
    // Z
    {{ 1, 1, 0, 0 }, { 0, 1, 1, 0 }, { 0, 0, 0, 0 }, { 0, 0, 0, 0 }}
  };
  if (type < 0 || type >= 7 || row < 0 || row >= 4 || col < 0 || col >= 4) return false;
  int orig_r, orig_c;
  switch (rot & 3) {
    case 0: orig_r = row; orig_c = col; break;
    case 1: orig_r = 3 - col; orig_c = row; break; // 90cw
    case 2: orig_r = 3 - row; orig_c = 3 - col; break; // 180
    case 3: orig_r = col; orig_c = 3 - row; break; // 270
    default: orig_r = row; orig_c = col; break;
  }
  return SHAPES[type][orig_r][orig_c];
}

int getTetroWidth(int type, int rot) {
  int minc = 4, maxc = -1;
  for (int r = 0; r < 4; ++r) for (int c = 0; c < 4; ++c) if (isTetroCell(type, r, c, rot)) { if (c < minc) minc = c; if (c > maxc) maxc = c; }
  if (maxc < minc) return 0;
  return maxc - minc + 1;
}

int getTetroHeight(int type, int rot) {
  int minr = 4, maxr = -1;
  for (int r = 0; r < 4; ++r) for (int c = 0; c < 4; ++c) if (isTetroCell(type, r, c, rot)) { if (r < minr) minr = r; if (r > maxr) maxr = r; }
  if (maxr < minr) return 0;
  return maxr - minr + 1;
}

int getTetroMinCol(int type, int rot) {
  int minc = 4;
  for (int r = 0; r < 4; ++r) for (int c = 0; c < 4; ++c) if (isTetroCell(type, r, c, rot)) if (c < minc) minc = c;
  if (minc == 4) return 0;
  return minc;
}

int getTetroMinRow(int type, int rot) {
  int minr = 4;
  for (int r = 0; r < 4; ++r) for (int c = 0; c < 4; ++c) if (isTetroCell(type, r, c, rot)) if (r < minr) minr = r;
  if (minr == 4) return 0;
  return minr;
}

bool tetrisCanPlace(int x, int y, int type, int rot) {
  int minc = getTetroMinCol(type, rot);
  int minr = getTetroMinRow(type, rot);
  for (int py = 0; py < 4; ++py) {
    for (int px = 0; px < 4; ++px) {
      if (!isTetroCell(type, py, px, rot)) continue;
      int gx = x + (px - minc);
      int gy = y + (py - minr);
      if (gx < 0 || gx >= TETRIS_COLS || gy < 0 || gy >= TETRIS_ROWS) return false;
      if (tetrisGrid[gy][gx]) return false;
    }
  }
  return true;
}

void tetrisPlacePiece(int x, int y, int type, int rot) {
  int minc = getTetroMinCol(type, rot);
  int minr = getTetroMinRow(type, rot);
  for (int py = 0; py < 4; ++py) {
    for (int px = 0; px < 4; ++px) {
      if (!isTetroCell(type, py, px, rot)) continue;
      int gx = x + (px - minc);
      int gy = y + (py - minr);
      if (gy >= 0 && gy < TETRIS_ROWS && gx >= 0 && gx < TETRIS_COLS) {
        tetrisGrid[gy][gx] = (uint8_t)(type + 1);
      }
    }
  }
}

void tetrisClearLines() {
  for (int r = TETRIS_ROWS - 1; r >= 0; --r) {
    bool full = true;
    for (int c = 0; c < TETRIS_COLS; ++c) {
      if (tetrisGrid[r][c] == 0) { full = false; break; }
    }
    if (full) {
      for (int rr = r; rr > 0; --rr) {
        for (int c = 0; c < TETRIS_COLS; ++c) {
          tetrisGrid[rr][c] = tetrisGrid[rr-1][c];
        }
      }
      for (int c = 0; c < TETRIS_COLS; ++c) tetrisGrid[0][c] = 0;
      tetrisScore += 10;
      r++; // recheck this row
    }
  }
}

void startTetris() {
  for (int r = 0; r < TETRIS_ROWS; ++r)
    for (int c = 0; c < TETRIS_COLS; ++c)
      tetrisGrid[r][c] = 0;
  tetrisCurPieceType = random(7);
  tetrisNextPieceType = random(7);
  tetrisCurRotation = 0;
  tetrisCurPieceX = (TETRIS_COLS - getTetroWidth(tetrisCurPieceType, tetrisCurRotation)) / 2;
  tetrisCurPieceY = 0;
  tetrisCurRotation = 0;
  tetrisScore = 0;
  t_tetrisStep = millis();
  appState = STATE_PLAY;
  currentGame = GAME_TETRIS;
}

void updateTetris() {
  if (millis() - t_tetrisStep < TETRIS_STEP_MS) return;
  t_tetrisStep = millis();

  // Try to move current piece down
  if (tetrisCanPlace(tetrisCurPieceX, tetrisCurPieceY + 1, tetrisCurPieceType, tetrisCurRotation)) {
    tetrisCurPieceY++;
  } else {
    // Lock piece and spawn new one
    tetrisPlacePiece(tetrisCurPieceX, tetrisCurPieceY, tetrisCurPieceType, tetrisCurRotation);
    tetrisClearLines();
    tetrisCurPieceType = tetrisNextPieceType;
    tetrisNextPieceType = random(7);
    tetrisCurRotation = 0;
    tetrisCurPieceX = (TETRIS_COLS - getTetroWidth(tetrisCurPieceType, tetrisCurRotation)) / 2;
    tetrisCurPieceY = 0;
    if (!tetrisCanPlace(tetrisCurPieceX, tetrisCurPieceY, tetrisCurPieceType, tetrisCurRotation)) {
      appState = STATE_GAMEOVER;
    }
  }
}

void renderTetris() {
  clearGrid();

  // Draw settled pieces
  for (int r = 0; r < TETRIS_ROWS; ++r) {
    for (int c = 0; c < TETRIS_COLS; ++c) {
      if (tetrisGrid[r][c]) {
        uint8_t t = tetrisGrid[r][c] - 1;
        CRGB col = (t < 7) ? TETRO_COLORS[t] : CRGB::White;
        setPixelXY(r, c + TETRIS_X_OFFSET, col);
      }
    }
  }

  // Draw playfield border
  int leftBorder = TETRIS_X_OFFSET - 1;
  int rightBorder = TETRIS_X_OFFSET + TETRIS_COLS;
  for (int r = 0; r < TETRIS_ROWS; ++r) {
    if (leftBorder >= 0) setPixelXY(r, leftBorder, CRGB(30,30,30));
    if (rightBorder >= 0 && rightBorder < GRID_COLS) setPixelXY(r, rightBorder, CRGB(30,30,30));
  }

  // Draw current piece
  int w = getTetroWidth(tetrisCurPieceType, tetrisCurRotation);
  int h = getTetroHeight(tetrisCurPieceType, tetrisCurRotation);
  int minc = getTetroMinCol(tetrisCurPieceType, tetrisCurRotation);
  int minr = getTetroMinRow(tetrisCurPieceType, tetrisCurRotation);
  for (int py = 0; py < 4; ++py) {
    for (int px = 0; px < 4; ++px) {
      if (!isTetroCell(tetrisCurPieceType, py, px, tetrisCurRotation)) continue;
      int gx = tetrisCurPieceX + (px - minc);
      int gy = tetrisCurPieceY + (py - minr);
      if (gy >= 0 && gy < TETRIS_ROWS && gx >= 0 && gx < TETRIS_COLS) {
        CRGB col = TETRO_COLORS[tetrisCurPieceType % 7];
        setPixelXY(gy, gx + TETRIS_X_OFFSET, col);
      }
    }
  }

  // Draw next piece in right-side preview area
  int previewX = TETRIS_X_OFFSET + TETRIS_COLS + 1; // start of preview (right margin)
  int previewY = 2;
  int nextType = tetrisNextPieceType;
  CRGB nextCol = TETRO_COLORS[nextType % 7];
  // draw a 4x4 preview box and piece
  for (int py = 0; py < 4; ++py) {
    for (int px = 0; px < 4; ++px) {
      if (isTetroCell(nextType, py, px, 0)) {
        int sx = previewX + px;
        int sy = previewY + py;
        if (sx >= 0 && sx < GRID_COLS && sy >= 0 && sy < GRID_ROWS) setPixelXY(sy, sx, nextCol);
      }
    }
  }
}
