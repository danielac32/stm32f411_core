/*
 * SPDX-FileCopyrightText: Copyright (c) 2021, Ha Thach (tinyusb.org)
 * SPDX-License-Identifier: MIT
 *
 * DWC2 STM32F411 adaptation for the xinu bare-metal core.
 * Uses the project's own CMSIS header (stm32.h -> stm32f411xe.h)
 * instead of the full STM32Cube stm32f4xx.h.
 */

#ifndef DWC2_STM32_H_
#define DWC2_STM32_H_

#include "stm32.h"

extern uint32_t SystemCoreClock;

// F411 OTG_FS: 4 bidirectional endpoints included EP0, 320 * 32-bit DFIFO words.
#define EP_MAX_FS       4
#define DFIFO_DEPTH_FS  320

// F411 has only the OTG_FS port (no OTG_HS). The inherited F405 header
// defines USB_OTG_HS_PERIPH_BASE, but the silicon does not exist: undef it
// so the DWC2 driver only registers port0.
#undef  USB_OTG_HS_PERIPH_BASE
#ifdef USB_OTG_HS_PERIPH_BASE
  #define EP_MAX_HS       USB_OTG_HS_MAX_IN_ENDPOINTS
  #define DFIFO_DEPTH_HS  1024
#endif

// OTG HS always has higher number of endpoints than FS
#ifdef USB_OTG_HS_PERIPH_BASE
  #define DWC2_EP_MAX   EP_MAX_HS
#else
  #define DWC2_EP_MAX   EP_MAX_FS
#endif

// Port0 = OTG_FS
static const dwc2_controller_t _dwc2_controller[] = {
    #ifdef USB_OTG_FS_PERIPH_BASE
    { .reg_base = USB_OTG_FS_PERIPH_BASE, .irqnum = OTG_FS_IRQn, .ep_count = EP_MAX_FS, .otg_dfifo_depth = DFIFO_DEPTH_FS },
    #endif

    #ifdef USB_OTG_HS_PERIPH_BASE
    { .reg_base = USB_OTG_HS_PERIPH_BASE, .irqnum = OTG_HS_IRQn, .ep_count = EP_MAX_HS, .otg_dfifo_depth = DFIFO_DEPTH_HS },
    #endif
};

// Reset the bits specific to the STM32 general core config register.
// TinyUSB expects the STM32Cube names; map them to the project header names.
#ifndef STM32_GCCFG_PWRDWN
#define STM32_GCCFG_PWRDWN       USB_OTG_GCCFG_PWRDWN
#endif
#ifndef STM32_GCCFG_I2CPADEN
#define STM32_GCCFG_I2CPADEN     USB_OTG_GCCFG_I2CPADEN
#endif
#ifndef STM32_GCCFG_VBUSASEN
#define STM32_GCCFG_VBUSASEN     USB_OTG_GCCFG_VBUSASEN
#endif
#ifndef STM32_GCCFG_VBUSBSEN
#define STM32_GCCFG_VBUSBSEN     USB_OTG_GCCFG_VBUSBSEN
#endif
#ifndef STM32_GCCFG_SOFOUTEN
#define STM32_GCCFG_SOFOUTEN     USB_OTG_GCCFG_SOFOUTEN
#endif
#ifndef STM32_GCCFG_NOVBUSSENS
#define STM32_GCCFG_NOVBUSSENS   USB_OTG_GCCFG_NOVBUSSENS
#endif

// Not present on F411 OTG_FS; define as 0 so the shared code path compiles.
#ifndef STM32_GCCFG_PHYHSEN
#define STM32_GCCFG_PHYHSEN      0u
#endif
#ifndef STM32_GCCFG_VBDEN
#define STM32_GCCFG_VBDEN        0u
#endif
#ifndef STM32_GCCFG_PULLDOWNEN
#define STM32_GCCFG_PULLDOWNEN   0u
#endif
#ifndef STM32_GCCFG_VBVALEXTOEN
#define STM32_GCCFG_VBVALEXTOEN  0u
#endif
#ifndef STM32_GCCFG_VBVALOVAL
#define STM32_GCCFG_VBVALOVAL    0u
#endif

// GUSBCFG_TRDT mapping
#ifndef GUSBCFG_TRDT_Pos
#define GUSBCFG_TRDT_Pos         USB_OTG_GUSBCFG_TRDT_Pos
#endif
#ifndef GUSBCFG_TRDT_Msk
#define GUSBCFG_TRDT_Msk         USB_OTG_GUSBCFG_TRDT_Msk
#endif

// MCU specific to enable dwc2 clock/power before any access to register.
// F411: enable OTG_FS clock and configure PA11 (DM), PA12 (DP) as AF10.
TU_ATTR_ALWAYS_INLINE static inline void dwc2_clock_init(uint8_t rhport, tusb_role_t role) {
  (void) rhport;
  (void) role;

  RCC->AHB2ENR |= RCC_AHB2ENR_OTGFSEN;

  // PA11 = USB_DM, PA12 = USB_DP on alternate function 10 (OTG_FS)
  if (RCC->AHB1ENR & RCC_AHB1ENR_GPIOAEN) {}
  else { RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN; }

  _BMD(GPIOA->MODER, (0x03 << 22) | (0x03 << 24), (0x02 << 22) | (0x02 << 24));   // PA11/PA12 -> AF
  _BST(GPIOA->AFR[1], (0x0A << 12) | (0x0A << 16));                              // AF10 for PA11, PA12
}

TU_ATTR_ALWAYS_INLINE static inline void dwc2_int_set(uint8_t rhport, tusb_role_t role, bool enabled) {
  (void) role;
  const IRQn_Type irqn = (IRQn_Type) _dwc2_controller[rhport].irqnum;
  if (enabled) {
    NVIC_EnableIRQ(irqn);
  } else {
    NVIC_DisableIRQ(irqn);
  }
}

#define dwc2_dcd_int_enable(_rhport)  dwc2_int_set(_rhport, TUSB_ROLE_DEVICE, true)
#define dwc2_dcd_int_disable(_rhport) dwc2_int_set(_rhport, TUSB_ROLE_DEVICE, false)

TU_ATTR_ALWAYS_INLINE static inline void dwc2_remote_wakeup_delay(void) {
  uint32_t count = SystemCoreClock / 1000;
  while (count--) {
    __NOP();
  }
}

// MCU specific PHY init, called BEFORE core reset
static inline void dwc2_phy_init(dwc2_regs_t* dwc2, uint8_t hs_phy_type) {
  if (hs_phy_type == GHWCFG2_HSPHY_NOT_SUPPORTED) {
    // Enable on-chip FS PHY.
    // On STM32 F4 the PWRDWN bit has inverted polarity: 1 = PHY powered on.
    dwc2->stm32_gccfg |= STM32_GCCFG_PWRDWN;
  }
}

// MCU specific PHY deinit, disable PHY power
static inline void dwc2_phy_deinit(dwc2_regs_t* dwc2, uint8_t hs_phy_type) {
  if (hs_phy_type == GHWCFG2_HSPHY_NOT_SUPPORTED) {
    dwc2->stm32_gccfg &= ~STM32_GCCFG_PWRDWN;
  }
}

// MCU specific PHY update, it is called AFTER init() and core reset
static inline void dwc2_phy_update(dwc2_regs_t* dwc2, uint8_t hs_phy_type) {
  if (hs_phy_type == GHWCFG2_HSPHY_NOT_SUPPORTED) {
    // Turnaround timeout depends on the AHB clock dictated by STM32 Reference Manual
    uint32_t turnaround;
    if (SystemCoreClock >= 32000000u)      turnaround = 0x6u;
    else if (SystemCoreClock >= 27500000u) turnaround = 0x7u;
    else if (SystemCoreClock >= 24000000u) turnaround = 0x8u;
    else if (SystemCoreClock >= 21800000u) turnaround = 0x9u;
    else if (SystemCoreClock >= 20000000u) turnaround = 0xAu;
    else if (SystemCoreClock >= 18500000u) turnaround = 0xBu;
    else if (SystemCoreClock >= 17200000u) turnaround = 0xCu;
    else if (SystemCoreClock >= 16000000u) turnaround = 0xDu;
    else if (SystemCoreClock >= 15000000u) turnaround = 0xEu;
    else                                  turnaround = 0xFu;

    dwc2->gusbcfg = (dwc2->gusbcfg & ~GUSBCFG_TRDT_Msk) | (turnaround << GUSBCFG_TRDT_Pos);
  }
}

// GCCFG configuration for the STM32 core.
static inline void dwc2_stm32_gccfg_cfg(dwc2_regs_t* dwc2, bool vbus_sensing, bool is_host) {
  if (is_host) {
    vbus_sensing = false;
  }

  // F411 OTG_FS: VBUS sensing is optional. Black-Pill style boards do not
  // route VBUS to the OTG_FS_VBUS pin, so disable sensing to connect D+.
  (void) vbus_sensing;
  dwc2->stm32_gccfg |= STM32_GCCFG_NOVBUSSENS;
}

#endif /* DWC2_STM32_H_ */