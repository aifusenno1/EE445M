
#include <string.h>
#include <stdlib.h>     /* atoi */
#include "ST7735.h"
#include "Serial.h"
#include "LED.h"
#include "OS.h"
#include "ff.h"
#include "heap.h"
#include "loader.h"
#include "sysent.h"


static void parse_lcd(char cmd[][20], int len);
static void parse_led(char cmd[][20], int len);
static void parse_jitter(char cmd[][20], int len);
static void parse_ls(char cmd[][20], int len);
static void parse_format(char cmd[][20], int len);
static void parse_cat(char cmd[][20], int len);
static void parse_rm(char cmd[][20], int len);
static void parse_malloc(char cmd[][20], int len);
static void parse_free(char cmd[][20], int len);
static void parse_load(char cmd[][20], int len);
void parse_dg(char cmd[][20], int len);


char input[200];
void * pt = 0;

static const ELFSymbol_t exports[] = {
		{ "ST7735_Message", ST7735_Message }
};

static const ELFEnv_t env = { exports, sizeof(exports) / sizeof(*exports) };

void interpreter(void) {
	while (1) {
		Serial_printf("$ ");
		Serial_InString(input, 30);  // 200 will not work for some reason

		Serial_printf("\n\r");

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
						Serial_printf("Closing quote not found.\n\r");
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

		// lcd output
		if (strcmp(command[0], "lcd") == 0) {
			parse_lcd(command, len);

		}
		// led
		else if (strcmp(command[0], "led") == 0) {
			parse_led(command, len);
		}

		else if (strcmp(command[0], "jitter") == 0) {
			parse_jitter(command, len);
		}

//		display directory
//		else if (strcmp(command[0], "ls") == 0) {
//			parse_ls(command, len);
//		}

//		else if (strcmp(command[0], "format") == 0) {
//			parse_format(command, len);
//		}

		else if (strcmp(command[0], "cat") == 0) {
			parse_cat(command, len);
		}

//		else if (strcmp(command[0], "rm") == 0) {
//			parse_rm(command,  len);
//		}

		else if (strcmp(command[0], "malloc") == 0) {
			parse_malloc(command,  len);
		}

		else if (strcmp(command[0], "free") == 0) {
			parse_free(command,  len);
		}


		else if (strcmp(command[0], "load") == 0) {
			parse_load(command,  len);
		}


		else if (strcmp(command[0], "dg") == 0) {
			parse_dg(command,  len);
		}

		else {
			Serial_printf("Unrecognized command.\n\r");
		}
		END_OF_LOOP : ;
	}
}

/*
 *
 * len: number of the arguments
 */

static void parse_lcd(char cmd[][20], int len) {
	if (len == 1) {
		Serial_printf("lcd: need at least an argument.\n\r");
		return;
	}

	if (!strcmp(cmd[1], "message")) {
		if (len < 5) {
			Serial_printf("lcd message: insufficient arguments.\n\r");
			return;
		}

		int device = atoi(cmd[2]);
		int line = atoi(cmd[3]);

		// if device arg is not an integer
		if ((strlen(cmd[2]) != 1 || cmd[2][0] != '0') && device == 0) {
			Serial_printf("lcd message: incorrect arguments.\n\r");
			return;
		}

		if ((strlen(cmd[3]) != 1 || cmd[3][0] != '0') && line == 0) {
			Serial_printf("lcd message: incorrect arguments.\n\r");
			return;
		}

		ST7735_Message(device, line, cmd[4], 0);
	}
	else {
		Serial_printf("Unrecognized argument.\n\r");
	}
}



static void parse_led(char cmd[][20], int len) {
	if (len == 1) {
		Serial_printf("led: need at least an argument.\n\r");
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
		Serial_printf("Unrecognized argument.\n\r");
	}
}


extern unsigned long jitter1Histogram[JITTERSIZE];

static void parse_jitter(char cmd[][20], int len) {
	for (int i =0; i < JITTERSIZE; i++) {
		Serial_printf("%u %u\n\r", i, jitter1Histogram[i] );
	}
}


//static void parse_ls(char cmd[][20], int len) {
//
//}

//static void parse_format(char cmd[][20], int len) {
//	if (eFile_Format()) {
//		printf("format: failed.\n\r");
//	}
//}

static void parse_cat(char cmd[][20], int len) {
	if (len == 1) {
		Serial_printf("cat: no file specified.\n\r");
		return;
	}

	FIL handle;
	FRESULT res;
	UINT successfulreads;

	res = f_open(&handle, cmd[1], FA_READ);
	if (res != FR_OK) {
		Serial_printf("cat: open error: %u\n\r", res);
		return;
	}

	char ch;
	res = f_read(&handle, &ch, 1, &successfulreads);
	while (res == FR_OK && successfulreads == 1) {
		Serial_printf("%c", ch);
		res = f_read(&handle, &ch, 1, &successfulreads);
	}

	res = f_close(&handle);
	if (res != FR_OK) {
		Serial_printf("cat: close failed.\n\r");
		return;
	}

}

//static void parse_rm(char cmd[][20], int len) {
//	if (len == 1) {
//		Serial_printf("rm: no file name.\n\r");
//		return;
//	}
//
//	if (eFile_Delete(cmd[1])) {
//		Serial_printf("rm: delete failed.\n\r");
//		return;
//	}
//}

static void parse_malloc(char cmd[][20], int len) {
	if (len == 1) {
		Serial_printf("malloc: no size specified.\n\r");
		return;
	}

	if (pt) {
		Serial_printf("malloc: already allocated.\n\r");
		return;
	}

	uint32_t s = atoi(cmd[1]);
	pt = Heap_Malloc(s);
	if (pt == 0) {
		Serial_printf("malloc: allocation failed.\n\r");
	}
}

static void parse_free(char cmd[][20], int len) {
	if (Heap_Free(pt)) {
		Serial_printf("free: failed.\n\r");
		return;
	}
	pt = 0;
}

static void parse_load(char cmd[][20], int len) {
	if (len < 2) {
		Serial_printf("load: missing argument.\n\r");
		return;
	}
//	LED_RED_TOGGLE();
	uint32_t startTime = OS_MsTime();
	if (exec_elf(cmd[1], &env) != 1) {
		Serial_printf("load: exec_elf error.\n\r");
		return;
	}
//	LED_RED_TOGGLE();
//	uint32_t t = OS_MsTime() - startTime;
//	Serial_printf("load: process creation time is %u ms\r\n", t);
}

void parse_dg(char cmd[][20], int len) {


}
