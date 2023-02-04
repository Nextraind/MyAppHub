
#define F_CPU 16000000L

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/eeprom.h>
#include "mario.h"
#include "mario_tiles.h"
#include "bricks.h"
#include "font_digits.h"
#include "oled_lib.h"

#define TEST_BUTTONS 1


struct music_table {
  uint16_t startpos;
  uint8_t len;
};

enum mario_state {idle, left, right, squash, dead};
enum mario_jumpstate {nojump, jumpup, jumpdown};
enum mario_direction {faceleft, faceright};
enum game_state {normal, paused, paused_ready, pause_return, mario_dying};

struct character {
  int16_t x, y;
  int16_t oldx[2], oldy[2];
  enum mario_state state;
  enum mario_jumpstate jumpstate;
  enum mario_direction dir;
  int8_t vx, vy;
  uint8_t frame;
  uint8_t collision;
  uint8_t coincollector;
  uint8_t mask;
  uint8_t w;
};

#define MAX_GOOMBAS 3

struct character mario, goomba[MAX_GOOMBAS], fireball;

struct toneController {
  uint8_t music_index, music_pos, music_note, noteordelay;
  uint16_t current_delay;
  uint8_t soundeffect; // 0 = Music, 1=Sound effect (affects play priority and loop behaviour)
};

struct toneController MusicController;
struct toneController SoundEffectController;

#define COIN_SOUND 6
#define JUMP_SOUND 7
#define HITHEAD_SOUND 8
#define PAUSE_SOUND 9

#define FIREBALL_SPEED 3

const uint8_t music_seq[] = {0, 1, 1, 2, 3, 2, 4, 2, 3, 2, 4, 5}; // 12 elements: 0-11. Go back to position 1 on overflow.

// soundeffect = 0: 1 passed onto myTone: Timer1 is used for Music - SPEAKER 
// soundeffect = 1: 0 passed onto myTone: Timer0 is used for sound effects - SPEAKER2

//#define SINGLE_SPEAKER // means we want to mix MUSIC and SOUND_EFFECTS in hardware (using SOUND_EFFECTS pin PB1), else have a pin for each

#define MUSIC_SPEAKER 4 // IC pin 3: PB4, Timer 1, OC1B - MUSIC (was speaker)
#define SOUNDEFFECT_SPEAKER 1// IC pin 6: PB1, Timer 0, OCOB - Sound effects (was speaker 2)

#define LEFT_OLED_ADDRESS 0x3C;
#define RIGHT_OLED_ADDRESS 0x3D; // swapped
#define OLED_CONTRAST 0xFF //0x7F

#define BUTTON_LEFT 4
#define BUTTON_A 2
#define BUTTON_RIGHT 1

#define BUTTON2_DOWN 1
#define BUTTON2_B 2
#define BUTTON2_SELECT 4

#define WORLD_MAX_LEN 63

#define ADC0 0
#define ADC1 1
#define ADC2 2
#define ADC3 3

#define TEMPO_DEFAULT 800
#define TEMPO_FAST 500

uint8_t screen[WORLD_MAX_LEN + 1], soundeffectplaying = 0;

uint16_t viewx = 0, old_viewx = 0, rviewx_trigger, MusicSpeed = TEMPO_DEFAULT, mario_acc_timer = 0;
int delta_viewx = 0;

volatile long mymicros=0,ISR_micro_period=0; 
volatile long tone_timer1_toggle_count=0, tone_timer0_toggle_count = 0;

uint8_t coinframe = 1, cointimer = 0, coincount = 0;
uint32_t curr_seed = 0;
enum game_state gamestate = normal;

void drawSprite(int16_t, int16_t, uint8_t, uint8_t, const unsigned char *, uint8_t);
void getWorld(uint16_t);
void drawScreen(void);
void playSoundEffect(uint8_t);
void initMusic(struct toneController *);
void handleMusic(struct toneController *);
void mytone(unsigned long, unsigned long, uint8_t);
void handlemap_collisions(struct character *);
void collidecoins(uint8_t, uint8_t, uint8_t);
uint8_t collideThings(int, int, int, int, int, int, uint8_t, uint8_t);
uint8_t collideMario(int, int, uint8_t, uint8_t);
uint8_t findcoiny(uint8_t);
void animate_character(struct character *);
void draw_mario(void);
void blank_character(uint8_t, struct character *);
void vblankout(int, int, uint8_t);
void readbuttons(void);
void drawCoin(int, uint8_t);
void updateDisplay(uint8_t);
void oledWriteInteger(uint8_t, uint8_t, uint8_t);
void setup(void);
uint16_t readADC(uint8_t);
uint8_t next_random(uint8_t);