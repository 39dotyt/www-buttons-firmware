#include <MsTimer2.h>

#define PULSE_WIDTH_USEC 5                   // Save/load impulse for 74HC165.

#define CASC_DS 2
#define CASC_ST_CP 3
const int casc_sh_cp =  4; // clock pin
const int buzzer = 9; // buzzer pin
const int btn_sh_ld = 5;
const int btn_clk = 6;
const int btn_qh = 7;
const int fstart = 10;
const int reset = 11;
const int tmblr = 12;

// left buttons  : blue - 4 (7); green - 5 (5); yellow - 6 (3); red - 7 (1).
// right buttons : blue - 3 (8); green - 2 (6); yellow - 1 (4); red - 0 (2).

char *buttons[8];
const int digits[8] = {2, 4, 6, 8, 7, 5, 3, 1};

boolean casc_state[24] = {0, 0, 0, 0, 0, 0,
                          0, 0, 0, 0, 0, 0,
                          0, 0, 0, 0, 0, 0, 
                          0, 0, 0, 0, 0, 0};
const boolean numbers[23][8] = {{0, 0, 0, 0, 0, 0, 1, 1},
                                {1, 0, 0, 1, 1, 1, 1, 1},
                                {0, 0, 1, 0, 0, 1, 0, 1},
                                {0, 0, 0, 0, 1, 1, 0, 1},
                                {1, 0, 0, 1, 1, 0, 0, 1},
                                {0, 1, 0, 0, 1, 0, 0, 1},
                                {0, 1, 0, 0, 0, 0, 0, 1},
                                {0, 0, 0, 1, 1, 1, 1, 1},
                                {0, 0, 0, 0, 0, 0, 0, 1},
                                {0, 0, 0, 0, 1, 0, 0, 1},
                                {0, 0, 0, 0, 0, 0, 1, 0},
                                {1, 0, 0, 1, 1, 1, 1, 0},
                                {0, 0, 1, 0, 0, 1, 0, 0},
                                {0, 0, 0, 0, 1, 1, 0, 0},
                                {1, 0, 0, 1, 1, 0, 0, 0},
                                {0, 1, 0, 0, 1, 0, 0, 0},
                                {0, 1, 0, 0, 0, 0, 0, 0},
                                {0, 0, 0, 1, 1, 1, 1, 0},
                                {0, 0, 0, 0, 0, 0, 0, 0},
                                {0, 0, 0, 0, 1, 0, 0, 0},
                                {1, 1, 1, 1, 1, 1, 1, 1},
                                {1, 1, 1, 1, 1, 1, 1, 0},
                                {1, 1, 1, 1, 1, 0, 1, 1}};
                          
const int sounds[8] = {60, 80, 100, 120, 140, 160, 180, 200};

struct pressed_button {
  short button_id;
  short cycles_beeped;
};
pressed_button *order = new pressed_button[8];
int beep_pointer = 0;

#define BEEP_CYCLES 20

struct button_state {
  boolean state;
  boolean false_start;
  short cycles_away;
};

button_state *buttons_state = new button_state[8];

#define MAX_INACTIVE_CYCLES 2

boolean just_one_pressed; // indicates whether button has been pressed, this is important for tumbler

boolean locked;
boolean tmblr_state;
boolean fstart_state;

void setup() {
  Serial.begin(9600);
  pinMode(CASC_DS, OUTPUT);
  pinMode(CASC_ST_CP, OUTPUT);
  pinMode(casc_sh_cp, OUTPUT);
  pinMode(buzzer, OUTPUT);
  pinMode(btn_sh_ld, OUTPUT);
  pinMode(btn_clk, OUTPUT);
  pinMode(btn_qh, INPUT);
  pinMode(fstart, INPUT);
  pinMode(reset, INPUT);
  pinMode(tmblr, INPUT);
  digitalWrite(btn_clk, HIGH);
  digitalWrite(btn_sh_ld, HIGH);
  for (int i = 0; i < 8; ++i) {
    buttons[i] = new char[2];
  }
  buttons[0] = "lr";
  buttons[1] = "rr";
  buttons[2] = "ly";
  buttons[3] = "ry";
  buttons[4] = "lg";
  buttons[5] = "rg";
  buttons[6] = "lb";
  buttons[7] = "rb";
  resetApp();
  MsTimer2::set(25, mloop);
  MsTimer2::start();
}

void resetApp() {
  analogWrite(buzzer, 0);
  just_one_pressed = false;
  locked = false;
  tmblr_state = false;
  fstart_state = false;
  beep_pointer = 0;
  for (int i = 0; i < 8; ++i) {
    display_number(i + 1, 21);
    order[i].button_id = 0;
    order[i].cycles_beeped = 0;
    buttons_state[i].state = false;
    buttons_state[i].false_start = false;
    buttons_state[i].cycles_away = 0;
  }
}

void loop() {
  /* waiting for bluetooth commands */
  if (Serial.available())
  {
    char command = Serial.read();
    switch (command)
    {
      case 'r':
        resetApp();
        break;
      case 'f':
        fstart_state = fstart_state ? false : true;
        break;
      case 't':
        tmblr_state = tmblr_state ? false : true;
        break;
      case 'l':
        locked = locked ? false : true;
        break;
      default:
        break;
    }
  }
}

void mloop() {
  beep();
  if (digitalRead(reset) == HIGH) {
    resetApp();
    return;
  }
  if (!locked) {
    tmblr_state = digitalRead(tmblr) == HIGH ? true : false;
    fstart_state = digitalRead(fstart) == HIGH ? true : false;
  }
  if (tmblr_state && just_one_pressed) {
    return;
  }
  readButtonsState();
  for (int i = 0; i < 8; ++i) {
    if (buttons_state[i].state) {
      if (fstart_state) {
        buttons_state[i].false_start = true;
      } else {
        if (buttons_state[i].false_start) {
          continue;
        }
        buttonPressed(digits[i]);
        just_one_pressed = true;
        if (tmblr_state) {
          return;
        }
      }
    }
  }
}

void readButtonsState() {
  digitalWrite(btn_sh_ld, LOW);
  digitalWrite(btn_sh_ld, HIGH);
  int bstate;
  for (int i = 0; i < 8; ++i) {
    bstate = digitalRead(btn_qh);
    if (bstate == LOW) {
      if (buttons_state[i].state) {
        if (buttons_state[i].cycles_away == MAX_INACTIVE_CYCLES) {
          buttons_state[i].state = false;
          buttons_state[i].false_start = false;
          buttons_state[i].cycles_away = 0;
        } else {
          ++buttons_state[i].cycles_away;
        }
      }
    } else {
      buttons_state[i].state = true;
      buttons_state[i].cycles_away = 0;
    }
    digitalWrite(btn_clk, LOW);
    digitalWrite(btn_clk, HIGH);
  }
}

void buttonPressed(int button) {
  for (int i = 0; i < 8; ++i) {
    if (order[i].button_id == button) {
      return;
    } else if (order[i].button_id == 0) {
      order[i].button_id = button;
      display_number(button, i + 1);
      Serial.print(buttons[button - 1]);
      Serial.println(i + 1);
      return;
    }
  }
}

void beep() {
  if (order[beep_pointer].cycles_beeped == BEEP_CYCLES) {
    ++beep_pointer;
  }
  if (beep_pointer != 8) {
    if (order[beep_pointer].button_id != 0) {
      if (order[beep_pointer].cycles_beeped == 0) {
        analogWrite(buzzer, sounds[order[beep_pointer].button_id - 1]);
      }
      ++order[beep_pointer].cycles_beeped;
      return;
    }
  }
  analogWrite(buzzer, 0);
}

void display_number(int at, int number) {
  /**
   * at should be in [1:8] and number in [0:21], where 0:9 is numbers without dot and 10-19 - with,
   * 20 completely switches of led and 21 blip only dot.
   */
  int ds = 3 * (at - 1);
  int st_cp = ds + 1;
  int sh_cp = ds + 2;
  casc_state[st_cp] = 0;
  write_casc_state();
  casc_state[ds] = 0;
  write_casc_state();
  for (int i = 7; i >= 0; --i) {
    casc_state[sh_cp] = 0;
    write_casc_state();
    casc_state[ds] = numbers[number][i];
    write_casc_state();
    casc_state[sh_cp] = 1;
    write_casc_state();
    casc_state[ds] = 0;
    write_casc_state();
  }
  casc_state[st_cp] = 1;
  write_casc_state();
}

void write_casc_state() {
  digitalWrite(CASC_ST_CP, LOW);
  for (int i = 2; i >= 0; --i) {
    int data = 0;
    for (int j = 7; j >= 0; --j) {
      boolean cstate = casc_state[8 * i + j];
      if (cstate) {
        data += pow(int(2 * cstate), int(7 - j));
      }
    }
    shiftOut(CASC_DS, casc_sh_cp, LSBFIRST, data);
  }
  digitalWrite(CASC_ST_CP, HIGH);
}

void set_casc_state(boolean state) {
  for (int i = 0; i < 24; ++i) {
    casc_state[i] = state;
  }
}

int pow(int number, int exponent) {
  /* because internal does not work properly or i am a dumbass */
  int result = 1;
  for (int i = 0; i < exponent; ++i) {
    result *= number;
  }
  return result;
}

