/*
 * my_dma.h
 *
 *  Created on: Oct 20, 2020
 *      Author: abara
 */
#include "xparameters.h"
#ifndef SRC_MY_DMA_H_
#define SRC_MY_DMA_H_

#include "xparameters.h"
#include "xil_cache.h"
#include "xil_printf.h"
#include "xstatus.h"
#include <machine/_default_types.h>
#include "xtime_l.h"

#include "xaxidma.h"
#include "test_data.h"

class My_DMA
{
private:

public:
	XAxiDma dma;
	uint16_t device_id;
	XAxiDma_Config *CfgPtr;
	int Status;

	My_DMA(uint16_t device_id):device_id(device_id){}

	int poll_init()
	{
		/* Initialize DMA in polling mode
		 */
		CfgPtr = XAxiDma_LookupConfig(device_id);
		if (!CfgPtr)
		{
			xil_printf("No config found for %d\r\n", device_id);
			return XST_FAILURE;
		}

		Status = XAxiDma_CfgInitialize(&dma, CfgPtr);
		if (Status != XST_SUCCESS)
		{
			xil_printf("Initialization failed %d\r\n", Status);
			return XST_FAILURE;
		}

		if(XAxiDma_HasSg(&dma))
		{
			xil_printf("Device configured as SG mode \r\n");
			return XST_FAILURE;
		}

		/* Disable interrupts, we use polling mode
		 */
		XAxiDma_IntrDisable(&dma, XAXIDMA_IRQ_ALL_MASK,
							XAXIDMA_DEVICE_TO_DMA);
		XAxiDma_IntrDisable(&dma, XAXIDMA_IRQ_ALL_MASK,
							XAXIDMA_DMA_TO_DEVICE);
		return XST_SUCCESS;
	}

	int from_mem(auto data_out_ptr, long length)
	{
		Xil_DCacheFlushRange((uintptr_t)data_out_ptr, length);
		return XAxiDma_SimpleTransfer(&dma, (uintptr_t) data_out_ptr, length, XAXIDMA_DEVICE_TO_DMA);
	}

	int to_mem(auto data_in_ptr, long length)
	{
		Xil_DCacheFlushRange((uintptr_t)data_in_ptr, length);
		return XAxiDma_SimpleTransfer(&dma, (uintptr_t) data_in_ptr, length, XAXIDMA_DMA_TO_DEVICE);
	}

	int is_busy(bool from_mem, bool to_mem)
	{
		return (from_mem && XAxiDma_Busy(&dma,XAXIDMA_DEVICE_TO_DMA)) || (to_mem && XAxiDma_Busy(&dma,XAXIDMA_DMA_TO_DEVICE));
	}

	void poll_wait(bool from_mem, bool to_mem)
	{
		while(is_busy(from_mem, to_mem)) {}
	}

	int poll_test(auto data_in_ptr, auto data_out_ptr, long length, int num_transfers)
	{
		/* DMA successfully pulls 2^26-1 bytes = 64 MB
		 * 26 = maximum register size
		 *
		 	#define TX_BUFFER_BASE		0x01000000  // = XPAR_PS7_DDR_0_S_AXI_BASEADDR
			#define RX_BUFFER_BASE		0x05000000  // = TX_BUFFER_BASE + (2^26)
			#define RX_BUFFER_HIGH		0x3FFFFFFF  // = XPAR_PS7_DDR_0_S_AXI_HIGHADDR

			#define MAX_PKT_LEN		    67108863    // = 2^26-1
			#define TEST_START_VALUE	0xC
			#define NUMBER_OF_TRANSFERS	10
		 *
		 **/
		Test_Data test_data ((u8 *)data_in_ptr, (u8 *)data_out_ptr, 0, length);

		XTime start_loop, end_loop, start_transfer, end_transfer;
		XTime sum_transfer_clocks = 0;
		XTime loop_transfer_time = 0;

		XTime_GetTime(&start_loop);
		for(int i = 0; i < num_transfers; i ++)
		{

			Status = this->from_mem((UINTPTR) data_out_ptr, length);
			if (Status != XST_SUCCESS)
			{
				xil_printf("Failed to initiate from_mem with code: %d\r\n", Status);
				return XST_FAILURE;
			}

			Status = this->to_mem((UINTPTR) data_in_ptr, length);
			if (Status != XST_SUCCESS)
			{
				xil_printf("Failed to initiate to_mem with code: %d\r\n", Status);
				return XST_FAILURE;
			}

			xil_printf("%d : Started Transfer\r\n", i);
			XTime_GetTime(&start_transfer);

			this->poll_wait(true,true);

			XTime_GetTime(&end_transfer);
			sum_transfer_clocks += (end_transfer-start_transfer);
			xil_printf("%d : Finished Transfer\r\n", i);

			Status = test_data.check();
			if (Status != XST_SUCCESS)
			{
				xil_printf("Check data failed\r\n");
				return XST_FAILURE;
			}

			xil_printf("%d : Validated Transfer\r\n\n", i);

		}
		XTime_GetTime(&end_loop);

		loop_transfer_time = (end_loop-start_loop)/COUNTS_PER_SECOND;
		u64 bandwidth = length * num_transfers * COUNTS_PER_SECOND /(end_loop-start_loop);

		xil_printf("Time for %d setups and transfers: %d seconds\r\n",num_transfers, loop_transfer_time);
		xil_printf("Avg time for each setup and transfer: %d seconds\r\n", loop_transfer_time/num_transfers);
		xil_printf("Avg bandwidth (incl setup): %d bytes/sec\r\n\n", bandwidth);

		long sum_transfer_time = sum_transfer_clocks/COUNTS_PER_SECOND;
		bandwidth = length * num_transfers * COUNTS_PER_SECOND /sum_transfer_clocks;

		xil_printf("Time for %d transfers: %d seconds\r\n",num_transfers, sum_transfer_time);
		xil_printf("Avg time for each transfer: %d seconds\r\n", sum_transfer_time/num_transfers);
		xil_printf("Avg bandwidth (transfer only): %d bytes/sec\r\n\n", bandwidth);


		return XST_SUCCESS;
	}

};



#endif /* SRC_MY_DMA_H_ */
