/* usb_descriptors.c - USB device/configuration descriptors for CDC + MSC */

#include "tusb.h"

/* Invoked when GET DEVICE DESCRIPTOR */
uint8_t const* tud_descriptor_device_cb(void) {
  static tusb_desc_device_t const desc_device = {
      .bLength            = sizeof(tusb_desc_device_t),
      .bDescriptorType    = TUSB_DESC_DEVICE,
      .bcdUSB             = 0x0200,
      .bDeviceClass       = TUSB_CLASS_MISC,
      .bDeviceSubClass    = MISC_SUBCLASS_COMMON,
      .bDeviceProtocol    = MISC_PROTOCOL_IAD,
      .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
      .idVendor           = USB_VID,
      .idProduct          = USB_PID,
      .bcdDevice          = 0x0100,
      .iManufacturer      = 1,
      .iProduct           = 2,
      .iSerialNumber      = 3,
      .bNumConfigurations = 1,
  };
  return (uint8_t const*) &desc_device;
}

/* Invoked when GET CONFIGURATION DESCRIPTOR */
uint8_t const* tud_descriptor_configuration_cb(uint8_t index) {
  (void) index;

  /* Config number, interface count, string index, total length, attribute, power in mA */
  static uint8_t desc_configuration[] = {
      TUD_CONFIG_DESCRIPTOR(1, 3, 0, 0x0062, TUSB_DESC_CONFIG_ATT_SELF_POWERED, 100),

      /* Interface 0, Alternate 0, 2 endpoints, HS-CDC */
      TUD_CDC_DESCRIPTOR(0, 4, 0x81, 8, 0x02, 0x82, 64),

      /* Interface 2, Alternate 0, 2 endpoints, MSC */
      TUD_MSC_DESCRIPTOR(2, 5, 0x03, 0x83, 64),
  };
  return desc_configuration;
}

/* Invoked when GET STRING DESCRIPTOR */
uint16_t const* tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
  (void) langid;

  static uint16_t const langid_desc[] = {0x0409, 0x0409}; /* English */

  const char* str;
  switch (index) {
    case 0:  return langid_desc;
    case 1:  str = "daniel";                 break;
    case 2:  str = "STM32F411 xinu USB";     break;
    case 3:  str = "123456";                 break;
    case 4:  str = "xinu CDC";               break;
    case 5:  str = "W25Q128 xinu drive";     break;
    default: str = "xinu";                   break;
  }

  static uint16_t buf[32 + 1];
  buf[0] = (uint16_t) ((strlen(str) << 8) | TUSB_DESC_STRING);
  uint8_t i;
  for (i = 0; str[i] != '\0' && i < 32; i++) {
    buf[i + 1] = str[i];
  }
  return buf;
}