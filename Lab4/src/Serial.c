#include <stdarg.h>
#include "tm4c123gh6pm.h"

#include "FIFO.h"
#include "Serial.h"
#include "LED.h"
//#include <stdio.h>
#include "eFile.h"


#define NVIC_EN0_INT5           0x00000020  // Interrupt 5 enable

#define UART_FR_RXFF            0x00000040  // UART Receive FIFO Full
#define UART_FR_TXFF            0x00000020  // UART Transmit FIFO Full
#define UART_FR_RXFE            0x00000010  // UART Receive FIFO Empty
#define UART_LCRH_WLEN_8        0x00000060  // 8 bit word length
#define UART_LCRH_FEN           0x00000010  // UART Enable FIFOs
#define UART_CTL_UARTEN         0x00000001  // UART Enable
#define UART_IFLS_RX1_8         0x00000000  // RX FIFO >= 1/8 full
#define UART_IFLS_TX1_8         0x00000000  // TX FIFO <= 1/8 full
#define UART_IM_RTIM            0x00000040  // UART Receive Time-Out Interrupt
                                            // Mask
#define UART_IM_TXIM            0x00000020  // UART Transmit Interrupt Mask
#define UART_IM_RXIM            0x00000010  // UART Receive Interrupt Mask
#define UART_RIS_RTRIS          0x00000040  // UART Receive Time-Out Raw
                                            // Interrupt Status
#define UART_RIS_TXRIS          0x00000020  // UART Transmit Raw Interrupt
                                            // Status
#define UART_RIS_RXRIS          0x00000010  // UART Receive Raw Interrupt
                                            // Status
#define UART_ICR_RTIC           0x00000040  // Receive Time-Out Interrupt Clear
#define UART_ICR_TXIC           0x00000020  // Transmit Interrupt Clear
#define UART_ICR_RXIC           0x00000010  // Receive Interrupt Clear


#define FIFOSIZE   16         // size of the FIFOs (must be power of 2)
#define FIFOSUCCESS 1         // return value on success
#define FIFOFAIL    0         // return value on failure
                              // create index implementation FIFO (see FIFO.h)

int outstream;
Sema4Type output_lock;

// Initialize UART0
// Baud rate is 115200 bits/sec
void Serial_Init(void){
  SYSCTL_RCGCUART_R |= 0x01;            // activate UART0
  SYSCTL_RCGCGPIO_R |= 0x01;            // activate port A
  RxFifo_Init();                        // initialize empty FIFOs
  TxFifo_Init();
  UART0_CTL_R &= ~UART_CTL_UARTEN;      // disable UART
  UART0_IBRD_R = 43;                    // IBRD = int(80,000,000 / (16 * 115,200)) = int(43.4027)
  UART0_FBRD_R = 26;                     // FBRD = int(0.4027 * 64 + 0.5) = 26.2728
                                        // 8 bit word length (no parity bits, one stop bit, FIFOs)
  UART0_LCRH_R = (UART_LCRH_WLEN_8|UART_LCRH_FEN);
  UART0_IFLS_R &= ~0x3F;                // clear TX and RX interrupt FIFO level fields
                                        // configure interrupt for TX FIFO <= 1/8 full
                                        // configure interrupt for RX FIFO >= 1/8 full
  UART0_IFLS_R += (UART_IFLS_TX1_8|UART_IFLS_RX1_8);
                                        // enable TX and RX FIFO interrupts and RX time-out interrupt
  UART0_IM_R |= (UART_IM_RXIM|UART_IM_TXIM|UART_IM_RTIM);
  UART0_CTL_R |= 0x301;                 // enable UART
  GPIO_PORTA_AFSEL_R |= 0x03;           // enable alt funct on PA1-0
  GPIO_PORTA_DEN_R |= 0x03;             // enable digital I/O on PA1-0
                                        // configure PA1-0 as UART
  GPIO_PORTA_PCTL_R = (GPIO_PORTA_PCTL_R&0xFFFFFF00)+0x00000011;
  GPIO_PORTA_AMSEL_R = 0;               // disable analog functionality on PA
  NVIC_PRI1_R = (NVIC_PRI1_R&0xFFFF00FF)|0x00004000; // bits 13-15  UART0 = priority 2
  NVIC_EN0_R = NVIC_EN0_INT5;           // enable interrupt 5 in NVIC
  outstream = UART_STREAM;
  OS_InitSemaphore(&output_lock, 1);

}

// copy from hardware RX FIFO to software RX FIFO
// stop when hardware RX FIFO is empty or software RX FIFO is full
void static copyHardwareToSoftware(void){
  char letter;
  while(((UART0_FR_R&UART_FR_RXFE) == 0) && (RxFifo_Size() < (FIFOSIZE - 1))){
    letter = UART0_DR_R;
    RxFifo_Put(letter);
  }
}
// copy from software TX FIFO to hardware TX FIFO
// stop when software TX FIFO is empty or hardware TX FIFO is full
/*
 * copySoftwareToHardware needs to be atomic
 * when it is called in Serial_OutChar, it may be preempted by UART0_Handler, which may modify FIFO
 * when it is called in UART0_Handler, it is unclear why this is required, but adding this fix the output issue.
 * No background thread that can preempt UART0_Handler will use the serial port, yet problem still exists,
 * even setting UART0's priority to the highest wouldn't solve the issue. But disabling interrupt magically solves the issue.
 */
void static copySoftwareToHardware(void){
	OS_DisableInterrupts();
	char letter;
	while(((UART0_FR_R&UART_FR_TXFF) == 0) && (TxFifo_Size() > 0)){
		TxFifo_Get(&letter);  // due to while loop condition, never return 0
		UART0_DR_R = letter;
	}
	OS_EnableInterrupts();
}
// input ASCII character from UART
// spin if RxFifo is empty
char Serial_InChar(void){
  char letter;
  while(RxFifo_Get(&letter) == FIFOFAIL){};
  return(letter);
}
// output ASCII character to UART
// spin if TxFifo is full
void Serial_OutChar(char data){
  while(TxFifo_Put(data) == FIFOFAIL){};
  UART0_IM_R &= ~UART_IM_TXIM;          // disable TX FIFO interrupt does not affect TXRIS, so if UART0_Handler is triggered due to other reason, TxFIFO could stil be modified
  copySoftwareToHardware();
  UART0_IM_R |= UART_IM_TXIM;           // enable TX FIFO interrupt is a must, since UART0_Handler potentially disables it
}
// at least one of three things has happened:
// hardware TX FIFO goes from 3 to 2 or less items
// hardware RX FIFO goes from 1 to 2 or more items
// UART receiver has timed out
void UART0_Handler(void){
  if(UART0_RIS_R&UART_RIS_TXRIS){       // hardware TX FIFO <= 2 items
    UART0_ICR_R = UART_ICR_TXIC;        // acknowledge TX FIFO
    // copy from software TX FIFO to hardware TX FIFO
    copySoftwareToHardware();
    if(TxFifo_Size() == 0){             // software TX FIFO is empty
      UART0_IM_R &= ~UART_IM_TXIM;      // disable TX FIFO interrupt if empty, TXRIS is set as long as hardware TX FIFO <= 2 items
  	  	  	  							// if don't disable, interrupt may be requested all the time (since nothing can be put into hardware TX FIFO
    }
  }
  if(UART0_RIS_R&UART_RIS_RXRIS){       // hardware RX FIFO >= 2 items
    UART0_ICR_R = UART_ICR_RXIC;        // acknowledge RX FIFO
    // copy from hardware RX FIFO to software RX FIFO
    copyHardwareToSoftware();
  }
  if(UART0_RIS_R&UART_RIS_RTRIS){       // receiver timed out
    UART0_ICR_R = UART_ICR_RTIC;        // acknowledge receiver time out
    // copy from hardware RX FIFO to software RX FIFO
    copyHardwareToSoftware();
  }
}

//------------Serial_InUDec------------
// InUDec accepts ASCII input in unsigned decimal format
//     and converts to a 32-bit unsigned number
//     valid range is 0 to 4294967295 (2^32-1)
// Input: none
// Output: 32-bit unsigned number
// If you enter a number above 4294967295, it will return an incorrect value
// Backspace will remove last digit typed
uint32_t Serial_InUDec(void){
	uint32_t number=0, length=0;
	char character;
	character = Serial_InChar();
	while(character != CR){ // accepts until <enter> is typed
		// The next line checks that the input is a digit, 0-9.
		// If the character is not 0-9, it is ignored and not echoed
		if((character>='0') && (character<='9')) {
			number = 10*number+(character-'0');   // this line overflows if above 4294967295
			length++;
			Serial_OutChar(character);
		}
		// If the input is a backspace, then the return number is
		// changed and a backspace is outputted to the screen
		else if((character==BS) && length){
			number /= 10;
			length--;
			Serial_OutChar(character);
		}
		character = Serial_InChar();
	}
	return number;
}


//---------------------Serial_InUHex----------------------------------------
// Accepts ASCII input in unsigned hexadecimal (base 16) format
// Input: none
// Output: 32-bit unsigned number
// No '$' or '0x' need be entered, just the 1 to 8 hex digits
// It will convert lower case a-f to uppercase A-F
//     and converts to a 16 bit unsigned number
//     value range is 0 to FFFFFFFF
// If you enter a number above FFFFFFFF, it will return an incorrect value
// Backspace will remove last digit typed
uint32_t Serial_InUHex(void){
	uint32_t number=0, digit, length=0;
	char character;
	character = Serial_InChar();
	while(character != CR){
		digit = 0x10; // assume bad
		if((character>='0') && (character<='9')){
			digit = character-'0';
		}
		else if((character>='A') && (character<='F')){
			digit = (character-'A')+0xA;
		}
		else if((character>='a') && (character<='f')){
			digit = (character-'a')+0xA;
		}
		// If the character is not 0-9 or A-F, it is ignored and not echoed
		if(digit <= 0xF){
			number = number*0x10+digit;
			length++;
			Serial_OutChar(character);
		}
		// Backspace outputted and return value changed if a backspace is inputted
		else if((character==BS) && length){
			number /= 0x10;
			length--;
			Serial_OutChar(character);
		}
		character = Serial_InChar();
	}
	return number;
}



//------------Serial_InString------------
// Accepts ASCII characters from the serial port
//    and adds them to a string until <enter> is typed
//    or until max length of the string is reached.
// It echoes each character as it is inputted.
// If a backspace is inputted, the string is modified
//    and the backspace is echoed
// terminates the string with a null character
// uses busy-waiting synchronization on RDRF
// Input: pointer to empty buffer, size of buffer
// Output: Null terminated string
// -- Modified by Agustinus Darmawan + Mingjie Qiu --
void Serial_InString(char *bufPt, uint16_t max) {
//	OS_bWait(&serial_lock);

	int length=0;
	char character;
	character = Serial_InChar();
	while(character != CR){
		if(character == BS){
			if(length){
				bufPt--;
				length--;
				Serial_OutChar(BS);
			}
		}
		else if(length < max){
			*bufPt = character;
			bufPt++;
			length++;
			Serial_OutChar(character);
		}
		character = Serial_InChar();
	}
	*bufPt = 0;
//	OS_bSignal(&serial_lock);
}

typedef struct __FILE {
	int dummy;
} FILE;


FILE __stdout; // supposed to be able to access self-defined members in __FILE, but cannot for some reason

int fputc(int ch, FILE *f) {
	switch (outstream) {
	case UART_STREAM :
		Serial_OutChar(ch);
		break;
	case FILE_STREAM :
		if (eFile_Write(ch) != EFILE_SUCCESS) {
			eFile_EndRedirectToFile();
			return 1;  // not sure what to return
		}
		break;
	}

	return ch;
}


// Output String (NULL termination)
// Input: pointer to a NULL-terminated string to be transferred
// Output: none
void outString(char *pt){
  while(*pt){
	  fputc(*pt, &__stdout);
    pt++;
  }
}


// Output a 32-bit number in unsigned decimal format
// Input: 32-bit number to be transferred
// Output: none
// Variable format 1-10 digits with no space before or after
void outUDec(uint32_t n){
	// This function uses recursion to convert decimal number
	//   of unspecified length as an ASCII string
	if(n >= 10){
		outUDec(n/10);
		n = n%10;
	}
	fputc(n+'0', &__stdout); /* n is between 0 and 9 */
}

// Output a 32-bit number in unsigned hexadecimal format
// Input: 32-bit number to be transferred
// Output: none
// Variable format 1 to 8 digits with no space before or after
void outUHex(uint32_t number){
	// This function uses recursion to convert the number of
	//   unspecified length as an ASCII string
	if(number >= 0x10){
		outUHex(number/0x10);
		outUHex(number%0x10);
	}
	else{
		if(number < 0xA){
			fputc(number+'0', &__stdout);
		}
		else{
			fputc((number-0x0A)+'A', &__stdout);
		}
	}
}


void printf (const char *format, ...) {
	OS_bWait(&output_lock);
	va_list ap;
	va_start(ap, format);

	while (*format != '\0') {
		if (*format == '%') {
			format++;
			switch (*format) {
			case 'u' :
			case 'U' :
				outUDec(va_arg(ap, uint32_t));
				break;
			case 's' :
			case 'S' :
				outString(va_arg(ap, char *));
				break;
			case 'c' :
			case 'C' :
				fputc(va_arg(ap, int), &__stdout);
				break;
			case 'x' :
			case 'X' :
				outUHex(va_arg(ap, uint32_t));
				break;
			}
		}
		else {
//			if (*format == '\n') {
//				fputc(LF, &__stdout);
//				fputc(CR, &__stdout);
//			}
//			else
				fputc(*format, &__stdout);
		}
		format++;
	}
	va_end(ap);
	OS_bSignal(&output_lock);

}



