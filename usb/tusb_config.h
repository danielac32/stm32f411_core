/* tusb_config.h - TinyUSB configuration for the STM32F411 xinu bare-metal core
 *
 * Device: USB CDC (serial) + USB MSC (Mass Storage over W25Q128 SPI flash)
 */

#ifndef TUSB_CONFIG_H_
#define TUSB_CONFIG_H_

#include "stm32.h"

#ifdef __cplusplus
extern "C" {
#endif

//--------------------------------------------------------------------+
// MCU and OS
//--------------------------------------------------------------------+
#define CFG_TUSB_MCU              OPT_MCU_STM32F4

#ifndef CFG_TUSB_OS
#define CFG_TUSB_OS               OPT_OS_NONE
#endif

#define CFG_TUSB_DEBUG            0

// Port0 = USB OTG FS, device only
#define CFG_TUSB_RHPORT0_MODE     OPT_MODE_DEVICE

//--------------------------------------------------------------------+
// DWC2 (Synopsys OTG) driver
//--------------------------------------------------------------------+
// F411 OTG_FS has no internal DMA-abm transfer; use slave (register) mode.
#define CFG_TUD_DWC2_SLAVE_ENABLE 1
#define CFG_TUD_DWC2_DMA_ENABLE   0

//--------------------------------------------------------------------+
// Device stack
//--------------------------------------------------------------------+
#define CFG_TUD_ENDPOINT0_SIZE    64

// Class enable
#define CFG_TUD_CDC               1
#define CFG_TUD_MSC               1

// CDC buffer size (in bytes)
#define CFG_TUD_CDC_EP_BUFSIZE    256
#define CFG_TUD_CDC_TX_BUFSIZE    256
#define CFG_TUD_CDC_RX_BUFSIZE    256

// MSC: max logical unit
#define CFG_TUD_MSC_BUFSIZE       512

// USB descriptors (vendor/product ids)
#define USB_VID                   0xCafe
#define USB_PID                   0x4237

#ifdef __cplusplus
}
#endif

#endif /* TUSB_CONFIG_H_ */