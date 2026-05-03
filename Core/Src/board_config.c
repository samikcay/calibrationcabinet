#include "board_config.h"

static uint8_t initialized = 0U;

void Board_TIM3_PWM_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    uint32_t pclk1;
    uint32_t timer_clock;

    if (initialized != 0U) {
        return;
    }

    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_TIM3_CLK_ENABLE();

    HAL_GPIO_WritePin(BTS_R_EN_GPIO_Port, BTS_R_EN_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(BTS_L_EN_GPIO_Port, BTS_L_EN_Pin, GPIO_PIN_RESET);

    GPIO_InitStruct.Pin = BTS_R_EN_Pin | BTS_L_EN_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_1 | GPIO_PIN_5;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF2_TIM3;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);
    GPIO_InitStruct.Pin = GPIO_PIN_0;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_6;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF2_TIM3;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    pclk1 = HAL_RCC_GetPCLK1Freq();
    timer_clock = (pclk1 == HAL_RCC_GetHCLKFreq()) ? pclk1 : (pclk1 * 2U);

    TIM3->CR1 = 0U;
    TIM3->PSC = (timer_clock / 1000000U) - 1U;
    TIM3->ARR = BOARD_TIM3_PWM_PERIOD;
    TIM3->CCR1 = 0U;
    TIM3->CCR2 = 0U;
    TIM3->CCR3 = 0U;
    TIM3->CCR4 = 0U;

    TIM3->CCMR1 = (6U << TIM_CCMR1_OC1M_Pos) | TIM_CCMR1_OC1PE |
                  (6U << TIM_CCMR1_OC2M_Pos) | TIM_CCMR1_OC2PE;
    TIM3->CCMR2 = (6U << TIM_CCMR2_OC3M_Pos) | TIM_CCMR2_OC3PE |
                  (6U << TIM_CCMR2_OC4M_Pos) | TIM_CCMR2_OC4PE;
    TIM3->CCER = TIM_CCER_CC1E | TIM_CCER_CC2E | TIM_CCER_CC3E | TIM_CCER_CC4E;
    TIM3->EGR = TIM_EGR_UG;
    TIM3->CR1 = TIM_CR1_ARPE | TIM_CR1_CEN;

    initialized = 1U;
}

uint32_t Board_TIM3_PWM_Period(void)
{
    return BOARD_TIM3_PWM_PERIOD;
}

void Board_TIM3_PWM_Set(uint8_t channel, uint32_t compare)
{
    if (compare > BOARD_TIM3_PWM_PERIOD) {
        compare = BOARD_TIM3_PWM_PERIOD;
    }

    switch (channel) {
        case BOARD_TIM3_CH1:
            TIM3->CCR1 = compare;
            break;
        case BOARD_TIM3_CH2:
            TIM3->CCR2 = compare;
            break;
        case BOARD_TIM3_CH3:
            TIM3->CCR3 = compare;
            break;
        case BOARD_TIM3_CH4:
            TIM3->CCR4 = compare;
            break;
        default:
            break;
    }
}

void Board_MainPeltier_Enable(uint8_t enabled)
{
    GPIO_PinState state = (enabled != 0U) ? GPIO_PIN_SET : GPIO_PIN_RESET;

    HAL_GPIO_WritePin(BTS_R_EN_GPIO_Port, BTS_R_EN_Pin, state);
    HAL_GPIO_WritePin(BTS_L_EN_GPIO_Port, BTS_L_EN_Pin, state);
}

void Board_MainPeltier_SetHeat(uint32_t compare)
{
    Board_TIM3_PWM_Set(BOARD_TIM3_CH1, compare);
}

void Board_MainPeltier_SetCool(uint32_t compare)
{
    Board_TIM3_PWM_Set(BOARD_TIM3_CH2, compare);
}

void Board_Humidifier_Set(uint32_t compare)
{
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0,
                      (compare > 0U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void Board_CondensationPeltier_Set(uint32_t compare)
{
    Board_TIM3_PWM_Set(BOARD_TIM3_CH4, compare);
}
