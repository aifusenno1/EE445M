// FIFO.c
// Runs on any LM3Sxxx
// Provide functions that initialize a FIFO, put data in, get data out,
// and return the current size.  The file includes a transmit FIFO
// using index implementation and a receive FIFO using pointer
// implementation.  Other index or pointer implementation FIFOs can be
// created using the macros supplied at the end of the file.
// Daniel Valvano
// June 16, 2011

/* This example accompanies the book
   "Embedded Systems: Real Time Interfacing to the Arm Cortex M3",
   ISBN: 978-1463590154, Jonathan Valvano, copyright (c) 2011
   Programs 3.7, 3.8., 3.9 and 3.10 in Section 3.7

 Copyright 2011 by Jonathan W. Valvano, valvano@mail.utexas.edu
    You may use, edit, run or distribute this file
    as long as the above copyright notice remains
 THIS SOFTWARE IS PROVIDED "AS IS".  NO WARRANTIES, WHETHER EXPRESS, IMPLIED
 OR STATUTORY, INCLUDING, BUT NOT LIMITED TO, IMPLIED WARRANTIES OF
 MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE APPLY TO THIS SOFTWARE.
 VALVANO SHALL NOT, IN ANY CIRCUMSTANCES, BE LIABLE FOR SPECIAL, INCIDENTAL,
 OR CONSEQUENTIAL DAMAGES, FOR ANY REASON WHATSOEVER.
 For more information about my classes, my research, and my books, see
 http://users.ece.utexas.edu/~valvano/
 */

#include "FIFO.h"
#include "OS.h"

long StartCritical (void);    // previous I bit, disable interrupts
void EndCritical(long sr);    // restore I bit to previous value


// Two-index implementation of the transmit FIFO
// can hold 0 to TXFIFOSIZE elements
#define TXFIFOSIZE 16 // must be a power of 2
#define TXFIFOSUCCESS 1
#define TXFIFOFAIL    0


unsigned long volatile TxPutI;// put next
unsigned long volatile TxGetI;// get next
txDataType static TxFifo[TXFIFOSIZE];

static Sema4Type TxRoomLeft;
static Sema4Type TxMutex;		// multiple main thread may enter Serial Input routine, make it reentrant
// TxPut is called in foreground threads
// TxGet is called in ISR

// initialize index FIFO
void TxFifo_Init(void){ long sr;
  sr = StartCritical(); // make atomic
  TxPutI = TxGetI = 0;  // Empty
  OS_InitSemaphore(&TxRoomLeft, TXFIFOSIZE);
  OS_InitSemaphore(&TxMutex, 1);
  EndCritical(sr);
}

// add element to end of index FIFO
// return TXFIFOSUCCESS if successful
int TxFifo_Put(txDataType data){
  OS_Wait(&TxRoomLeft);    //  because of this, technically Serial output can only be used in main threads
//  OS_bWait(&TxMutex);
  TxFifo[TxPutI&(TXFIFOSIZE-1)] = data; // put
  TxPutI++;  //  PutI is never wrapped around
//  OS_bSignal(&TxMutex);
  return(TXFIFOSUCCESS);
}
// remove element from front of index FIFO
// return TXFIFOSUCCESS if successful
int TxFifo_Get(txDataType *datapt){
  if(TxPutI == TxGetI ){
    return(TXFIFOFAIL); // Empty if TxPutI=TxGetI
  }
  *datapt = TxFifo[TxGetI&(TXFIFOSIZE-1)];
  TxGetI++;  // GetI is never wrapped around
  OS_Signal(&TxRoomLeft);
  return(TXFIFOSUCCESS);
}
// number of elements in index FIFO
// 0 to TXFIFOSIZE-1
unsigned short TxFifo_Size(void){
 return ((unsigned short)(TxPutI-TxGetI));
}

// Two-pointer implementation of the receive FIFO
// can hold 0 to RXFIFOSIZE-1 elements
#define RXFIFOSIZE 16 // can be any size
#define RXFIFOSUCCESS 1
#define RXFIFOFAIL    0

rxDataType volatile *RxPutPt; // put next
rxDataType volatile *RxGetPt; // get next
rxDataType static RxFifo[RXFIFOSIZE];
static Sema4Type RxDataAva;
static Sema4Type RxMutex;     // multiple threads may call Serial input, so need to implement mutex
// RX Put is called in ISR
// RX Get is called by main thread

// initialize pointer FIFO
void RxFifo_Init(void){ long sr;
  sr = StartCritical();      // make atomic
  RxPutPt = RxGetPt = &RxFifo[0]; // Empty
  OS_InitSemaphore(&RxDataAva, 0);
  OS_InitSemaphore(&RxMutex, 1);
  EndCritical(sr);
}
// add element to end of pointer FIFO
// return RXFIFOSUCCESS if successful
int RxFifo_Put(rxDataType data){
  rxDataType volatile *nextPutPt;
  nextPutPt = RxPutPt+1;
  if(nextPutPt == &RxFifo[RXFIFOSIZE]){
    nextPutPt = &RxFifo[0];  // wrap
  }
  if(nextPutPt == RxGetPt){
    return(RXFIFOFAIL);      // Failed, fifo full; Since cannot wait in Put
  }
  else{
    *(RxPutPt) = data;       // Put
    RxPutPt = nextPutPt;     // Success, update
    OS_Signal(&RxDataAva);
    return(RXFIFOSUCCESS);
  }
}
// remove element from front of pointer FIFO
// return RXFIFOSUCCESS if successful
int RxFifo_Get(rxDataType *datapt){
  OS_Wait(&RxDataAva);
  OS_bWait(&RxMutex);
  *datapt = *(RxGetPt++);
  if(RxGetPt == &RxFifo[RXFIFOSIZE]){
     RxGetPt = &RxFifo[0];   // wrap
  }
  OS_bSignal(&RxMutex);
  return(RXFIFOSUCCESS);
}
// number of elements in pointer FIFO
// 0 to RXFIFOSIZE-1
unsigned short RxFifo_Size(void){
  if(RxPutPt < RxGetPt){
    return ((unsigned short)(RxPutPt-RxGetPt+(RXFIFOSIZE*sizeof(rxDataType)))/sizeof(rxDataType));
  }
  return ((unsigned short)(RxPutPt-RxGetPt)/sizeof(rxDataType));
}
