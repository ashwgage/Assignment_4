#include "stm32l476xx.h"

#define OUTPUT_PIN 5U

#define TIMER_PRESCALER 0U
#define TIMER_ARR       799U
#define TIMER_CCR1      200U

static void system_clock_init(void);
static void output_pin_init(void);
static void mco_init(void);
static void tim2_init(void);

/**
 * Main program.
 */
int main(void)
{
    system_clock_init();
    output_pin_init();
    mco_init();
    tim2_init();

    while (1) {
        /*
         * Wait for interrupts.
         * All waveform timing is handled by TIM2.
         */
        __WFI();
    }
}

/**
 * Configure MSI as the 4 MHz system clock.
 */
static void system_clock_init(void)
{
    /*
     * Enable MSI.
     */
    RCC->CR |= RCC_CR_MSION;

    while ((RCC->CR & RCC_CR_MSIRDY) == 0U) {
    }

    /*
     * Select MSI range 6, which is 4 MHz.
     */
    RCC->CR &= ~RCC_CR_MSIRANGE;
    RCC->CR |= RCC_CR_MSIRGSEL | RCC_CR_MSIRANGE_6;

    /*
     * Use no division for AHB, APB1, or APB2.
     */
    RCC->CFGR &= ~(
        RCC_CFGR_HPRE  |
        RCC_CFGR_PPRE1 |
        RCC_CFGR_PPRE2
    );

    /*
     * Select MSI as the system clock.
     */
    RCC->CFGR &= ~RCC_CFGR_SW;

    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_MSI) {
    }

    /*
     * Four MHz does not require Flash wait states.
     */
    FLASH->ACR &= ~FLASH_ACR_LATENCY;

    while ((FLASH->ACR & FLASH_ACR_LATENCY) != FLASH_ACR_LATENCY_0WS) {
    }

    /*
     * Update the CMSIS system-clock variable.
     */
    SystemCoreClockUpdate();
}

/**
 * Configure PA5 as the square-wave output.
 */
static void output_pin_init(void)
{
    /*
     * Enable the GPIOA peripheral clock.
     */
    RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN;

    /*
     * PA5 = general-purpose output mode.
     */
    GPIOA->MODER &= ~GPIO_MODER_MODE5;
    GPIOA->MODER |= GPIO_MODER_MODE5_0;

    /*
     * Push-pull output.
     */
    GPIOA->OTYPER &= ~GPIO_OTYPER_OT5;

    /*
     * No pull-up or pull-down resistor.
     */
    GPIOA->PUPDR &= ~GPIO_PUPDR_PUPD5;

    /*
     * High-speed output.
     */
    GPIOA->OSPEEDR |= GPIO_OSPEEDR_OSPEED5;

    /*
     * Start the output low.
     */
    GPIOA->BRR = (1UL << OUTPUT_PIN);
}

/**
 * Output the 4 MHz MSI clock through MCO on PA8.
 */
static void mco_init(void)
{
    /*
     * Enable the GPIOA peripheral clock.
     */
    RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN;

    /*
     * Select MSI as the MCO source with no prescaling.
     */
    RCC->CFGR =
        (RCC->CFGR &
         ~(RCC_CFGR_MCOSEL | RCC_CFGR_MCOPRE)) |
        RCC_CFGR_MCOSEL_0;

    /*
     * PA8 = alternate-function mode.
     */
    GPIOA->MODER &= ~GPIO_MODER_MODE8;
    GPIOA->MODER |= GPIO_MODER_MODE8_1;

    /*
     * Push-pull output.
     */
    GPIOA->OTYPER &= ~GPIO_OTYPER_OT8;

    /*
     * No pull-up or pull-down resistor.
     */
    GPIOA->PUPDR &= ~GPIO_PUPDR_PUPD8;

    /*
     * High-speed output.
     */
    GPIOA->OSPEEDR |= GPIO_OSPEEDR_OSPEED8;

    /*
     * PA8 uses AF0 for MCO.
     */
    GPIOA->AFR[1] &= ~GPIO_AFRH_AFSEL8;
}

/**
 * Configure TIM2 for a 5 kHz period and 25% duty cycle.
 *
 * Timer clock = 4 MHz
 * Timer tick  = 0.25 us
 *
 * ARR = 799:
 * 800 ticks x 0.25 us = 200 us
 * 1 / 200 us = 5 kHz
 *
 * CCR1 = 200:
 * 200 ticks x 0.25 us = 50 us high time
 */
static void tim2_init(void)
{
    /*
     * Enable the TIM2 peripheral clock.
     */
    RCC->APB1ENR1 |= RCC_APB1ENR1_TIM2EN;

    /*
     * Stop and reset the timer configuration.
     *
     * DIR = 0: up-counting
     * CMS = 0: edge-aligned
     * ARPE = 1: buffered ARR
     */
    TIM2->CR1 = TIM_CR1_ARPE;

    /*
     * No prescaling:
     * 4 MHz / (PSC + 1) = 4 MHz.
     */
    TIM2->PSC = TIMER_PRESCALER;

    /*
     * Full waveform period.
     */
    TIM2->ARR = TIMER_ARR;

    /*
     * Compare event at 25% of the period.
     */
    TIM2->CCR1 = TIMER_CCR1;

    /*
     * Begin counting at zero.
     */
    TIM2->CNT = 0U;

    /*
     * Enable:
     * - update interrupt
     * - channel 1 compare interrupt
     */
    TIM2->DIER = TIM_DIER_UIE | TIM_DIER_CC1IE;

    /*
     * Load PSC and ARR values into the timer.
     */
    TIM2->EGR = TIM_EGR_UG;

    /*
     * Clear any flags generated during initialization.
     */
    TIM2->SR = 0U;

    /*
     * Configure and enable the TIM2 interrupt in the NVIC.
     */
    NVIC_SetPriority(TIM2_IRQn, 0U);
    NVIC_ClearPendingIRQ(TIM2_IRQn);
    NVIC_EnableIRQ(TIM2_IRQn);

    /*
     * Start the first period high.
     */
    GPIOA->BSRR = (1UL << OUTPUT_PIN);

    /*
     * Start TIM2.
     */
    TIM2->CR1 |= TIM_CR1_CEN;
}

/**
 * TIM2 interrupt service routine.
 */
void TIM2_IRQHandler(void)
{
    /*
     * Update event:
     * the counter completed a period and restarted at zero.
     */
    if ((TIM2->SR & TIM_SR_UIF) != 0U) {
        /*
         * Clear the update interrupt flag.
         */
        TIM2->SR &= ~TIM_SR_UIF;

        /*
         * Start the new period high.
         */
        GPIOA->BSRR = (1UL << OUTPUT_PIN);
    }

    /*
     * Channel 1 compare event:
     * 25% of the period has elapsed.
     */
    if ((TIM2->SR & TIM_SR_CC1IF) != 0U) {
        /*
         * Clear the channel 1 interrupt flag.
         */
        TIM2->SR &= ~TIM_SR_CC1IF;

        /*
         * Set the output low for the remaining 75%.
         */
        GPIOA->BRR = (1UL << OUTPUT_PIN);
    }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
