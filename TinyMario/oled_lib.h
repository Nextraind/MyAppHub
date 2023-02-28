
#define CLEAR_SCL asm("cbi 0x18, 2\n")   // 0x18 = PORTB, i.e. PORTB &= ~(1<<2);
#define CLEAR_SDA asm("cbi 0x18, 0\n")   // PORTB &= ~ (1<<0);
#define SET_SCL asm("sbi 0x18, 2\n")    //
#define SET_SDA asm("sbi 0x18, 0\n")

#define OUTPUT_SCL asm("sbi 0x17, 2\n")   // 0x17 = DDRB, 2 = PB2 = SCL
#define OUTPUT_SDA asm("sbi 0x17, 0\n")   // 0x17 = DDRB, 0 = PB0 = SDA

#define INPUT_SCL asm("cbi 0x17, 2\n")    // DDRB &= ~(1<<2);
#define INPUT_SDA asm("cbi 0x17, 0\n")    // DDRB &= ~(1<<0);

//#define DIRECT_PORT
#define I2CPORT PORTB
// A bit set to 1 in the DDR is an output, 0 is an INPUT
#define I2CDDR DDRB

// Pin or port numbers for SDA and SCL
#define BB_SDA 0 //2
#define BB_SCL 2 //3

//#define I2C_CLK_LOW() I2CPORT &= ~(1 << BB_SCL) //compiles t