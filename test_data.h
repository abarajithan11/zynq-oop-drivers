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
	__uint8_t *data_in_ptr;
	__uint8_t *data_out_ptr;
	char start_value;
	long length;

	// Constructor fills the test data
	Test_Data(	auto * data_in_ptr,
				auto * data_out_ptr,
				char start_value,
				long length
			 ):
				data_in_ptr((__uint8_t*)data_in_ptr),
				data_out_ptr((__uint8_t*)data_out_ptr),
				start_value(start_value),
				length(length)
	{
		value = start_value;

		for(long i = 0; i < length; i ++)
		{
				data_in_ptr[i] = value;
				value += 1;
		}
	}

	int check()
	{

		// Invalidate cache
		#ifndef __aarch64__
			Xil_DCacheInvalidateRange((UINTPTR)data_out_ptr, length);
		#endif

		value = start_value;
		for(long i = 0; i < length; i++)
		{
			if (data_out_ptr[i] != value)
			{
				xil_printf("Data error %d: %x/%x\r\n", i, (unsigned int)data_out_ptr[i], (unsigned int)value);
				return XST_FAILURE;
			}
			value += 1;
		}

		return XST_SUCCESS;
	}
};



#endif /* SRC_TEST_DATA_H_ */
