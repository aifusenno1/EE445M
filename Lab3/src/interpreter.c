
#include <string.h>
#include <stdlib.h>     /* atoi */
#include "ST7735.h"
#include "Serial.h"
#include "LED.h"
#include "OS.h"


static void parse_lcd(char cmd[][20], int len);
static void parse_led(char cmd[][20], int len);

char input[200];

void interpreter(void) {
	while (1) {
		Serial_printf("$ ");

		Serial_InString(input, 30);  // 200 will not work for some reason

		Serial_println("");

		char *c = &input[0];

		char command[10][20];
		memset(command, '\0', sizeof(command));

		int len = 0; // number of tokens in command
		int i = 0;

//		OS_DisableInterrupts();
		while (*c != '\0') {
			if (*c == '"') {
				c++;
				while (*c != '"') {
					if (*c == '\0') {
//						OS_EnableInterrupts();
						Serial_println("Closing quote not found.");
						goto END_OF_LOOP;  // due to error, jump out of the current input
					}
					command[len][i++] = *c++;
				}
			}
			else if (*c == ' ' || *c == '\t') {
				if (i) {  // if i = 0, don't end since no argument
					command[len++][i] = '\0';
					i = 0;
				}
			}
			else {  // add to the current token
				command[len][i++] = *c;
			}
			c++;
		}
//		OS_EnableInterrupts();

		if (command[0][0] == '\0') {
			goto END_OF_LOOP;  // no input
		}

		command[len++][i] = '\0';   // terminate the final token
		command[len][0] = '\0';     // terminate the array with a NULL

//		 lcd output
		if (strcmp(command[0], "lcd") == 0) {
			parse_lcd(command, len);

		}
//		 led
		if (strcmp(command[0], "led") == 0) {
			parse_led(command, len);
		}
		else {
			Serial_println("Unrecognized command.");
		}
		END_OF_LOOP : ;
	}
}

/*
 *
 * len: number of the arguments
 */

static void parse_lcd(char cmd[][20], int len) {
//	Serial_OutUDec(len);
//	long sr = StartCritical();
	if (len == 1) {
		Serial_println("lcd: need at least an argument.");
		return;
	}

	if (!strcmp(cmd[1], "message")) {
		if (len < 5) {
			Serial_println("lcd message: insufficient arguments.");
			return;
		}

		int device = atoi(cmd[2]);
		int line = atoi(cmd[3]);

		// if device arg is not an integer
		if ((strlen(cmd[2]) != 1 || cmd[2][0] != '0') && device == 0) {
			Serial_println("lcd message: incorrect arguments.");
			return;
		}

		if ((strlen(cmd[3]) != 1 || cmd[3][0] != '0') && line == 0) {
			Serial_println("lcd message: incorrect arguments.");
			return;
		}

		ST7735_Message(device, line, cmd[4], 0);
	}
	else {
		Serial_println("Unrecognized argument.");
	}
//	EndCritical(sr);
}



static void parse_led(char cmd[][20], int len) {
//	long sr = StartCritical();
	if (len == 1) {
		Serial_println("led: need at least an argument.");
		return;
	}

	if (!strcmp(cmd[1], "r")) {
		LED_RED_TOGGLE();
	} else if (!strcmp(cmd[1], "g")) {
		LED_GREEN_TOGGLE();
	} else if (!strcmp(cmd[1], "b")) {
		LED_BLUE_TOGGLE();
	}
	else {
		Serial_println("Unrecognized argument.");
	}
//	EndCritical(sr);
}

