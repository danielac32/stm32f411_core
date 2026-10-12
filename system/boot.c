#include <memory.h>
#include <interrupt.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stm32.h>
#include <gpio.h>
#include <spi.h>
#include <w25qxxx.h>
 


extern void (*vector_table[])(void); 
extern void nulluser();
extern  void meminit(void); /* Initializes the free memory list */

int ready_preemptive=0;
#if !defined  (HSE_VALUE) 
  #define HSE_VALUE    ((uint32_t)25000000) /*!< Default value of the External oscillator in Hz */
#endif /* HSE_VALUE */

#if !defined  (HSI_VALUE)
  #define HSI_VALUE    ((uint32_t)16000000) /*!< Value of the Internal oscillator in Hz*/
#endif /* HSI_VALUE */

uint32_t SystemCoreClock = 16000000;
const uint8_t AHBPrescTable[16] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 6, 7, 8, 9};
const uint8_t APBPrescTable[8]  = {0, 0, 0, 0, 1, 2, 3, 4};


/* F411CE: HSE 25MHz -> SYSCLK 96MHz y PLL48CLK = 48MHz exacto para USB.
 * PLL_VCO = HSE / PLL_M * PLL_N = 25/25 * 384 = 384MHz (dentro de 100-432MHz)
 * SYSCLK  = PLL_VCO / PLL_P      = 384 / 4 = 96MHz
 * PLL48CLK= PLL_VCO / PLL_Q      = 384 / 8 = 48MHz (USB OTG FS requiere 48MHz)
 */
#define PLL_M      25
#define PLL_N      384
#define PLL_P      4
#define PLL_Q      8

#define SYSCLK_HZ 96000000

static void SetSysClock(void)
{
    /* HSE clock enable */
    RCC->CR |= RCC_CR_HSEON;
    while (!(RCC->CR & RCC_CR_HSERDY));

    /* Power interface clock enable */
    RCC->APB1ENR |= RCC_APB1ENR_PWREN;
    /* Scale 1 mode: 2.7-3.6V, soporta hasta 100MHz en F411 */
    PWR->CR |= PWR_CR_VOS;

    /* AHB = SYSCLK / 1 */
    RCC->CFGR = RCC_CFGR_HPRE_DIV1;
    /* APB2 = AHB / 2 (50MHz max) */
    RCC->CFGR |= RCC_CFGR_PPRE2_DIV2;
    /* APB1 = AHB / 4 (50MHz max) */
    RCC->CFGR |= RCC_CFGR_PPRE1_DIV4;

    /* Configure the main PLL */
    RCC->PLLCFGR = PLL_M | (PLL_N << 6) | (((PLL_P >> 1) - 1) << 16) |
                   RCC_PLLCFGR_PLLSRC_HSE | (PLL_Q << 24);

    /* Enable the main PLL */
    RCC->CR |= RCC_CR_PLLON;
    while (!(RCC->CR & RCC_CR_PLLRDY));

    /* Apartir de 60MHz el F411 necesita 3 wait states (96MHz) + prefetch + caches */
    FLASH->ACR = FLASH_ACR_PRFTEN | FLASH_ACR_ICEN | FLASH_ACR_DCEN |
                 FLASH_ACR_LATENCY_3WS;

    /* Select the main PLL as system clock source */
    RCC->CFGR &= ~RCC_CFGR_SW;
    RCC->CFGR |= RCC_CFGR_SW_PLL;
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL);
}



 void init_systick(uint32_t s)
{
    // Clear CTRL register
    SysTick->CTRL = 0x00000;
    // Main clock source is running with HSI by default which is at 8 Mhz.
    // SysTick clock source can be set with CTRL register (Bit 2)
    // 0: Processor clock/8 (AHB/8)
    // 1: Processor clock (AHB)
    SysTick->CTRL |= (1 << 2);
    // Enable callback (bit 1)
    SysTick->CTRL |= (1 << 1);
    // Load the value
    SysTick->LOAD = (uint32_t)(s-1);
    // Set the current value to 0
    SysTick->VAL = 0;
    // Enable SysTick (bit 0)
    SysTick->CTRL |= (1 << 0);
}




void SystemCoreClockUpdate(void)
{
  uint32_t tmp = 0, pllvco = 0, pllp = 2, pllsource = 0, pllm = 2;
  
  /* Get SYSCLK source -------------------------------------------------------*/
  tmp = RCC->CFGR & RCC_CFGR_SWS;

  switch (tmp)
  {
    case 0x00:  /* HSI used as system clock source */
      SystemCoreClock = HSI_VALUE;
      break;
    case 0x04:  /* HSE used as system clock source */
      SystemCoreClock = HSE_VALUE;
      break;
    case 0x08:  /* PLL used as system clock source */

      /* PLL_VCO = (HSE_VALUE or HSI_VALUE / PLL_M) * PLL_N
         SYSCLK = PLL_VCO / PLL_P
         */    
      pllsource = (RCC->PLLCFGR & RCC_PLLCFGR_PLLSRC) >> 22;
      pllm = RCC->PLLCFGR & RCC_PLLCFGR_PLLM;
      
      if (pllsource != 0)
      {
        /* HSE used as PLL clock source */
        pllvco = (HSE_VALUE / pllm) * ((RCC->PLLCFGR & RCC_PLLCFGR_PLLN) >> 6);
      }
      else
      {
        /* HSI used as PLL clock source */
        pllvco = (HSI_VALUE / pllm) * ((RCC->PLLCFGR & RCC_PLLCFGR_PLLN) >> 6);
      }

      pllp = (((RCC->PLLCFGR & RCC_PLLCFGR_PLLP) >>16) + 1 ) *2;
      SystemCoreClock = pllvco/pllp;
      break;
    default:
      SystemCoreClock = HSI_VALUE;
      break;
  }
  /* Compute HCLK frequency --------------------------------------------------*/
  /* Get HCLK prescaler */
  tmp = AHBPrescTable[((RCC->CFGR & RCC_CFGR_HPRE) >> 4)];
  /* HCLK frequency */
  SystemCoreClock >>= tmp;
}



static void DWTInit(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk; 
}



void Reset_Handler(void) {
    unsigned int* src = &_sidata;
    unsigned int* dst = &_sdata;

    while (dst < &_edata)
        *dst++ = *src++;

    for (dst = &_sbss; dst < &_ebss; dst++)
        *dst = 0;
     disable();
    

    _BST(RCC->AHB1ENR, RCC_AHB1ENR_GPIOAEN);
    _BST(RCC->AHB1ENR, RCC_AHB1ENR_GPIOBEN);
    _BST(RCC->AHB1ENR, RCC_AHB1ENR_GPIOCEN);

    SCB->VTOR = (uintptr_t)&vector_table; /* Vector Table Relocation in Internal FLASH. */
    SetSysClock();
    SystemCoreClock = SYSCLK_HZ;
    init_systick(SYSCLK_HZ / 1000);
    DWTInit();
 
    nulluser();
    for (;;)
        __NOP(); // hang
}



