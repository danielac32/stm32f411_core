/* usb_app.c – TinyUSB CDC echo + MSC over W25Q128 (stage 2). */

#include "tusb.h"
#include "w25qxxx.h"

/* ------------------------------------------------------------------ */
/*  xinu clock tick (ms)                                               */
/* ------------------------------------------------------------------ */
extern uint32 count1000;

uint32_t tusb_time_millis_api(void) {
  return count1000;
}

/* ------------------------------------------------------------------ */
/*  USB IRQ handler (vector table entry: OTG_FS_IRQHandler)            */
/* ------------------------------------------------------------------ */
void OTG_FS_IRQHandler(void) { tud_int_handler(0); }
TU_ATTR_WEAK void tusb_apply_dcd_update(void) {}

volatile uint32_t cdc_rx_bytes = 0; /* counts bytes consumed by tud_cdc_rx_cb */

/* Send a NUL-terminated string to the CDC IN FIFO. */
static void cdc_puts(const char *s) {
  uint32_t n = 0;
  while (s[n]) n++;
  tud_cdc_write(s, (uint16_t)n);
  tud_cdc_write_flush();
}

/* DTR callback – send a banner when the host opens the port. */
void tud_cdc_line_state_cb(uint8_t itf, bool dtr, bool rts) {
  (void)itf; (void)rts;
  if (dtr) cdc_puts("CDC echo ready\r\n> ");
}

void tud_cdc_line_coding_cb(uint8_t itf, cdc_line_coding_t const *p) {
  (void)itf; (void)p;
}
void tud_cdc_set_line_coding_cb(uint8_t itf, cdc_line_coding_t const *p) {
  (void)itf; (void)p;
}

/* tud_cdc_rx_cb – echo received bytes back. */
void tud_cdc_rx_cb(uint8_t itf) {
  (void)itf;
  uint8_t buf[64];
  while (tud_cdc_available()) {
    uint32_t n = tud_cdc_read(buf, sizeof(buf));
    if (n == 0) break;
    cdc_rx_bytes += n;
    tud_cdc_write(buf, n);
  }
  tud_cdc_write_flush();
}

/* ================================================================== */
/*  MSC over W25Q128                                                    */
/* ================================================================== */

/* disk_read / disk_write in w25qxxx.c return 0 on success. TinyUSB
 * expects a positive byte-count, so always return the request size. */

static uint32_t msc_block_count;
static uint16_t msc_block_size;

uint32_t tud_msc_inquiry2_cb(uint8_t lun, scsi_inquiry_resp_t *r,
                             uint32_t bufsize) {
  (void)lun; (void)bufsize;
  const char vendor[]  = "DANIEL  ";
  const char product[] = "W25Q128 XINU  ";
  const char rev[]     = "1.0 ";
  r->peripheral_device_type = 0;
  r->peripheral_qualifier   = 0;
  r->is_removable           = 1;
  r->version                = 6;
  r->response_data_format   = 2;
  r->additional_length      = 0x1F;
  for (uint8_t i = 0; i < 8;  i++) r->vendor_id[i]  = vendor[i];
  for (uint8_t i = 0; i < 16; i++) r->product_id[i] = (i < 14) ? product[i] : ' ';
  for (uint8_t i = 0; i < 4;  i++) r->product_rev[i] = rev[i];
  return sizeof(scsi_inquiry_resp_t);
}

bool tud_msc_test_unit_ready_cb(uint8_t lun) { (void)lun; return true; }

void tud_msc_capacity_cb(uint8_t lun, uint32_t *block_count,
                         uint16_t *block_size) {
  (void)lun;
  flash_info_t *info = flash_spi_getcardinfo();
  msc_block_count = info->card_size;   /* 31250 */
  msc_block_size  = info->sect_size;   /* 512   */
  *block_count = msc_block_count;
  *block_size  = msc_block_size;
}

bool tud_msc_start_stop_cb(uint8_t lun, uint8_t pc, bool start,
                           bool load_eject) {
  (void)lun; (void)pc; (void)start; (void)load_eject;
  return true;
}

int32_t tud_msc_read10_cb(uint8_t lun, uint32_t lba, uint32_t offset,
                          void *buffer, uint32_t bufsize) {
  (void)lun; (void)offset;
  w25qxxx_drv.read(buffer, lba, bufsize / msc_block_size);
  return (int32_t)bufsize;
}

int32_t tud_msc_write10_cb(uint8_t lun, uint32_t lba, uint32_t offset,
                           uint8_t *buffer, uint32_t bufsize) {
  (void)lun; (void)offset;
  w25qxxx_drv.write(buffer, lba, bufsize / msc_block_size);
  return (int32_t)bufsize;
}

bool tud_msc_is_writable_cb(uint8_t lun) { (void)lun; return true; }

int32_t tud_msc_scsi_cb(uint8_t lun, uint8_t const cmd[16],
                        void *buffer, uint16_t bufsize) {
  (void)lun; (void)cmd; (void)buffer; (void)bufsize;
  return -1;
}