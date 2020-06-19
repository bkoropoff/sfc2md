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
/* Indication from timer to restart output schedule.
 * This skips 6-button phases when the game only
 * polls us as a 3-button controller. */
// static uint8_t volatile restart;
uint8_t register restart asm("r5");

/* Return appropriate MD button values based on
 * button mode */
static inline uint8_t md_sched_a(uint16_t state)
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

static inline uint8_t md_sched_b(uint16_t state)
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

static inline uint8_t md_sched_c(uint16_t state)
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

static inline uint8_t md_sched_x(uint16_t state)
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

static inline uint8_t md_sched_y(uint16_t state)
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

static inline uint8_t md_sched_z(uint16_t state)
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

/*
 * Data output schedule.
 *
 * Games typically keep the select line high when idle and
 * issue negative pulses when polling the controller.  The
 * first two downward pulses act like the ordinary multiplexer
 * in 3-button controller.  On the 3rd negative edge, D0-D4
 * are set low to indicate that we are a 6-button controller.
 * On the subsequent positive edge, D0-D4 are set to the
 * state of the extra buttons.  On the 4th negative edge,
 * D0-D4 are set high.  Most games issue this pulse but ignore
 * the output, but Xenocrisis actually checks it. The schedule
 * then repeats.
 */

static uint8_t md_sched0(uint16_t state)
{
    return MD_ENC(MD_D0, SFC_DEC(state, SFC_UP)) | MD_ENC(MD_D1, SFC_DEC(state, SFC_DOWN)) |
           MD_ENC(MD_D2, SFC_DEC(state, SFC_LEFT)) | MD_ENC(MD_D3, SFC_DEC(state, SFC_RIGHT)) |
           MD_ENC(MD_D4, md_sched_b(state)) | MD_ENC(MD_D5, md_sched_c(state));
}

static uint8_t md_sched1(uint16_t state)
{
    return MD_ENC(MD_D0, SFC_DEC(state, SFC_UP)) | MD_ENC(MD_D1, SFC_DEC(state, SFC_DOWN)) |
           MD_ENC(MD_D2, 0) | MD_ENC(MD_D3, 0) | MD_ENC(MD_D4, md_sched_a(state)) |
           MD_ENC(MD_D5, SFC_DEC(state, SFC_START));
}

static uint8_t md_sched2(uint16_t state) { return md_sched0(state); }

static uint8_t md_sched3(uint16_t state) { return md_sched1(state); }

static uint8_t md_sched4(uint16_t state) { return md_sched0(state); }

static uint8_t md_sched5(uint16_t state)
{
    return MD_ENC(MD_D0, 0) | MD_ENC(MD_D1, 0) | MD_ENC(MD_D2, 0) | MD_ENC(MD_D3, 0) |
           MD_ENC(MD_D4, md_sched_a(state)) | MD_ENC(MD_D5, SFC_DEC(state, SFC_START));
}

static uint8_t md_sched6(uint16_t state)
{
    return MD_ENC(MD_D0, md_sched_z(state)) | MD_ENC(MD_D1, md_sched_y(state)) |
           MD_ENC(MD_D2, md_sched_x(state)) | MD_ENC(MD_D3, SFC_DEC(state, SFC_SELECT)) |
           MD_ENC(MD_D4, md_sched_b(state)) | MD_ENC(MD_D5, md_sched_c(state));
}

static uint8_t md_sched7(uint16_t state)
{
    return MD_ENC(MD_D0, 1) | MD_ENC(MD_D1, 1) | MD_ENC(MD_D2, 1) | MD_ENC(MD_D3, 1) |
           MD_ENC(MD_D4, md_sched_a(state)) | MD_ENC(MD_D5, SFC_DEC(state, SFC_START));
}

static void md_sched_update(uint16_t state)
{
    schedule[0] = md_sched0(state);
    schedule[1] = md_sched1(state);
    schedule[2] = md_sched2(state);
    schedule[3] = md_sched3(state);
    schedule[4] = md_sched4(state);
    schedule[5] = md_sched5(state);
    schedule[6] = md_sched6(state);
    schedule[7] = md_sched7(state);
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
    md_sched_update(0xFFFF);

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

    /* Start SFC read timer */
    TCNT1 = 0;
    TCCR1A = 0;
    SET(TIMSK1, TOIE1);
}

ISR(TIMER1_OVF_vect)
{
    /* Turn timer off until next select line change */
    TCCR1B = 0;
    /* Tell output loop to restart */
    restart = 1;
    /* Poll the controller */
    md_sched_update(sfc_read());
}

/* Restart output schedule at phase n if indicated */
#define RESTART_(n)                                                                                \
    if (restart)                                                                                   \
        goto phase_##n;

#define PHASE_(n, rs, inv)                                                                         \
    phase_##n : rs;                                                                                \
    if (inv)                                                                                       \
    {                                                                                              \
        if (!TEST(MD_SELECT_PINR, MD_SELECT))                                                      \
            goto phase_##n;                                                                        \
    }                                                                                              \
    else                                                                                           \
    {                                                                                              \
        if (TEST(MD_SELECT_PINR, MD_SELECT))                                                       \
            goto phase_##n;                                                                        \
    }                                                                                              \
    MD_PORT = next;                                                                                \
    next = schedule[(n + 1) % 8];                                                                  \
    TCNT1 = 0;                                                                                     \
    TCCR1B = _BV(CS10);

#define PHASE_HIGH(n) PHASE_(n, RESTART_(0), true)
#define PHASE_LOW(n) PHASE_(n, RESTART_(1), false)
#define PHASE_ZERO() PHASE_(0, restart = 0, true)
#define PHASE_ONE() PHASE_(1, restart = 0, false)

/* 6-button output loop
 *
 * This function is manually unrolled to keep response times
 * to select line changes as low as possible (measured at around
 * 440 nanoseconds).
 */
static inline void loop6(void)
{
    /* Keeping the next value to write to the output port ready in a register
     * reduces the response time by an instruction.  Yes, it matters.
     */
    register uint8_t next = schedule[0];
    for (;;)
    {
        PHASE_ZERO();
        PHASE_ONE();
        PHASE_HIGH(2);
        PHASE_LOW(3);
        PHASE_HIGH(4);
        PHASE_LOW(5);
        PHASE_HIGH(6);
        PHASE_LOW(7);
    }
}

void setup()
{
    sfc_init();
    md_init();
    sei();
}

void loop()
{
    switch (mode)
    {
    // FIXME: forced 3-button modes
    case MODE_6BUTTON_AB:
    case MODE_6BUTTON_BC:
    case MODE_6BUTTON_XC:
        loop6();
    }
}

int main(void)
{
    setup();
    loop();
}
