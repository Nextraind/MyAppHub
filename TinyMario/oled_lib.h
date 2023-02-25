
#define CLEAR_SCL asm("cbi 0x18, 2\n")   // 0x18 = PORTB, i.e. PORTB &= ~(1<<2);
#define CLEAR_SDA asm("cbi 0x18, 0\n")   // PORTB &= ~ (1<<0);
#define SET_SCL asm("sbi 0x18, 2\n")    //
#define SET_SDA asm("sbi 0x18, 0\n")

#define OUTPUT_SCL asm("sbi 0x17, 2\n")   // 0x17 = DDRB, 2 = PB2 = SCL
#define OUTPUT_SDA asm("sbi 0x17, 0\n")   // 0x17 = DDRB, 0 = PB0 = SDA

#define INPUT_SCL asm("cbi 0x17, 2\n")   