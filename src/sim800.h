/*
 * sim800.h
 *
 *  Created on: Nov 5, 2023
 *      Author: Viktor
 */

#ifndef INC_SIM800_H_
#define INC_SIM800_H_

#include "main.h"


#define SIM800_MAX_DELAY						32000

#define CODE_MAX_LENGTH							10

#define RECEIVE_DATA_MAX_LENGTH					100

#define RX_BUFFER_LENGTH						100

#define EXPECTED_CODES_MAX_COUNT				10

#define SMS_SENDER_MAX_LEN						20
#define SMS_TEXT_MAX_LEN						100
#define SMS_TX_MAX_LEN							100

extern UART_HandleTypeDef *sim800_uart;
#define SIM800_UART								sim800_uart


/**
 * @brief   Enumeration of SIM800 module operation statuses.
 */
typedef enum
{
    SIM800_OK,       							/*!< Operation completed successfully. */
    SIM800_ERROR,    							/*!< Operation encountered an error. */
    SIM800_TIMEOUT,  							/*!< Operation timed out. */
} SIM800_Status_t;


/**
 * @brief   Enumeration of expected message code states.
 */
typedef enum
{
    SIM800_DoesntExpects,       				/*!< No expectation for pending message. */
    SIM800_WaitingFor,          				/*!< Awaiting response from the module. */
    SIM800_Received,            				/*!< Message received. */
    SIM800_ReceivedSecondPart,  				/*!< Second part of a message received. */
    SIM800_ReceivedStatus,      				/*!< Status message received. */
} SIM800_ExpectedCodeState_t;


/**
 * @brief   Enumeration of receiving statuses.
 *
 * This enumeration defines possible receiving statuses for the SIM800 module.
 */
typedef enum
{
    SIM800_DoesntReceive,  						/*!< Module is not receiving. */
    SIM800_Receives,      						/*!< Module is in receiving state. */
} SIM800_ReceivingStatus_t;


/**
 * @brief   Enumeration of network registration statuses.
 *
 * This enumeration defines possible network registration statuses for the SIM800 module.
 */
typedef enum
{
    SIM800_NotRegistered_DoesntSearch,  		/*!< Not registered, not searching. */
    SIM800_Registered_HomeNetwork,      		/*!< Registered on the home network. */
    SIM800_NotRegistered_Serching,      		/*!< Not registered, searching for network. */
    SIM800_Registration_Deneid,         		/*!< Registration denied. */
    SIM800_Registration_Unknown,        		/*!< Registration status unknown. */
    SIM800_Registered_Roaming,          		/*!< Registered while roaming. */
    SIM800_FAIL,                        		/*!< Registration failed. */
} SIM800_NetworkRegStatus_t;


/**
 * @brief   Structure representing a expected message codes.
 */
typedef struct
{
    char code[CODE_MAX_LENGTH];                 /*!< Message code. */
    size_t code_length;                       	/*!< Length of the message code. */
    char data[RECEIVE_DATA_MAX_LENGTH];         /*!< Message data. */
    SIM800_ExpectedCodeState_t state;         /*!< Message state. */
    void (*handle)(void *, uint32_t);           /*!< Message handler function. */
} SIM800_ExpectedCode_t;


/**
 * @brief   Structure representing the SIM800 module handle.
 */
typedef struct
{
    char rxBuffer[RX_BUFFER_LENGTH];             /*!< Receive buffer. */
    uint32_t rxCounter;                          /*!< Receive counter. */
    char rcvdByte;                               /*!< Received byte. */

    SIM800_ExpectedCode_t expected_codes[EXPECTED_CODES_MAX_COUNT];  /*!< Array of expected codes and messages. */

    size_t expected_codes_count;                  /*!< Count of expected codes. */
    uint32_t curProccesPacket_index;              /*!< Index of the current processing packet. */
    SIM800_ReceivingStatus_t recStatus;           /*!< Receiving status. */
} SIM800_Handle_t;


/**
 * @brief   Structure representing battery charge information.
 *
 * This structure represents battery charge information, including charge status, connection level, and battery voltage.
 */
typedef struct
{
    uint8_t charge_status;     						/*!< Charge status: 0 - Not charging, 1 - Charging, 2 - Charging has finished. */
    uint8_t conection_level;   						/*!< Battery connection level. */
    uint16_t battery_level;    						/*!< Battery voltage (mV). */
} SIM800_Battery_t;


/**
 * @brief   Structure representing SMS message information.
 *
 * This structure represents SMS message information, including sender phone number, and text of this message.
 */
typedef struct
{
    char sender[SMS_SENDER_MAX_LEN];
    char text[SMS_TEXT_MAX_LEN];
} SIM800_SMSMessage_t;




/*********************************************************************************************
 *								Supported user functions
 ********************************************************************************************/

SIM800_Status_t SIM800_ManageReceiving				(SIM800_Handle_t *handle, uint8_t enordi);
SIM800_Status_t SIM800_GetBatteryInfo				(SIM800_Handle_t *handle, SIM800_Battery_t *battery);
SIM800_Status_t SIM800_GetStatus					(SIM800_Handle_t *handle);

SIM800_NetworkRegStatus_t SIM800_GetNetworkRegStatus(SIM800_Handle_t *handle);

SIM800_Status_t SIM800_SetSMSTextMode				(SIM800_Handle_t *handle);
SIM800_Status_t SIM800_DeleteAllSMSMessages			(SIM800_Handle_t *handle);
SIM800_Status_t SIM800_SendSMSMessage				(SIM800_Handle_t *handle, char *destination, char *message);
SIM800_Status_t SIM800_ManageSMSNotifications		(SIM800_Handle_t *handle, uint8_t enordi);
SIM800_Status_t SIM800_RequestSMSMessage			(SIM800_Handle_t *handle, uint32_t sms_index);

void SIM800_MessageHandler							(SIM800_Handle_t *handle);




/*********************************************************************************************
 *										Callback functions
 ********************************************************************************************/

void SIM800_NewSMSNotificationCallBack(SIM800_Handle_t *handle, uint32_t sms_index);
void SIM800_RcvdSMSCallBack(SIM800_Handle_t *handle, SIM800_SMSMessage_t *message);






#endif /* INC_SIM800_H_ */
