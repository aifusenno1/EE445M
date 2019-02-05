

#include "tm4c123gh6pm.h"
#include "SysTick.h"
#include "ST7735.h"
#include "PLL.h"
#include "LED.h"
#include "Serial.h"

int main(void) {
    PLL_Init(Bus80MHz);                  // set system clock to 80 MHz
    LED_Init();
    Serial_Init();

    ST7735_InitR(INITR_REDTAB);

    LED_GREEN_ON();
    ST7735_FillScreen(ST7735_BLACK);            // set screen to white
	ST7735_SetTextColor(ST7735_WHITE);
    ST7735_Message (0, 0, "Test Message", 0);
    ST7735_Message (0, 1, "Test Message", 0);
    ST7735_Message (0, 2, "Test Message", 0);
    ST7735_Message (0, 3, "", 0);
    ST7735_Message (1, 0, "Test Message1", 0);
    ST7735_Message (1, 1, "Test Message2", 0);
    ST7735_Message (1, 2, "", 0);
    ST7735_Message (1, 3, "Test Message3", 0);
}

