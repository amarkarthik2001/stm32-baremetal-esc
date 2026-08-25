#include <stdint.h>

/* ============================================================
 * BARE-METAL SENSORLESS BLDC ESC - STM32F103C8T6 (Blue Pill)
 * ============================================================
 *
 * PIN MAP
 * -------
 * PA8  -> TIM1_CH1  Phase A high-side PWM
 * PA9  -> TIM1_CH2  Phase B high-side PWM
 * PA10 -> TIM1_CH3  Phase C high-side PWM
 *
 * PB0  -> Phase A low-side switch (GPIO)
 * PB1  -> Phase B low-side switch (GPIO)
 * PB10 -> Phase C low-side switch (GPIO)
 *
 * PA4  -> ADC1_IN4  Phase A BEMF sense
 * PA5  -> ADC1_IN5  Phase B BEMF sense
 * PA6  -> ADC1_IN6  Phase C BEMF sense
 *
 * PA1  -> ADC1_IN1  Bus voltage sense (divider)
 * PA2  -> ADC1_IN2  Current sense (shunt/amplifier)
 *
 * PA0  -> TIM2_CH1  RC throttle input capture (1000-2000us)
 *
 * PB6  -> I2C SCL (bit-banged)  -> SSD1306 OLED
 * PB7  -> I2C SDA (bit-banged)  -> SSD1306 OLED
 *
 * PC13 -> Onboard LED (active LOW on Blue Pill) - 2s heartbeat blink
 *
 * IMPORTANT / DISCLAIMER
 * -----------------------
 * - This drives LOGIC-LEVEL gate signals only. You still need a real
 *   3-phase gate driver / MOSFET half-bridge stage (e.g. IR2101-type
 *   or a dedicated driver IC) between these pins and the motor.
 * - BEMF zero-cross detection here compares the floating phase
 *   directly against a fixed mid-scale ADC value (~Vdda/2). Real
 *   designs usually scale the phase voltage down through a resistor
 *   divider referenced to a virtual neutral point. Tune THRESHOLD_RAW
 *   and the sense divider ratio for your hardware.
 * - There is no over-current / over-temperature shutdown, no
 *   regenerative braking, no reverse. Add protection before running
 *   this on a real motor at meaningful power.
 * - Pole-pair count (POLE_PAIRS) must be set for your motor for the
 *   RPM readout to be correct.
 * - Timing note: millis() (SysTick, 1ms resolution) is used for
 *   coarse, "don't-care-about-jitter" waits like arming and OLED
 *   power-up settle. The BEMF blanking/timeout window, the I2C
 *   bit-bang bit time, and the open-loop startup ramp still use the
 *   microsecond-resolution delay_us()/delay() helpers on purpose -
 *   commutation timing needs microsecond precision that a 1kHz
 *   millis tick cannot provide, and those waits are intentionally
 *   blocking because the control loop depends on them.
 * ============================================================ */


/* ============================================================
 * RCC / FLASH
 * ============================================================ */

#define RCC_BASE       0x40021000UL
#define RCC_CR         (*(volatile uint32_t *)(RCC_BASE + 0x00))
#define RCC_CFGR       (*(volatile uint32_t *)(RCC_BASE + 0x04))
#define RCC_APB2ENR    (*(volatile uint32_t *)(RCC_BASE + 0x18))
#define RCC_APB1ENR    (*(volatile uint32_t *)(RCC_BASE + 0x1C))

#define FLASH_BASE     0x40022000UL
#define FLASH_ACR      (*(volatile uint32_t *)(FLASH_BASE + 0x00))


/* ============================================================
 * GPIOA / GPIOB / GPIOC
 * ============================================================ */

#define GPIOA_BASE     0x40010800UL
#define GPIOA_CRL      (*(volatile uint32_t *)(GPIOA_BASE + 0x00))
#define GPIOA_CRH      (*(volatile uint32_t *)(GPIOA_BASE + 0x04))
#define GPIOA_IDR      (*(volatile uint32_t *)(GPIOA_BASE + 0x08))
#define GPIOA_ODR      (*(volatile uint32_t *)(GPIOA_BASE + 0x0C))

#define GPIOB_BASE     0x40010C00UL
#define GPIOB_CRL      (*(volatile uint32_t *)(GPIOB_BASE + 0x00))
#define GPIOB_CRH      (*(volatile uint32_t *)(GPIOB_BASE + 0x04))
#define GPIOB_IDR      (*(volatile uint32_t *)(GPIOB_BASE + 0x08))
#define GPIOB_ODR      (*(volatile uint32_t *)(GPIOB_BASE + 0x0C))

#define GPIOC_BASE     0x40011000UL
#define GPIOC_CRH      (*(volatile uint32_t *)(GPIOC_BASE + 0x04))
#define GPIOC_ODR      (*(volatile uint32_t *)(GPIOC_BASE + 0x0C))


/* ============================================================
 * ADC1
 * ============================================================ */

#define ADC1_BASE      0x40012400UL
#define ADC1_SR        (*(volatile uint32_t *)(ADC1_BASE + 0x00))
#define ADC1_CR1       (*(volatile uint32_t *)(ADC1_BASE + 0x04))
#define ADC1_CR2       (*(volatile uint32_t *)(ADC1_BASE + 0x08))
#define ADC1_SMPR1     (*(volatile uint32_t *)(ADC1_BASE + 0x0C))
#define ADC1_SMPR2     (*(volatile uint32_t *)(ADC1_BASE + 0x10))
#define ADC1_SQR1      (*(volatile uint32_t *)(ADC1_BASE + 0x2C))
#define ADC1_SQR3      (*(volatile uint32_t *)(ADC1_BASE + 0x34))
#define ADC1_DR        (*(volatile uint32_t *)(ADC1_BASE + 0x4C))


/* ============================================================
 * TIM1 (advanced timer - 3-phase PWM)
 * ============================================================ */

#define TIM1_BASE      0x40012C00UL
#define TIM1_CR1       (*(volatile uint32_t *)(TIM1_BASE + 0x00))
#define TIM1_CR2       (*(volatile uint32_t *)(TIM1_BASE + 0x04))
#define TIM1_DIER      (*(volatile uint32_t *)(TIM1_BASE + 0x0C))
#define TIM1_SR        (*(volatile uint32_t *)(TIM1_BASE + 0x10))
#define TIM1_EGR       (*(volatile uint32_t *)(TIM1_BASE + 0x14))
#define TIM1_CCMR1     (*(volatile uint32_t *)(TIM1_BASE + 0x18))
#define TIM1_CCMR2     (*(volatile uint32_t *)(TIM1_BASE + 0x1C))
#define TIM1_CCER      (*(volatile uint32_t *)(TIM1_BASE + 0x20))
#define TIM1_CNT       (*(volatile uint32_t *)(TIM1_BASE + 0x24))
#define TIM1_PSC       (*(volatile uint32_t *)(TIM1_BASE + 0x28))
#define TIM1_ARR       (*(volatile uint32_t *)(TIM1_BASE + 0x2C))
#define TIM1_CCR1      (*(volatile uint32_t *)(TIM1_BASE + 0x34))
#define TIM1_CCR2      (*(volatile uint32_t *)(TIM1_BASE + 0x38))
#define TIM1_CCR3      (*(volatile uint32_t *)(TIM1_BASE + 0x3C))
#define TIM1_BDTR      (*(volatile uint32_t *)(TIM1_BASE + 0x44))


/* ============================================================
 * TIM2 (RC throttle input capture)
 * ============================================================ */

#define TIM2_BASE      0x40000000UL
#define TIM2_CR1       (*(volatile uint32_t *)(TIM2_BASE + 0x00))
#define TIM2_SR        (*(volatile uint32_t *)(TIM2_BASE + 0x10))
#define TIM2_CCMR1     (*(volatile uint32_t *)(TIM2_BASE + 0x18))
#define TIM2_CCER      (*(volatile uint32_t *)(TIM2_BASE + 0x20))
#define TIM2_CNT       (*(volatile uint32_t *)(TIM2_BASE + 0x24))
#define TIM2_PSC       (*(volatile uint32_t *)(TIM2_BASE + 0x28))
#define TIM2_ARR       (*(volatile uint32_t *)(TIM2_BASE + 0x2C))
#define TIM2_CCR1      (*(volatile uint32_t *)(TIM2_BASE + 0x34))


/* ============================================================
 * TIM4 (free-running 1us timebase)
 * ============================================================ */

#define TIM4_BASE      0x40000800UL
#define TIM4_CR1       (*(volatile uint32_t *)(TIM4_BASE + 0x00))
#define TIM4_CNT       (*(volatile uint32_t *)(TIM4_BASE + 0x24))
#define TIM4_PSC       (*(volatile uint32_t *)(TIM4_BASE + 0x28))
#define TIM4_ARR       (*(volatile uint32_t *)(TIM4_BASE + 0x2C))


/* ============================================================
 * SYSTICK (Cortex-M core peripheral - 1ms millis() timebase)
 * ============================================================ */

#define SYSTICK_BASE   0xE000E010UL
#define SYSTICK_CTRL   (*(volatile uint32_t *)(SYSTICK_BASE + 0x00))
#define SYSTICK_LOAD   (*(volatile uint32_t *)(SYSTICK_BASE + 0x04))
#define SYSTICK_VAL    (*(volatile uint32_t *)(SYSTICK_BASE + 0x08))


/* ============================================================
 * MOTOR / CONTROL PARAMETERS
 * ============================================================ */

#define POLE_PAIRS          7       /* set to your motor's pole pairs */

#define PWM_ARR              3599   /* ~20kHz PWM at 72MHz / (ARR+1)  */
#define MAX_DUTY             (PWM_ARR * 8 / 10) /* cap at 80% for safety   */

#define THROTTLE_MIN_US      1000
#define THROTTLE_MAX_US      2000
#define THROTTLE_ARM_US      1100  /* must be below this to arm       */
#define THROTTLE_ARM_HOLD_MS 200   /* how long throttle must stay low */

#define OPENLOOP_STEPS       60
#define OPENLOOP_START_US    18000
#define OPENLOOP_END_US      2500
#define OPENLOOP_DUTY        (PWM_ARR / 4)  /* 25% duty during ramp   */

#define BEMF_BLANK_US        80
#define BEMF_TIMEOUT_US      20000
#define THRESHOLD_RAW        2048   /* approx Vdda/2 in 12-bit counts  */

#define LED_PIN              13     /* PC13, Blue Pill onboard LED     */
#define LED_BLINK_PERIOD_MS  2000   /* toggle every 2s -> 4s full cycle */


/* ============================================================
 * DELAY / TIMEBASE HELPERS
 * ============================================================
 * TIM4 gives a free-running 1us tick, used for microsecond-precision
 * blocking waits (I2C bit timing, BEMF blanking/timeout, commutation
 * half-period waits). SysTick gives a 1ms tick (millis()), used for
 * coarse waits and non-blocking timing in the main loop.
 * ============================================================ */

static void delay(volatile uint32_t count)
{
    while (count--)
    {
        __asm volatile ("nop");
    }
}

static uint16_t tim4_now(void)
{
    return (uint16_t)TIM4_CNT;
}

static void delay_us(uint16_t us)
{
    uint16_t start = tim4_now();
    while ((uint16_t)(tim4_now() - start) < us)
    {
        /* spin */
    }
}

/* Incremented once per ms by SysTick_Handler (see systick_init below).
 * NOTE: this relies on the project's startup file / vector table
 * having a weak SysTick_Handler entry that this definition overrides -
 * true for standard CMSIS startup_stm32f10x.s. */
static volatile uint32_t g_millis = 0;

void SysTick_Handler(void)
{
    g_millis++;
}

static void systick_init(void)
{
    /* Core clock is 72MHz (set by system_clock_init) and SysTick's
     * CLKSOURCE bit selects the core clock directly, so
     * 72,000,000 / 72,000 = 1000 ticks/sec = 1ms per interrupt. */
    SYSTICK_LOAD = 72000UL - 1UL;
    SYSTICK_VAL  = 0;
    SYSTICK_CTRL = (1 << 2) | (1 << 1) | (1 << 0); /* CLKSOURCE=core, TICKINT, ENABLE */
}

static uint32_t millis(void)
{
    return g_millis;
}

/* Blocking millisecond wait built on millis(). Only used for
 * init-time settle waits (e.g. OLED power-up) where jitter of a
 * millisecond or two genuinely does not matter. Do NOT use this
 * anywhere near BEMF/commutation timing. */
static void delay_ms(uint32_t ms)
{
    uint32_t start = millis();
    while ((millis() - start) < ms)
    {
        /* spin */
    }
}


/* ============================================================
 * ONBOARD LED (PC13, active LOW)
 * ============================================================ */

#define LED_ON()       (GPIOC_ODR &= ~(1UL << LED_PIN))
#define LED_OFF()      (GPIOC_ODR |=  (1UL << LED_PIN))
#define LED_TOGGLE()   (GPIOC_ODR ^=  (1UL << LED_PIN))

/* Standalone heartbeat blink, independent of arming / throttle / OLED
 * state. Call this every pass of ANY loop (arming wait, main loop,
 * even a future error-halt loop) and it will keep toggling PC13 every
 * LED_BLINK_PERIOD_MS on its own internal timer. This is the "is the
 * firmware actually alive" signal - useful for confirming the board
 * is running before the OLED or an RC receiver is even connected. */
static void led_heartbeat_update(void)
{
    static uint32_t last_toggle_ms = 0;

    if ((millis() - last_toggle_ms) >= LED_BLINK_PERIOD_MS)
    {
        LED_TOGGLE();
        last_toggle_ms = millis();
    }
}


/* ============================================================
 * OLED I2C PINS (bit-banged)
 * ============================================================ */

#define OLED_SCL_PIN   6
#define OLED_SDA_PIN   7

#define SCL_HIGH()     (GPIOB_ODR |=  (1 << OLED_SCL_PIN))
#define SCL_LOW()      (GPIOB_ODR &= ~(1 << OLED_SCL_PIN))

#define SDA_HIGH()     (GPIOB_ODR |=  (1 << OLED_SDA_PIN))
#define SDA_LOW()      (GPIOB_ODR &= ~(1 << OLED_SDA_PIN))


static void i2c_delay(void)
{
    delay(100);
}

static void i2c_start(void)
{
    SDA_HIGH();
    SCL_HIGH();
    i2c_delay();

    SDA_LOW();
    i2c_delay();

    SCL_LOW();
}

static void i2c_stop(void)
{
    SDA_LOW();
    i2c_delay();

    SCL_HIGH();
    i2c_delay();

    SDA_HIGH();
    i2c_delay();
}

static void i2c_write(uint8_t data)
{
    uint8_t i;

    for (i = 0; i < 8; i++)
    {
        if (data & 0x80)
        {
            SDA_HIGH();
        }
        else
        {
            SDA_LOW();
        }

        i2c_delay();
        SCL_HIGH();
        i2c_delay();
        SCL_LOW();
        i2c_delay();

        data <<= 1;
    }

    /* Ignore ACK for now */
    SDA_HIGH();
    i2c_delay();
    SCL_HIGH();
    i2c_delay();
    SCL_LOW();
}


/* ============================================================
 * SSD1306 OLED
 * ============================================================ */

#define OLED_ADDRESS   0x3C

static void oled_command(uint8_t command)
{
    i2c_start();
    i2c_write(OLED_ADDRESS << 1);
    i2c_write(0x00);
    i2c_write(command);
    i2c_stop();
}

static void oled_data(uint8_t data)
{
    i2c_start();
    i2c_write(OLED_ADDRESS << 1);
    i2c_write(0x40);
    i2c_write(data);
    i2c_stop();
}

static void oled_init(void)
{
    delay_ms(50); /* power-up settle, was a raw NOP loop before */

    oled_command(0xAE);
    oled_command(0x20);
    oled_command(0x00);
    oled_command(0xB0);
    oled_command(0xC8);
    oled_command(0x00);
    oled_command(0x10);
    oled_command(0x40);
    oled_command(0x81);
    oled_command(0x7F);
    oled_command(0xA1);
    oled_command(0xA6);
    oled_command(0xA8);
    oled_command(0x3F);
    oled_command(0xA4);
    oled_command(0xD3);
    oled_command(0x00);
    oled_command(0xD5);
    oled_command(0x80);
    oled_command(0xD9);
    oled_command(0xF1);
    oled_command(0xDA);
    oled_command(0x12);
    oled_command(0xDB);
    oled_command(0x40);
    oled_command(0x8D);
    oled_command(0x14);
    oled_command(0xAF);
}

static void oled_clear(void)
{
    uint8_t page;
    uint8_t column;

    for (page = 0; page < 8; page++)
    {
        oled_command(0xB0 + page);
        oled_command(0x00);
        oled_command(0x10);

        for (column = 0; column < 128; column++)
        {
            oled_data(0x00);
        }
    }
}

static void oled_set_cursor(uint8_t page, uint8_t column)
{
    oled_command(0xB0 + page);
    oled_command(column & 0x0F);
    oled_command(0x10 | (column >> 4));
}


/* ============================================================
 * SIMPLE FONT
 * ============================================================ */

static void oled_char(char c)
{
    const uint8_t *font = 0;

    static const uint8_t blank[]  = {0x00,0x00,0x00,0x00,0x00};
    static const uint8_t zero[]   = {0x3E,0x51,0x49,0x45,0x3E};
    static const uint8_t one[]    = {0x00,0x42,0x7F,0x40,0x00};
    static const uint8_t two[]    = {0x42,0x61,0x51,0x49,0x46};
    static const uint8_t three[]  = {0x21,0x41,0x45,0x4B,0x31};
    static const uint8_t four[]   = {0x18,0x14,0x12,0x7F,0x10};
    static const uint8_t five[]   = {0x27,0x45,0x45,0x45,0x39};
    static const uint8_t six[]    = {0x3C,0x4A,0x49,0x49,0x30};
    static const uint8_t seven[]  = {0x01,0x71,0x09,0x05,0x03};
    static const uint8_t eight[]  = {0x36,0x49,0x49,0x49,0x36};
    static const uint8_t nine[]   = {0x06,0x49,0x49,0x29,0x1E};
    static const uint8_t A[]      = {0x7E,0x11,0x11,0x11,0x7E};
    static const uint8_t C[]      = {0x3E,0x41,0x41,0x41,0x22};
    static const uint8_t E[]      = {0x7F,0x49,0x49,0x49,0x41};
    static const uint8_t H[]      = {0x7F,0x08,0x08,0x08,0x7F};
    static const uint8_t I[]      = {0x00,0x41,0x7F,0x41,0x00};
    static const uint8_t L[]      = {0x7F,0x40,0x40,0x40,0x40};
    static const uint8_t M[]      = {0x7F,0x02,0x04,0x02,0x7F};
    static const uint8_t N[]      = {0x7F,0x04,0x08,0x10,0x7F};
    static const uint8_t O[]      = {0x3E,0x41,0x41,0x41,0x3E};
    static const uint8_t P[]      = {0x7F,0x09,0x09,0x09,0x06};
    static const uint8_t R[]      = {0x7F,0x09,0x19,0x29,0x46};
    static const uint8_t S[]      = {0x26,0x49,0x49,0x49,0x32};
    static const uint8_t T[]      = {0x01,0x01,0x7F,0x01,0x01};
    static const uint8_t U[]      = {0x3F,0x40,0x40,0x40,0x3F};
    static const uint8_t V[]      = {0x1F,0x20,0x40,0x20,0x1F};
    static const uint8_t dot[]    = {0x00,0x60,0x60,0x00,0x00};
    static const uint8_t minus[]  = {0x08,0x08,0x08,0x08,0x08};
    static const uint8_t colon[]  = {0x00,0x36,0x36,0x00,0x00};

    switch (c)
    {
        case '0': font = zero;  break;
        case '1': font = one;   break;
        case '2': font = two;   break;
        case '3': font = three; break;
        case '4': font = four;  break;
        case '5': font = five;  break;
        case '6': font = six;   break;
        case '7': font = seven; break;
        case '8': font = eight; break;
        case '9': font = nine;  break;

        case 'A': font = A; break;
        case 'C': font = C; break;
        case 'E': font = E; break;
        case 'H': font = H; break;
        case 'I': font = I; break;
        case 'L': font = L; break;
        case 'M': font = M; break;
        case 'N': font = N; break;
        case 'O': font = O; break;
        case 'P': font = P; break;
        case 'R': font = R; break;
        case 'S': font = S; break;
        case 'T': font = T; break;
        case 'U': font = U; break;
        case 'V': font = V; break;

        case '.': font = dot;   break;
        case '-': font = minus; break;
        case ':': font = colon; break;

        default: font = blank; break;
    }

    for (uint8_t i = 0; i < 5; i++)
    {
        oled_data(font[i]);
    }

    oled_data(0x00);
}

static void oled_string(const char *str)
{
    while (*str)
    {
        oled_char(*str++);
    }
}

/* mv -> "V.VVV" */
static void oled_print_voltage(uint32_t mv)
{
    uint32_t whole    = mv / 1000;
    uint32_t fraction = mv % 1000;

    oled_char('0' + (whole % 10));
    oled_char('.');
    oled_char('0' + (fraction / 100));
    oled_char('0' + ((fraction / 10) % 10));
    oled_char('0' + (fraction % 10));
}

/* temp x10 -> "TT.T" */
static void oled_print_temperature(int32_t temp_x10)
{
    if (temp_x10 < 0)
    {
        oled_char('-');
        temp_x10 = -temp_x10;
    }

    uint32_t whole    = temp_x10 / 10;
    uint32_t fraction = temp_x10 % 10;

    if (whole >= 100)
    {
        oled_char('0' + ((whole / 100) % 10));
    }
    if (whole >= 10)
    {
        oled_char('0' + ((whole / 10) % 10));
    }

    oled_char('0' + (whole % 10));
    oled_char('.');
    oled_char('0' + fraction);
}

/* plain unsigned integer, up to 5 digits */
static void oled_print_uint(uint32_t value)
{
    char digits[6];
    int8_t i = 0;

    if (value == 0)
    {
        oled_char('0');
        return;
    }

    while (value > 0 && i < 5)
    {
        digits[i++] = '0' + (value % 10);
        value /= 10;
    }

    while (i > 0)
    {
        oled_char(digits[--i]);
    }
}


/* ============================================================
 * CLOCK INIT - 72MHz from 8MHz HSE crystal (Blue Pill)
 * ============================================================ */

static void system_clock_init(void)
{
    /* Flash: 2 wait states, prefetch enable, needed for 72MHz */
    FLASH_ACR = (1 << 4) | (2 << 0);

    /* Enable HSE, wait for ready */
    RCC_CR |= (1 << 16);
    while (!(RCC_CR & (1 << 17)));

    /* PLL source = HSE (not divided), PLL x9 -> 8MHz * 9 = 72MHz */
    RCC_CFGR &= ~(0xF << 18);
    RCC_CFGR |= (7 << 18);     /* PLLMUL = x9 */
    RCC_CFGR |= (1 << 16);     /* PLLSRC = HSE */

    /* AHB = /1, APB1 = /2 (max 36MHz), APB2 = /1 */
    RCC_CFGR &= ~(0xF << 4);
    RCC_CFGR |= (4 << 8);      /* APB1 prescaler /2 */

    /* Enable PLL, wait for lock */
    RCC_CR |= (1 << 24);
    while (!(RCC_CR & (1 << 25)));

    /* Switch system clock to PLL */
    RCC_CFGR &= ~(3 << 0);
    RCC_CFGR |= (2 << 0);
    while (((RCC_CFGR >> 2) & 3) != 2);
}


/* ============================================================
 * GPIO INIT
 * ============================================================ */

static void gpio_init(void)
{
    /* GPIOA, GPIOB, GPIOC, AFIO */
    RCC_APB2ENR |= (1 << 2) | (1 << 3) | (1 << 4) | (1 << 0);

    /* ---- GPIOA low pins (PA0-PA7) ---- */
    GPIOA_CRL = 0;

    /* PA0: floating input (TIM2_CH1 capture) -> CNF=01 MODE=00 */
    GPIOA_CRL |= (0x4 << (0 * 4));

    /* PA1, PA2: analog input (voltage, current) -> 0x0 (already zero) */

    /* PA4, PA5, PA6: analog input (BEMF A/B/C) -> 0x0 (already zero) */

    /* ---- GPIOA high pins (PA8-PA15) ---- */
    GPIOA_CRH = 0;

    /* PA8, PA9, PA10: AF push-pull 50MHz (TIM1 CH1/2/3) -> CNF=10 MODE=11 */
    GPIOA_CRH |= (0xB << ((8  - 8) * 4));
    GPIOA_CRH |= (0xB << ((9  - 8) * 4));
    GPIOA_CRH |= (0xB << ((10 - 8) * 4));

    /* ---- GPIOB low pins (PB0-PB7) ---- */
    GPIOB_CRL = 0;

    /* PB0, PB1: push-pull output 50MHz (low-side switches A, B) */
    GPIOB_CRL |= (0x3 << (0 * 4));
    GPIOB_CRL |= (0x3 << (1 * 4));

    /* PB6, PB7: open-drain output 50MHz (I2C SCL/SDA) */
    GPIOB_CRL |= (0x7 << (6 * 4));
    GPIOB_CRL |= (0x7 << (7 * 4));

    /* ---- GPIOB high pins (PB8-PB15) ---- */
    GPIOB_CRH = 0;

    /* PB10: push-pull output 50MHz (low-side switch C) */
    GPIOB_CRH |= (0x3 << ((10 - 8) * 4));

    /* ---- GPIOC high pins (PC8-PC15) ---- */
    GPIOC_CRH = 0;

    /* PC13: push-pull output 2MHz (onboard LED) -> CNF=00 MODE=10 */
    GPIOC_CRH |= (0x2 << ((13 - 8) * 4));

    SCL_HIGH();
    SDA_HIGH();

    GPIOB_ODR &= ~((1 << 0) | (1 << 1) | (1 << 10));

    LED_OFF(); /* active-low: OFF means pin driven high */
}


/* ============================================================
 * TIM1 - 3-PHASE HIGH-SIDE PWM (~20kHz)
 * ============================================================ */

static void tim1_pwm_init(void)
{
    RCC_APB2ENR |= (1 << 11); /* TIM1 clock enable */

    TIM1_PSC = 0;
    TIM1_ARR = PWM_ARR;

    /* CH1, CH2: PWM mode 1, preload enable */
    TIM1_CCMR1 = (6 << 4) | (1 << 3) | (6 << 12) | (1 << 11);

    /* CH3: PWM mode 1, preload enable */
    TIM1_CCMR2 = (6 << 4) | (1 << 3);

    /* Enable CH1, CH2, CH3 outputs, active high */
    TIM1_CCER = (1 << 0) | (1 << 4) | (1 << 8);

    TIM1_CCR1 = 0;
    TIM1_CCR2 = 0;
    TIM1_CCR3 = 0;

    TIM1_CR1 |= (1 << 7);   /* ARPE: auto-reload preload enable */

    /* MOE: main output enable - required on advanced timers (TIM1) */
    TIM1_BDTR = (1 << 15);

    TIM1_EGR |= (1 << 0);   /* force update to load registers */

    TIM1_CR1 |= (1 << 0);   /* CEN: enable counter */
}


/* ============================================================
 * TIM2 - RC THROTTLE INPUT CAPTURE (PA0, 1MHz tick)
 * ============================================================ */

static volatile uint16_t throttle_us      = THROTTLE_MIN_US;
static volatile uint8_t  throttle_valid   = 0;
static uint8_t  tim2_waiting_falling      = 0;
static uint16_t tim2_rise_time            = 0;
static uint32_t tim2_last_edge_ms_ticker  = 0;

static void tim2_capture_init(void)
{
    RCC_APB1ENR |= (1 << 0); /* TIM2 clock enable */

    TIM2_PSC = 71;      /* 72MHz / 72 = 1MHz -> 1 tick = 1us */
    TIM2_ARR = 0xFFFF;

    /* CC1S = 01: IC1 mapped to TI1 */
    TIM2_CCMR1 = (1 << 0);

    /* CC1E = 1, CC1P = 0 (rising edge first) */
    TIM2_CCER = (1 << 0);

    TIM2_CR1 |= (1 << 0);
}

/* Call this frequently from the main loop */
static void throttle_poll(void)
{
    if (TIM2_SR & (1 << 1)) /* CC1IF */
    {
        uint16_t captured = (uint16_t)TIM2_CCR1;
        TIM2_SR &= ~(1 << 1);

        if (!tim2_waiting_falling)
        {
            tim2_rise_time = captured;
            TIM2_CCER |= (1 << 1);   /* CC1P = 1: capture falling edge next */
            tim2_waiting_falling = 1;
        }
        else
        {
            uint16_t width;

            if (captured >= tim2_rise_time)
            {
                width = captured - tim2_rise_time;
            }
            else
            {
                width = (uint16_t)(0xFFFFu - tim2_rise_time) + captured + 1u;
            }

            if (width >= 800 && width <= 2200)
            {
                throttle_us = width;
                throttle_valid = 1;
            }

            TIM2_CCER &= ~(1 << 1);  /* CC1P = 0: back to rising edge */
            tim2_waiting_falling = 0;
        }
    }
}


/* ============================================================
 * TIM4 - FREE-RUNNING 1us TIMEBASE
 * ============================================================ */

static void tim4_timebase_init(void)
{
    RCC_APB1ENR |= (1 << 2); /* TIM4 clock enable */

    TIM4_PSC = 71;   /* 1MHz -> 1 tick = 1us */
    TIM4_ARR = 0xFFFF;
    TIM4_CR1 |= (1 << 0);
}


/* ============================================================
 * ADC INIT / READ
 * ============================================================ */

static void adc_init(void)
{
    RCC_APB2ENR |= (1 << 9); /* ADC1 clock enable */

    /* ADC clock = PCLK2 / 6 = 72MHz / 6 = 12MHz */
    RCC_CFGR &= ~(3 << 14);
    RCC_CFGR |= (2 << 14);

    /* Enable temperature sensor + VREFINT */
    ADC1_CR2 |= (1 << 23);

    /* Long sampling time on all used channels: 1,2,4,5,6,16,17 */
    ADC1_SMPR2 |= (7 << 3)  | (7 << 6)  | (7 << 12) | (7 << 15) | (7 << 18);
    ADC1_SMPR1 |= (7 << 18) | (7 << 21);

    ADC1_CR2 |= (1 << 0); /* ADC ON */
    delay(10000);

    ADC1_CR2 |= (1 << 3); /* reset calibration */
    while (ADC1_CR2 & (1 << 3));

    ADC1_CR2 |= (1 << 2); /* calibrate */
    while (ADC1_CR2 & (1 << 2));
}

static uint16_t adc_read(uint8_t channel)
{
    ADC1_SQR1 = 0;
    ADC1_SQR3 = channel;

    ADC1_CR2 |= (7 << 17); /* EXTSEL = software */
    ADC1_CR2 |= (1 << 20); /* EXTTRIG */
    ADC1_CR2 |= (1 << 22); /* SWSTART */

    while (!(ADC1_SR & (1 << 1)));

    return (uint16_t)ADC1_DR;
}


/* ============================================================
 * MOTOR COMMUTATION
 * ============================================================ */

typedef struct
{
    uint8_t high_phase;   /* 0=A, 1=B, 2=C */
    uint8_t low_phase;    /* 0=A, 1=B, 2=C */
    uint8_t float_phase;  /* 0=A, 1=B, 2=C */
    uint8_t edge_rising;  /* 1 = expect rising BEMF edge, 0 = falling */
} commutation_step_t;

static const commutation_step_t comm_table[6] =
{
    {0, 1, 2, 0}, /* Step 1: A+ B-, float C, falling */
    {0, 2, 1, 1}, /* Step 2: A+ C-, float B, rising  */
    {1, 2, 0, 0}, /* Step 3: B+ C-, float A, falling */
    {1, 0, 2, 1}, /* Step 4: B+ A-, float C, rising  */
    {2, 0, 1, 0}, /* Step 5: C+ A-, float B, falling */
    {2, 1, 0, 1}, /* Step 6: C+ B-, float A, rising  */
};

static const uint8_t bemf_adc_channel[3] = {4, 5, 6}; /* A, B, C */

/* Reads bus voltage / current / temperature and redraws the OLED.
 * Called both from the arming wait loop and the main run loop, so
 * you can see live VOLT/CURR/TEMP on the display even before a
 * throttle/PWM signal is present on PA0 (i.e. before "armed"). RPM
 * only means anything once the motor is actually commutating, so the
 * bottom line shows "ARMING" until armed_state is true. */
static void oled_render_status(uint8_t armed_state, uint8_t motor_running,
                                uint16_t comm_period_us)
{
    uint16_t voltage_raw, current_raw, temperature_raw, vref_raw;
    uint32_t vdda_mv, voltage_mv, current_mv, temperature_mv;
    int32_t  temperature_x10;
    uint32_t rpm = 0;

    voltage_raw     = adc_read(1);
    current_raw     = adc_read(2);
    temperature_raw = adc_read(16);
    vref_raw        = adc_read(17);

    vdda_mv = (vref_raw != 0) ? ((1200UL * 4095UL) / vref_raw) : 3300;

    voltage_mv = ((uint32_t)voltage_raw * vdda_mv) / 4095UL;
    current_mv = ((uint32_t)current_raw * vdda_mv) / 4095UL;

    temperature_mv = ((uint32_t)temperature_raw * vdda_mv) / 4095UL;
    temperature_x10 = 250 + ((int32_t)(1430 - temperature_mv) * 100) / 43;

    if (armed_state && motor_running && comm_period_us > 0)
    {
        /* 6 commutation steps per electrical revolution */
        uint32_t erpm = 60000000UL / ((uint32_t)comm_period_us * 6);
        rpm = erpm / POLE_PAIRS;
    }

    oled_clear();

    oled_set_cursor(0, 0);
    oled_string("VOLT:");
    oled_print_voltage(voltage_mv);
    oled_string("V");

    oled_set_cursor(2, 0);
    oled_string("CURR:");
    oled_print_voltage(current_mv);
    oled_string("V");

    oled_set_cursor(4, 0);
    oled_string("TEMP:");
    oled_print_temperature(temperature_x10);
    oled_string("C");

    oled_set_cursor(6, 0);
    if (!armed_state)
    {
        oled_string("ARMING");
    }
    else
    {
        oled_string("RPM:");
        oled_print_uint(rpm);
    }
}

static void motor_stop(void)
{
    TIM1_CCR1 = 0;
    TIM1_CCR2 = 0;
    TIM1_CCR3 = 0;

    GPIOB_ODR &= ~((1 << 0) | (1 << 1) | (1 << 10));
}

static void set_phase_outputs(const commutation_step_t *step, uint16_t duty)
{
    TIM1_CCR1 = (step->high_phase == 0) ? duty : 0;
    TIM1_CCR2 = (step->high_phase == 1) ? duty : 0;
    TIM1_CCR3 = (step->high_phase == 2) ? duty : 0;

    GPIOB_ODR &= ~((1 << 0) | (1 << 1) | (1 << 10));

    if (step->low_phase == 0) GPIOB_ODR |= (1 << 0);
    if (step->low_phase == 1) GPIOB_ODR |= (1 << 1);
    if (step->low_phase == 2) GPIOB_ODR |= (1 << 10);
}

/* Blocking wait for BEMF zero-cross on the floating phase.
 * Returns 1 if detected, 0 on timeout.
 * Deliberately uses the microsecond TIM4 timebase, not millis() -
 * this window is 80us-20ms and needs real precision. */
static uint8_t bemf_wait_zero_cross(uint8_t float_phase, uint8_t rising)
{
    uint16_t sample;
    uint16_t start;

    delay_us(BEMF_BLANK_US);

    start = tim4_now();

    while ((uint16_t)(tim4_now() - start) < BEMF_TIMEOUT_US)
    {
        sample = adc_read(bemf_adc_channel[float_phase]);

        if (rising)
        {
            if (sample > THRESHOLD_RAW) return 1;
        }
        else
        {
            if (sample < THRESHOLD_RAW) return 1;
        }
    }

    return 0;
}

/* Open-loop startup ramp: steps through commutation blind,
 * speeding up, so the rotor is spinning fast enough to produce
 * a usable BEMF signal before handing off to closed loop.
 * Uses delay_us() on purpose - step_us goes down to 2.5ms, well
 * below millis() resolution, and precise step timing matters here. */
static void motor_start_openloop(uint16_t duty)
{
    uint32_t step_us = OPENLOOP_START_US;
    uint32_t decrement = (OPENLOOP_START_US - OPENLOOP_END_US) / OPENLOOP_STEPS;

    for (uint16_t i = 0; i < OPENLOOP_STEPS; i++)
    {
        const commutation_step_t *step = &comm_table[i % 6];

        set_phase_outputs(step, duty);

        /* delay_us() takes a uint16_t, split longer waits */
        uint32_t remaining = step_us;
        while (remaining > 60000)
        {
            delay_us(60000);
            remaining -= 60000;
        }
        delay_us((uint16_t)remaining);

        if (step_us > OPENLOOP_END_US + decrement)
        {
            step_us -= decrement;
        }
        else
        {
            step_us = OPENLOOP_END_US;
        }
    }
}


/* ============================================================
 * MAIN
 * ============================================================ */

int main(void)
{
    system_clock_init();
    systick_init();   /* start millis() as early as possible */

    gpio_init();
    tim1_pwm_init();
    tim2_capture_init();
    tim4_timebase_init();
    adc_init();

    motor_stop();

    oled_init();
    oled_clear();

    /* ---- Arming: throttle must sit low for THROTTLE_ARM_HOLD_MS ----
     * Rewritten on millis() instead of a NOP-counted delay() loop, so
     * the hold time is an accurate 200ms regardless of compiler
     * optimization level, and throttle_poll() still runs every pass
     * (no blind delay() blocking edge capture). */
    uint32_t arm_low_since_ms = millis();
    uint8_t  armed            = 0;
    uint32_t telemetry_last_ms = millis();

    while (!armed)
    {
        throttle_poll();
        led_heartbeat_update();

        if ((millis() - telemetry_last_ms) >= 250)
        {
            telemetry_last_ms = millis();
            oled_render_status(0, 0, 0); /* not armed yet: shows "ARMING" */
        }

        if (throttle_valid && throttle_us < THROTTLE_ARM_US)
        {
            if ((millis() - arm_low_since_ms) >= THROTTLE_ARM_HOLD_MS)
            {
                armed = 1;
            }
        }
        else
        {
            arm_low_since_ms = millis();
        }
    }

    uint8_t   motor_running   = 0;
    uint8_t   step_index      = 0;
    uint16_t  comm_period_us  = OPENLOOP_END_US;
    uint16_t  last_comm_time  = tim4_now();

    /* telemetry_last_ms carries over from the arming loop above, so
     * the 250ms cadence stays continuous across the armed transition */

    while (1)
    {
        throttle_poll();
        led_heartbeat_update();  /* fully non-blocking, 2s toggle */

        uint8_t throttle_ok = throttle_valid &&
                              (throttle_us > THROTTLE_ARM_US) &&
                              (throttle_us <= THROTTLE_MAX_US + 100);

        if (!throttle_ok)
        {
            motor_stop();
            motor_running = 0;
        }
        else
        {
            /* map throttle pulse width to PWM duty */
            uint32_t clamped = throttle_us;
            if (clamped < THROTTLE_MIN_US) clamped = THROTTLE_MIN_US;
            if (clamped > THROTTLE_MAX_US) clamped = THROTTLE_MAX_US;

            uint32_t duty = ((clamped - THROTTLE_MIN_US) * MAX_DUTY)
                             / (THROTTLE_MAX_US - THROTTLE_MIN_US);

            if (!motor_running)
            {
                motor_start_openloop(OPENLOOP_DUTY);
                motor_running = 1;
                step_index = 0;
                last_comm_time = tim4_now();
            }
            else
            {
                const commutation_step_t *step = &comm_table[step_index];

                set_phase_outputs(step, (uint16_t)duty);

                uint16_t comm_time = tim4_now();

                uint8_t zc = bemf_wait_zero_cross(step->float_phase,
                                                   step->edge_rising);

                if (zc)
                {
                    uint16_t zc_time = tim4_now();
                    uint16_t half_period =
                        (uint16_t)((zc_time - last_comm_time) / 2);

                    delay_us(half_period);

                    comm_period_us = (uint16_t)(zc_time - last_comm_time);
                    last_comm_time = tim4_now();
                }
                else
                {
                    /* lost sync: hold timing, keep trying */
                    last_comm_time = comm_time;
                }

                step_index = (step_index + 1) % 6;
            }
        }

        /* ---- Telemetry + display, updated every 250ms (millis-based,
         * not tied to loop speed) ---- */
        if ((millis() - telemetry_last_ms) >= 250)
        {
            telemetry_last_ms = millis();
            oled_render_status(1, motor_running, comm_period_us);
        }
    }
}
