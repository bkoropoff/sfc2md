#include <avr/cpufunc.h>
#include <avr/interrupt.h>
#include <avr/io.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <util/delay.h>

/* Helper macros */
#define SET(R, P) ((R) |= _BV(P))
#define CLEAR(R, P) ((R) &= ~_BV(P))
#define TEST(R, P) ((R)&_BV(P))

/*
 * SFC/SNES controller reading
 */

/* Pin definitions */
#define SFC_DDR DDRB
#define SFC_OUT_REG PORTB
#define SFC_IN_REG PINB
#define SFC_CLK PORTB1
#define SFC_CLKDD DDB1
#define SFC_LATCH PORTB3
#define SFC_LATCHDD DDB3
#define SFC_DATA PINB2
#define SFC_DATADD DDB2

/* Read timings in microseconds */
#define SFC_LATCH_PULSE 12
#define SFC_CLK_HALF_CYCLE 6

/* Button definitions */
#define SFC_B 15
#define SFC_Y 14
#define SFC_SELECT 13
#define SFC_START 12
#define SFC_UP 11
#define SFC_DOWN 10
#define SFC_LEFT 9
#define SFC_RIGHT 8
#define SFC_A 7
#define SFC_X 6
#define SFC_L 5
#define SFC_R 4

/* Decode SFC button (active low) */
#define SFC_DEC(s, b) (((s) >> (b)) & 1)

/* Set up SFC/SNES pins */
void sfc_init(void)
{
    /* Latch and clock are outputs */
    SET(SFC_DDR, SFC_LATCHDD);
    SET(SFC_DDR, SFC_CLKDD);
    /* Data is input */
    CLEAR(SFC_DDR, SFC_DATADD);
    /* Clock idles high */
    SET(SFC_OUT_REG, SFC_CLK);
}

/* Read controller */
uint16_t sfc_read(void)
{
    uint8_t i;
    uint16_t state = 0;

    /* A SFC/SNES controller is basically a 16-bit shift register on
     * the end of a cable.  Latch the data and shift it out. */

    /* Send latch pulse */
    SET(SFC_OUT_REG, SFC_LATCH);
    _delay_us(SFC_LATCH_PULSE);
    CLEAR(SFC_OUT_REG, SFC_LATCH);

    /* Clock all button states */
    for (i = 0; i < 16; ++i)
    {
        _delay_us(SFC_CLK_HALF_CYCLE);
        CLEAR(SFC_OUT_REG, SFC_CLK);
        state = (state << 1) | (TEST(SFC_IN_REG, SFC_DATA) ? 1 : 0);
        _delay_us(SFC_CLK_HALF_CYCLE);
        SET(SFC_OUT_REG, SFC_CLK);
    }

    return state;
}

/*
 * MD/Gen controller emulation
 */

/* Pin definitions */
#define MD_DDR DDRD
#define MD_PORT PORTD
#define MD_D0 PD3
#define MD_D0DD DDD3
#define MD_D1 PD2
#define MD_D1DD DDD2
#define MD_D2 PD1
#define MD_D2DD DDD1
#define MD_D3 PD0
#define MD_D3DD DDD0
#define MD_D4 PD4
#define MD_D4DD DDD4
#define MD_D5 PD7
#define MD_D5DD DDD7

#define MD_SELECT_PINR PINB
#define MD_SELECT_DDR DDRB
#define MD_SELECT_DD DDB6
#define MD_SELECT PINB6
#define MD_SELECT_PCINT PCINT6

/* Encode MD button value
 * (put it in correct location for writing to data port register) */
#define MD_ENC(b, v) ((v) << (b))

/* Encode MD data lines */
#define MD_DATA(d0, d1, d2, d3, d4, d5)                                                            \
    MD_ENC(MD_D0, d0) | MD_ENC(MD_D1, d1) | MD_ENC(MD_D2, d2) | MD_ENC(MD_D3, d3) |                \
        MD_ENC(MD_D4, d4) | MD_ENC(MD_D5, d5)

/* Button mode enum */
static enum {
    /* 6 buttons, B and C are action and jump */
    MODE_6BUTTON_BC,
    /* 6 buttons, A and B are action and jump */
    MODE_6BUTTON_AB,
    /* Xenocrisis */
    MODE_6BUTTON_XC
} __attribute__((packed)) mode;

/* Data output schedule at each select line change */
static uint8_t schedule[8];

/* Return appropriate MD button values based on
 * button mode */
static uint8_t sched_a(uint16_t state)
{
    switch (mode)
    {
    case MODE_6BUTTON_BC:
        return SFC_DEC(state, SFC_A);
    case MODE_6BUTTON_AB:
        return SFC_DEC(state, SFC_Y);
    case MODE_6BUTTON_XC:
        return SFC_DEC(state, SFC_B);
    }
    return 1;
}

static uint8_t sched_b(uint16_t state)
{
    switch (mode)
    {
    case MODE_6BUTTON_BC:
        return SFC_DEC(state, SFC_Y);
    case MODE_6BUTTON_AB:
        return SFC_DEC(state, SFC_B);
    case MODE_6BUTTON_XC:
        return SFC_DEC(state, SFC_A);
    }
    return 1;
}

static uint8_t sched_c(uint16_t state)
{
    switch (mode)
    {
    case MODE_6BUTTON_BC:
        return SFC_DEC(state, SFC_B);
    case MODE_6BUTTON_AB:
        return SFC_DEC(state, SFC_A);
    case MODE_6BUTTON_XC:
        return SFC_DEC(state, SFC_R);
    }
    return 1;
}

static uint8_t sched_x(uint16_t state)
{
    switch (mode)
    {
    case MODE_6BUTTON_BC:
        return SFC_DEC(state, SFC_L);
    case MODE_6BUTTON_AB:
        return SFC_DEC(state, SFC_L);
    case MODE_6BUTTON_XC:
        return SFC_DEC(state, SFC_Y);
    }
    return 1;
}

static uint8_t sched_y(uint16_t state)
{
    switch (mode)
    {
    case MODE_6BUTTON_BC:
        return SFC_DEC(state, SFC_X);
    case MODE_6BUTTON_AB:
        return SFC_DEC(state, SFC_X);
    case MODE_6BUTTON_XC:
        return SFC_DEC(state, SFC_X);
    }
    return 1;
}

static uint8_t sched_z(uint16_t state)
{
    switch (mode)
    {
    case MODE_6BUTTON_BC:
        return SFC_DEC(state, SFC_R);
    case MODE_6BUTTON_AB:
        return SFC_DEC(state, SFC_R);
    case MODE_6BUTTON_XC:
        return SFC_DEC(state, SFC_L);
    }
    return 1;
}

static void sched_update(uint16_t state)
{
    uint8_t a = sched_a(state);
    uint8_t b = sched_b(state);
    uint8_t c = sched_c(state);
    uint8_t x = sched_x(state);
    uint8_t y = sched_y(state);
    uint8_t z = sched_z(state);
    uint8_t up = SFC_DEC(state, SFC_UP);
    uint8_t down = SFC_DEC(state, SFC_DOWN);
    uint8_t left = SFC_DEC(state, SFC_LEFT);
    uint8_t right = SFC_DEC(state, SFC_RIGHT);
    uint8_t start = SFC_DEC(state, SFC_START);
    uint8_t mode = SFC_DEC(state, SFC_SELECT);

    /*
     * Data output schedule.
     *
     * Games typically keep the select line high when idle and issue negative
     * pulses when polling the controller.  The first two downward pulses act
     * like the ordinary multiplexer in a 3-button controller.  On the 3rd
     * negative edge, D0-D4 are set low to indicate that we are a 6-button
     * controller.  On the subsequent positive edge, D0-D4 are set to the state
     * of the extra buttons.  On the 4th negative edge, D0-D4 are set high.
     * Most games issue this pulse but ignore the output, but Xenocrisis
     * actually checks it. The schedule then repeats.
     */
    schedule[0] = MD_DATA(up, down, left, right, b, c);
    schedule[1] = MD_DATA(up, down, 0, 0, a, start);
    schedule[2] = schedule[0];
    schedule[3] = schedule[1];
    schedule[4] = schedule[0];
    schedule[5] = MD_DATA(0, 0, 0, 0, a, start);
    schedule[6] = MD_DATA(z, y, x, mode, b, c);
    schedule[7] = MD_DATA(1, 1, 1, 1, a, start);
}

static void md_init(void)
{
    uint16_t state;

    /* Set data pins as outputs */
    SET(MD_DDR, MD_D0DD);
    SET(MD_DDR, MD_D1DD);
    SET(MD_DDR, MD_D2DD);
    SET(MD_DDR, MD_D3DD);
    SET(MD_DDR, MD_D4DD);
    SET(MD_DDR, MD_D5DD);
    /* Set select pin as input */
    CLEAR(MD_SELECT_DDR, MD_SELECT_DD);

    /* Fill initial output schedule with idle buttons */
    sched_update(0xFFFF);

    /* Switch mode based on buttons held on powerup */
    state = sfc_read();

    if (SFC_DEC(state, SFC_LEFT) == 0)
    {
        mode = MODE_6BUTTON_AB;
    }
    else if (SFC_DEC(state, SFC_RIGHT) == 0)
    {
        mode = MODE_6BUTTON_BC;
    }
    else
    {
        mode = MODE_6BUTTON_XC;
    }

    /* Initialize interrupt timer */
    TCNT1 = 0;
    TCCR1A = 0;
    SET(TIMSK1, TOIE1);
}

ISR(TIMER1_OVF_vect, ISR_NAKED)
{
    static uint8_t tmp;
    /* Cause the loop to jump to the interrupted label
     * by overwriting the interrupt return address */
    asm volatile("sts %0, r31\n\t"
                 "pop r31\n\t"
                 "pop r31\n\t"
                 "ldi r31, lo8(gs(interrupted))\n\t"
                 "push r31\n\t"
                 "ldi r31, hi8(gs(interrupted))\n\t"
                 "push r31\n\t"
                 "lds r31, %0\n\t"
                 "reti\n\t"
                 : "=m"(tmp));
}

#define PHASE(n)                                                                                   \
    next = schedule[n];                                                                            \
    /* Force memory load *NOW*, prior to busy wait */                                              \
    asm volatile("" : : "r"(next));                                                                \
    /* Busy wait for select line to change to correct level for phase */                           \
    if (n % 2)                                                                                     \
    {                                                                                              \
        while (TEST(MD_SELECT_PINR, MD_SELECT))                                                    \
            ;                                                                                      \
    }                                                                                              \
    else                                                                                           \
    {                                                                                              \
        while (!TEST(MD_SELECT_PINR, MD_SELECT))                                                   \
            ;                                                                                      \
    }                                                                                              \
    /* Update output */                                                                            \
    MD_PORT = next;                                                                                \
    /* If we have passed first negative edge, start or restart interrupt timer */                  \
    if (n > 0)                                                                                     \
    {                                                                                              \
        TCNT1 = 0;                                                                                 \
        TCCR1B = _BV(CS10);                                                                        \
    }

/* Main loop
 *
 * This function is manually unrolled to keep response times to select line
 * changes as low as possible (measured at around 500 nanoseconds).
 */
static void loop(void)
{
    /* Keeping the next value to write to the output port ready in a register
     * reduces the response time by an instruction.  Yes, it matters.
     */
    register uint8_t next;

    asm volatile("interrupted:");
    /* Stop interrupt timer and poll controller */
    TCCR1B = 0;
    sched_update(sfc_read());

    for (;;)
    {
        PHASE(0);
        PHASE(1);
        PHASE(2);
        PHASE(3);
        PHASE(4);
        PHASE(5);
        PHASE(6);
        PHASE(7);
    }
}

static void setup()
{
    sfc_init();
    md_init();
    sei();
}

int main(void)
{
    setup();
    loop();
}
