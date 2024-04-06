/*
	ifdhandler.c: IFDH API
	Copyright (C) 2003-2010   Ludovic Rousseau

	This library is free software; you can redistribute it and/or
	modify it under the terms of the GNU Lesser General Public
	License as published by the Free Software Foundation; either
	version 2.1 of the License, or (at your option) any later version.

	This library is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	Lesser General Public License for more details.

	You should have received a copy of the GNU Lesser General Public License
	along with this library; if not, write to the Free Software Foundation,
	Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <libusb-1.0/libusb.h>

#include "pcsclite.h"
#include "ifdhandler.h"

#define EXTERNAL __attribute__ ((visibility("default")))

#define EZ100PU_VID 0x0ca6
#define EZ100PU_PID 0x0010

#define TIMEOUT 1000

// TODO: Support multiple devices
static libusb_device_handle* dev;
static int inited;
static int present;
static unsigned char seq;
static unsigned char atr[MAX_ATR_SIZE];
static int atrlen;


static void init() {
	dev = NULL;

	// TODO: check for error
	libusb_init(NULL);

	inited = 1;
}

static void hexdump(unsigned char *buffer, int size)
{
	int i;

	for (i = 0; i < size; i++) {
		if ((i != 0) && ((i % 4) == 0))
			fprintf(stderr, " ");
		fprintf(stderr, "%02x", buffer[i]);
	}
}

// TODO: Support TX payload
static int icc_bulk_transfer(unsigned char cmd, unsigned char *rxbuffer, int rxsize, int *rxrecv)
{
	unsigned char buffer[256];
	int transferred;
	int ret;
	buffer[0] = cmd;
	buffer[1] = buffer[2] = buffer[3] = buffer[4] = 0;
	buffer[5] = 0; // slot
	buffer[6] = seq++; // seq
	buffer[7] = buffer[8] = buffer[9] = 0; // reserved?

	ret = libusb_bulk_transfer(dev, 0x01, buffer, 10, &transferred, TIMEOUT);
	fprintf(stderr, "out %02x %d %d (", cmd, ret, transferred);
	hexdump(buffer, transferred);
	fprintf(stderr, ")\n");
	if (ret != 0)
		return ret;
	if (transferred != 10)
		return EAGAIN;

	ret = libusb_bulk_transfer(dev, 0x82, rxbuffer, rxsize, rxrecv, TIMEOUT);
	fprintf(stderr, "in %02x %d %d (", cmd, ret, *rxrecv);
	hexdump(rxbuffer, *rxrecv);
	fprintf(stderr, ")\n");
	return ret;
}

EXTERNAL RESPONSECODE IFDHCreateChannelByName(DWORD Lun, LPSTR lpcDevice)
{
	unsigned char buffer[256];
	int transferred;
	int ret;
	fprintf(stderr, "%s: %d %s\n", __func__, Lun, lpcDevice);

	if (!inited)
		init();

	// TODO: Make sure only one Lun/device is active at a time

	// TODO: parse the actual string...
	dev = libusb_open_device_with_vid_pid(NULL, EZ100PU_VID, EZ100PU_PID);

	if (!dev)
		return IFD_COMMUNICATION_ERROR;

	seq = 0;
	atrlen = -1;

	ret = icc_bulk_transfer(0x60, buffer, sizeof(buffer), &transferred);
	if (ret != 0 || transferred < 10)
		return IFD_COMMUNICATION_ERROR;
	// TODO: Validate data, seems like the data is a human-readable string after offset 10.

	// TODO: One of these bytes indicates presence
	present = (buffer[7] == 0x01);

	fprintf(stderr, "%s POWER_DOWN\n", __func__);
	ret = icc_bulk_transfer(0x63, buffer, sizeof(buffer), &transferred);
	if (ret)
		return IFD_COMMUNICATION_ERROR;

	return IFD_SUCCESS;		
}

EXTERNAL RESPONSECODE IFDHCreateChannel(DWORD Lun, DWORD Channel)
{
	/*
	 * Lun - Logical Unit Number, use this for multiple card slots or
	 * multiple readers. 0xXXXXYYYY - XXXX multiple readers, YYYY multiple
	 * slots. The resource manager will set these automatically.  By
	 * default the resource manager loads a new instance of the driver so
	 * if your reader does not have more than one smartcard slot then
	 * ignore the Lun in all the functions. Future versions of PC/SC might
	 * support loading multiple readers through one instance of the driver
	 * in which XXXX would be important to implement if you want this.
	 */

	/*
	 * Channel - Channel ID.  This is denoted by the following: 0x000001 -
	 * /dev/pcsc/1 0x000002 - /dev/pcsc/2 0x000003 - /dev/pcsc/3
	 *
	 * USB readers may choose to ignore this parameter and query the bus
	 * for the particular reader.
	 */

	/*
	 * This function is required to open a communications channel to the
	 * port listed by Channel.  For example, the first serial reader on
	 * COM1 would link to /dev/pcsc/1 which would be a sym link to
	 * /dev/ttyS0 on some machines This is used to help with intermachine
	 * independence.
	 *
	 * Once the channel is opened the reader must be in a state in which
	 * it is possible to query IFDHICCPresence() for card status.
	 *
	 * returns:
	 *
	 * IFD_SUCCESS IFD_COMMUNICATION_ERROR
	 */
	// We do not support creating by channel ID, only IFDHCreateChannelByName.
	fprintf(stderr, "%s: NOT SUPPORTED\n", __func__);
	return IFD_COMMUNICATION_ERROR;
} /* IFDHCreateChannel */


EXTERNAL RESPONSECODE IFDHCloseChannel(DWORD Lun)
{
	/*
	 * This function should close the reader communication channel for the
	 * particular reader.  Prior to closing the communication channel the
	 * reader should make sure the card is powered down and the terminal
	 * is also powered down.
	 *
	 * returns:
	 *
	 * IFD_SUCCESS IFD_COMMUNICATION_ERROR
	 */

	fprintf(stderr, "%s\n", __func__);

	if (dev) {
		libusb_close(dev);
		dev = NULL;
	}

	return IFD_SUCCESS;
} /* IFDHCloseChannel */

EXTERNAL RESPONSECODE IFDHGetCapabilities(DWORD Lun, DWORD Tag,
	PDWORD Length, PUCHAR Value)
{
	/*
	 * This function should get the slot/card capabilities for a
	 * particular slot/card specified by Lun.  Again, if you have only 1
	 * card slot and don't mind loading a new driver for each reader then
	 * ignore Lun.
	 *
	 * Tag - the tag for the information requested example: TAG_IFD_ATR -
	 * return the Atr and its size (required). these tags are defined in
	 * ifdhandler.h
	 *
	 * Length - the length of the returned data Value - the value of the
	 * data
	 *
	 * returns:
	 *
	 * IFD_SUCCESS IFD_ERROR_TAG
	 */

	fprintf(stderr, "%s Lun %d Tag %04x Length %d\n", __func__, Lun, Tag, *Length);

	switch (Tag) {
		case TAG_IFD_POLLING_THREAD_WITH_TIMEOUT:
			// Not supported
			*Length = 0;
			break;
		case TAG_IFD_SLOTS_NUMBER:
			if (*Length >= 1) {
				*Length = 1;
				*Value = 1;
			} else {
				return IFD_ERROR_INSUFFICIENT_BUFFER;
			}
			break;
		default:
			fprintf(stderr, "%s Unknown tag %04x\n", Tag);
			return IFD_ERROR_TAG;
	}

	return IFD_SUCCESS;
} /* IFDHGetCapabilities */


EXTERNAL RESPONSECODE IFDHSetCapabilities(DWORD Lun, DWORD Tag,
	/*@unused@*/ DWORD Length, /*@unused@*/ PUCHAR Value)
{
	/*
	 * This function should set the slot/card capabilities for a
	 * particular slot/card specified by Lun.  Again, if you have only 1
	 * card slot and don't mind loading a new driver for each reader then
	 * ignore Lun.
	 *
	 * Tag - the tag for the information needing set
	 *
	 * Length - the length of the returned data Value - the value of the
	 * data
	 *
	 * returns:
	 *
	 * IFD_SUCCESS IFD_ERROR_TAG IFD_ERROR_SET_FAILURE
	 * IFD_ERROR_VALUE_READ_ONLY
	 */

	fprintf(stderr, "%s\n", __func__);
	return IFD_ERROR_TAG;
} /* IFDHSetCapabilities */


EXTERNAL RESPONSECODE IFDHSetProtocolParameters(DWORD Lun, DWORD Protocol,
	UCHAR Flags, UCHAR PTS1, UCHAR PTS2, UCHAR PTS3)
{
	/*
	 * This function should set the PTS of a particular card/slot using
	 * the three PTS parameters sent
	 *
	 * Protocol - SCARD_PROTOCOL_T0 or SCARD_PROTOCOL_T1
	 * Flags - Logical OR of possible values:
	 *  IFD_NEGOTIATE_PTS1
	 *  IFD_NEGOTIATE_PTS2
	 *  IFD_NEGOTIATE_PTS3
	 * to determine which PTS values to negotiate.
	 * PTS1,PTS2,PTS3 - PTS Values.
	 *
	 * returns:
	 *  IFD_SUCCESS
	 *  IFD_ERROR_PTS_FAILURE
	 *  IFD_COMMUNICATION_ERROR
	 *  IFD_PROTOCOL_NOT_SUPPORTED
	 */

	fprintf(stderr, "%s\n", __func__);
	return IFD_COMMUNICATION_ERROR;
} /* IFDHSetProtocolParameters */


EXTERNAL RESPONSECODE IFDHPowerICC(DWORD Lun, DWORD Action,
	PUCHAR Atr, PDWORD AtrLength)
{
	int ret;
	unsigned char buffer[256];
	int transferred;
	int len;
	/*
	 * This function controls the power and reset signals of the smartcard
	 * reader at the particular reader/slot specified by Lun.
	 *
	 * Action - Action to be taken on the card.
	 *
	 * IFD_POWER_UP - Power and reset the card if not done so (store the
	 * ATR and return it and its length).
	 *
	 * IFD_POWER_DOWN - Power down the card if not done already
	 * (Atr/AtrLength should be zero'd)
	 *
	 * IFD_RESET - Perform a quick reset on the card.  If the card is not
	 * powered power up the card.  (Store and return the Atr/Length)
	 *
	 * Atr - Answer to Reset of the card.  The driver is responsible for
	 * caching this value in case IFDHGetCapabilities is called requesting
	 * the ATR and its length.  This should not exceed MAX_ATR_SIZE.
	 *
	 * AtrLength - Length of the Atr.  This should not exceed
	 * MAX_ATR_SIZE.
	 *
	 * Notes:
	 *
	 * Memory cards without an ATR should return IFD_SUCCESS on reset but
	 * the Atr should be zero'd and the length should be zero
	 *
	 * Reset errors should return zero for the AtrLength and return
	 * IFD_ERROR_POWER_ACTION.
	 *
	 * returns:
	 *
	 * IFD_SUCCESS IFD_ERROR_POWER_ACTION IFD_COMMUNICATION_ERROR
	 * IFD_NOT_SUPPORTED
	 */

	switch (Action) {
		case IFD_POWER_UP:
			// Should we only do this if the card is powered down?
			fprintf(stderr, "%s IFD_POWER_UP\n", __func__);
			// Down then up.
			ret = icc_bulk_transfer(0x63, buffer, sizeof(buffer), &transferred);
			if (ret)
				return IFD_COMMUNICATION_ERROR;
			ret = icc_bulk_transfer(0x62, buffer, sizeof(buffer), &transferred);
			if (ret)
				return IFD_COMMUNICATION_ERROR;
			if (buffer[0] != 0x80 ||
				buffer[1] != 0 || buffer[2] != 0 || buffer[3] != 0) { // Size < 256
				fprintf(stderr, "%s IFD_POWER_UP incorrect header %02x%02x%02x%02x %02x\n", __func__, buffer[0], buffer[1], buffer[2], buffer[3], buffer[4]);
				return IFD_COMMUNICATION_ERROR;
			}
			if (buffer[4] > MAX_ATR_SIZE) {
				fprintf(stderr, "%s IFD_POWER_UP atr too large %02x\n", __func__, buffer[4]);
				return IFD_COMMUNICATION_ERROR;
			}

			// Cache ATR
			atrlen = buffer[4]-1;
			memcpy(atr, buffer+11, atrlen);

			// Return data
			if (atrlen > *AtrLength)
				return IFD_ERROR_POWER_ACTION;
			memcpy(Atr, atr, atrlen);
			*AtrLength = atrlen;

			// The binary driver also sends 0x6f, 0x61, not sure what for...

			return IFD_SUCCESS;
			break;
		case IFD_POWER_DOWN:
			fprintf(stderr, "%s IFD_POWER_DOWN\n", __func__);
			ret = icc_bulk_transfer(0x63, buffer, sizeof(buffer), &transferred);
			if (ret)
				return IFD_COMMUNICATION_ERROR;
			return IFD_SUCCESS;
			break;
		case IFD_RESET:
			fprintf(stderr, "%s IFD_RESET\n", __func__);
			break;
	}
	return IFD_NOT_SUPPORTED;
} /* IFDHPowerICC */


EXTERNAL RESPONSECODE IFDHTransmitToICC(DWORD Lun, SCARD_IO_HEADER SendPci,
	PUCHAR TxBuffer, DWORD TxLength,
	PUCHAR RxBuffer, PDWORD RxLength, /*@unused@*/ PSCARD_IO_HEADER RecvPci)
{
	/*
	 * This function performs an APDU exchange with the card/slot
	 * specified by Lun.  The driver is responsible for performing any
	 * protocol specific exchanges such as T=0/1 ... differences.  Calling
	 * this function will abstract all protocol differences.
	 *
	 * SendPci Protocol - 0, 1, .... 14 Length - Not used.
	 *
	 * TxBuffer - Transmit APDU example (0x00 0xA4 0x00 0x00 0x02 0x3F
	 * 0x00) TxLength - Length of this buffer. RxBuffer - Receive APDU
	 * example (0x61 0x14) RxLength - Length of the received APDU.  This
	 * function will be passed the size of the buffer of RxBuffer and this
	 * function is responsible for setting this to the length of the
	 * received APDU.  This should be ZERO on all errors.  The resource
	 * manager will take responsibility of zeroing out any temporary APDU
	 * buffers for security reasons.
	 *
	 * RecvPci Protocol - 0, 1, .... 14 Length - Not used.
	 *
	 * Notes: The driver is responsible for knowing what type of card it
	 * has.  If the current slot/card contains a memory card then this
	 * command should ignore the Protocol and use the MCT style commands
	 * for support for these style cards and transmit them appropriately.
	 * If your reader does not support memory cards or you don't want to
	 * then ignore this.
	 *
	 * RxLength should be set to zero on error.
	 *
	 * returns:
	 *
	 * IFD_SUCCESS IFD_COMMUNICATION_ERROR IFD_RESPONSE_TIMEOUT
	 * IFD_ICC_NOT_PRESENT IFD_PROTOCOL_NOT_SUPPORTED
	 */

	fprintf(stderr, "%s\n", __func__);
	return IFD_PROTOCOL_NOT_SUPPORTED;
} /* IFDHTransmitToICC */


EXTERNAL RESPONSECODE IFDHControl(DWORD Lun, DWORD dwControlCode,
	PUCHAR TxBuffer, DWORD TxLength, PUCHAR RxBuffer, DWORD RxLength,
	PDWORD pdwBytesReturned)
{
	/*
	 * This function performs a data exchange with the reader (not the
	 * card) specified by Lun.  Here XXXX will only be used. It is
	 * responsible for abstracting functionality such as PIN pads,
	 * biometrics, LCD panels, etc.  You should follow the MCT, CTBCS
	 * specifications for a list of accepted commands to implement.
	 *
	 * TxBuffer - Transmit data TxLength - Length of this buffer. RxBuffer
	 * - Receive data RxLength - Length of the received data.  This
	 * function will be passed the length of the buffer RxBuffer and it
	 * must set this to the length of the received data.
	 *
	 * Notes: RxLength should be zero on error.
	 */
	RESPONSECODE return_value = IFD_ERROR_NOT_SUPPORTED;

	fprintf(stderr, "%s\n", __func__);
	return return_value;
} /* IFDHControl */


EXTERNAL RESPONSECODE IFDHICCPresence(DWORD Lun)
{
	/*
	 * This function returns the status of the card inserted in the
	 * reader/slot specified by Lun.  It will return either:
	 *
	 * returns: IFD_ICC_PRESENT IFD_ICC_NOT_PRESENT
	 * IFD_COMMUNICATION_ERROR
	 */

	unsigned char buffer[8];
	int transferred = 0;
	int ret;

	ret = libusb_interrupt_transfer(dev, 0x83, buffer, sizeof(buffer), &transferred, 1);
	if (ret != LIBUSB_ERROR_TIMEOUT) {
		fprintf(stderr, "%s: IRQ in %d %d (", __func__, ret, transferred);
		hexdump(buffer, transferred);
		fprintf(stderr, ")\n");
	}

	if (ret == 0) {
		if (transferred != 2)
			return IFD_COMMUNICATION_ERROR;
		if (buffer[0] != 0x50)
			return IFD_COMMUNICATION_ERROR;

		if (buffer[1] == 0x02)
			present = 0;
		else if (buffer[1] == 0x03)
			present = 1;
		else
			return IFD_COMMUNICATION_ERROR;
	} else if (ret != LIBUSB_ERROR_TIMEOUT) {
		return IFD_COMMUNICATION_ERROR;
	}

	return present ? IFD_ICC_PRESENT: IFD_ICC_NOT_PRESENT;
} /* IFDHICCPresence */