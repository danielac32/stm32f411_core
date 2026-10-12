/* Xinu for STM32
 *
 * Original license applies
 * Modifications for STM32 by Robin Krens
 * Please see LICENSE and AUTHORS 
 * 
 * $LOG$
 * 2019/11/11 - ROBIN KRENS
 * Initial version 
 * 
 * $DESCRIPTION$
 *
 * */

/* initialize.c - nulluser, sysinit */

/* Handle system initialization and become the null process */

#include <xinu.h>
 
#include <disk.h>
#include <fat_filelib.h>
#include <w25qxxx.h>
#include <tusb.h>

extern volatile uint32_t cdc_rx_bytes;
 

struct	memblk	memlist;	/* List of free memory blocks		*/



extern	void	start(void);	/* Start of Xinu code			*/
extern	void	*_end;		/* End of Xinu code			*/

/* Function prototypes */

extern	void main(void);	/* Main is the first process created	*/
static	void sysinit(); 	/* Internal system initialization	*/
extern	void meminit(void);	/* Initializes the free memory list	*/
 

 

 

int  initFat32(){
	uint32 size=sd_init();
    fl_init();
      // Attach media access functions to library
	if (fl_attach_media(sd_readsector, sd_writesector) != FAT_INIT_OK)
	{
	      printf("ERROR: Failed to init file system\n");
	      return -1;
	}
 
    return OK;
}
 
 



 
 

void	nulluser()
{	
	struct	memblk	*memptr;	/* Ptr to memory block		*/
	uint32	free_mem;		/* Total amount of free memory	*/

    hw_cfg_pin(GPIOx(GPIO_C),13,GPIOCFG_MODE_OUT | GPIOCFG_OSPEED_VHIGH  | GPIOCFG_OTYPE_PUPD | GPIOCFG_PUPD_PUP);
    hw_cfg_pin(GPIOx(GPIO_A),0,GPIOCFG_MODE_INP | GPIOCFG_OSPEED_VHIGH  | GPIOCFG_OTYPE_OPEN | GPIOCFG_PUPD_PUP);

    meminit();
	platinit();
    /* Enable interrupts */
	enable();

	/* Initialize the system */
   
    /* Wait up to ~2s for PA0 to be pulled low (manual button), then boot
     * anyway so the USB devices are always started autonomously. */
    {
      uint32 bcnt = 0;
      while(1){
        if(!hw_get_pin(GPIOx(GPIO_A),0)) break;
        hw_toggle_pin(GPIOx(GPIO_C),13);
        delay(50);
        if (++bcnt >= 40) break;
      }
    }
	
 
	/* Output Xinu memory layout */
	free_mem = 0;
	for (memptr = memlist.mnext; memptr != NULL;
						memptr = memptr->mnext) {
		free_mem += memptr->mlength;
	}
	kprintf ("Build date: %s %s\n\n", __DATE__, __TIME__);
	kprintf("%10d bytes of free memory.  Free list:\n", free_mem);
	for (memptr=memlist.mnext; memptr!=NULL;memptr = memptr->mnext) {
	    kprintf("           [0x%08X to 0x%08X]\n",
		(uint32)memptr, ((uint32)memptr) + memptr->mlength - 1);
	}

	kprintf("%10d bytes of Xinu code.\n",
		(uint32)&_etext - (uint32)&_text);
	kprintf("           [0x%08X to 0x%08X]\n",
		(uint32)&_text, (uint32)&_etext - 1);
	kprintf("%10d bytes of data.\n",
		(uint32)&_ebss - (uint32)&_sdata);
	kprintf("           [0x%08X to 0x%08X]\n\n",
		(uint32)&_sdata, (uint32)&_ebss - 1);

   
	/* Initialize the real time clock */
	clkinit();

    TIM2->CR1 |= (1 << 0); /* CEN: run (ARPE already set in clkinit) */ 
    NVIC_EnableIRQ(TIM2_IRQn);
   
    initFat32();

    /* TinyUSB device init: CDC + MSC */
    SPI_Flash_Init();
    tusb_init();





	{
      uint32_t last_led = 0;
      uint32_t last_hb  = 0;
      char hb[24];
      for(;;){
        tud_task();  /* USB device task (non-RTOS) */

        /* diagnostic heartbeat: blink PC13 every 500 count1000 ticks,
         * and emit a live status line on CDC once per second: reports
         * how many RX bytes tud_cdc_rx_cb consumed, so we can tell if
         * OUT data is reaching the app when commands seem unresponsive */
        if ((uint32_t)(count1000 - last_led) >= 500U) {
          last_led = count1000;
          hw_toggle_pin(GPIOx(GPIO_C),13);
        }
        if (tud_cdc_connected() && (uint32_t)(count1000 - last_hb) >= 1000U) {
          int n = sprintf(hb, ".rx=%u\r\n", (unsigned)cdc_rx_bytes);
          last_hb = count1000;
          tud_cdc_write(hb, (uint16_t)n);
          tud_cdc_write_flush();
        }
      }
    }

}

 