

#include "tm4c123gh6pm.h"
#include "SysTick.h"
#include "ST7735.h"
#include "PLL.h"
#include "LED.h"
#include "Serial.h"
#include "interpreter.h"
#include "OS.h"


void dummy() {}


int main(void) {
    PLL_Init(Bus80MHz);                  // set system clock to 80 MHz
    LED_Init();
    Serial_Init();
    SysTick_Init();
    ST7735_InitR(INITR_REDTAB);

    ST7735_FillScreen(ST7735_BLACK);            // set screen to white
	ST7735_SetTextColor(ST7735_WHITE);

	OS_AddPeriodicThread(dummy, 100000, 4);
	SysTick_Wait10ms(10);
	ST7735_Message(0,0, "", OS_ReadPeriodicTime());
	SysTick_Wait10ms(10);
	Serial_println("%s %u", "The periodic time is ", OS_ReadPeriodicTime());

	interpreter();
}
