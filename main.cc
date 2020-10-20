
#include "xparameters.h"
#include "xdebug.h"
#include "test_data.h"
#include "my_dma.h"

#if (!defined(DEBUG))
extern void xil_printf(const char *format, ...);
#endif

#define DMA_DEV_ID			XPAR_AXIDMA_0_DEVICE_ID
#define TX_BUFFER_BASE		0x01000000
#define RX_BUFFER_BASE		0x05000000
#define RX_BUFFER_HIGH		0x3FFFFFFF
#define MAX_PKT_LEN			67108863
#define TEST_START_VALUE	0xC

#define NUMBER_OF_TRANSFERS	10

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
