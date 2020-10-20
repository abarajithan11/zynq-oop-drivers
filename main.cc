
/***************************** Include Files *********************************/

#include "xparameters.h"
#include "xdebug.h"
#include "test_data.h"
#include "my_dma.h"


/******************** Constant Definitions **********************************/

/*
 * Device hardware build related constants.
 */

#define DMA_DEV_ID		XPAR_AXIDMA_0_DEVICE_ID

// XPAR_PS7_DDR_0_S_AXI_BASEADDR 0x00100000 also works

#ifdef XPAR_AXI_7SDDR_0_S_AXI_BASEADDR
#define DDR_BASE_ADDR		XPAR_AXI_7SDDR_0_S_AXI_BASEADDR
#elif XPAR_MIG7SERIES_0_BASEADDR
#define DDR_BASE_ADDR	XPAR_MIG7SERIES_0_BASEADDR
#elif XPAR_MIG_0_BASEADDR
#define DDR_BASE_ADDR	XPAR_MIG_0_BASEADDR
#elif XPAR_PSU_DDR_0_S_AXI_BASEADDR
#define DDR_BASE_ADDR	XPAR_PSU_DDR_0_S_AXI_BASEADDR
#endif

#ifndef DDR_BASE_ADDR
#warning CHECK FOR THE VALID DDR ADDRESS IN XPARAMETERS.H, \
		 DEFAULT SET TO 0x01000000
#define MEM_BASE_ADDR		0x01000000
#else
#define MEM_BASE_ADDR		(DDR_BASE_ADDR + 0x1000000)
#endif

//#define TX_BUFFER_BASE		(MEM_BASE_ADDR + 0x00100000) //0x1100000
//#define RX_BUFFER_BASE		(MEM_BASE_ADDR + 0x00300000) //0x1300000
//#define RX_BUFFER_HIGH		(MEM_BASE_ADDR + 0x004FFFFF) //0x14FFFFF

#define TX_BUFFER_BASE		0x01000000
#define RX_BUFFER_BASE		0x05000000
#define RX_BUFFER_HIGH		0x3FFFFFFF

#define MAX_PKT_LEN		67108863

#define TEST_START_VALUE	0xC

#define NUMBER_OF_TRANSFERS	10


#if (!defined(DEBUG))
extern void xil_printf(const char *format, ...);
#endif

//class New_DMA: public My_DMA{
//public:
//	New_DMA(uint16_t device_id): My_DMA{device_id}{}
//	void tx_done_callback()
//	{
//		xil_printf("TX callback, DMA derived class \r\n");
//	}
//
//	void rx_done_callback()
//	{
//		xil_printf("RX callback, DMA derived class \r\n");
//	}
//};

void tx_done_callback(My_DMA* dma)
{
	xil_printf("TX callback, custom function \r\n");
}

void rx_done_callback(My_DMA* dma)
{
	xil_printf("RX callback, custom function \r\n");
}

int main()
{
	int Status;
	xil_printf("\r\n--- Entering main() --- \r\n");

	My_DMA dma(DMA_DEV_ID);
	Status = dma.intr_init(TX_INTR_ID, RX_INTR_ID);
	if (Status != XST_SUCCESS) xil_printf("Init failed with code: %d\r\n", Status);

	// Attach custom callbacks
	dma.tx_done_callback = tx_done_callback;
	dma.rx_done_callback = rx_done_callback;

	Status = dma.intr_test(TX_BUFFER_BASE, RX_BUFFER_BASE, MAX_PKT_LEN, NUMBER_OF_TRANSFERS);
	if (Status != XST_SUCCESS)
	{
		xil_printf("Failed\r\n");
		return XST_FAILURE;
	}

	xil_printf("Successfully ran  Example\r\n");
	xil_printf("--- Exiting main() --- \r\n");
	return XST_SUCCESS;

}
