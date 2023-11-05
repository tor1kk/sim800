# SIM800 HAL based library for simply SMS applications

The SIM800 Library is a C library for interfacing with SIM800 GSM/GPRS modules. It provides easy-to-use functions for managing various features of the SIM800 module, 
including sending and receiving SMS messages, retrieving battery information, checking network registration status, and more.

## Supported Features

The SIM800 Library supports the following key features:

- **Sending SMS Messages:** Easily send SMS messages using the SIM800 module.
- **Receiving SMS Messages:** Receive and handle SMS messages with notification support.
- **Battery Information:** Retrieve battery information, including charge status and voltage.
- **Network Registration:** Check the network registration status of the SIM800 module.
- **SMS Text Mode:** Set the SIM800 module to SMS text mode for sending and receiving SMS messages.
> To more information about this functions, please visit `sim800.c` and `sim800.h` files in src folder.

## Installation

To use the SIM800 Library in your project, follow these steps:

1. Clone this repository to your local development environment.

2. Include the library in your project by adding the necessary source files to your build system.

3. Initialize the SIM800 module using the provided functions and customize them to suit your needs.

## To use this library in your project, follow these steps:
* Create a handle structure and a pointer to the UART handle structure used with the module.
  ```
  UART_HandleTypeDef *sim800_uart = &huart2;
  
  SIM800_Handle_t sim800h = {0};
  ```
* Call the `SIM800_MessageHandler` function within the `HAL_UART_RxCpltCallback` function.
  
  ```
  void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
  {
	  if(huart == sim800_uart)
	  {
		  SIM800_MessageHandler(&sim800h);
	  }
  }
  ```
* Start receiving data from module
  ```
  SIM800_ManageReceiving(&sim800h, ENABLE);
  ```
* Wait for SIM800 registration in network.
  ```
  while( SIM800_GetNetworkRegStatus(&sim800h) != SIM800_Registered_HomeNetwork &&
         SIM800_GetNetworkRegStatus(&sim800h) != SIM800_Registered_Roaming)
  {
      HAL_Delay(500);
  }
  ```
* Delete all exists SMS messages to free memory(If the memory is full, notifications about new SMS messages will not come.).
  ```
  SIM800_ManageReceiving(&sim800h, ENABLE);
  ```
* Set SMS text mode.
  ```
  SIM800_SetSMSTextMode(&sim800h);
  ```
* Enable SMS notifications callback.
  ```
  SIM800_ManageSMSNotifications(&sim800h, ENABLE);
  ```
* Now you transmit SMS messages:
  ```
  SIM800_SendSMSMessage(&sim800h, PHONE_NUMBER, MESSAGE);
  ```
* Whenever new SMS recived, `SIM800_NewSMSNotificationCallBack` function will be called, there you can use received SMS message index to request this message, then `SIM800_RcvdSMSCallBack` will occur.
  ```
  void SIM800_NewSMSNotificationCallBack(SIM800_Handle_t *handle, uint32_t sms_index)
  {
     SIM800_RequestSMSMessage(handle, sms_index);
  }

  void SIM800_RcvdSMSCallBack(SIM800_Handle_t *handle, SIM800_SMSMessage_t *message)
  {
 
  }
  ```
## Simple example:
  ```
  #include <string.h>
#include <stdio.h>
#include "sim800.h"


UART_HandleTypeDef *sim800_uart = &huart2;

SIM800_Handle_t sim800h = {0};


int main(void)
{
    	SIM800_Battery_t battery = {0};
    	char tx_buff[100];

    	//Start receiving data from module
    	if( SIM800_ManageReceiving(&sim800h, ENABLE) != SIM800_OK )
    	{
		Error_Handler();
	}

	//Wait for sim800 registration in network. Do a little break while iterating.
  	while( SIM800_GetNetworkRegStatus(&sim800h) != SIM800_Registered_HomeNetwork &&
  		   SIM800_GetNetworkRegStatus(&sim800h) != SIM800_Registered_Roaming)
	{
  		HAL_Delay(500);
	}

	//Delete all exists SMS messages to free memory
	if( SIM800_DeleteAllSMSMessages(&sim800h) != SIM800_OK )
	{
		Error_Handler();
	}

	//Set SMS text mode
	if( SIM800_SetSMSTextMode(&sim800h) != SIM800_OK )
	{
		Error_Handler();
	}

	//Enable SMS notifications callback
	if( SIM800_ManageSMSNotifications(&sim800h, ENABLE) != SIM800_OK )
	{
		Error_Handler();
	}

	//Send initial SMS message
	if( SIM800_SendSMSMessage(&sim800h, PHONE_NUMBER, "Ready!") != SIM800_OK )
	{
		Error_Handler();
	}
  while (1)
  {
 

	  //Check if MCU received new message
	  if(flag)
	  {
		  //If received message text is "Batt" receive information about battery and send it.
		  if( !strcmp(sms_buff, "Batt") )
		  {
			  SIM800_GetBatteryInfo(&sim800h, &battery);

			  sprintf(tx_buff, "Battery level: %d\r\nConnection level: %d", battery.battery_level, battery.conection_level);

			  SIM800_SendSMSMessage(&sim800h, PHONE_NUMBER, tx_buff);
		  } else
		  {
			  SIM800_SendSMSMessage(&sim800h, PHONE_NUMBER, "Unknown command!!!");
		  }
		  flag = 0;
	  }
  }
}


void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
	if(huart == sim800_uart)
	{
		SIM800_MessageHandler(&sim800h);
	}
}


void SIM800_NewSMSNotificationCallBack(SIM800_Handle_t *handle, uint32_t sms_index)
{
	SIM800_RequestSMSMessage(handle, sms_index);
}


void SIM800_RcvdSMSCallBack(SIM800_Handle_t *handle, SIM800_SMSMessage_t *message)
{
	strcpy(sms_buff, message->text);

	flag = 1;
}
```
