/* This file has been prepared for Doxygen automatic documentation generation.*/
/*! \file *********************************************************************
 *
 * \brief
 *      XMEGA USART FreeRTOS driver source file.
 *
 *      This file contains the function implementations of the XMEGA interrupt
 *      FreeRTOS-based driver.
 *
 * \par Application note:
 *      AVR1307: Using the XMEGA USART
 *
 * \par Documentation
 *      For comprehensive code documentation, supported compilers, compiler
 *      settings and supported devices see readme.html
 *
 * \author
 *      Atmel Corporation: http://www.atmel.com \n
 *      Support email: avr@atmel.com
 *  \author
 *   	Universität Erlangen-Nürnberg
 *		LS Informationstechnik (Kommunikationselektronik)
 *		Yuriy Kulikov
 *      Support email: Yuriy.Kulikov.87@googlemail.com
 *
 * $Revision: 1694 $
 * $Date: 2008-07-29 14:21:58 +0200 (ti, 29 jul 2008) $  \n
 *
 * Copyright (c) 2008, Atmel Corporation All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3. The name of ATMEL may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY ATMEL "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE EXPRESSLY AND
 * SPECIFICALLY DISCLAIMED. IN NO EVENT SHALL ATMEL BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *****************************************************************************/

/* Scheduler include files. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "port_driver.h"
#include "usart.h"
#include "usart_driver_RTOS.h"
//Structures, representing uart and its buffer. for internal use.
//Allocated dynamically
USART_buffer_struct_t * pUsartBufferC;
USART_buffer_struct_t * pUsartBufferD;
USART_buffer_struct_t * pUsartBufferE;

signed char USART_RXComplete(USART_buffer_struct_t *);
signed char USART_DataRegEmpty(USART_buffer_struct_t *);
/*! \brief Receive complete interrupt service routine.
 *
 *  Receive complete interrupt service routine.
 *  Calls the common receive complete handler with pointer to the correct USART
 *  as argument.
 */
ISR(USARTC0_RXC_vect){ if( USART_RXComplete(pUsartBufferC) ) taskYIELD(); }
ISR(USARTD0_RXC_vect){ if( USART_RXComplete(pUsartBufferD) ) taskYIELD(); }
ISR(USARTE0_RXC_vect){ if( USART_RXComplete(pUsartBufferE) ) taskYIELD(); }
/*! \brief Data register empty  interrupt service routine.
 *
 *  Data register empty  interrupt service routine.
 *  Calls the common data register empty complete handler with pointer to the
 *  correct USART as argument.
 */
ISR(USARTC0_DRE_vect){USART_DataRegEmpty(pUsartBufferC);}
ISR(USARTD0_DRE_vect){USART_DataRegEmpty(pUsartBufferD);}
ISR(USARTE0_DRE_vect){USART_DataRegEmpty(pUsartBufferE);}

/*! \brief Initializes buffer and selects what USART module to use.
 *
 *  Initializes receive and transmit buffer and selects what USART module to use,
 *  and stores the data register empty interrupt level.
 *
 *
 *  \param usart                Pointer to the USART module to use.
 *  \return usart_buffer        The USART_buffer_struct_t instance which can be used to read and write.
 */
USART_buffer_struct_t * USART_InterruptDriver_Initialize(USART_t * usart, Baudrate_enum baudrate ,char bufferSize)
{
	//We use only low level interrupts, but leave local variable in case we change the mind
	USART_DREINTLVL_t dreIntLevel = USART_DREINTLVL_LO_gc;
	PORT_t * port_struct;
	USART_buffer_struct_t * pUsartBuffer;
	//switch by pointer to usart structure, which will be used. Usually it is not a good idea to
	//switch by pointer, but in our case pointers are defined by hardware
	switch ((int)usart) {
		case (int)&USARTC0:
			//Allocate memory for usart structure and store the pointer
			pUsartBuffer = ( USART_buffer_struct_t * ) pvPortMalloc( sizeof( USART_buffer_struct_t ) );
			//copy pointer pUsartBuffer to global pUsartBufferC, which is use to handle interrups
			pUsartBufferC = pUsartBuffer;
			//Since usart is on the port C, we will need to use PORTC
			port_struct = &PORTC;
			break;
		case (int)&USARTD0:
			//Allocate memory for usart structure and store the pointer
			pUsartBuffer = ( USART_buffer_struct_t * ) pvPortMalloc( sizeof( USART_buffer_struct_t ) );
			//copy pointer pUsartBuffer to global pUsartBufferD, which is use to handle interrups
			pUsartBufferD = pUsartBuffer;
			//Since usart is on the port D, we will need to use PORTC
			port_struct = &PORTD;
			break;
		default:
			break;
			//use default, TODO: report error here with LED

	}

	/* (TX0) as output. */
	port_struct->DIRSET = PIN3_bm;
	/* (RX0) as input. */
	port_struct->DIRCLR = PIN2_bm;
	//totempole and pullup
	PORT_ConfigurePins( port_struct,PIN3_bm,false,false,PORT_OPC_PULLUP_gc,PORT_ISC_BOTHEDGES_gc );
	/* Initialize buffers. Create a queue (allocate memory) and store queue handle in usart_struct
	 * On XMEGA port create all queues before vStartTaskScheduler() to ensure that heap size is enough */
	/* Store pointer to USART module */
	pUsartBuffer->usart = usart;
	/*Store DRE level so we will know which level to enable when we put data and want it to be sent. */
	pUsartBuffer->dreIntLevel = dreIntLevel;
	/* \brief  Receive buffer size: 2,4,8,16,32,64,128 bytes. */
	pUsartBuffer->xQueueRX = xQueueCreate(bufferSize,sizeof(char));
	pUsartBuffer->xQueueTX = xQueueCreate(bufferSize,sizeof(char));

	/* USARTD0, 8 Data bits, No Parity, 1 Stop bit. */
	USART_Format_Set(pUsartBuffer->usart, USART_CHSIZE_8BIT_gc, USART_PMODE_DISABLED_gc, false);
	/* Enable RXC interrupt. */
	USART_RxdInterruptLevel_Set(pUsartBuffer->usart, USART_RXCINTLVL_LO_gc);

	//http://prototalk.net/forums/showthread.php?t=188
	switch (baudrate) {
		case BAUD9600:
			USART_Baudrate_Set(pUsartBuffer->usart, 3317 , -4);
			break;
		default:
			//9600
			USART_Baudrate_Set(pUsartBuffer->usart, 3317 , -4);
			break;
	}

	/* Enable both RX and TX. */
	USART_Rx_Enable(pUsartBuffer->usart);
	USART_Tx_Enable(pUsartBuffer->usart);
	//return the buffer struct, to be used for reading and writing
	return pUsartBuffer;
}

/*! \brief Put data (5-8 bit character).
 *
 *  Stores data byte in TX software buffer and enables DRE interrupt if there
 *  is free space in the TX software buffer.
 *
 *  \param usart_struct The USART_buffer_struct_t struct instance.
 *  \param data       The data to send.
 *  \param xTicksToWait       Amount of RTOS ticks (1 ms default) to wait if there is space in queue.
 */
void USART_Buffer_PutByte(USART_buffer_struct_t * usart_buffer_t, uint8_t data, int ticksToWait )
{
	uint8_t tempCTRLA;
	/* If we successfully loaded byte to queue */
	if (xQueueSendToBack(usart_buffer_t->xQueueTX,&data,ticksToWait))
	{
		/* Enable DRE interrupt. */
		tempCTRLA = usart_buffer_t->usart->CTRLA;
		tempCTRLA = (tempCTRLA & ~USART_DREINTLVL_gm) | usart_buffer_t->dreIntLevel;
		usart_buffer_t->usart->CTRLA = tempCTRLA;
	}
}

/*! \brief Get received data (5-8 bit character).
 *
 *
 *  Returns pdTRUE is data is available and puts byte into &receivedChar variable
 *
 *  \param usart_struct       The USART_struct_t struct instance.
 *	\param receivedChar       Pointer to char variable for to save result.
 *	\param xTicksToWait       Amount of RTOS ticks (1 ms default) to wait if there is data in queue.
 *  \return					  Success.
 */
inline int8_t USART_Buffer_GetByte(USART_buffer_struct_t * usart_buffer_t, char * receivedChar, int ticksToWait )
{
	return xQueueReceive(usart_buffer_t->xQueueRX, receivedChar, ticksToWait);
}
/*! \brief Put data (5-8 bit character).
 *
 *  Stores data string in TX software buffer and enables DRE interrupt if there
 *  is free space in the TX software buffer.
 *
 *  \param usart_struct The USART_struct_t struct instance.
 *  \param string       The string to send.
 *  \param xTicksToWait       Amount of RTOS ticks (1 ms default) to wait if there is space in queue.
 */
inline void USART_Buffer_PutString(USART_buffer_struct_t * usart_buffer_t, const char *string, int ticksToWait )
{
	//send the whole string. Note that if buffer is full, USART_TXBuffer_PutByte will do nothing
	while (*string) USART_Buffer_PutByte(usart_buffer_t,*string++, ticksToWait );
}
/*! \brief Put data (5-8 bit character).
 *
 *  Stores data integer represented as string in TX software buffer and enables DRE interrupt if there
 *  is free space in the TX software buffer.
 *
 *  \param usart_struct The USART_struct_t struct instance.
 *  \param Int       The integer to send.
 *  \param radix	Integer basis - 10 for decimal, 16 for hex
 *  \param xTicksToWait       Amount of RTOS ticks (1 ms default) to wait if there is space in queue.
 */
void USART_Buffer_PutInt(USART_buffer_struct_t * usart_buffer_t, int16_t Int,int16_t radix, int ticksToWait )
{
	char * str="big string for some itoa uses";
	USART_Buffer_PutString(usart_buffer_t,itoa(Int,str,radix), ticksToWait );
}

/*! \brief RX Complete Interrupt Service Routine.
 *
 *  RX Complete Interrupt Service Routine.
 *  Stores received data in RX software buffer.
 *
 *  \param usart_struct      The USART_struct_t struct instance.
 *  \return xHigherPriorityTaskWoken boolean which is used to yield
 */
inline signed char USART_RXComplete(USART_buffer_struct_t * usart_buffer_t)
{
	/* We have to check is we have woke higher priority task, because we post to
	 * queue and high priority task might be blocked waiting for items appear on
	 * this queue */
	signed char xHigherPriorityTaskWoken = pdFALSE;
	signed char cChar;
	/* Get the character and post it on the queue of Rxed characters.
	If the post causes a task to wake force a context switch as the woken task
	may have a higher priority than the task we have interrupted. */
	cChar = usart_buffer_t->usart->DATA;
	xQueueSendToBackFromISR( usart_buffer_t->xQueueRX, &cChar, &xHigherPriorityTaskWoken );
	return xHigherPriorityTaskWoken;
}

/*! \brief Data Register Empty Interrupt Service Routine.
 *
 *  Data Register Empty Interrupt Service Routine.
 *  Transmits one byte from TX software buffer. Disables DRE interrupt if buffer
 *  is empty. Argument is pointer to USART (USART_struct_t).
 *
 *  \param usart_struct      The USART_struct_t struct instance.
 */
inline signed char USART_DataRegEmpty(USART_buffer_struct_t * usart_struct)
{
	signed char cChar, cTaskWoken;
		if( xQueueReceiveFromISR( usart_struct->xQueueTX, &cChar, &cTaskWoken ) == pdTRUE )
		{
			/* Send the next character queued for Tx. */
			usart_struct->usart->DATA = cChar;
		}
		else
		{
			/* Queue empty, nothing to send. */
		    /* Disable DRE interrupts. */
			uint8_t tempCTRLA = usart_struct->usart->CTRLA;
			tempCTRLA = (tempCTRLA & ~USART_DREINTLVL_gm) | USART_DREINTLVL_OFF_gc;
			usart_struct->usart->CTRLA = tempCTRLA;
		}
	return cTaskWoken;
}

