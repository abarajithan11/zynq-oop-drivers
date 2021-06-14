/*
 * my_dma.h
 *
 *  Created on: Oct 20, 2020
 *      Author: abara
 */
#pragma once

#include "xparameters.h"

#include "xparameters.h"
#include "xil_cache.h"
#include "xil_printf.h"
#include "xstatus.h"
#include <machine/_default_types.h>
#include "xtime_l.h"

#include "xaxidma.h"
#include "my_test_data.h"


#if defined XPAR_SCUGIC_SINGLE_DEVICE_ID || XPAR_INTC_0_DEVICE_ID
	#ifdef XPAR_INTC_0_DEVICE_ID
		#include "xintc.h"
		#define INTC_DEVICE_ID  	XPAR_INTC_0_DEVICE_ID
//		#define S2MM_INTR_ID		XPAR_INTC_0_AXIDMA_0_S2MM_INTROUT_VEC_ID
//		#define MM2S_INTR_ID		XPAR_INTC_0_AXIDMA_0_MM2S_INTROUT_VEC_ID
		#define INTC		    	XIntc
		#define INTC_HANDLER		XIntc_InterruptHandler
	#endif
	#ifdef XPAR_SCUGIC_SINGLE_DEVICE_ID
		#include "xscugic.h"
		#define INTC_DEVICE_ID  	XPAR_SCUGIC_SINGLE_DEVICE_ID
//		#define S2MM_INTR_ID		XPAR_FABRIC_AXIDMA_0_S2MM_INTROUT_VEC_ID
//		#define MM2S_INTR_ID		XPAR_FABRIC_AXIDMA_0_MM2S_INTROUT_VEC_ID
		#define INTC		    	XScuGic
		#define INTC_HANDLER		XScuGic_InterruptHandler
	#endif

	#define RESET_TIMEOUT_COUNTER	10000
	static INTC Intc;
	static void MM2SIntrHandler(void *Callback);
	static void S2MMIntrHandler(void *Callback);
#endif


void do_nothing(){}

class My_DMA
{
public:
	XAxiDma dma;
	const char * name;
	uint16_t device_id;
	XAxiDma_Config *CfgPtr;
	int status;
	UINTPTR s2mm_buffer_base;
	UINTPTR s2mm_buffer_length;

	volatile bool mm2s_done=false, s2mm_done=false, error=false;

	// These function pointers are to be assigned from outside
	void (*mm2s_done_callback)() = do_nothing;
	void (*s2mm_done_callback)() = do_nothing;

	My_DMA(const char * name, uint16_t device_id): name(name), device_id(device_id)
	{
		/* Initialize DMA engine */
		CfgPtr = XAxiDma_LookupConfig(device_id);
		if (!CfgPtr)
			xil_printf("dma(%s): constructor init failed. No config found for %d\r\n",name , device_id);

		status = XAxiDma_CfgInitialize(&dma, CfgPtr);
		if (status != XST_SUCCESS)
			xil_printf("dma(%s): constructor init failed. XAxiDma_CfgInitialize failed with code: %d\r\n",name , status);

		if(XAxiDma_HasSg(&dma))
			xil_printf("dma(%s): constructor init failed. Device configured as SG mode \r\n",name );

		/* INITIALIZE INTERRUPTS */
#ifdef XPAR_INTC_0_DEVICE_ID
		status = XIntc_Initialize(&Intc, INTC_DEVICE_ID);
		if (status != XST_SUCCESS)
			xil_printf("dma(%s): constructor intr init failed. XIntc_Initialize\r\n",name );
#else
		XScuGic_Config *IntcConfig;

		IntcConfig = XScuGic_LookupConfig(INTC_DEVICE_ID);
		if (NULL == IntcConfig)
			xil_printf("dma(%s): constructor intr init failed. XScuGic_LookupConfig\r\n",name );

		status = XScuGic_CfgInitialize(&Intc, IntcConfig, IntcConfig->CpuBaseAddress);
		if (status != XST_SUCCESS)
			xil_printf("dma(%s): constructor intr init failed. XScuGic_LookupConfig\r\n",name );
#endif
	}

	int poll_init()
	{
		/* Initialize DMA in polling mode
		 */
		XAxiDma_IntrDisable(&dma, XAXIDMA_IRQ_ALL_MASK, XAXIDMA_DEVICE_TO_DMA);
		XAxiDma_IntrDisable(&dma, XAXIDMA_IRQ_ALL_MASK,	XAXIDMA_DMA_TO_DEVICE);
		mm2s_done = false;
		s2mm_done = false;
		error = false;
		return XST_SUCCESS;
	}

	int s2mm_start(UINTPTR s2mm_ptr, long length)
	{

		s2mm_done = false;
		status = XAxiDma_SimpleTransfer(&dma, (uintptr_t) s2mm_ptr, length, XAXIDMA_DEVICE_TO_DMA);

		// Store these to be used by the done_intr to to clear cache
		s2mm_buffer_base = s2mm_ptr;
		s2mm_buffer_length = length;

		if (status != XST_SUCCESS)
		{
			xil_printf("dma(%s): Failed to initiate s2mm with code: %d\r\n",name , status);
			return XST_FAILURE;
		}
		else
		{
			return XST_SUCCESS;
		}
	}

	int mm2s_start(UINTPTR mm2s_ptr, long length)
	{
		mm2s_done = false;
		status = XAxiDma_SimpleTransfer(&dma, (uintptr_t) mm2s_ptr, length, XAXIDMA_DMA_TO_DEVICE);

		if (status != XST_SUCCESS)
		{
			xil_printf("dma(%s): Failed to initiate mm2s with code: %d\r\n", name, status);
			return XST_FAILURE;
		}
		else
		{
			return XST_SUCCESS;
		}
	}

	int is_busy(bool s2mm, bool mm2s)
	{
		bool s2mm_result, mm2s_result;
		if (s2mm)
		{
			s2mm_result = XAxiDma_Busy(&dma,XAXIDMA_DEVICE_TO_DMA);
			if (!s2mm_result) s2mm_done = true;
		}
		if (mm2s)
		{
			mm2s_result = XAxiDma_Busy(&dma,XAXIDMA_DMA_TO_DEVICE);
			if (!mm2s_result) mm2s_done = true;
		}
		return (s2mm && s2mm_result) || (mm2s && mm2s_result);
	}

	void poll_wait(bool s2mm, bool mm2s)
	{
		while(is_busy(s2mm, mm2s)) {}
	}

	int poll_test(UINTPTR mm2s_ptr, UINTPTR s2mm_ptr, long length, int num_transfers)
	{
		/* DMA successfully pulls 2^26-1 bytes = 64 MB
		 * 26 = maximum register size
		 *
		 	#define MM2S_BUFFER_BASE	0x01000000  // = XPAR_PS7_DDR_0_S_AXI_BASEADDR
			#define S2MM_BUFFER_BASE	0x05000000  // = MM2S_BUFFER_BASE + (2^26)
			#define S2MM_BUFFER_HIGH	0x3FFFFFFF  // = XPAR_PS7_DDR_0_S_AXI_HIGHADDR

			#define MAX_PKT_LEN		    67108863    // = 2^26-1
			#define TEST_START_VALUE	0xC
			#define NUMBER_OF_TRANSFERS	10
		 *
		 **/
		Test_Data test_data ((UINTPTR)mm2s_ptr, (UINTPTR)s2mm_ptr, 0, length);

		XTime start_loop, end_loop, start_transfer, end_transfer;
		XTime sum_transfer_clocks = 0;
		XTime loop_transfer_time = 0;

		XTime_GetTime(&start_loop);
		for(int i = 0; i < num_transfers; i ++)
		{

			status = this->s2mm_start((UINTPTR) s2mm_ptr, length);
			status = this->mm2s_start((UINTPTR) mm2s_ptr, length);

			xil_printf("%d : Started Transfer\r\n", i);
			XTime_GetTime(&start_transfer);

			this->poll_wait(true,true);

			XTime_GetTime(&end_transfer);
			sum_transfer_clocks += (end_transfer-start_transfer);
			xil_printf("%d : Finished Transfer\r\n", i);

			status = test_data.check();

			if (status != XST_SUCCESS)
				return XST_FAILURE;
			else
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

	#if defined XPAR_SCUGIC_SINGLE_DEVICE_ID || XPAR_INTC_0_DEVICE_ID

		int intr_init_mm2s(int MM2SIntrId)
		{

			/* Set up Interrupt system  */

			#ifdef XPAR_INTC_0_DEVICE_ID

				status = XIntc_Connect(&Intc, MM2SIntrId,	(XInterruptHandler) MM2SIntrHandler, this);
				if (status != XST_SUCCESS) {
					xil_printf("dma(%s): intr_init_mm2s failed. XIntc_Connect. Failed mm2s connect intc\r\n",name );
					return XST_FAILURE;
				}

				/* Start the interrupt controller */
				status = XIntc_Start(&Intc, XIN_REAL_MODE);
				if (status != XST_SUCCESS)
				{
					xil_printf("dma(%s): intr_init_mm2s failed. XIntc_Start. Failed to start intc\r\n",name );
					return XST_FAILURE;
				}

				XIntc_Enable(&Intc, MM2SIntrId);

			#else

				XScuGic_SetPriorityTriggerType(&Intc, MM2SIntrId, 0xA0, 0x3);

				/*
				 * Connect the device driver handler that will be called when an
				 * interrupt for the device occurs, the handler defined above performs
				 * the specific interrupt processing for the device.
				 */
				status = XScuGic_Connect(&Intc, MM2SIntrId, (Xil_InterruptHandler)MM2SIntrHandler, this);
				if (status != XST_SUCCESS)
					return status;

				XScuGic_Enable(&Intc, MM2SIntrId);

			#endif

			/* Enable interrupts from the hardware */

			Xil_ExceptionInit();
			Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_INT, (Xil_ExceptionHandler)INTC_HANDLER, (void *)&Intc);
			Xil_ExceptionEnable();

			if (status != XST_SUCCESS)
			{
				xil_printf("dma(%s): intr_init_mm2s failed. \r\n",name );
				return XST_FAILURE;
			}

			/* Disable and enable */
			XAxiDma_IntrDisable(&dma, XAXIDMA_IRQ_ALL_MASK, XAXIDMA_DMA_TO_DEVICE);
			XAxiDma_IntrEnable(&dma, XAXIDMA_IRQ_ALL_MASK, XAXIDMA_DMA_TO_DEVICE);

			/* Initialize flags before start transfer  */
			mm2s_done = false;
			error = false;

			return XST_SUCCESS;
		}

		int intr_init_s2mm(int S2MMIntrId)
		{

			/* Set up Interrupt system  */

			#ifdef XPAR_INTC_0_DEVICE_ID


				status = XIntc_Connect(&Intc, S2MMIntrId, (XInterruptHandler) S2MMIntrHandler, this);
				if (status != XST_SUCCESS)
				{
					xil_printf("dma(%s): intr_init_s2mm failed. XIntc_Connect. Failed s2mm connect intc\r\n",name );
					return XST_FAILURE;
				}

				/* Start the interrupt controller */
				status = XIntc_Start(&Intc, XIN_REAL_MODE);
				if (status != XST_SUCCESS)
				{
					xil_printf("dma(%s): intr_init_s2mm failed. XIntc_Start. Failed to start intc\r\n",name );
					return XST_FAILURE;
				}

				XIntc_Enable(&Intc, S2MMIntrId);

			#else

				XScuGic_SetPriorityTriggerType(&Intc, S2MMIntrId, 0xA0, 0x3);
				/*
				 * Connect the device driver handler that will be called when an
				 * interrupt for the device occurs, the handler defined above performs
				 * the specific interrupt processing for the device.
				 */

				status = XScuGic_Connect(&Intc, S2MMIntrId, (Xil_InterruptHandler)S2MMIntrHandler, this);
				if (status != XST_SUCCESS)
					return status;

				XScuGic_Enable(&Intc, S2MMIntrId);

			#endif

			/* Enable interrupts from the hardware */

			Xil_ExceptionInit();
			Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_INT, (Xil_ExceptionHandler)INTC_HANDLER, (void *)&Intc);
			Xil_ExceptionEnable();

			if (status != XST_SUCCESS)
			{
				xil_printf("dma(%s): intr_init_s2mm failed. \r\n",name );
				return XST_FAILURE;
			}

			/* Disable and enable */
			XAxiDma_IntrDisable(&dma, XAXIDMA_IRQ_ALL_MASK, XAXIDMA_DEVICE_TO_DMA);
			XAxiDma_IntrEnable(&dma, XAXIDMA_IRQ_ALL_MASK, XAXIDMA_DEVICE_TO_DMA);

			/* Initialize flags before start transfer  */
			s2mm_done = false;
			error = false;

			return XST_SUCCESS;
		}

		int intr_test(UINTPTR mm2s_ptr, UINTPTR s2mm_ptr, long length, int num_transfers)
		{
			/* DMA successfully pulls 2^26-1 bytes = 64 MB
			 * 26 = maximum register size
			 *
				#define MM2S_BUFFER_BASE	0x01000000  // = XPAR_PS7_DDR_0_S_AXI_BASEADDR
				#define S2MM_BUFFER_BASE	0x05000000  // = MM2S_BUFFER_BASE + (2^26)
				#define S2MM_BUFFER_HIGH	0x3FFFFFFF  // = XPAR_PS7_DDR_0_S_AXI_HIGHADDR

				#define MAX_PKT_LEN		    67108863    // = 2^26-1
				#define TEST_START_VALUE	0xC
				#define NUMBER_OF_TRANSFERS	10
			 *
			 **/
			Test_Data test_data ((UINTPTR)mm2s_ptr, (UINTPTR)s2mm_ptr, 0, length);

			XTime start_loop, end_loop, start_transfer, end_transfer;
			XTime sum_transfer_clocks = 0;
			XTime loop_transfer_time = 0;

			XTime_GetTime(&start_loop);
			for(int i = 0; i < num_transfers; i ++)
			{

				status = this->s2mm_start((UINTPTR) s2mm_ptr, length);
				status = this->mm2s_start((UINTPTR) mm2s_ptr, length);

				xil_printf("%d : Started Transfer\r\n", i);
				XTime_GetTime(&start_transfer);

				// Wait for All done or Error
				while (!mm2s_done && !s2mm_done && !error) {}

				if (error)
				{
					xil_printf("Failed test transmit%s done, receive%s done\r\n", mm2s_done? "":" not", s2mm_done? "":" not");
					return XST_FAILURE;
				}

				XTime_GetTime(&end_transfer);
				sum_transfer_clocks += (end_transfer-start_transfer);
				xil_printf("%d : Finished Transfer\r\n", i);

				status = test_data.check();
				if (status != XST_SUCCESS)
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
	#endif
};

/* Interrupt handlers
 *
 * They verify the interrupts, check for errors and call the custom-defined handler
 * */
#if defined XPAR_SCUGIC_SINGLE_DEVICE_ID || XPAR_INTC_0_DEVICE_ID
	static void MM2SIntrHandler(void *Callback)
	{
		u32 IrqStatus;
		int TimeOut;
		My_DMA *my_dma_ptr = (My_DMA *)Callback;
		XAxiDma *AxiDmaInst = &(my_dma_ptr->dma);

		/* Read pending interrupts */
		IrqStatus = XAxiDma_IntrGetIrq(AxiDmaInst, XAXIDMA_DMA_TO_DEVICE);

		/* Acknowledge pending interrupts */
		XAxiDma_IntrAckIrq(AxiDmaInst, IrqStatus, XAXIDMA_DMA_TO_DEVICE);

		/*
		 * If no interrupt is asserted, we do not do anything
		 */
		if (!(IrqStatus & XAXIDMA_IRQ_ALL_MASK))
			return;

		/*
		 * If error interrupt is asserted, raise error flag, reset the
		 * hardware to recover from the error, and return with no further
		 * processing.
		 */
		if ((IrqStatus & XAXIDMA_IRQ_ERROR_MASK)) {

			my_dma_ptr->error = true;

			/*
			 * Reset should never fail for transmit channel
			 */
			XAxiDma_Reset(AxiDmaInst);

			TimeOut = RESET_TIMEOUT_COUNTER;

			while (TimeOut)
			{
				if (XAxiDma_ResetIsDone(AxiDmaInst))
					break;
				TimeOut -= 1;
			}

			return;
		}

		/*
		 * If Completion interrupt is asserted, then set the MM2SDone flag
		 */
		if ((IrqStatus & XAXIDMA_IRQ_IOC_MASK)){
			my_dma_ptr->mm2s_done = true;
			my_dma_ptr->mm2s_done_callback();
		}
	}

	static void S2MMIntrHandler(void *Callback)
	{
		u32 IrqStatus;
		int TimeOut;
		My_DMA *my_dma_ptr = (My_DMA *)Callback;
		XAxiDma *AxiDmaInst = &(my_dma_ptr->dma);

		/* Read pending interrupts */
		IrqStatus = XAxiDma_IntrGetIrq(AxiDmaInst, XAXIDMA_DEVICE_TO_DMA);

		/* Acknowledge pending interrupts */
		XAxiDma_IntrAckIrq(AxiDmaInst, IrqStatus, XAXIDMA_DEVICE_TO_DMA);

		/*
		 * If no interrupt is asserted, we do not do anything
		 */
		if (!(IrqStatus & XAXIDMA_IRQ_ALL_MASK))
			return;

		/*
		 * If error interrupt is asserted, raise error flag, reset the
		 * hardware to recover from the error, and return with no further
		 * processing.
		 */
		if ((IrqStatus & XAXIDMA_IRQ_ERROR_MASK)) {

			my_dma_ptr->error = true;

			/* Reset could fail and hang
			 * NEED a way to handle this or do not call it??
			 */
			XAxiDma_Reset(AxiDmaInst);

			TimeOut = RESET_TIMEOUT_COUNTER;

			while (TimeOut)
			{
				if(XAxiDma_ResetIsDone(AxiDmaInst))
					break;
				TimeOut -= 1;
			}
			return;
		}

		/*
		 * If completion interrupt is asserted, then set S2MMDone flag
		 */
		if (IrqStatus & XAXIDMA_IRQ_IOC_MASK){
			my_dma_ptr->s2mm_done = true;
			my_dma_ptr->s2mm_done_callback();
		}
	}
#endif


