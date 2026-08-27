#include <stdint.h>

#define RCC_BASE       0x40021000UL
#define RCC_CR         (*(volatile uint32_t *)(RCC_BASE + 0x00))
#define RCC_CFGR       (*(volatile uint32_t *)(RCC_BASE + 0x04))
#define RCC_APB2ENR    (*(volatile uint32_t *)(RCC_BASE + 0x18))

#define FLASH_BASE     0x40022000UL
#define FLASH_ACR      (*(volatile uint32_t *)(FLASH_BASE + 0x00))

#define GPIOB_BASE     0x40010C00UL
#define GPIOB_CRL      (*(volatile uint32_t *)(GPIOB_BASE + 0x00))
#define GPIOB_ODR      (*(volatile uint32_t *)(GPIOB_BASE + 0x0C))

#define GPIOC_BASE     0x40011000UL
#define GPIOC_CRH      (*(volatile uint32_t *)(GPIOC_BASE + 0x04))
#define GPIOC_ODR      (*(volatile uint32_t *)(GPIOC_BASE + 0x0C))

#define ADC1_BASE      0x40012400UL
#define ADC1_SR        (*(volatile uint32_t *)(ADC1_BASE + 0x00))
#define ADC1_CR2       (*(volatile uint32_t *)(ADC1_BASE + 0x08))
#define ADC1_SMPR1     (*(volatile uint32_t *)(ADC1_BASE + 0x0C))
#define ADC1_SMPR2     (*(volatile uint32_t *)(ADC1_BASE + 0x10))
#define ADC1_SQR1      (*(volatile uint32_t *)(ADC1_BASE + 0x2C))
#define ADC1_SQR3      (*(volatile uint32_t *)(ADC1_BASE + 0x34))
#define ADC1_DR        (*(volatile uint32_t *)(ADC1_BASE + 0x4C))

#define SYSTICK_BASE   0xE000E010UL
#define SYSTICK_CTRL   (*(volatile uint32_t *)(SYSTICK_BASE + 0x00))
#define SYSTICK_LOAD   (*(volatile uint32_t *)(SYSTICK_BASE + 0x04))
#define SYSTICK_VAL    (*(volatile uint32_t *)(SYSTICK_BASE + 0x08))
#define LED_PIN              13     /* PC13, Blue Pill onboard LED     */
#define LED_BLINK_PERIOD_MS  2000   /* toggle every 2s -> 4s full cycle */

#define TELEMETRY_UPDATE_MS  250    /* how often VOLT/CURR/TEMP refresh */

#define ADC_AVERAGE_COUNT    8

#define TEMP_CAL_OFFSET_X10  0
static void delay(volatile uint32_t count)
{
    while (count--)
    {
        __asm volatile ("nop");
    }
}

static volatile uint32_t g_millis = 0;

void SysTick_Handler(void)
{
    g_millis++;
}

static void systick_init(void)
{
    SYSTICK_LOAD = 72000UL - 1UL;
    SYSTICK_VAL  = 0;
    SYSTICK_CTRL = (1 << 2) | (1 << 1) | (1 << 0); /* CLKSOURCE=core, TICKINT, ENABLE */
}

static uint32_t millis(void)
{
    return g_millis;
}

static void delay_ms(uint32_t ms)
{
    uint32_t start = millis();
    while ((millis() - start) < ms)
    {
        /* spin */
    }
}
#define LED_OFF()      (GPIOC_ODR |=  (1UL << LED_PIN))
#define LED_TOGGLE()   (GPIOC_ODR ^=  (1UL << LED_PIN))
static void led_heartbeat_update(void)
{
    static uint32_t last_toggle_ms = 0;

    if ((millis() - last_toggle_ms) >= LED_BLINK_PERIOD_MS)
    {
        LED_TOGGLE();
        last_toggle_ms = millis();
    }
}
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
    delay_ms(50); /* power-up settle */
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
    static const uint8_t B[]      = {0x7F,0x49,0x49,0x49,0x36};
    static const uint8_t C[]      = {0x3E,0x41,0x41,0x41,0x22};
    static const uint8_t D[]      = {0x7F,0x41,0x41,0x41,0x3E};
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
    static const uint8_t Y[]      = {0x03,0x04,0x78,0x04,0x03};
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
        case 'B': font = B; break;
        case 'C': font = C; break;
        case 'D': font = D; break;
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
        case 'Y': font = Y; break;

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
static void oled_write_field(uint8_t page, uint8_t col,
                              const char *text, uint8_t field_width)
{
    uint8_t printed = 0;

    oled_set_cursor(page, col);

    while (*text && printed < field_width)
    {
        oled_char(*text++);
        printed++;
    }

    while (printed < field_width)
    {
        oled_char(' ');
        printed++;
    }
}
static void format_millivolts(uint32_t mv, char *out)
{
    uint32_t whole    = mv / 1000;
    uint32_t fraction = mv % 1000;

    out[0] = '0' + (uint8_t)(whole % 10);
    out[1] = '.';
    out[2] = '0' + (uint8_t)(fraction / 100);
    out[3] = '0' + (uint8_t)((fraction / 10) % 10);
    out[4] = '0' + (uint8_t)(fraction % 10);
    out[5] = '\0';
}
static void format_temperature(int32_t temp_x10, char *out)
{
    uint8_t idx = 0;
    uint32_t whole;
    uint32_t fraction;

    if (temp_x10 < 0)
    {
        out[idx++] = '-';
        temp_x10 = -temp_x10;
    }

    whole    = (uint32_t)temp_x10 / 10;
    fraction = (uint32_t)temp_x10 % 10;

    if (whole >= 100)
    {
        out[idx++] = '0' + (uint8_t)((whole / 100) % 10);
    }
    if (whole >= 10)
    {
        out[idx++] = '0' + (uint8_t)((whole / 10) % 10);
    }

    out[idx++] = '0' + (uint8_t)(whole % 10);
    out[idx++] = '.';
    out[idx++] = '0' + (uint8_t)fraction;
    out[idx]   = '\0';
}
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
static void gpio_init(void)
{
    /* GPIOB, GPIOC, AFIO */
    RCC_APB2ENR |= (1 << 3) | (1 << 4) | (1 << 0);

    /* ---- GPIOB low pins (PB0-PB7) ---- */
    GPIOB_CRL = 0;

    /* PB6, PB7: open-drain output 50MHz (I2C SCL/SDA) */
    GPIOB_CRL |= (0x7 << (6 * 4));
    GPIOB_CRL |= (0x7 << (7 * 4));

    /* ---- GPIOC high pins (PC8-PC15) ---- */
    GPIOC_CRH = 0;

    /* PC13: push-pull output 2MHz (onboard LED) -> CNF=00 MODE=10 */
    GPIOC_CRH |= (0x2 << ((13 - 8) * 4));

    SCL_HIGH();
    SDA_HIGH();

    LED_OFF(); /* active-low: OFF means pin driven high */
}
static void adc_init(void)
{
    RCC_APB2ENR |= (1 << 9); /* ADC1 clock enable */

    /* ADC clock = PCLK2 / 6 = 72MHz / 6 = 12MHz */
    RCC_CFGR &= ~(3 << 14);
    RCC_CFGR |= (2 << 14);

    /* Enable temperature sensor + VREFINT */
    ADC1_CR2 |= (1 << 23);

    /* Long sampling time on used channels: 1, 2, 16, 17 */
    ADC1_SMPR2 |= (7 << 3)  | (7 << 6);
    ADC1_SMPR1 |= (7 << 18) | (7 << 21);

    ADC1_CR2 |= (1 << 0); /* ADC ON */
    delay(10000);

    ADC1_CR2 |= (1 << 3); /* reset calibration */
    while (ADC1_CR2 & (1 << 3));

    ADC1_CR2 |= (1 << 2); /* calibrate */
    while (ADC1_CR2 & (1 << 2));
}

/* Single raw ADC conversion on the given channel. */
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

/* Same as adc_read(), but averages ADC_AVERAGE_COUNT samples to
 * smooth out noise (especially on a floating/unwired pin). */
static uint16_t adc_read_averaged(uint8_t channel)
{
    uint32_t sum = 0;
    uint8_t  i;

    for (i = 0; i < ADC_AVERAGE_COUNT; i++)
    {
        sum += adc_read(channel);
    }

    return (uint16_t)(sum / ADC_AVERAGE_COUNT);
}
static uint32_t read_vdda_mv(void)
{
    uint16_t vref_raw = adc_read_averaged(17);

    if (vref_raw == 0)
    {
        return 3300UL; /* fallback, should not normally happen */
    }

    return (1200UL * 4095UL) / vref_raw;
}
static uint32_t read_bus_voltage_mv(uint32_t vdda_mv)
{
    uint16_t raw = adc_read_averaged(1);
    return ((uint32_t)raw * vdda_mv) / 4095UL;
}
static uint32_t read_current_sense_mv(uint32_t vdda_mv)
{
    uint16_t raw = adc_read_averaged(2);
    return ((uint32_t)raw * vdda_mv) / 4095UL;
}
static int32_t read_temperature_x10(uint32_t vdda_mv)
{
    uint16_t raw       = adc_read_averaged(16);
    uint32_t sensor_mv = ((uint32_t)raw * vdda_mv) / 4095UL;

    int32_t temp_x10 = 250 + ((int32_t)(1430 - sensor_mv) * 100) / 43;

    return temp_x10 + TEMP_CAL_OFFSET_X10;
}
#define OLED_LABEL_COL    0
#define OLED_VALUE_COL    36   /* just past the 5-character labels   */
#define OLED_VALUE_WIDTH  10   /* enough chars for "12.345" + margin  */

#define OLED_HEADER_ROW   0
#define OLED_VOLT_ROW     2
#define OLED_CURR_ROW     4
#define OLED_TEMP_ROW     6
static void oled_draw_labels(void)
{
    oled_clear();

    oled_set_cursor(OLED_HEADER_ROW, OLED_LABEL_COL);
    oled_string("BLUEPILL SENSITIVITY");

    oled_set_cursor(OLED_VOLT_ROW, OLED_LABEL_COL);
    oled_string("VOLT:");

    oled_set_cursor(OLED_CURR_ROW, OLED_LABEL_COL);
    oled_string("CURR:");

    oled_set_cursor(OLED_TEMP_ROW, OLED_LABEL_COL);
    oled_string("TEMP:");
}
static void oled_update_readings(uint32_t voltage_mv, uint32_t current_mv,
                                  int32_t temperature_x10)
{
    char text[12];

    format_millivolts(voltage_mv, text);
    oled_write_field(OLED_VOLT_ROW, OLED_VALUE_COL, text, OLED_VALUE_WIDTH);

    format_millivolts(current_mv, text);
    oled_write_field(OLED_CURR_ROW, OLED_VALUE_COL, text, OLED_VALUE_WIDTH);

    format_temperature(temperature_x10, text);
    oled_write_field(OLED_TEMP_ROW, OLED_VALUE_COL, text, OLED_VALUE_WIDTH);
}
static void update_telemetry_display(void)
{
    uint32_t vdda_mv         = read_vdda_mv();
    uint32_t voltage_mv      = read_bus_voltage_mv(vdda_mv);
    uint32_t current_mv      = read_current_sense_mv(vdda_mv);
    int32_t  temperature_x10 = read_temperature_x10(vdda_mv);

    oled_update_readings(voltage_mv, current_mv, temperature_x10);
}
int main(void)
{
    system_clock_init();
    systick_init();   /* start millis() as early as possible */

    gpio_init();
    adc_init();

    oled_init();
    oled_draw_labels();

    uint32_t telemetry_last_ms = millis();

    while (1)
    {
        led_heartbeat_update();  /* fully non-blocking, 2s toggle */

        if ((millis() - telemetry_last_ms) >= TELEMETRY_UPDATE_MS)
        {
            telemetry_last_ms = millis();
            update_telemetry_display();
        }
    }
}
