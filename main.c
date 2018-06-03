/*
 * Basic firmware to control Some Energenie 433MHz sockets (ENER002-4)
 * as if they were a www.dcttech.com 4 port USB Relay board
 *
 * Copyright 2018 Jonathan McDowell <noodles@earth.li>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <stdbool.h>
#include <avr/eeprom.h>
#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/wdt.h>
#include <util/delay.h>

#include <avr/pgmspace.h>
#include "usbdrv.h"
#include "libs-device/osccal.h"

#define REPEAT_COUNT 10

#define CMD_ALL_ON 0xfe
#define CMD_ALL_OFF 0xfc
#define CMD_ON 0xff
#define CMD_OFF 0xfd
#define CMD_SET_SERIAL 0xfa

int serno_str[] = {
	USB_STRING_DESCRIPTOR_HEADER(5),
	'1', '2', '3', '4', '5',
};
uint32_t serno;
unsigned long cmd = 0;
int repeat = 0, wait = 0;
uint8_t state = 0;

PROGMEM const char usbHidReportDescriptor[22] = {
	0x06, 0x00, 0xff,		/* USAGE PAGE (Generic Desktop) */
	0x09, 0x01,			/* USAGE (Vendor Usage 1) */
	0xa1, 0x01,			/* COLLECTION (Application) */
	0x15, 0x00,			/*   LOGICAL_MINIMUM (0) */
	0x26, 0xff, 0x00,		/*   LOGICAL_MAXIMUM (255) */
	0x75, 0x08,			/*   REPORT_SIZE (8) */
	0x95, 0x08,			/*   REPORT_COUNT (8) */
	0x09, 0x00,			/*   USAGE (Undefined) */
	0xb2, 0x02, 0x01,		/*   FEATURE (Data, Var, Abs, Buf) */
	0xc0				/* END_COLLECTION */
};

inline char hexdigit(int i)
{
	return (i < 10) ? ('0' + i) : ('A' - 10 + i);
}

inline int digithex(char i)
{
	if (i >= '0' && i <= '9')
		return i - '0';

	if (i >= 'A' && i <= 'F')
		return i - 'A' + 10;

	if (i >= 'a' && i <= 'f')
		return i - 'a' + 10;

	return 0;
}

void fetch_serno(void)
{
	eeprom_read_block(&serno, 0, 4);
	if (serno == 0xffffffff) {
		/* If the EEPROM is blank, return a default serial # */
		serno_str[1] = '1';
		serno_str[2] = '2';
		serno_str[3] = '3';
		serno_str[4] = '4';
		serno_str[5] = '5';
	} else {
		serno_str[1] = hexdigit((serno >> 20) & 0xF);
		serno_str[2] = hexdigit((serno >> 16) & 0xF);
		serno_str[3] = hexdigit((serno >> 12) & 0xF);
		serno_str[4] = hexdigit((serno >>  8) & 0xF);
		serno_str[5] = hexdigit((serno >>  4) & 0xF);
	}
}

void update_serno(uchar *buf, uchar len)
{
	uchar i;

	serno = 0;
	for (i = 0; i < 5; i++) {
		serno |= digithex(buf[i]);
		serno <<= 4;
	}

	/*
	 * I have no idea why this gets stored 3 times, but the original
	 * firmware does it.
	 */
	eeprom_write_block(&serno, (void *) 0x00, 4);
	eeprom_write_block(&serno, (void *) 0x40, 4);
	eeprom_write_block(&serno, (void *) 0x80, 4);

	for (i = 0; i < 5; i++) {
		serno_str[i + 1] = buf[i];
	}
}

usbMsgLen_t usbFunctionSetup(uchar data[8])
{
	usbRequest_t *rq = (void *) data;

	if ((rq->bmRequestType & USBRQ_TYPE_MASK) == USBRQ_TYPE_CLASS) {
		if ((rq->bRequest == USBRQ_HID_GET_REPORT) ||
				(rq->bRequest == USBRQ_HID_SET_REPORT)) {
			return 0xFF;
		}
	}

	return 0;
}

usbMsgLen_t usbFunctionDescriptor(usbRequest_t *rq)
{
	if (rq->wValue.bytes[1] == USBDESCR_STRING &&
			rq->wValue.bytes[0] == 3) {
		usbMsgPtr = (usbMsgPtr_t) serno_str;
		return sizeof(serno_str);
	}
	return 0;
}

uchar usbFunctionRead(uchar *data, uchar len)
{
	uchar i;

	if (len != 0) {
		for (i = 0; i < 5; i++) {
			data[i] = serno_str[i + 1];
		}
		data[5] = data[6] = 0;
		data[7] = state;
		return len;
	}

	return 0;
}

uchar usbFunctionWrite(uchar *data, uchar len)
{
	if (data[0] == CMD_ALL_ON) {
		cmd = serno | 0xd;
		state = 0xf;
		wait = 200;
		repeat = REPEAT_COUNT;
	} else if (data[0] == CMD_ALL_OFF) {
		cmd = serno | 0xc;
		state = 0;
		wait = 10;
		repeat = REPEAT_COUNT;
	} else if (data[0] == CMD_ON) {
		wait = 200;
		switch (data[1]) {
		case 1:
			cmd = serno | 0xf;
			break;
		case 2:
			cmd = serno | 0x7;
			break;
		case 3:
			cmd = serno | 0xb;
			break;
		case 4:
			cmd = serno | 0x3;
			break;
		default:
			return len;
		}
		repeat = REPEAT_COUNT;
		state |= (1 << (data[1] - 1));
	} else if (data[0] == CMD_OFF) {
		wait = 200;
		switch (data[1]) {
		case 1:
			cmd = serno | 0xe;
			break;
		case 2:
			cmd = serno | 0x6;
			break;
		case 3:
			cmd = serno | 0xa;
			break;
		case 4:
			cmd = serno | 0x2;
			break;
		default:
			return len;
		}
		repeat = REPEAT_COUNT;
		state &= ~(1 << (data[1] - 1));
	} else if (data[0] == CMD_SET_SERIAL) {
		update_serno(&data[1], 6);
	}

	return len;
}

void t433_transmit_bit(bool value)
{
	PORTB |= 1 << PB0;
	if (value)
		_delay_us(600);
	else
		_delay_us(200);

	PORTB &= ~(1 << PB0);
	if (value)
		_delay_us(200);
	else
		_delay_us(600);
}

void t433_send(unsigned long code, unsigned int length)
{
	int i;

	cli();
	for (i = length - 1; i >= 0; i--) {
		if (code & (1L << i)) {
			t433_transmit_bit(true);
		} else {
			t433_transmit_bit(false);
		}
	}
	/* Send a sync bit */
	PORTB |= 1 << PB0;
	_delay_us(200);
	PORTB &= ~(1 << PB0);
	sei();
	_delay_ms(30);
}

int __attribute__((noreturn)) main(void)
{
	unsigned char i;

	wdt_enable(WDTO_1S);

	fetch_serno();

	usbInit();
	usbDeviceDisconnect();

	i = 0;
	while (--i) {
		wdt_reset();
		_delay_ms(1);
	}

	usbDeviceConnect();

	/* Set the 433MHz transmitter bit to output mode */
	DDRB |= (1 << PB0);
	PORTB &= (1 << PB0);

	sei(); /* We're ready to go; enable interrupts */

	while (1) {
		wdt_reset();
		usbPoll();
		if (cmd) {
			if (wait) {
				wait--;
			} else {
				t433_send(cmd, 24);
				if (--repeat == 0)
					cmd = 0;
			}
		}
	}
}
