/*
Name:    ladaduino.ino
Created: 20.11.2016 12:26:05
Author:  StellaLupus
*/
// the setup function runs once when you press reset or power the board
#include <Arduino_FreeRTOS.h>
#include <list.h>
#include <queue.h>
#include <semphr.h>
#include <task.h>
#include <timers.h>

// Declare a mutex Semaphore Handle which we will use to manage the Serial Port.
// It will be used to ensure only only one Task is accessing this resource at any time.
SemaphoreHandle_t xSerialSemaphore;

//Пин подключен к ST_CP входу 74HC595
#define latchPin 8
//Пин подключен к SH_CP входу 74HC595
#define clockPin 12
//Пин подключен к DS входу 74HC595
#define dataPin 9
//Пины кнопок
#define ButtonCount 4
#define button1 2
#define button2 3
#define button3 4
#define button4 5

TickType_t ButtonRelease[ButtonCount];
bool RelayTimeLock[ButtonCount];

//Tasking
void TaskCheckButtons(void *pvParameters);
//void TaskSwitchRelay(void *pvParameters);

//Relay status 1 == off
volatile bool relay[8];
uint8_t buttons[ButtonCount];
uint8_t buttonsstate[4] = { LOW };

void setup() {
	pinMode(latchPin, OUTPUT);
	digitalWrite(latchPin, LOW);
	pinMode(dataPin, OUTPUT);
	pinMode(clockPin, OUTPUT);
	buttons[0] = button1;
	buttons[1] = button2;
	buttons[2] = button3;
	buttons[3] = button4;
	for (int i = 0; i < 4; i++) {
		pinMode(buttons[i], INPUT);
		ButtonRelease[i] = millis();
	}
	for (int i = 0; i < 8; i++) {
		relay[i] = 1;
	}
	Serial.begin(9600);
	while (!Serial) {
		; // wait for serial port to connect. Needed for native USB, on LEONARDO, MICRO, YUN, and other 32u4 based boards.
	}
	if (xSerialSemaphore == NULL)  // Check to confirm that the Serial Semaphore has not already been created.
	{
		xSerialSemaphore = xSemaphoreCreateMutex();  // Create a mutex semaphore we will use to manage the Serial Port
		if ((xSerialSemaphore) != NULL)
			xSemaphoreGive((xSerialSemaphore));  // Make the Serial Port available for use, by "Giving" the Semaphore.
	}

	xTaskCreate(
		TaskCheckButtons
		, (const portCHAR *)"Buttons"
		, 128
		, NULL
		, 1
		, NULL);
}

// the loop function runs over and over again until power down or reset
void loop() {
	// Empty. Things are done in Tasks
}

void TaskCheckButtons(void *pvParameters) {
	(void)pvParameters;
	TickType_t xTimeBefore;
	for (;;) {
		for (uint8_t i = 0; i < 4; i++) {
			if (digitalRead(buttons[i]) != buttonsstate[i]) {
				buttonsstate[i] = !buttonsstate[i];
				ButtonSwitched(i, buttonsstate[i]);
			}
		}
		for (uint8_t i = 0; i < 4; i++) {
			if (RelayTimeLock[i]) {
				if (!(ButtonRelease[i] + 200 > xTaskGetTickCount())) {
					RelayTimeLock[i] = false;
					relay[i] = 1;
				}
			}
		}
		registerWrite();
		vTaskDelay(1);
	}
}

void ButtonSwitched(uint8_t nButton, uint8_t ButState) {
	if (ButState == LOW) {
		ButtonRelease[nButton] = xTaskGetTickCount();
		if(!RelayTimeLock[nButton])
			relay[nButton] = 1;
	}
	else {
		if (nButton == 0 || nButton == 1) {
			RelayTimeLock[0] = false;
			RelayTimeLock[1] = false;
			relay[0] = 1;
			relay[1] = 1;
		}
		else if (nButton == 2 || nButton == 3) {
			RelayTimeLock[2] = false;
			RelayTimeLock[3] = false;
			relay[2] = 1;
			relay[3] = 1;
		}

		if (nButton < 4) {
			if (!(ButtonRelease[nButton] + 8 < xTaskGetTickCount())) {
				relay[nButton] = 0;
				RelayTimeLock[nButton] = true;
				registerWrite();
			}
			else {
				if((nButton == 0 || nButton == 1)&& (!RelayTimeLock[0] && !RelayTimeLock[1]))
					relay[nButton] = 0;
				else if ((nButton == 2 || nButton == 3) && (!RelayTimeLock[2] && !RelayTimeLock[3]))
					relay[nButton] = 0;
			}
		}
	}
}

// Этот метот записывает байт в регистр
void registerWrite() {
	uint8_t sendd = 0;
	for (int i = 0; i < 8; i++) {
		sendd |= (relay[i]) ? (1 << i) : 0;
	}
	if (xSemaphoreTake(xSerialSemaphore, (TickType_t)5) == pdTRUE)
	{
		// We were able to obtain or "Take" the semaphore and can now access the shared resource.
		// We want to have the Serial Port for us alone, as it takes some time to print,
		// so we don't want it getting stolen during the middle of a conversion.
		// print out the state of the button:
		digitalWrite(latchPin, LOW);
		// передаем последовательно на dataPin
		shiftOut(dataPin, clockPin, MSBFIRST, sendd);
		//"защелкиваем" регистр, тем самым устанавливая значения на выходах
		digitalWrite(latchPin, HIGH);

		xSemaphoreGive(xSerialSemaphore); // Now free or "Give" the Serial Port for others.
	}

}