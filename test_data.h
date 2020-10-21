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
	Test_Data(	auto * from_ddr_ptr,
				auto * to_ddr_ptr,
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
				from_ddr_ptr[i] = value;
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
				xil_printf("Data error %d: %x/%x\r\n", i, (unsigned int)to_ddr_ptr[i], (unsigned int)value);
				return XST_FAILURE;
			}
			value += 1;
		}

		return XST_SUCCESS;
	}
};



#endif /* SRC_TEST_DATA_H_ */
