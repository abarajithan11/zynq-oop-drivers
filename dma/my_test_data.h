/*
 * test_data.h
 *
 *  Created on: Oct 20, 2020
 *      Author: abara
 */
#ifndef SRC_TEST_DATA_H_
#define SRC_TEST_DATA_H_

#include "xparameters.h"
#include "xil_cache.h"
#include "xil_printf.h"
#include "xstatus.h"
#include <machine/_default_types.h>

class Test_Data
{

private:
	char value;
public:
	__uint8_t *from_ddr_ptr;
	__uint8_t *to_ddr_ptr;
	char start_value;
	long length;

	// Constructor fills the test data
	Test_Data(	UINTPTR from_ddr_ptr,
				UINTPTR to_ddr_ptr,
				char start_value,
				long length
			 ):
				from_ddr_ptr((__uint8_t*)from_ddr_ptr),
				to_ddr_ptr((__uint8_t*)to_ddr_ptr),
				start_value(start_value),
				length(length)
	{
		value = start_value;

		for(long i = 0; i < length; i ++)
		{
				((u8*)from_ddr_ptr)[i] = value;
				value += 1;
		}
	}

	int check()
	{
		// Invalidate cache
		#ifndef __aarch64__
			Xil_DCacheInvalidateRange((UINTPTR)to_ddr_ptr, length);
		#endif

		value = start_value;
		for(long i = 0; i < length; i++)
		{
			if (to_ddr_ptr[i] != value)
			{
				xil_printf("Data error at %x, %d != %d \r\n", i, (unsigned int)from_ddr_ptr[i], (unsigned int)to_ddr_ptr[i]);
				return XST_FAILURE;
			}
			value += 1;
		}

		return XST_SUCCESS;
	}

	void print_out()
	{
		#ifndef __aarch64__
			Xil_DCacheInvalidateRange((UINTPTR)to_ddr_ptr, length);
		#endif
			xil_printf("Data out at address: %p, legnth: %d \r\n", from_ddr_ptr, length);
			for(long i = 0; i < length; i++)
				xil_printf("%d \r\n", (unsigned int)to_ddr_ptr[i]);
	}
};



#endif /* SRC_TEST_DATA_H_ */
