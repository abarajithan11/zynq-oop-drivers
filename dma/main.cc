
#include "xparameters.h"
#include "xdebug.h"
#include "my_test_data.h"
#include "my_dma.h"

#if (!defined(DEBUG))
extern void xil_printf(const char *format, ...);
#endif

#if defined XPAR_SCUGIC_SINGLE_DEVICE_ID || XPAR_INTC_0_DEVICE_ID
	#ifdef XPAR_INTC_0_DEVICE_ID
		#define S2MM_INTR_ID		XPAR_INTC_0_AXIDMA_0_S2MM_INTROUT_VEC_ID
		#define MM2S_INTR_ID		XPAR_INTC_0_AXIDMA_0_MM2S_INTROUT_VEC_ID
	#endif
	#ifdef XPAR_SCUGIC_SINGLE_DEVICE_ID
		#define S2MM_INTR_ID		XPAR_FABRIC_AXIDMA_0_S2MM_INTROUT_VEC_ID
		#define MM2S_INTR_ID		XPAR_FABRIC_AXIDMA_0_MM2S_INTROUT_VEC_ID
	#endif
#endif

//---------- MAIN CODE ---------------

#define DMA_DEV_ID				XPAR_AXIDMA_0_DEVICE_ID
#define MM2S_BUFFER_BASE		0x01000000
#define S2MM_BUFFER_BASE		0x05000000
#define S2MM_BUFFER_HIGH		0x3FFFFFFF
#define MAX_PKT_LEN				10//67108863
#define TEST_START_VALUE		0xC

#define NUMBER_OF_TRANSFERS	10

bool done = false;

void mm2s_done_callback(My_DMA* dma)
{
	xil_printf("mm2s_done callback, custom function \r\n");
}

void s2mm_done_callback(My_DMA* dma)
{
	xil_printf("s2mm_done callback, custom function \r\n");
	done = true;
}

int main()
{
	int status;
	init_platform();
	xil_printf("\r\n--- Entering main() --- \r\n");

	My_DMA dma("dma", DMA_DEV_ID);
	status = dma.intr_init_mm2s(MM2S_INTR_ID);
	status = dma.intr_init_s2mm(S2MM_INTR_ID);
	if (status != XST_SUCCESS) xil_printf("Init failed with code: %d\r\n", status);

	// Attach custom callbacks
	dma.mm2s_done_callback = mm2s_done_callback;
	dma.s2mm_done_callback = s2mm_done_callback;

//	status = dma.intr_test(MM2S_BUFFER_BASE, S2MM_BUFFER_BASE, MAX_PKT_LEN, NUMBER_OF_TRANSFERS);

	for (int i=0; i<MAX_PKT_LEN; i++)
		(s8*)MM2S_BUFFER_BASE[i] = 100+i;

	dma.s2mm_start(MM2S_BUFFER_BASE, 16);
	dma.mm2s_start(MM2S_BUFFER_BASE, 16);

	while (!done){};

	bool all_correct=true;
	for (int i=0; i<MAX_PKT_LEN; i++)
		if ((s8*)S2MM_BUFFER_BASE[i] != (s8*)MM2S_BUFFER_BASE[i])
			xil_printf("mm2s[%d]=%d doesnt match s2mm[%d]=%d \r\n", i, (s8*)S2MM_BUFFER_BASE[i], i, (s8*)MM2S_BUFFER_BASE[i]);

	if (status != XST_SUCCESS)
	{
		xil_printf("Failed\r\n");
		return XST_FAILURE;
	}

	xil_printf("Successfully ran  Example\r\n");
	xil_printf("--- Exiting main() --- \r\n");
	return XST_SUCCESS;

}
