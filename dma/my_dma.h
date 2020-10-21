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
#include "my_test_data.h"


#if defined XPAR_SCUGIC_SINGLE_DEVICE_ID || XPAR_INTC_0_DEVICE_ID
	#ifdef XPAR_INTC_0_DEVICE_ID
		#include "xintc.h"
		#define INTC_DEVICE_ID  	XPAR_INTC_0_DEVICE_ID
		#define TO_DDR_INTR_ID		XPAR_INTC_0_AXIDMA_0_S2MM_INTROUT_VEC_ID
		#define FROM_DDR_INTR_ID	XPAR_INTC_0_AXIDMA_0_MM2S_INTROUT_VEC_ID
		#define INTC		    	XIntc
		#define INTC_HANDLER		XIntc_InterruptHandler
	#endif
	#ifdef XPAR_SCUGIC_SINGLE_DEVICE_ID
		#include "xscugic.h"
		#define INTC_DEVICE_ID  	XPAR_SCUGIC_SINGLE_DEVICE_ID
		#define TO_DDR_INTR_ID		XPAR_FABRIC_AXIDMA_0_S2MM_INTROUT_VEC_ID
		#define FROM_DDR_INTR_ID	XPAR_FABRIC_AXIDMA_0_MM2S_INTROUT_VEC_ID
		#define INTC		    	XScuGic
		#define INTC_HANDLER		XScuGic_InterruptHandler
	#endif

	#define RESET_TIMEOUT_COUNTER	10000
	static INTC Intc;
	static void FromDDRIntrHandler(void *Callback);
	static void ToDDRIntrHandler(void *Callback);
#endif


class My_DMA
{
public:
	XAxiDma dma;
	uint16_t device_id;
	XAxiDma_Config *CfgPtr;
	int status;

	volatile int from_ddr_done, to_ddr_done, error;

	// These function pointers are to be assigned from outside
	void (*from_ddr_done_callback)(My_DMA*);
	void (*to_ddr_done_callback)(My_DMA*);

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

		status = XAxiDma_CfgInitialize(&dma, CfgPtr);
		if (status != XST_SUCCESS)
		{
			xil_printf("Initialization failed %d\r\n", status);
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
		from_ddr_done = 0;
		to_ddr_done = 0;
		error = 0;
		return XST_SUCCESS;
	}

	int to_ddr_start(auto to_ddr_ptr, long length)
	{
		Xil_DCacheFlushRange((uintptr_t)to_ddr_ptr, length);
		to_ddr_done = 0;
		status = XAxiDma_SimpleTransfer(&dma, (uintptr_t) to_ddr_ptr, length, XAXIDMA_DEVICE_TO_DMA);
		return status;
	}

	int from_ddr_start(auto from_ddr_ptr, long length)
	{
		Xil_DCacheFlushRange((uintptr_t)from_ddr_ptr, length);
		from_ddr_done = 0;
		status = XAxiDma_SimpleTransfer(&dma, (uintptr_t) from_ddr_ptr, length, XAXIDMA_DMA_TO_DEVICE);
		return status;
	}

	int is_busy(bool to_ddr, bool from_ddr)
	{
		bool to_ddr_result, from_ddr_result;
		if (to_ddr)
		{
			to_ddr_result = XAxiDma_Busy(&dma,XAXIDMA_DEVICE_TO_DMA);
			if (!to_ddr_result) to_ddr_done = 1;
		}
		if (from_ddr)
		{
			from_ddr_result = XAxiDma_Busy(&dma,XAXIDMA_DMA_TO_DEVICE);
			if (!from_ddr_result) from_ddr_done = 1;
		}
		return (to_ddr && to_ddr_result) || (from_ddr && from_ddr_result);
	}

	void poll_wait(bool to_ddr, bool from_ddr)
	{
		while(is_busy(to_ddr, from_ddr)) {}
	}

	int poll_test(auto from_ddr_ptr, auto to_ddr_ptr, long length, int num_transfers)
	{
		/* DMA successfully pulls 2^26-1 bytes = 64 MB
		 * 26 = maximum register size
		 *
		 	#define FROM_DDR_BUFFER_BASE	0x01000000  // = XPAR_PS7_DDR_0_S_AXI_BASEADDR
			#define TO_DDR_BUFFER_BASE		0x05000000  // = FROM_DDR_BUFFER_BASE + (2^26)
			#define TO_DDR_BUFFER_HIGH		0x3FFFFFFF  // = XPAR_PS7_DDR_0_S_AXI_HIGHADDR

			#define MAX_PKT_LEN		    67108863    // = 2^26-1
			#define TEST_START_VALUE	0xC
			#define NUMBER_OF_TRANSFERS	10
		 *
		 **/
		Test_Data test_data ((u8 *)from_ddr_ptr, (u8 *)to_ddr_ptr, 0, length);

		XTime start_loop, end_loop, start_transfer, end_transfer;
		XTime sum_transfer_clocks = 0;
		XTime loop_transfer_time = 0;

		XTime_GetTime(&start_loop);
		for(int i = 0; i < num_transfers; i ++)
		{

			status = this->to_ddr_start((UINTPTR) to_ddr_ptr, length);
			if (status != XST_SUCCESS)
			{
				xil_printf("Failed to initiate to_ddr with code: %d\r\n", status);
				return XST_FAILURE;
			}

			status = this->from_ddr_start((UINTPTR) from_ddr_ptr, length);
			if (status != XST_SUCCESS)
			{
				xil_printf("Failed to initiate from_ddr with code: %d\r\n", status);
				return XST_FAILURE;
			}

			xil_printf("%d : Started Transfer\r\n", i);
			XTime_GetTime(&start_transfer);

			this->poll_wait(true,true);

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

	#if defined XPAR_SCUGIC_SINGLE_DEVICE_ID || XPAR_INTC_0_DEVICE_ID
		int intr_init(int FromDDRIntrId, int ToDDRIntrId)
		{
			/* Initialize DMA engine */
			CfgPtr = XAxiDma_LookupConfig(device_id);
			if (!CfgPtr)
			{
				xil_printf("No config found for %d\r\n", device_id);
				return XST_FAILURE;
			}
			status = XAxiDma_CfgInitialize(&dma, CfgPtr);

			if (status != XST_SUCCESS)
			{
				xil_printf("Initialization failed %d\r\n", status);
				return XST_FAILURE;
			}

			if(XAxiDma_HasSg(&dma))
			{
				xil_printf("Device configured as SG mode \r\n");
				return XST_FAILURE;
			}

			/* Set up Interrupt system  */

			#ifdef XPAR_INTC_0_DEVICE_ID

				/* Initialize the interrupt controller and connect the ISRs */
				status = XIntc_Initialize(&Intc, INTC_DEVICE_ID);
				if (status != XST_SUCCESS) {
					xil_printf("Failed init intc\r\n");
					return XST_FAILURE;
				}

				status = XIntc_Connect(&Intc, FromDDRIntrId,	(XInterruptHandler) FromDDRIntrHandler, this);
				if (status != XST_SUCCESS) {
					xil_printf("Failed from_ddr connect intc\r\n");
					return XST_FAILURE;
				}

				status = XIntc_Connect(&Intc, ToDDRIntrId, (XInterruptHandler) ToDDRIntrHandler, this);
				if (status != XST_SUCCESS)
				{
					xil_printf("Failed to_ddr connect intc\r\n");
					return XST_FAILURE;
				}

				/* Start the interrupt controller */
				status = XIntc_Start(&Intc, XIN_REAL_MODE);
				if (status != XST_SUCCESS)
				{
					xil_printf("Failed to start intc\r\n");
					return XST_FAILURE;
				}

				XIntc_Enable(&Intc, FromDDRIntrId);
				XIntc_Enable(&Intc, ToDDRIntrId);

			#else

				XScuGic_Config *IntcConfig;

				/*
				 * Initialize the interrupt controller driver so that it is ready to
				 * use.
				 */
				IntcConfig = XScuGic_LookupConfig(INTC_DEVICE_ID);
				if (NULL == IntcConfig)
					return XST_FAILURE;

				status = XScuGic_CfgInitialize(&Intc, IntcConfig,
								IntcConfig->CpuBaseAddress);
				if (status != XST_SUCCESS)
					return XST_FAILURE;


				XScuGic_SetPriorityTriggerType(&Intc, FromDDRIntrId, 0xA0, 0x3);

				XScuGic_SetPriorityTriggerType(&Intc, ToDDRIntrId, 0xA0, 0x3);
				/*
				 * Connect the device driver handler that will be called when an
				 * interrupt for the device occurs, the handler defined above performs
				 * the specific interrupt processing for the device.
				 */
				status = XScuGic_Connect(&Intc, FromDDRIntrId, (Xil_InterruptHandler)FromDDRIntrHandler, this);
				if (status != XST_SUCCESS)
					return status;

				status = XScuGic_Connect(&Intc, ToDDRIntrId, (Xil_InterruptHandler)ToDDRIntrHandler, this);
				if (status != XST_SUCCESS)
					return status;

				XScuGic_Enable(&Intc, FromDDRIntrId);
				XScuGic_Enable(&Intc, ToDDRIntrId);

			#endif

				/* Enable interrupts from the hardware */

				Xil_ExceptionInit();
				Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_INT, (Xil_ExceptionHandler)INTC_HANDLER, (void *)&Intc);
				Xil_ExceptionEnable();

			if (status != XST_SUCCESS)
			{
				xil_printf("Failed intr setup\r\n");
				return XST_FAILURE;
			}

			/* Disable all interrupts before setup */
			XAxiDma_IntrDisable(&dma, XAXIDMA_IRQ_ALL_MASK, XAXIDMA_DMA_TO_DEVICE);
			XAxiDma_IntrDisable(&dma, XAXIDMA_IRQ_ALL_MASK, XAXIDMA_DEVICE_TO_DMA);

			/* Enable all interrupts */
			XAxiDma_IntrEnable(&dma, XAXIDMA_IRQ_ALL_MASK, XAXIDMA_DMA_TO_DEVICE);
			XAxiDma_IntrEnable(&dma, XAXIDMA_IRQ_ALL_MASK, XAXIDMA_DEVICE_TO_DMA);

			/* Initialize flags before start transfer  */
			from_ddr_done = 0;
			to_ddr_done = 0;
			error = 0;

			return XST_SUCCESS;
		}

		int intr_test(auto from_ddr_ptr, auto to_ddr_ptr, long length, int num_transfers)
		{
			/* DMA successfully pulls 2^26-1 bytes = 64 MB
			 * 26 = maximum register size
			 *
				#define FROM_DDR_BUFFER_BASE	0x01000000  // = XPAR_PS7_DDR_0_S_AXI_BASEADDR
				#define TO_DDR_BUFFER_BASE		0x05000000  // = FROM_DDR_BUFFER_BASE + (2^26)
				#define TO_DDR_BUFFER_HIGH		0x3FFFFFFF  // = XPAR_PS7_DDR_0_S_AXI_HIGHADDR

				#define MAX_PKT_LEN		    67108863    // = 2^26-1
				#define TEST_START_VALUE	0xC
				#define NUMBER_OF_TRANSFERS	10
			 *
			 **/
			Test_Data test_data ((u8 *)from_ddr_ptr, (u8 *)to_ddr_ptr, 0, length);

			XTime start_loop, end_loop, start_transfer, end_transfer;
			XTime sum_transfer_clocks = 0;
			XTime loop_transfer_time = 0;

			XTime_GetTime(&start_loop);
			for(int i = 0; i < num_transfers; i ++)
			{

				status = this->to_ddr_start((UINTPTR) to_ddr_ptr, length);
				if (status != XST_SUCCESS)
				{
					xil_printf("Failed to initiate to_ddr with code: %d\r\n", status);
					return XST_FAILURE;
				}

				status = this->from_ddr_start((UINTPTR) from_ddr_ptr, length);
				if (status != XST_SUCCESS)
				{
					xil_printf("Failed to initiate from_ddr with code: %d\r\n", status);
					return XST_FAILURE;
				}

				xil_printf("%d : Started Transfer\r\n", i);
				XTime_GetTime(&start_transfer);

				// Wait for All done or Error
				while (!from_ddr_done && !to_ddr_done && !error) {}

				if (error)
				{
					xil_printf("Failed test transmit%s done, receive%s done\r\n", from_ddr_done? "":" not", to_ddr_done? "":" not");
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
	static void FromDDRIntrHandler(void *Callback)
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

			my_dma_ptr->error = 1;

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
		 * If Completion interrupt is asserted, then set the FromDDRDone flag
		 */
		if ((IrqStatus & XAXIDMA_IRQ_IOC_MASK)){
			my_dma_ptr->from_ddr_done = 1;
			my_dma_ptr->from_ddr_done_callback(my_dma_ptr);
		}
	}

	static void ToDDRIntrHandler(void *Callback)
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

			my_dma_ptr->error = 1;

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
		 * If completion interrupt is asserted, then set ToDDRDone flag
		 */
		if (IrqStatus & XAXIDMA_IRQ_IOC_MASK){
			my_dma_ptr->to_ddr_done = 1;
			my_dma_ptr->to_ddr_done_callback(my_dma_ptr);
		}
	}
#endif


#endif /* SRC_MY_DMA_H_ */
