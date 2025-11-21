#include <Arduino.h>
#include <FastLED.h>
#include <gamer_pins.h>
#include <driver/adc.h>
#include <esp_adc_cal.h>
#include <esp_sleep.h>



// Test configuration: 8 sections, 40 pixels each
#define NUM_SECTIONS 8
#define PIXELS_PER_SECTION 40

// Logical display layout derived from hardware wiring:
// Each physical section (strip) contains two horizontal rows of pixels,
// each row is PIXELS_PER_SECTION/2 pixels wide. So the logical grid is:
// GRID_ROWS = NUM_SECTIONS * 2 (16), GRID_COLS = PIXELS_PER_SECTION / 2 (20).
const uint8_t SECTION_PINS[NUM_SECTIONS] = { DISP1, DISP2, DISP3, DISP4, DISP5, DISP6, DISP7, DISP8 };
CRGB leds[NUM_SECTIONS][PIXELS_PER_SECTION];

// Logical grid
#define GRID_ROWS (NUM_SECTIONS * 2)
#define GRID_COLS (PIXELS_PER_SECTION / 2)

// Time variable for battery handling
unsigned long t_battRead = 0; // Time of last battery read

// Brightness
const uint8_t BASE_BRIGHTNESS = 40; // moderate default

// helper prototypes
void setPixelXY(int row, int col, const CRGB &c);
void clearGrid();

// --- Game states ---
enum AppState { STATE_MENU, STATE_PLAY, STATE_GAMEOVER };
AppState appState = STATE_MENU;

// Input sampling
unsigned long t_inputPoll = 0;
const unsigned long INPUT_POLL_MS = 30;

// joystick presence
bool joystickPresent = true;

// --- Snake game data ---
const int MAX_CELLS = GRID_ROWS * GRID_COLS;
uint16_t snake[MAX_CELLS]; // packed row*GRID_COLS + col
int snakeLen = 0;
int dirRow = 0, dirCol = 1; // start moving right
int pendingDirRow = 0, pendingDirCol = 1; // direction requested by input (applied at step)
uint16_t food = 0xFFFF;
unsigned long t_snakeStep = 0;
unsigned long snakeInterval = 200; // ms per move

// Debounce state for buttons
bool lastButtonA = false;
bool lastButtonB = false;
bool lastStart = false;
bool lastUp = false;
bool lastDown = false;
bool lastLeft = false;
bool lastRight = false;
bool lastJoyLeft = false;
bool lastJoyRight = false;

// Menu / multi-game
enum GameType { GAME_SNAKE = 0, GAME_BREAKOUT = 1, GAME_ART = 2 };
GameType currentGame = GAME_SNAKE;
const int MENU_ITEMS = 4;
int menuIndex = 0;
unsigned long t_menuScroll = 0;
int menuScrollOffset = 0;
int menuTargetOffset = 0;
int menuScrollPos = 0;
unsigned long t_menuMessage = 0;
const unsigned long MENU_MESSAGE_MS = 1500;
const char *menuMessage = nullptr;
bool powerPending = false;

// Breakout game data
const int BRICK_ROWS = 3;
bool bricks[BRICK_ROWS][GRID_COLS];
int paddleCol = 0;
const int PADDLE_WIDTH = 5;
int ballR = 0, ballC = 0;
int ballDirR = -1, ballDirC = 1;
unsigned long t_ballStep = 0;
unsigned long ballInterval = 150;
int breakoutScore = 0;

// forward
void spawnFood();
bool pointEquals(uint16_t a, uint16_t b);
void render();
void startNewGame();

// ADC calibration variables
esp_adc_cal_characteristics_t *adc_chars;
#define DEFAULT_VREF    1100        // Use adc2_vref_to_gpio() to obtain a better estimate
#define NO_OF_SAMPLES   64          // Multisampling for stability
#define ADC_ATTEN       ADC_ATTEN_DB_12  // 11dB attenuation (~0-3.6V)

float readBatteryVoltage();

// --- Rendering helpers ---
// Map logical (row,col) to physical section and pixel index.
inline void setPixelXY(int row, int col, const CRGB &c) {
  if (row < 0 || row >= GRID_ROWS) return;
  if (col < 0 || col >= GRID_COLS) return;
  int section = row / 2; // which strip
  if (row % 2 == 0) {
    // top row of the pair: 0..GRID_COLS-1
    leds[section][col] = c;
  } else {
    // bottom row of the pair: mapped to PIXELS_PER_SECTION-1 - col down to GRID_COLS
    int idx = PIXELS_PER_SECTION - 1 - col; // maps 0->39,19->20
    leds[section][idx] = c;
  }
}

void clearGrid() {
  for (int r = 0; r < GRID_ROWS; ++r)
    for (int c = 0; c < GRID_COLS; ++c)
      setPixelXY(r, c, CRGB::Black);
}

// spawn food in an empty cell
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

bool pointEquals(uint16_t a, uint16_t b) { return a == b; }

void render() {
  clearGrid();
  // draw food
  if (food != 0xFFFF) {
    int fr = food / GRID_COLS;
    int fc = food % GRID_COLS;
    setPixelXY(fr, fc, CRGB::Red);
  }
  // draw snake (head brighter)
  for (int i = 0; i < snakeLen; ++i) {
    int r = snake[i] / GRID_COLS;
    int c = snake[i] % GRID_COLS;
    if (i == 0) setPixelXY(r, c, CRGB::Green);
    else setPixelXY(r, c, CRGB(0, 120, 0));
  }
  FastLED.setBrightness(BASE_BRIGHTNESS);
  FastLED.show();
}

void renderBreakout() {
  clearGrid();
  // draw bricks
  for (int r = 0; r < BRICK_ROWS; ++r) {
    for (int c = 0; c < GRID_COLS; ++c) {
      if (bricks[r][c]) setPixelXY(r, c, CRGB::Orange);
    }
  }
  // draw paddle (bottom row)
  int prow = GRID_ROWS - 1;
  for (int i = 0; i < PADDLE_WIDTH; ++i) {
    int pc = constrain(paddleCol + i, 0, GRID_COLS - 1);
    setPixelXY(prow, pc, CRGB::Blue);
  }
  // draw ball
  if (ballR >= 0 && ballR < GRID_ROWS && ballC >= 0 && ballC < GRID_COLS)
    setPixelXY(ballR, ballC, CRGB::White);

  FastLED.setBrightness(BASE_BRIGHTNESS);
  FastLED.show();
}

void startNewGame() {
  snakeLen = 3;
  int sr = GRID_ROWS / 2;
  int sc = GRID_COLS / 2 - 1;
  snake[0] = sr * GRID_COLS + sc;         // head
  snake[1] = sr * GRID_COLS + (sc - 1);
  snake[2] = sr * GRID_COLS + (sc - 2);
  dirRow = 0; dirCol = 1;
  pendingDirRow = dirRow; pendingDirCol = dirCol;
  snakeInterval = 200;
  t_snakeStep = millis();
  spawnFood();
  appState = STATE_PLAY;
}

void startBreakout() {
  // initialize bricks
  for (int r = 0; r < BRICK_ROWS; ++r)
    for (int c = 0; c < GRID_COLS; ++c)
      bricks[r][c] = true;

  paddleCol = (GRID_COLS - PADDLE_WIDTH) / 2;
  // place ball just above paddle
  ballR = GRID_ROWS - 3;
  ballC = GRID_COLS / 2;
  ballDirR = -1; ballDirC = 1;
  t_ballStep = millis();
  ballInterval = 150;
  breakoutScore = 0;
  appState = STATE_PLAY;
  currentGame = GAME_BREAKOUT;
}

void startGame(GameType g) {
  currentGame = g;
  if (g == GAME_SNAKE) startNewGame();
  else if (g == GAME_BREAKOUT) startBreakout();
}

// --- Art program data ---
CRGB gridColors[GRID_ROWS][GRID_COLS];
int cursorR = 0, cursorC = 0;
// Global absolute cursor column across the concatenated menu cards (0 .. MENU_ITEMS*GRID_COLS-1)
int globalCursorCol = 0;
int selectedColorIndex = 0;
const CRGB ART_PALETTE[] = { CRGB::White, CRGB::Black, CRGB::Red, CRGB::Green, CRGB::Blue, CRGB::Yellow, CRGB::Purple, CRGB::Cyan, CRGB::Orange };
const int ART_PALETTE_LEN = sizeof(ART_PALETTE) / sizeof(ART_PALETTE[0]);

void startArt() {
  for (int r = 0; r < GRID_ROWS; ++r) for (int c = 0; c < GRID_COLS; ++c) gridColors[r][c] = CRGB::Black;
  cursorR = GRID_ROWS/2; cursorC = GRID_COLS/2;
  selectedColorIndex = 2; // default red
  appState = STATE_PLAY;
  currentGame = GAME_ART;
}


// Setup and main loop
void setup() {
  Serial.begin(115200);
  delay(50);
  Serial.println("Starting LED matrix with Snake game");

  pinMode(StatusLED, OUTPUT);
  digitalWrite(StatusLED, HIGH);

  pinMode(ChgStat, INPUT);

  // Configure ADC for battery voltage reading and joystick
  analogReadResolution(12);  // 12-bit resolution (0-4095)
  analogSetAttenuation(ADC_11db);

  adc_chars = (esp_adc_cal_characteristics_t *)calloc(1, sizeof(esp_adc_cal_characteristics_t));
  esp_adc_cal_value_t val_type = esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN, ADC_WIDTH_BIT_12, DEFAULT_VREF, adc_chars);

  // Register each section with FastLED.
  FastLED.addLeds<WS2812, DISP1, GRB>(leds[0], PIXELS_PER_SECTION);
  FastLED.addLeds<WS2812, DISP2, GRB>(leds[1], PIXELS_PER_SECTION);
  FastLED.addLeds<WS2812, DISP3, GRB>(leds[2], PIXELS_PER_SECTION);
  FastLED.addLeds<WS2812, DISP4, GRB>(leds[3], PIXELS_PER_SECTION);
  FastLED.addLeds<WS2812, DISP5, GRB>(leds[4], PIXELS_PER_SECTION);
  FastLED.addLeds<WS2812, DISP6, GRB>(leds[5], PIXELS_PER_SECTION);
  FastLED.addLeds<WS2812, DISP7, GRB>(leds[6], PIXELS_PER_SECTION);
  FastLED.addLeds<WS2812, DISP8, GRB>(leds[7], PIXELS_PER_SECTION);

  FastLED.setBrightness(BASE_BRIGHTNESS);
  clearGrid();
  FastLED.show();

  randomSeed(analogRead(ABAT));

  // init state
  appState = STATE_MENU;

  // Initialize menu cursor
  cursorR = GRID_ROWS / 2;
  cursorC = GRID_COLS / 2;
  menuIndex = 0;
  menuTargetOffset = 0;
  globalCursorCol = menuIndex * GRID_COLS + cursorC;

  // Configure inputs (buttons / D-pad) as input pullups
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

  // Detect joystick presence by sampling AX/AY once. If both read near 0, assume absent.
  int sampleAX = analogRead(AX);
  int sampleAY = analogRead(AY);
  if (sampleAX < 8 && sampleAY < 8) joystickPresent = false;
  else joystickPresent = true;
  Serial.print("Joystick present: "); Serial.println(joystickPresent ? "yes" : "no");
}

// helper to read directional input (D-pad or joystick)
void readDirectionalInput() {
  // Determine candidate direction, prioritize D-pad (digital) over joystick analog
  int candR = 0, candC = 0;
  if (digitalRead(UP) == LOW) { candR = -1; candC = 0; }
  else if (digitalRead(DOWN) == LOW) { candR = 1; candC = 0; }
  else if (digitalRead(LEFT) == LOW) { candR = 0; candC = -1; }
  else if (digitalRead(RIGHT) == LOW) { candR = 0; candC = 1; }
  else {
    // Joystick analog (only if present)
    if (joystickPresent) {
      int ax = analogRead(AX);
      int ay = analogRead(AY);
      const int JOY_DEAD = 200; // deadzone
      const int MID = 2048;
      if (abs(ax - MID) > JOY_DEAD) {
        if (ax > MID) { candR = 0; candC = 1; }
        else { candR = 0; candC = -1; }
      }
      if (abs(ay - MID) > JOY_DEAD) {
        if (ay > MID) { candR = 1; candC = 0; }
        else { candR = -1; candC = 0; }
      }
    }
  }

  // Prevent 180-degree reversal: only accept candidate if not opposite of current dir
  if (!(candR == -dirRow && candC == -dirCol) && !(candR == 0 && candC == 0)) {
    pendingDirRow = candR;
    pendingDirCol = candC;
  }
}

void loop() {
  unsigned long now = millis();

  // Poll inputs at a lower rate
  if (now - t_inputPoll >= INPUT_POLL_MS) {
    t_inputPoll = now;

    // Read buttons (active-low assumed for many GPIOs)
    bool btnA = (digitalRead(A) == LOW);
    bool btnB = (digitalRead(B) == LOW);
    bool btnStart = (digitalRead(START) == LOW);

    // handle d-pad / joystick menu navigation (edge-triggered left/right)
    bool left = (digitalRead(LEFT) == LOW);
    bool right = (digitalRead(RIGHT) == LOW);
    // analog joystick horizontal for menu nav when idle (only if present)
    int jx = 2048;
    const int MID = 2048;
    const int JOY_DEAD = 300;
    if (joystickPresent) jx = analogRead(AX);
    bool joyLeft = joystickPresent && (jx < MID - JOY_DEAD);
    bool joyRight = joystickPresent && (jx > MID + JOY_DEAD);
    if (appState == STATE_MENU) {
      // Move a global cursor across concatenated menu cards. The viewport
      // (`menuTargetOffset`) will be adjusted to keep the cursor near center,
      // producing the same scrolling UX used by the drawing program.
      if ((left && !lastLeft) || (joyLeft && !lastJoyLeft)) {
        globalCursorCol = max(0, globalCursorCol - 1);
      }
      if ((right && !lastRight) || (joyRight && !lastJoyRight)) {
        globalCursorCol = min(MENU_ITEMS * GRID_COLS - 1, globalCursorCol + 1);
      }

      // Recompute which card we're on and choose a target scroll offset that
      // keeps the cursor near the screen center. The scroll target is clamped
      // so we don't scroll past the full extent of all cards.
      menuIndex = globalCursorCol / GRID_COLS;
      int center = GRID_COLS / 2;
      int desiredOffset = globalCursorCol - center;
      int maxOffset = max(0, MENU_ITEMS * GRID_COLS - GRID_COLS);
      menuTargetOffset = constrain(desiredOffset, 0, maxOffset);

      // update joystick edge state
      lastJoyLeft = joyLeft;
      lastJoyRight = joyRight;
    }
    lastLeft = left; lastRight = right;

    // Handle START in menu/gameover
    if (appState == STATE_MENU && btnStart && !lastStart) {
      if (menuIndex == GAME_SNAKE) {
        startNewGame();
        currentGame = GAME_SNAKE;
      } else if (menuIndex == GAME_BREAKOUT) {
        // breakout not implemented yet: show message
        menuMessage = "Breakout coming soon";
        t_menuMessage = millis();
      } else if (menuIndex == GAME_ART) {
        startArt();
      } else if (menuIndex == 3) {
        // Power down selected: instruct user to release START to actually power down
        menuMessage = "Release START to power off";
        t_menuMessage = millis();
        powerPending = true;
      }
    } else if (appState == STATE_GAMEOVER && btnStart && !lastStart) {
      appState = STATE_MENU; // back to menu
    }
    lastStart = btnStart;

    // If the user selected Power and released the START button, perform deep sleep.
    if (powerPending && !btnStart) {
      powerPending = false;
      Serial.println("Preparing to enter deep sleep: clearing display and sleeping...");
      // Clear LEDs so the display appears off
      clearGrid();
      FastLED.setBrightness(0);
      FastLED.show();
      delay(50);

      // Configure START pin as pullup input and enable ext0 wake on LOW (button press)
      pinMode(START, INPUT_PULLUP);
      esp_sleep_enable_ext0_wakeup((gpio_num_t)START, 0);

      Serial.println("Entering deep sleep now. Press START to wake.");
      esp_deep_sleep_start();
    }

    // While playing, route inputs per game
      if (appState == STATE_PLAY) {
      if (currentGame == GAME_SNAKE) {
        readDirectionalInput();
        // A speeds up, B slows down
        if (btnA && !lastButtonA) { if (snakeInterval > 60) snakeInterval -= 40; }
        if (btnB && !lastButtonB) { snakeInterval += 40; }
        lastButtonA = btnA; lastButtonB = btnB;
      } else if (currentGame == GAME_ART) {
        // Art mode: joystick moves cursor (if present), D-pad also works. A paints, B cycles color
        if (joystickPresent) {
          int ax = analogRead(AX);
          int ay = analogRead(AY);
          const int MID = 2048;
          const int JOY_DEAD = 200;
          if (abs(ax - MID) > JOY_DEAD) {
            if (ax > MID) cursorC = min(GRID_COLS-1, cursorC+1);
            else cursorC = max(0, cursorC-1);
          }
          if (abs(ay - MID) > JOY_DEAD) {
            if (ay > MID) cursorR = min(GRID_ROWS-1, cursorR+1);
            else cursorR = max(0, cursorR-1);
          }
        } else {
          if (digitalRead(LEFT) == LOW) cursorC = max(0, cursorC-1);
          if (digitalRead(RIGHT) == LOW) cursorC = min(GRID_COLS-1, cursorC+1);
          if (digitalRead(UP) == LOW) cursorR = max(0, cursorR-1);
          if (digitalRead(DOWN) == LOW) cursorR = min(GRID_ROWS-1, cursorR+1);
        }
        // Buttons
        if (btnA && !lastButtonA) {
          // paint current pixel with selected color
          gridColors[cursorR][cursorC] = ART_PALETTE[selectedColorIndex];
        }
        if (btnB && !lastButtonB) {
          selectedColorIndex = (selectedColorIndex + 1) % ART_PALETTE_LEN;
        }
        lastButtonA = btnA; lastButtonB = btnB;
      } else if (currentGame == GAME_BREAKOUT) {
        // Paddle control: prefer joystick if present, else use D-pad
        if (joystickPresent) {
          int ax = analogRead(AX);
          int maxVal = 4095;
          int mapped = map(ax, 0, maxVal, 0, GRID_COLS - PADDLE_WIDTH);
          paddleCol = constrain(mapped, 0, GRID_COLS - PADDLE_WIDTH);
        } else {
          // D-pad left/right to move paddle
          bool leftPad = (digitalRead(LEFT) == LOW);
          bool rightPad = (digitalRead(RIGHT) == LOW);
          if (leftPad && !lastLeft) paddleCol = max(0, paddleCol - 1);
          if (rightPad && !lastRight) paddleCol = min(GRID_COLS - PADDLE_WIDTH, paddleCol + 1);
        }
      }
    }
  }

  // Game logic: snake stepping
  if (appState == STATE_PLAY && (now - t_snakeStep >= (unsigned long)snakeInterval)) {
    t_snakeStep = now;
    // apply pending direction (validated in readDirectionalInput)
    dirRow = pendingDirRow;
    dirCol = pendingDirCol;

    // compute new head
    int hr = snake[0] / GRID_COLS;
    int hc = snake[0] % GRID_COLS;
    int nr = hr + dirRow;
    int nc = hc + dirCol;
    // check boundaries
    if (nr < 0 || nr >= GRID_ROWS || nc < 0 || nc >= GRID_COLS) {
      appState = STATE_GAMEOVER;
    } else {
      uint16_t np = nr * GRID_COLS + nc;
      // collision with body?
      // Allow moving into the current tail position if we are NOT growing,
      // because the tail will move away in the same step.
      bool willGrow = pointEquals(np, food);
      bool hit = false;
      int checkLen = snakeLen - (willGrow ? 0 : 1);
      for (int i = 0; i < checkLen; ++i) if (snake[i] == np) { hit = true; break; }
      if (hit) { appState = STATE_GAMEOVER; }
      else {
        // move: shift array right
        for (int i = snakeLen; i > 0; --i) snake[i] = snake[i-1];
        snake[0] = np; snakeLen++;
        // ate food?
        if (pointEquals(np, food)) {
          spawnFood();
        } else {
          // remove tail
          snakeLen--;
        }
        if (snakeLen > MAX_CELLS) snakeLen = MAX_CELLS;
      }
    }
    render();
  }

  // Breakout stepping
  if (appState == STATE_PLAY && currentGame == GAME_BREAKOUT && (now - t_ballStep >= ballInterval)) {
    t_ballStep = now;
    int nr = ballR + ballDirR;
    int nc = ballC + ballDirC;
    // horizontal wall bounce
    if (nc < 0) { nc = 0; ballDirC = -ballDirC; }
    if (nc >= GRID_COLS) { nc = GRID_COLS - 1; ballDirC = -ballDirC; }
    // top wall bounce
    if (nr < 0) { nr = 0; ballDirR = -ballDirR; }
    // paddle row collision
    int paddleRow = GRID_ROWS - 1;
    if (nr >= paddleRow) {
      // if hitting paddle
      if (nc >= paddleCol && nc < paddleCol + PADDLE_WIDTH) {
        // reflect up
        nr = paddleRow - 1;
        ballDirR = -abs(ballDirR);
        // tweak horizontal based on hit position
        int hitPos = nc - paddleCol - (PADDLE_WIDTH/2);
        if (hitPos < 0) ballDirC = -1;
        else if (hitPos > 0) ballDirC = 1;
        else ballDirC = (ballDirC==0?1:ballDirC);
      } else {
        // missed paddle: game over
        appState = STATE_GAMEOVER;
      }
    }
    // brick collision
    if (nr >= 0 && nr < BRICK_ROWS) {
      if (bricks[nr][nc]) {
        bricks[nr][nc] = false;
        breakoutScore++;
        ballDirR = -ballDirR;
      }
    }
    // update ball
    ballR = nr; ballC = nc;
    renderBreakout();
  }

  // menu rendering (horizontal scroll between game cards)
    if (appState == STATE_MENU) {
    clearGrid();
    // animate scroll position toward target
    if (menuScrollPos < menuTargetOffset) menuScrollPos++;
    else if (menuScrollPos > menuTargetOffset) menuScrollPos--;

    // draw four cards positioned at x = i*GRID_COLS - menuScrollPos
    for (int i = 0; i < 4; ++i) {
      int baseX = i * GRID_COLS;
      for (int cardCol = 0; cardCol < GRID_COLS; ++cardCol) {
        int screenCol = cardCol + baseX - menuScrollPos;
        if (screenCol < 0 || screenCol >= GRID_COLS) continue;
        // card content
        if (i == 0) {
          // Snake card: draw a small snake icon in middle
          int top = GRID_ROWS/2 - 2;
          if (cardCol >= 4 && cardCol < 16) {
            int offset = cardCol - 4;
            int rr = top + (offset % 2);
            CRGB col = (menuIndex==0) ? CRGB::Lime : CRGB::Green;
            setPixelXY(rr, screenCol, col);
          }
          // title bar
          if (cardCol >= 6 && cardCol < 14) setPixelXY(1, screenCol, CRGB::White);
        } else if (i == 1) {
          // Breakout card: bricks near top
          int brow = 2;
          if (cardCol % 3 != 0) setPixelXY(brow, screenCol, (menuIndex==1)?CRGB::Yellow:CRGB::Orange);
          if (cardCol % 5 == 0) setPixelXY(brow+1, screenCol, CRGB::Orange);
        } else if (i == 2) {
          // Art card: show a palette strip at bottom and a cursor icon
          int brow = GRID_ROWS - 3;
          if (cardCol < ART_PALETTE_LEN) setPixelXY(brow, screenCol, ART_PALETTE[cardCol % ART_PALETTE_LEN]);
          // small cursor icon
          if (cardCol == GRID_COLS/2) setPixelXY(GRID_ROWS/2, screenCol, CRGB::White);
        } else if (i == 3) {
          // Power card: simple power icon
          if (cardCol >= GRID_COLS/2 - 2 && cardCol <= GRID_COLS/2 + 2) {
            int pr = GRID_ROWS/2 - (abs(cardCol - GRID_COLS/2) < 2 ? 1 : 0);
            setPixelXY(pr, screenCol, CRGB::Red);
          }
          if (cardCol == GRID_COLS/2) setPixelXY(GRID_ROWS/2 + 2, screenCol, CRGB::White);
        }
        // highlight border for selected card
        if (i == menuIndex) {
          // vertical highlight on left/right of the card area
          if (cardCol == 0 || cardCol == GRID_COLS-1) {
            for (int r = 0; r < GRID_ROWS; ++r) setPixelXY(r, screenCol, CRGB::White);
          }
        }
      }
    }

    // show temporary menu message if set
    if (menuMessage && (now - t_menuMessage) < MENU_MESSAGE_MS) {
      // draw message as centered bar
      int msgRow = GRID_ROWS/2;
      for (int c = 4; c < GRID_COLS-4; ++c) setPixelXY(msgRow, c, CRGB::White);
    } else {
      menuMessage = nullptr;
    }

    FastLED.setBrightness(BASE_BRIGHTNESS);
    FastLED.show();
  }
  // draw menu cursor indicator when in menu
  if (appState == STATE_MENU) {
    // compute screen-local cursor column from global cursor and current scroll
    int screenCursor = globalCursorCol - menuScrollPos;
    if (screenCursor >= 0 && screenCursor < GRID_COLS) {
      int crow = GRID_ROWS - 2;
      setPixelXY(crow, screenCursor, CRGB::White);
    }
    FastLED.setBrightness(BASE_BRIGHTNESS);
    FastLED.show();
  }

  if (appState == STATE_GAMEOVER) {
    // flash red
    clearGrid();
    for (int r = 0; r < GRID_ROWS; ++r) for (int c = 0; c < GRID_COLS; ++c) setPixelXY(r, c, CRGB::DarkRed);
    FastLED.setBrightness(BASE_BRIGHTNESS);
    FastLED.show();
  }

  // Art rendering while playing
  if (appState == STATE_PLAY && currentGame == GAME_ART) {
    clearGrid();
    // draw canvas
    for (int r = 0; r < GRID_ROWS; ++r) for (int c = 0; c < GRID_COLS; ++c) setPixelXY(r, c, gridColors[r][c]);
    // draw palette on top-right corner
    int prow = 0;
    for (int i = 0; i < ART_PALETTE_LEN && i < GRID_COLS; ++i) setPixelXY(prow, i, ART_PALETTE[i]);
    // draw cursor as an outline (invert color)
    setPixelXY(cursorR, cursorC, CRGB::White);
    FastLED.setBrightness(BASE_BRIGHTNESS);
    FastLED.show();
  }

  // Periodically check battery every 2s (unchanged)
  if (now - t_battRead > 2000) {
    t_battRead = now;

    // Raw ADC for diagnostics
    int raw_adc = analogRead(ABAT);
    float voltage = readBatteryVoltage();

    Serial.println("-------------------------------");
    Serial.print("Raw ADC Value: ");
    Serial.println(raw_adc);
    Serial.print("Calibrated Battery Voltage: ");
    Serial.print(voltage, 2);
    Serial.println(" V");

    // Check if battery likely present (adjust threshold as needed)
    if (raw_adc < 3000) {
      // Battery connected - check charge status
      if (digitalRead(ChgStat) == LOW) {
        Serial.println("Battery is charging");
      } else {
        Serial.println("Battery is full / not charging");
      }
    } else {
      Serial.println("No battery detected!");
    }
    Serial.println();
  }

  delay(5);
}

float readBatteryVoltage() {
  uint32_t adc_reading = 0;
  for (int i = 0; i < NO_OF_SAMPLES; i++) {
    adc_reading += analogRead(ABAT);
  }
  adc_reading /= NO_OF_SAMPLES;

  uint32_t voltage_mv = esp_adc_cal_raw_to_voltage(adc_reading, adc_chars);
  float battery_voltage = (voltage_mv / 1000.0) * 2.0; // account for divider
  battery_voltage += 0.01; // small offset for FET drop
  return battery_voltage;
}
