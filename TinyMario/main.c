
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
void killGoomba(uint8_t g_id);
void spawnGoomba(uint16_t);

void setup() {
	//_delayms(50);
	oledInit(0x3c, 0, 0);
	oledFill(0);
	oledInit(0x3d, 0, 0);
	oledFill(0);

	//oledFill(0b00001111);

	mario.mask = 3; //fireball.mask = 3;
	mario.w = 2;//fireball.w = 2;
	
	for (uint8_t i=0;i<MAX_GOOMBAS;i++)
	{
		goomba[i].mask = 3;
		goomba[i].w = 2;
		goomba[i].state=dead;
	}
	
	mario.x = 5;
	mario.y = 0;
	mario.state = idle;
	mario.jumpstate = jumpdown;
	mario.vx = 0;
	mario.vy = 0;
	mario.frame = 0;
	mario.dir = faceright;
	mario.coincollector = 1;

	fireball.mask=1;
	fireball.w=1;
	fireball.state=dead;
	
	//analogReference(0); // 0 = Vcc as Aref (2 = 1.1v internal voltage reference)
	
	//pinMode(SPEAKER, OUTPUT);
	//pinMode(SPEAKER2, OUTPUT);

#ifdef SINGLE_SPEAKER
	DDRB |= (1 << SOUNDEFFECT_SPEAKER);
#else
	DDRB |= (1 << MUSIC_SPEAKER); // OUTPUT
	DDRB |= (1 << SOUNDEFFECT_SPEAKER); // OUTPUT
#endif
	// Setup screen
	getWorld(0); //WORLD_MAX_LEN + 1); //,16);
	getWorld(16);
	getWorld(32);
	getWorld(48);

	rviewx_trigger = 24;//12;// if viewx > trigger*8 getWorld element in certain position.
	//drawScreen();

	MusicController.soundeffect = 0;
	SoundEffectController.soundeffect = 1;
	soundeffectplaying = 0;
	initMusic(&MusicController);

	// Change music speaker to a hardware pin that Timer1 supports - i.e. pin4 = PB4
	TCCR0A = 0;//notone0A();
	TIMSK = 0;

	TCCR1 = 0 ;
	GTCCR = 0;
	TIFR = 0;
	
	//goomba.x = 100; goomba.y = 0;
	//goomba.vx = 1; goomba.state=dead;

}


int main(void)
{
	static long loop_micros=0;
	
	setup();
	
	sei();
	
	while (1)
	{
		readbuttons();

		if (gamestate == normal)
		{
			handlemap_collisions(&mario); // Check for collisions with Mario and the world
			
			if (fireball.state!=dead)
			{
				handlemap_collisions(&fireball);
				if (fireball.y<0 || fireball.y>=63) {fireball.frame=90;fireball.y=0;fireball.vy=0;} 
				
				// 1 = UP, 2=RIGHT, 4=DOWN, 8=LEFT
				if (fireball.collision &1) fireball.vy=FIREBALL_SPEED;
				if (fireball.collision &4) fireball.vy=-FIREBALL_SPEED;
				if (fireball.collision &2) fireball.vx=-FIREBALL_SPEED;
				if (fireball.collision &8) fireball.vx=FIREBALL_SPEED;

				if (fireball.collision!=0) fireball.frame+=30;

				fireball.frame++;
				if (fireball.frame>100) fireball.state=dead;
			}

			animate_character(&mario); // just applies gravity, could be done elsewhere (i.e. handlemap_collisions physics?)
			
			for (uint8_t g=0;g<MAX_GOOMBAS;g++)
			{
				if (goomba[g].state!=dead) // Not dead. Alive you might say.
				{
					handlemap_collisions(&goomba[g]);
					animate_character(&goomba[g]);
				
					if (goomba[g].collision & 2) goomba[g].vx = -1;
					else if (goomba[g].collision & 8) goomba[g].vx = 1;

					if (goomba[g].x <= 0) {
						goomba[g].x = 0;
						goomba[g].vx = 1;
					}						// rviewx_trigger+40 is the *next* screen to write (i.e. doesn't exist yet), so turn around goomba if near edge
					if ((goomba[g].x>>3) >= rviewx_trigger+38) goomba[g].vx=-1; // if goomba attempst to walk off the edge of the known universe
					goomba[g].frame++;
					if (goomba[g].state==idle && goomba[g].frame >= 10) goomba[g].frame = 0;
					if (goomba[g].state==squash && goomba[g].frame>200) goomba[g].state=dead;
				} // end enemy not dead
				
				if (goomba[g].state==idle)
				{
					if (collideMario(goomba[g].x, goomba[g].y, 16, 16))
					{
						if (mario.vy > 0 && mario.jumpstate==jumpdown) {killGoomba(g);mario.vy = -7;mario.jumpstate = jumpup;playSoundEffect(JUMP_SOUND);}
						else playSoundEffect(HITHEAD_SOUND);
					}
					if (fireball.state==idle && collideThings(goomba[g].x, goomba[g].y, 16,16,fireball.x, fireball.y,8,8)) killGoomba(g);
					if (goomba[g].x<((int16_t)viewx-40)) killGoomba(g);
				}
			} // end goomba loop
			
			
			
			if (mario.frame++ >= 6) mario.frame = 0;

			cointimer++;  // Animate coin
			if (cointimer >= 5)
			{
				cointimer = 0;
				coinframe++;
				if (coinframe >= 5) coinframe = 1;
			}

			if (coincount >= 10 && coincount < 20) MusicSpeed = TEMPO_FAST; else MusicSpeed = TEMPO_DEFAULT;

			if (mario.x <= 192) viewx = 0;
			else if (viewx < mario.x - 192) viewx = mario.x - 192; // Do we need to change the viewport coordinate?

			uint16_t blockx = viewx >> 3; // viewx / 8;
			if (blockx >= rviewx_trigger) // convert viewx to blocks
			{
				getWorld(rviewx_trigger + 40);

				rviewx_trigger += 16;
			}

		} // gamestate == normal

		updateDisplay(0); // Draw screen
		oledWriteInteger(10, 0, coincount);
		
		drawCoin(0, 0);
		updateDisplay(1);
		
		// 1E6 / fps
		
		if (gamestate == normal) handleMusic(&MusicController);
		handleMusic(&SoundEffectController);
	
		while (mymicros-loop_micros<=25000) // 40 fps
		{
			if (gamestate == normal) handleMusic(&MusicController);
			handleMusic(&SoundEffectController);
		}
		loop_micros = mymicros;
		
	}

}

uint8_t next_random(uint8_t range)
{
	curr_seed = (curr_seed*1664525 + 1013904223) % 65536;
	return (uint8_t) (curr_seed % range);
}

uint16_t readADC(uint8_t adcpin)
{
	uint16_t value=0;
	
	for (uint8_t i=0;i<16;i++)
	{
	ADCSRA &=~(1<<ADEN); // disable ADC Enable, to be safe
	
	//while (ADCSRA & (1<<ADSC)); // wait for ADSC bit to clear
	
	ADMUX = (adcpin & 7) << (MUX0); // REF bits are zero = Vcc as voltage reference
	// ref = 0 : Vcc as voltage reference
	// ref = (1<<REFS1) : 1.1 V internal as reference
	ADCSRA = (1<<ADEN) | (1<<ADPS2) | (1<<ADPS1) | (1<<ADPS0);// divide by 16 | (1<<ADPS1);// | (1<<ADPS0); // Enable ADC in general
	
	ADCSRA |= (1<<ADSC); // Start ADC conversion
	while(ADCSRA & (1<<ADSC)); // ADSC goes to zero when finished
	//while(!(ADCSRA & (1<<ADIF)));
	value+=ADC;
	}
	return value>>4; // divide by eight, average
}

void drawSprite(int16_t x, int16_t y, uint8_t w, uint8_t h, const unsigned char *buf, uint8_t flip)
{
  // Do clipping:
  //if (x<0) x=0

  // Assume don't need clipping for now
  //ssd1306_send_command3(renderingFrame | (y >> 3), 0x10 | ((x & 0xf0) >> 4), x & 0x0f);
  //uint8_t offset_y = y & 0x07;

  int16_t sx, sy;
  unsigned char *pos = (unsigned char*)buf;
  uint8_t start_height=0, stop_height=h/8, xoff = 0, xw = w;
  // i.e. for write_higeht 16 means 2 lots of 8 bits.

  if (x >= 128 || x <= -w) return;
  if (y>63) return;
  if (x < 0) {
    xoff = -x;
    x = 0;
  }
  if (x + w > 128) xw = 128 - x;

  uint8_t temp[16];
  
  if (y<0)
  {
	start_height=1+((-y)>>3); // i.e. -7 to -1 (inclusive): start_height = 1
	y=0;
  }
  
  if (y+h>64) stop_height = 8-(y>>3);
	
  for (uint8_t j = start_height; j < stop_height; j++) // goes from 0 to 1 (across y-axis)
  {
    sx = x ;
    sy = y + (j-start_height) * 8;

    // SetCursor
    //ssd1306_send_command3(renderingFrame | (sy >> 3), 0x10 | ((sx & 0xf0) >> 4), sx & 0x0f);
    oledSetPosition(sx, sy);

    //ssd1306_send_data_start();

    // Copy into local buffer forwards or backwards, then pass on :)

    for (uint8_t i = xoff; i < xw; i++) // i.e. i goes 0 to 15 (across x axis) if not clipped
    {
      if (flip == 0) pos = (unsigned char*)buf + j + (i * (h>>3));
      else pos = (unsigned char*)buf + j + ((w - 1 - i) * (h>>3)); // h/8
      temp[i - xoff] =  pgm_read_byte(pos);
    }

    // cursor is incremented auto-magically to the right (x++) per write
    
    oledWriteDataBlock(temp, xw - xoff); 
    
  }
}

void killGoomba(uint8_t g_id)
{
	goomba[g_id].state = squash;
	goomba[g_id].frame=17;
	goomba[g_id].vx=0;
	goomba[g_id].x++;//.x++ forces a blank replot
}
void spawnGoomba(uint16_t pos)
{	// Check here for goomba availability
	for (uint8_t g=0;g<MAX_GOOMBAS;g++)
	{
		if (goomba[g].state==dead)
		{
			goomba[g].x = (pos<<3);
			goomba[g].y = 0;
			goomba[g].vx = 1;
			goomba[g].vy = 0;
			goomba[g].frame = 0;
			goomba[g].state=idle;
			return;
		}	
	}
}

// getWorld function is inspired by the "Map::GenerateRoom" function in the Squario project on the Arduboy, found here:
// https://github.com/arduboychris/Squario/blob/master/SquarioGame.cpp

void getWorld(uint16_t vx)
{
  // vx is in units of bricks (i.e. vx/8), one screen is 16 wide
  
  uint8_t floor, gap=0;
  uint8_t cx = (vx & 63);
  
  floor = 5 + next_random(3);  // can be 5, 6 or 7

  for (uint8_t i=cx; i<cx+16;i++)
  {
	screen[i]=0;
	
	if (!next_random(5)) screen[i]=1; // Spawn a coin
	
	if (!gap)
	{
		//SetColumn(ifloor,7);
		for (uint8_t f=floor;f<=7;f++)
			screen[i] |= (1<<f);
		if (!next_random(10)) // 1 in 10 chance of setting a gap
		{
			gap = 2 + next_random(4); // 2-5 
			if (vx==0) gap=0; // no gaps on first screen
		}
		else if (!next_random(5) && vx!=0) // change floor height
		{
			if (!next_random(2) && floor<7) floor++;
			else if (floor>1) floor--;
		}
		if (!next_random(16)) spawnGoomba((uint16_t)vx+8);// spawn half way across screen so there are no boundary issues on the right
	} 
	else gap--;
  }

}

void drawScreen()
{
  uint8_t starti, stopi, wrapped_i=0, coin = 0, pipe = 0;
  int xoffset;

  starti = (viewx >> 3) & WORLD_MAX_LEN;  // wrap round (could be WORLD_BUFFER - 1)
  xoffset = -(viewx & 0x07);
  stopi = starti + 16;

  if (starti > 0) {
    starti--;  // Handles blanking blacking out edge when *just* disappeared off screen
    xoffset -= 8;
  }

  if (xoffset != 0) stopi++; // Need to add an extra brick on the right because we are not drawing left brick fully
  
  for (uint8_t i = starti; i < stopi; i++) // limit of 256
  {
    if (delta_viewx < 0) wrapped_i = (i + 1) & WORLD_MAX_LEN; // wrap round (could be WORLD_BUFFER - 1)
    else if (delta_viewx > 0) wrapped_i = (i - 1) & WORLD_MAX_LEN;

    coin = 0;
    if (screen[i & WORLD_MAX_LEN] & (1 << 0)) coin = 1;

    for (uint8_t y = 1; y < 8; y++)
    {
      if (screen[i & WORLD_MAX_LEN] & (1 << y)) // There is a block here
      {
        if (delta_viewx < 0) // screen is scrolling right and we are within array limit