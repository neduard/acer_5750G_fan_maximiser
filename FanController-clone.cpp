/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2014   Eduard Nicodei
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 * */
#include "stdafx.h"
#include <stdio.h>
#include <stdlib.h>
#include <Windows.h>
#include <assert.h>

#define LPCFILTER_IOPORT_READ_FUNC 0x800
#define LPCFILTER_IOPORT_WRITE_FUNC 0x801

bool read_uchar(const HANDLE hDevice, const unsigned char port,
			    unsigned char* const value)
{
	const DWORD nInBufferSize = 8;
	const DWORD nOutBufferSize = 1;
	const DWORD dwIoControlCode = CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800,
			METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS);
	assert(dwIoControlCode == 0x22E000);

	BYTE lpOutBuffer[nOutBufferSize];
	BYTE lpInBuffer[nInBufferSize] = {};
	DWORD bytesReturned;
	bool status;

	if (!hDevice) {
		printf("Invalid device handle: %u\n", hDevice);
		return false;
	}

	lpInBuffer[0] = port;
	status = DeviceIoControl(hDevice, dwIoControlCode, lpInBuffer, nInBufferSize,
			lpOutBuffer, nOutBufferSize, &bytesReturned, NULL);

	if (!status || bytesReturned != 1) {
		printf("%s: Unable to read from port 0x%02x\n", __FUNCTIONW__, port);
		printf("Error code: 0x%x; bytesReturned: %u\n",
				GetLastError(), bytesReturned);
		return false;
	}

	*value = lpOutBuffer[0];
	return true;
}

bool write_uchar(const HANDLE hDevice, const unsigned char port,
				 const unsigned char value)
{
	const DWORD dwIoControlCode = CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801,
			METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS);
	const DWORD nInBufferSize = 8;

	BYTE lpInBuffer[nInBufferSize] = {port, 0x00, 0x00, 0x00,
			value, 0x00, 0x00, 0x00};
	DWORD bytesReturned;
	BOOL status;

	if (hDevice < 0) {
		printf("Invalid file handle %x\n", hDevice);
		return false;
	}

	status = DeviceIoControl(hDevice, dwIoControlCode, lpInBuffer, nInBufferSize,
			NULL, 0, &bytesReturned, NULL);

	if (!status) {
		printf("Error writing 0x%2x to port 0x%2x. Code: 0x%x", value, port, GetLastError());
		return false;
	}

	return true;
}

bool wait_until_bitmask_is_value(const HANDLE hDevice,
								const unsigned int bitmask,
								const unsigned char value)
{
	BYTE currentValue;
	for (WORD i = 0; i < 20000; ++i) {
		if (!read_uchar(hDevice, 0x6C, &currentValue)) {
			return false;
		}
		if ((currentValue & bitmask) == value) {
			return true;
		}
	}
	printf("current value: 0x%2x\n", currentValue);
	printf("%s: timeout\n", __FUNCTION__);
	return false;
}

bool ec_intro_sequence(HANDLE hDevice)
{
	unsigned char value;
	if (!read_uchar(hDevice, 0x68, &value)) {
		return false;
	}
	printf("%s: read 0x%02x from port 0x68\n", __FUNCTION__, value);
	if (!wait_until_bitmask_is_value(hDevice, 0x02, 0x00)) {
		return false;
	}
	if (!write_uchar(hDevice, 0x6C, 0x59)) {
		return false;
	}
	printf("%s: successfully writting 0x59 to port 0x6C\n", __FUNCTION__);
	return true;
}

bool ec_close_sequence(HANDLE hDevice)
{
	unsigned char value;
	if (!read_uchar(hDevice, 0x68, &value)) {
		return false;
	}

	if (!wait_until_bitmask_is_value(hDevice, 0x02, 0x00)) {
		return false;
	}

	if (!write_uchar(hDevice, 0x6C, 0xFF)) {
		return false;
	}
	printf("%s: Successful.\n", __FUNCTION__);
	return true;
}

int _tmain(int argc, _TCHAR* argv[])
{
	HANDLE lpcDriverHandle;
	char port_number_string[128];
	unsigned int port_number;
	BYTE value;

	lpcDriverHandle = CreateFileA("\\\\.\\LPCFilter", GENERIC_READ|GENERIC_WRITE,
		FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);

	if (lpcDriverHandle == INVALID_HANDLE_VALUE) {
		printf("Unable to open LPC driver. Error 0x%x\n", GetLastError());
		system("pause");
		return 1;
	}

	if (!wait_until_bitmask_is_value(lpcDriverHandle, 0x80, 0x0)) {
		goto closing_sequence;
	}
	if (!ec_intro_sequence(lpcDriverHandle)) {
		goto closing_sequence;
	}
	// magic sequence begins
	if (!wait_until_bitmask_is_value(lpcDriverHandle, 0x02, 0x00)) {
		goto closing_sequence;
	}
	// in the following line, replace 0x76 with 0x77 to have fan spin at
	// maximum speed.
	if (!write_uchar(lpcDriverHandle, 0x68, 0x76)) {
		goto closing_sequence;
	} else {
		printf("FAN ON\n");
	}

closing_sequence:

	ec_close_sequence(lpcDriverHandle);

	if (CloseHandle(lpcDriverHandle)) {
		printf("Driver handler closed successfully\n");
	} else {
		printf("Unable to close handle. Error: %x\n", GetLastError());
	}
	system("pause");
	return 1;

	printf("Enter port number: "); scanf("%s", &port_number_string);
	port_number = (unsigned int)strtoul(port_number_string, NULL, 0);
	printf("You entered 0x%2X\n", port_number);


	system("pause");
	return 0;
}

