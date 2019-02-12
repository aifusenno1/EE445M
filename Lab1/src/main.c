

#include "tm4c123gh6pm.h"
#include "SysTick.h"
#include "ST7735.h"
#include "PLL.h"
#include "LED.h"
#include "Serial.h"
#include "interpreter.h"


int main(void) {
    PLL_Init(Bus80MHz);                  // set system clock to 80 MHz
    LED_Init();
    Serial_Init();

    ST7735_InitR(INITR_REDTAB);

    ST7735_FillScreen(ST7735_BLACK);            // set screen to white
	ST7735_SetTextColor(ST7735_WHITE);

	interpreter();
}
