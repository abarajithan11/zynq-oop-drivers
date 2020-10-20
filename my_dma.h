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
		Test_Data test_data ((u8 *)data_in_ptr, (u8 *)data_out_ptr, 0, length);

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

			this->poll_wait(true,true);

			Status = test_data.check();
			if (Status != XST_SUCCESS)
			{
				xil_printf("Check data failed\r\n");
				return XST_FAILURE;
			}

		}
		return XST_SUCCESS;
	}

};



#endif /* SRC_MY_DMA_H_ */
