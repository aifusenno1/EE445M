#include "FIFO.h"
#include "LED.h"
#include "OS.h"

// Two-index implementation of the transmit FIFO
// can hold 0 to TXFIFOSIZE elements
#define TXFIFOSIZE 16 // must be a power of 2
#define TXFIFOSUCCESS 1
#define TXFIFOFAIL    0


unsigned long volatile TxPutI;// put next
unsigned long volatile TxGetI;// get next
txDataType static TxFifo[TXFIFOSIZE];
static Sema4Type TxRoomLeft;

// initialize index FIFO
void TxFifo_Init(void){ long sr;
  sr = StartCritical(); // make atomic
  TxPutI = TxGetI = 0;  // Empty
  EndCritical(sr);
}
// add element to end of index FIFO
// return TXFIFOSUCCESS if successful
int TxFifo_Put(txDataType data){
//  OS_DisableInterrupts();
  if((TxPutI-TxGetI) & ~(TXFIFOSIZE-1)){
//	OS_EnableInterrupts();
    return(TXFIFOFAIL); // Failed, fifo full
  }
  TxFifo[TxPutI&(TXFIFOSIZE-1)] = data; // put
  TxPutI++;  // Success, update
//  OS_EnableInterrupts();
  return(TXFIFOSUCCESS);
}
// remove element from front of index FIFO
// return TXFIFOSUCCESS if successful
int TxFifo_Get(txDataType *datapt){
  if(TxPutI == TxGetI ){
    return(TXFIFOFAIL); // Empty if TxPutI=TxGetI
  }
  *datapt = TxFifo[TxGetI&(TXFIFOSIZE-1)];
  TxGetI++;  // Success, update
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

// initialize pointer FIFO
void RxFifo_Init(void){ long sr;
  sr = StartCritical();      // make atomic
  RxPutPt = RxGetPt = &RxFifo[0]; // Empty
  OS_InitSemaphore(&RxDataAva, 0);
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
    return(RXFIFOFAIL);       // Failed, fifo full; Since cannot wait here (in ISR)
  }
  else{
    *(RxPutPt) = data;       // Put
    RxPutPt = nextPutPt;     // Success, update
//    OS_Signal(&RxDataAva);
    return(RXFIFOSUCCESS);
  }
}
// remove element from front of pointer FIFO
// return RXFIFOSUCCESS if successful
int RxFifo_Get(rxDataType *datapt){
//	OS_Wait(&RxDataAva);
  if(RxPutPt == RxGetPt){
    return(RXFIFOFAIL);      // Empty if PutPt=GetPt
  }
  *datapt = *(RxGetPt++);
  if(RxGetPt == &RxFifo[RXFIFOSIZE]){
     RxGetPt = &RxFifo[0];   // wrap
  }
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
