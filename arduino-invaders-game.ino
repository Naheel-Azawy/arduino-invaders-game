#define USE_I2C_LCD
#define BUZZER_PIN 4
#define JOYSTICK_X_PIN A3
#define JOYSTICK_Y_PIN A2
#define JOYSTICK_S_PIN 5
//#define AUTOPILOT

#ifndef USE_I2C_LCD
#include <LiquidCrystal.h>
const int rs = 12, en = 11, d4 = 5, d5 = 4, d6 = 3, d7 = 7;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);
#else
#include <LiquidCrystal_I2C.h>
LiquidCrystal_I2C lcd(0x27, 16, 2);
#endif

byte inv_left[8] = {0b00000,
                    0b00000,
                    0b00000,
                    0b10110,
                    0b01011,
                    0b01110,
                    0b01011,
                    0b10110};

byte inv_right[8] = {0b10110,
                     0b01011,
                     0b01110,
                     0b01011,
                     0b10110,
                     0b00000,
                     0b00000,
                     0b00000};

byte player_left[8] = {0b00000,
                       0b00000,
                       0b00000,
                       0b00011,
                       0b00011,
                       0b00111,
                       0b00011,
                       0b00011};

byte player_right[8] = {0b00011,
                        0b00011,
                        0b00111,
                        0b00011,
                        0b00011,
                        0b00000,
                        0b00000,
                        0b00000};

byte player_bullet_left[8] = {0b00000,
                              0b00000,
                              0b00000,
                              0b00000,
                              0b00000,
                              0b01110,
                              0b00000,
                              0b00000};

byte player_bullet_right[8] = {0b00000,
                               0b00000,
                               0b01110,
                               0b00000,
                               0b00000,
                               0b00000,
                               0b00000,
                               0b00000};

byte inv_bullet_left[8] = {0b00000,
                           0b00000,
                           0b00000,
                           0b00000,
                           0b00010,
                           0b10101,
                           0b01000,
                           0b00000};

byte inv_bullet_right[8] = {0b00000,
                            0b00010,
                            0b10101,
                            0b01000,
                            0b00000,
                            0b00000,
                            0b00000,
                            0b00000};

// types
#define NONE  0xff
#define PLR   0x0
#define ALN   0x1
#define PLR_B 0x2
#define ALN_B 0x3

// if types a and b are in fr and to
#define types_in(fr, to, a, b)                        \
  (((1 << fr) | (1 << to)) == ((1 << a) | (1 << b)))

// shapes (ored with the type). Used also for directions
#define LEFT  0x0
#define RIGHT 0x4

// remaining of directions
#define UP   0x10
#define DOWN 0x20

// space dimensions
#define W 4
#define H 16

// helpers
#define type(thing)  thing.type & 0x3
#define shape(thing) thing.type & 0x4

typedef struct {
  byte type;
  byte speed;
  bool done;
} thing;

// game matrix
thing space[W][H];

// used to store movements temporarily
thing space_tmp[W][H];

// player i location
int player;

// measured by how many things the player killed
int score;

bool game_over;

// https://www.norwegiancreations.com/2017/09/arduino-tutorial-using-millis-instead-of-delay/
unsigned long time_now = 0;

void clear() {
  for (int i = 0; i < W; ++i) {
    for (int j = 0; j < H; ++j) {
      space[i][j].type = NONE;
      space[i][j].done = true;
    }
  }
}

void printSerial() {
  for (int j = 0; j < H; ++j) {
    for (int i = 0; i < W; ++i) {
      switch (space[i][j].type) {
      case NONE:  Serial.print('.'); break;
      case PLR:   Serial.print('_'); break;
      case ALN:   Serial.print('#'); break;
      case PLR_B: Serial.print('^'); break;
      case ALN_B: Serial.print('!'); break;
      }
    }
    Serial.println();
  }
  Serial.println();
}

void draw() {
  int ci, cj, i, j;
  for (ci = 0; ci < H; ++ci) {
    for (cj = 0; cj < 2; ++cj) {
      lcd.setCursor(ci, cj);
      j = ci;

      if (cj == 1) {
        if (space[0][j].type == NONE && space[1][j].type == NONE) {
          lcd.write(' ');
        } else {
          i = 0;
          if (space[i][j].type != NONE)
            lcd.write(space[i][j].type | LEFT);
          i = 1;
          if (space[i][j].type != NONE)
            lcd.write(space[i][j].type | RIGHT);
        }
      } else {
        if (space[2][j].type == NONE && space[3][j].type == NONE) {
          lcd.write(' ');
        } else {
          i = 2;
          if (space[i][j].type != NONE)
            lcd.write(space[i][j].type | LEFT);
          i = 3;
          if (space[i][j].type != NONE)
            lcd.write(space[i][j].type | RIGHT);
        }
      }

    }
  }
  for (int i = 0; i < W; ++i) {
    for (int j = 0; j < H; ++j) {
      space[i][j].done = true;
    }
  }
  printSerial();
}

String type_str(byte t) {
  switch (t) {
  case NONE:  return "none";
  case PLR:   return "player";
  case ALN:   return "alien";
  case PLR_B: return "player's bullet";
  case ALN_B: return "alien's bullet";
  default:    return "unknown";
  }
}

int mod(int x, int N){
  return (x % N + N) % N;
}

int thing_move(int fr_i, int fr_j, int dir) {
  int to_i = fr_i, to_j = fr_j;
  switch (dir) {
  case UP:
    --to_j;
    break;
  case DOWN:
    ++to_j;
    break;
  case LEFT:
    to_i = mod(to_i - 1, W);
    break;
  case RIGHT:
    to_i = mod(to_i + 1, W);
    break;
  }

  byte fr_type = space[fr_i][fr_j].type;
  byte to_type = space[to_i][to_j].type;

  // goes outside
  if (to_j < 0 || to_j >= H) {
    space[fr_i][fr_j].type = NONE;
    space[fr_i][fr_j].done = false;
    Serial.print(type_str(fr_type));
    Serial.print(" moved outside to ");
    Serial.print(to_i); Serial.print(","); Serial.println(to_j);
    return -1;
  }

  // anything touches the player, game over
  if ((fr_type == PLR && to_type != NONE) ||
      (to_type == PLR && fr_type != NONE)) {
    // killer
    byte k_i, k_j, k_type;
    if (to_type == PLR) {
      k_i = fr_i, k_j = fr_j, k_type = fr_type;
    } else {
      k_i = to_i, k_j = to_j, k_type = to_type;
    }
    Serial.print(type_str(k_type)); Serial.print(" killed player at ");
    Serial.print(k_i); Serial.print(","); Serial.println(k_j);
    game_over = true;
    return -1;
  }

  // anything touches player's bullet dies
  if ((to_type == PLR_B && fr_type != NONE) ||
      (fr_type == PLR_B && to_type != NONE)) {
    // victim
    byte v_i, v_j, v_type;
    if (to_type == PLR_B) {
      v_i = fr_i, v_j = fr_j, v_type = fr_type;
    } else {
      v_i = to_i, v_j = to_j, v_type = to_type;
    }
    // kill it and remove the bullet too
    space[fr_i][fr_j].type = NONE;
    space[fr_i][fr_j].done = false;
    space[to_i][to_j].type = NONE;
    space[to_i][to_j].done = false;
    ++score;
    tone(BUZZER_PIN, 500);
    delay(50);
    noTone(BUZZER_PIN);
    delay(50);
    Serial.print("player killed "); Serial.print(type_str(v_type));
    Serial.print(" at ");           Serial.print(v_i);
    Serial.print(",");              Serial.println(v_j);
    return -1;
  }

  space[to_i][to_j] = space[fr_i][fr_j];
  int res = to_i;

  Serial.print("moved "); Serial.print(type_str(fr_type));
  Serial.print(" from "); Serial.print(fr_i);
  Serial.print(",");      Serial.print(fr_j);
  Serial.print(" to ");   Serial.print(to_i);
  Serial.print(",");      Serial.println(to_j);
  space[fr_i][fr_j].type = NONE;
  space[fr_i][fr_j].done = false;
  space[to_i][to_j].done = false;

  return res;
}

void reset() {
  clear();
  space[0][H - 1].type = PLR;
  player = 0;
  score = 0;
  game_over = false;
  draw();
}

void setup() {

  Serial.begin(9600);

#ifndef USE_I2C_LCD
  lcd.begin(16, 2);
#else
  lcd.init();
  lcd.backlight();
#endif

  lcd.createChar(ALN   | LEFT,  inv_left           );
  lcd.createChar(ALN   | RIGHT, inv_right          );
  lcd.createChar(PLR   | LEFT,  player_left        );
  lcd.createChar(PLR   | RIGHT, player_right       );
  lcd.createChar(PLR_B | LEFT,  player_bullet_left );
  lcd.createChar(PLR_B | RIGHT, player_bullet_right);
  lcd.createChar(ALN_B | LEFT,  inv_bullet_left    );
  lcd.createChar(ALN_B | RIGHT, inv_bullet_right   );

  pinMode(JOYSTICK_S_PIN, INPUT);
  digitalWrite(JOYSTICK_S_PIN, HIGH);

  reset();

}

bool joystick_fire = false;
byte joystick_dir = NONE;

void check_joystick() {
  int s, x, y;

  s = digitalRead(JOYSTICK_S_PIN);
  if (s == LOW) {
    joystick_fire = true;
  }

  y = analogRead(JOYSTICK_Y_PIN);
  if (y < 341) {
    joystick_dir = DOWN;
  } else if (y > 682) {
    joystick_dir = UP;
  }

  x = analogRead(JOYSTICK_X_PIN);
  if (x < 341) {
    joystick_dir = LEFT;
  } else if (x > 682) {
    joystick_dir = RIGHT;
  }

  /*Serial.print("JOYSTICK: ");
    Serial.print(s); Serial.print(" - ");
    Serial.print(x); Serial.print(" - ");
    Serial.println(y);*/
}

void loop() {

  check_joystick();

  // main loop
  int period = 700 / log(M_E + score);
  if (millis() >= time_now + period) {
    time_now += period;

    int r;

    // move things
    for (int i = 0; i < W; ++i) {
      for (int j = 0; j < H; ++j) {
        if (!space[i][j].done) continue;
        if (space[i][j].type == ALN) {
          r = random() % 5;
          if (r < 1 && j + 1 < H && space[i][j + 1].type != ALN_B) {
            thing_move(i, j, DOWN);
          }
          r = random() % 3;
          if (r == 0) {
            thing_move(i, j, LEFT);
          } else if (r == 1) {
            thing_move(i, j, RIGHT);
          }
        } else if (space[i][j].type == ALN_B) {
          thing_move(i, j, DOWN);
        } else if (space[i][j].type == PLR_B) {
          thing_move(i, j, UP);
        }

        check_joystick();
      }
    }

    if (game_over) {
      clear();
      draw();
      lcd.setCursor(0, 0);
      lcd.print("GAME OVER!");
      lcd.setCursor(0, 1);
      lcd.print("SCORE: ");
      lcd.print(score);
      Serial.print("GAME OVER! SCORE: ");
      Serial.println(score);

      for (int i = 0; i < 3; ++i) {
        tone(BUZZER_PIN, 200);
        delay(100);
        noTone(BUZZER_PIN);
        delay(100);
      }

      delay(3000);
      reset();
      return;
    }

#ifdef AUTOPILOT
    // move player
    r = random() % 3;
    if (r == 0) {
      player = thing_move(player, H - 1, RIGHT);
    } else if (r == 1) {
      player = thing_move(player, H - 1, LEFT);
    }

    // fire
    r = random() % 10;
    if (r == 0) {
      space[player][H - 2].type = PLR_B;
    }
#else
    // move player
    if (joystick_dir == RIGHT) {
      player = thing_move(player, H - 1, RIGHT);
      joystick_dir = NONE;
    } else if (joystick_dir == LEFT) {
      player = thing_move(player, H - 1, LEFT);
      joystick_dir = NONE;
    }

    // fire
    if (joystick_fire) {
      space[player][H - 2].type = PLR_B;
      joystick_fire = false;
    }
#endif

    check_joystick();

    // make alien
    bool first_line_empty = true;
    for (int i = 0; i < W; ++i) {
      if (space[i][0].type != NONE) {
        first_line_empty = false;
      }
    }
    r = random() % 10;
    if (first_line_empty && r < W) {
      space[r][0].type = ALN;
    }

    check_joystick();

    // alien fire
    for (int i = 0; i < W; ++i) {
      for (int j = 0; j < H / 2; ++j) {
        if (space[i][j].type == ALN) {
          r = random() % 10;
          if (r == 0) {
            space[i][j + 1].type = ALN_B;
          }
        }
      }
    }

    check_joystick();

    draw();
  }

}
