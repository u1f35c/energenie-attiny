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

#define CMD_ALL_ON 0xfe
#define CMD_ALL_OFF 0xfc
#define CMD_ON 0xff
#define CMD_OFF 0xfd
#define CMD_SET_SERIAL 0xfa

/* Energenie 24 bit address , last 4 all 0 for code */
#define ENER_ADDR 0x123450

uchar serno_read = 0;
uchar serno[6];
unsigned long cmd = 0;
int repeat = 0;

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

void fetch_serno(void)
{
	if (!serno_read) {
		eeprom_read_block(serno, 0, 6);
		if (serno[0] == 0xff) {
			/* If the EEPROM is blank, return a default serial # */
			serno[0] = 'U';
			serno[1] = 'N';
			serno[2] = 'S';
			serno[3] = 'E';
			serno[4] = 'T';
			serno[5] = 0;
		}
		serno_read = 1;
	}
}

void update_serno(uchar *buf, uchar len)
{
	uchar i;

	/*
	 * I have no idea why this gets stored 3 times, but the original
	 * firmware does it.
	 */
	eeprom_write_block(buf, (void *) 0x00, len);
	eeprom_write_block(buf, (void *) 0x40, len);
	eeprom_write_block(buf, (void *) 0x80, len);

	for (i = 0; i < 6; i++) {
		serno[i] = buf[i];
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

uchar usbFunctionRead(uchar *data, uchar len)
{
	uchar i;

	if (len != 0) {
		fetch_serno();
		for (i = 0; i < 6; i++) {
			data[i] = serno[i];
		}
		data[6] = data[7] = 0;
		if (PORTB & (1 << PB0)) {
			data[7] = 1;
		}
		return len;
	}

	return 0;
}

uchar usbFunctionWrite(uchar *data, uchar len)
{
	if (data[0] == CMD_ALL_ON) {
		cmd = ENER_ADDR | 0xd;
		repeat = 5;
	} else if (data[0] == CMD_ALL_OFF) {
		cmd = ENER_ADDR | 0xc;
		repeat = 5;
	} else if (data[0] == CMD_ON) {
		switch (data[1]) {
		case 1:
			cmd = ENER_ADDR | 0xf;
			repeat = 5;
			break;
		case 2:
			cmd = ENER_ADDR | 0x7;
			repeat = 5;
			break;
		case 3:
			cmd = ENER_ADDR | 0xb;
			repeat = 5;
			break;
		case 4:
			cmd = ENER_ADDR | 0x3;
			repeat = 5;
			break;
		default:
			break;
		}
	} else if (data[0] == CMD_OFF) {
		switch (data[1]) {
		case 1:
			cmd = ENER_ADDR | 0xe;
			repeat = 5;
			break;
		case 2:
			cmd = ENER_ADDR | 0x6;
			repeat = 5;
			break;
		case 3:
			cmd = ENER_ADDR | 0xa;
			repeat = 5;
			break;
		case 4:
			cmd = ENER_ADDR | 0x2;
			repeat = 5;
			break;
		default:
			break;
		}
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
			t433_send(cmd, 24);
			if (--repeat == 0)
				cmd = 0;
		}
	}
}
