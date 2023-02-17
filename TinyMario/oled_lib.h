
#define CLEAR_SCL asm("cbi 0x18, 2\n")   // 0x18 = PORTB, i.e. PORTB &= ~(1<<2);
#define CLEAR_SDA asm("cbi 0x18, 0\n")   // PORTB &= ~ (1<<0);
#define SET_SCL asm("sbi 0x18, 2\n")    //
#define SET_SDA asm("sbi 0x18, 0\n")

