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


#if defined XPAR_SCUGIC_SINGLE_DEVICE_ID || XPAR_INTC_0_DEVICE_ID
	#ifdef XPAR_INTC_0_DEVICE_ID
		#include "xintc.h"
		#define INTC_DEVICE_ID  XPAR_INTC_0_DEVICE_ID
		#define RX_INTR_ID		XPAR_INTC_0_AXIDMA_0_S2MM_INTROUT_VEC_ID
		#define TX_INTR_ID		XPAR_INTC_0_AXIDMA_0_MM2S_INTROUT_VEC_ID
		#define INTC		    XIntc
		#define INTC_HANDLER	XIntc_InterruptHandler
	#endif
	#ifdef XPAR_SCUGIC_SINGLE_DEVICE_ID
		#include "xscugic.h"
		#define INTC_DEVICE_ID  XPAR_SCUGIC_SINGLE_DEVICE_ID
		#define RX_INTR_ID		XPAR_FABRIC_AXIDMA_0_S2MM_INTROUT_VEC_ID
		#define TX_INTR_ID		XPAR_FABRIC_AXIDMA_0_MM2S_INTROUT_VEC_ID
		#define INTC		    XScuGic
		#define INTC_HANDLER	XScuGic_InterruptHandler
	#endif

	#define RESET_TIMEOUT_COUNTER	10000
	static INTC Intc;
	static void TxIntrHandler(void *Callback);
	static void RxIntrHandler(void *Callback);
#endif


class My_DMA
{
private:

public:
	XAxiDma dma;
	uint16_t device_id;
	XAxiDma_Config *CfgPtr;
	int Status;

	volatile int tx_done, rx_done, error;

	// These function pointers can be assigned from outside
	void (*tx_done_callback)(My_DMA*);
	void (*rx_done_callback)(My_DMA*);

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
		tx_done = 0;
		rx_done = 0;
		error = 0;
		return XST_SUCCESS;
	}

	int intr_init(int TxIntrId, int RxIntrId)
	{
		#if defined XPAR_SCUGIC_SINGLE_DEVICE_ID || XPAR_INTC_0_DEVICE_ID
			/* Initialize DMA engine */
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

			/* Set up Interrupt system  */

			#ifdef XPAR_INTC_0_DEVICE_ID

				/* Initialize the interrupt controller and connect the ISRs */
				Status = XIntc_Initialize(&Intc, INTC_DEVICE_ID);
				if (Status != XST_SUCCESS) {
					xil_printf("Failed init intc\r\n");
					return XST_FAILURE;
				}

				Status = XIntc_Connect(&Intc, TxIntrId,	(XInterruptHandler) TxIntrHandler, this);
				if (Status != XST_SUCCESS) {
					xil_printf("Failed tx connect intc\r\n");
					return XST_FAILURE;
				}

				Status = XIntc_Connect(&Intc, RxIntrId, (XInterruptHandler) RxIntrHandler, this);
				if (Status != XST_SUCCESS)
				{
					xil_printf("Failed rx connect intc\r\n");
					return XST_FAILURE;
				}

				/* Start the interrupt controller */
				Status = XIntc_Start(&Intc, XIN_REAL_MODE);
				if (Status != XST_SUCCESS)
				{
					xil_printf("Failed to start intc\r\n");
					return XST_FAILURE;
				}

				XIntc_Enable(&Intc, TxIntrId);
				XIntc_Enable(&Intc, RxIntrId);

			#else

				XScuGic_Config *IntcConfig;

				/*
				 * Initialize the interrupt controller driver so that it is ready to
				 * use.
				 */
				IntcConfig = XScuGic_LookupConfig(INTC_DEVICE_ID);
				if (NULL == IntcConfig)
					return XST_FAILURE;

				Status = XScuGic_CfgInitialize(&Intc, IntcConfig,
								IntcConfig->CpuBaseAddress);
				if (Status != XST_SUCCESS)
					return XST_FAILURE;


				XScuGic_SetPriorityTriggerType(&Intc, TxIntrId, 0xA0, 0x3);

				XScuGic_SetPriorityTriggerType(&Intc, RxIntrId, 0xA0, 0x3);
				/*
				 * Connect the device driver handler that will be called when an
				 * interrupt for the device occurs, the handler defined above performs
				 * the specific interrupt processing for the device.
				 */
				Status = XScuGic_Connect(&Intc, TxIntrId, (Xil_InterruptHandler)TxIntrHandler, this);
				if (Status != XST_SUCCESS)
					return Status;

				Status = XScuGic_Connect(&Intc, RxIntrId, (Xil_InterruptHandler)RxIntrHandler, this);
				if (Status != XST_SUCCESS)
					return Status;

				XScuGic_Enable(&Intc, TxIntrId);
				XScuGic_Enable(&Intc, RxIntrId);

			#endif

				/* Enable interrupts from the hardware */

				Xil_ExceptionInit();
				Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_INT, (Xil_ExceptionHandler)INTC_HANDLER, (void *)&Intc);
				Xil_ExceptionEnable();

			if (Status != XST_SUCCESS)
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
			tx_done = 0;
			rx_done = 0;
			error = 0;

			return XST_SUCCESS;
		#else
			xil_printf("Interrupts are not available. Cannot initiate in interrupt mode.  \r\n");
			return XST_FAILURE;
		#endif
	}

	int from_mem(auto data_out_ptr, long length)
	{
		Xil_DCacheFlushRange((uintptr_t)data_out_ptr, length);
		rx_done = 0;
		Status = XAxiDma_SimpleTransfer(&dma, (uintptr_t) data_out_ptr, length, XAXIDMA_DEVICE_TO_DMA);
		rx_done = (Status == XST_SUCCESS);
		return Status;
	}

	int to_mem(auto data_in_ptr, long length)
	{
		Xil_DCacheFlushRange((uintptr_t)data_in_ptr, length);
		tx_done = 0;
		Status = XAxiDma_SimpleTransfer(&dma, (uintptr_t) data_in_ptr, length, XAXIDMA_DMA_TO_DEVICE);
		tx_done = (Status == XST_SUCCESS);
		return Status;
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

	int intr_test(auto data_in_ptr, auto data_out_ptr, long length, int num_transfers)
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

			// Wait for All done or Error
			while (!tx_done && !rx_done && !error) {}

			if (error)
			{
				xil_printf("Failed test transmit%s done, receive%s done\r\n", tx_done? "":" not", rx_done? "":" not");
				return XST_FAILURE;
			}

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

#if defined XPAR_SCUGIC_SINGLE_DEVICE_ID || XPAR_INTC_0_DEVICE_ID
	static void TxIntrHandler(void *Callback)
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
		 * If Completion interrupt is asserted, then set the TxDone flag
		 */
		if ((IrqStatus & XAXIDMA_IRQ_IOC_MASK)){
			my_dma_ptr->tx_done_callback(my_dma_ptr);
			my_dma_ptr->tx_done = 1;
		}
	}

	static void RxIntrHandler(void *Callback)
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
		 * If completion interrupt is asserted, then set RxDone flag
		 */
		if (IrqStatus & XAXIDMA_IRQ_IOC_MASK){
			my_dma_ptr->rx_done_callback(my_dma_ptr);
			my_dma_ptr->rx_done = 1;
		}
	}
#endif


#endif /* SRC_MY_DMA_H_ */
