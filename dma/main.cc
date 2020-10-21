
#include "xparameters.h"
#include "xdebug.h"
#include "my_test_data.h"
#include "my_dma.h"

#if (!defined(DEBUG))
extern void xil_printf(const char *format, ...);
#endif

#define DMA_DEV_ID				XPAR_AXIDMA_0_DEVICE_ID
#define FROM_DDR_BUFFER_BASE	0x01000000
#define TO_DDR_BUFFER_BASE		0x05000000
#define TO_DDR_BUFFER_HIGH		0x3FFFFFFF
#define MAX_PKT_LEN				67108863
#define TEST_START_VALUE		0xC

#define NUMBER_OF_TRANSFERS	10

void from_ddr_done_callback(My_DMA* dma)
{
	xil_printf("from_ddr_done callback, custom function \r\n");
}

void to_ddr_done_callback(My_DMA* dma)
{
	xil_printf("to_ddr_done callback, custom function \r\n");
}

int main()
{
	int status;
	xil_printf("\r\n--- Entering main() --- \r\n");

	My_DMA dma(DMA_DEV_ID);
	status = dma.intr_init(FROM_DDR_INTR_ID, TO_DDR_INTR_ID);
	if (status != XST_SUCCESS) xil_printf("Init failed with code: %d\r\n", status);

	// Attach custom callbacks
	dma.from_ddr_done_callback = from_ddr_done_callback;
	dma.to_ddr_done_callback = to_ddr_done_callback;

	status = dma.intr_test(FROM_DDR_BUFFER_BASE, TO_DDR_BUFFER_BASE, MAX_PKT_LEN, NUMBER_OF_TRANSFERS);
	if (status != XST_SUCCESS)
	{
		xil_printf("Failed\r\n");
		return XST_FAILURE;
	}

	xil_printf("Successfully ran  Example\r\n");
	xil_printf("--- Exiting main() --- \r\n");
	return XST_SUCCESS;

}
