/*
 * sim800.c
 *
 *  Created on: Nov 5, 2023
 *      Author: Viktor
 */

#include "string.h"
#include "stdlib.h"
#include "sim800.h"


/*
 * Static helpful functions
 * See the functions definitions for more details
 */
static uint8_t add_pending_message(SIM800_Handle_t *handle, char *code, void (*messageHandler)(void*, uint32_t));
static void remove_expected_code(SIM800_Handle_t *handle, uint8_t index);

static SIM800_Status_t send_command(char *cmd);
static SIM800_Status_t validate_message(char *message);

static SIM800_Status_t wait_for_state(SIM800_Handle_t *handle, SIM800_ExpectedCodeState_t state, uint8_t index);

static void cmti_handler(void *handle_ptr, uint32_t msg_index);
static void cmgr_handler(void *handle_ptr, uint32_t msg_index);

static SIM800_Status_t cbc_parser(SIM800_Battery_t *batt, char *message);
static void cmgr_parser(SIM800_SMSMessage_t *sms_message, char *message);


/*********************************************************************************************
 *								Supported user functions
 ********************************************************************************************/

/**
 * @brief   Controls the parameter that determines whether messages from the module are received.
 *
 * This function is used to control the reception of messages from the module. If receiving
 * needs to be started (enordi = 1), it initiates the receiving process. If receiving needs
 * to be stopped (enordi = 0), it stops the receiving process.
 *
 * @param   *handle: Pointer to the handle structure.
 * @param   enordi: ENABLE(1) to start receiving, DISABLE(0) to stop receiving.
 * @retval  SIM800_OK on success, SIM800_ERROR on failure, SIM800_TIMEOUT on timeout.
 * @return  Execution status. For details, refer to the SIM800_Status_t in the corresponding .h file.
 */
SIM800_Status_t SIM800_ManageReceiving(SIM800_Handle_t *handle, uint8_t enordi)
{
	if( enordi )
	{
		if( handle->recStatus == SIM800_DoesntReceive )
		{
			if( HAL_UART_Receive_IT(SIM800_UART, (uint8_t *)&handle->rcvdByte, 1) != HAL_OK )
			{
				return SIM800_ERROR;
			}

			handle->recStatus = SIM800_Receives;
			return SIM800_OK;
		}
	}

	if( handle->recStatus == SIM800_Receives )
	{
		handle->recStatus = SIM800_DoesntReceive;
		return SIM800_OK;
	}

	return SIM800_OK;
}


/**
 * @brief   Retrieves battery information from the SIM800 module.
 *
 * This function sends the "AT+CBC" command to the SIM800 module to retrieve battery information.
 * It then waits for the response, validates the received message, and parses the battery data
 * into the provided `battery` structure.
 *
 * @param   *handle: Pointer to the SIM800 handle structure.
 * @param   *battery: Pointer to the SIM800_Battery_t structure to store battery information.
 * @retval  SIM800_OK on success, SIM800_ERROR on failure, SIM800_TIMEOUT on timeout.
 * @return  Execution status. For details, refer to the SIM800_Status_t in the corresponding .h file.
 */
SIM800_Status_t SIM800_GetBatteryInfo(SIM800_Handle_t *handle, SIM800_Battery_t *battery)
{
	char cmd[] = "AT+CBC\r\n";
	uint8_t index = add_pending_message(handle, "+CBC", NULL);

	if( send_command(cmd) == SIM800_ERROR )
	{
		remove_expected_code(handle, index);
		return SIM800_ERROR;
	}

	if( wait_for_state(handle, SIM800_ReceivedStatus, index) == SIM800_TIMEOUT )
	{
		remove_expected_code(handle, index);
		return SIM800_TIMEOUT;
	}

	if( validate_message(handle->expected_codes[index].data) == SIM800_ERROR)
	{
		remove_expected_code(handle, index);
		return SIM800_ERROR;
	}

	cbc_parser(battery, handle->expected_codes[index].data);

	remove_expected_code(handle, index);

	return  SIM800_OK;
}


/**
 * @brief   Retrieves the status from the SIM800 module.
 *
 * This function sends the "AT" command to the SIM800 module to check its status.
 * It waits for a response and validates the received message.
 *
 * @param   *handle: Pointer to the SIM800 handle structure.
 * @retval  SIM800_OK on success, SIM800_ERROR on failure, SIM800_TIMEOUT on timeout.
 * @return  Execution status. For details, refer to the SIM800_Status_t in the corresponding .h file.
 */
SIM800_Status_t SIM800_GetStatus(SIM800_Handle_t *handle)
{
	char cmd[] = "AT\r\n";
	uint8_t index = add_pending_message(handle, "DUMMY", NULL);

	if( send_command(cmd) == SIM800_ERROR )
	{
		remove_expected_code(handle, index);
		return SIM800_ERROR;
	}

	if( wait_for_state(handle, SIM800_ReceivedStatus, index) == SIM800_TIMEOUT )
	{
		remove_expected_code(handle, index);
		return SIM800_TIMEOUT;
	}

	if( validate_message(handle->expected_codes[index].data) == SIM800_ERROR)
	{
		remove_expected_code(handle, index);
		return SIM800_ERROR;
	}

	return  SIM800_OK;
}


/**
 * @brief   Retrieves the network registration status from the SIM800 module.
 *
 * This function sends the "AT+CREG?" command to the SIM800 module to query its network registration status.
 * It waits for a response, validates the received message, and returns the network registration status.
 *
 * @param   *handle: Pointer to the SIM800 handle structure.
 * @retval  SIM800_NetworkRegStatus_t enumeration value representing the network registration status.
 * @return  Network registration status. For possible values, refer to the SIM800_NetworkRegStatus_t enumeration.
 */
SIM800_NetworkRegStatus_t SIM800_GetNetworkRegStatus(SIM800_Handle_t *handle)
{
	char cmd[] = "AT+CREG?\r\n", *ptr;
	uint8_t index = add_pending_message(handle, "+CREG", NULL);
	SIM800_NetworkRegStatus_t status;

	if( send_command(cmd) == SIM800_ERROR )
	{
		remove_expected_code(handle, index);
		return SIM800_FAIL;
	}

	if( wait_for_state(handle, SIM800_ReceivedStatus, index) == SIM800_TIMEOUT )
	{
		remove_expected_code(handle, index);
		return SIM800_FAIL;
	}

	if( validate_message(handle->expected_codes[index].data) == SIM800_ERROR)
	{
		remove_expected_code(handle, index);
		return SIM800_FAIL;
	}

	ptr = strchr(handle->expected_codes[index].data, ',');
	status = strtol(++ptr, NULL, 10);

	remove_expected_code(handle, index);

	return status;
}


/**
 * @brief   Sets the SMS text mode for the SIM800 module.
 *
 * This function sends the "AT+CMGF=1" command to the SIM800 module to set it to SMS text mode.
 * It then waits for the response, validates the received message, and removes the expected message.
 *
 * @param   *handle: Pointer to the SIM800 handle structure.
 * @retval  SIM800_OK on success, SIM800_ERROR on failure, SIM800_TIMEOUT on timeout.
 * @return  Execution status. For details, refer to the SIM800_Status_t in the corresponding .h file.
 */
SIM800_Status_t SIM800_SetSMSTextMode(SIM800_Handle_t *handle)
{
    char cmd[] = "AT+CMGF=1\r\n";
    uint8_t index = add_pending_message(handle, "+CMGF", NULL);

    if (send_command(cmd) == SIM800_ERROR)
    {
        remove_expected_code(handle, index);
        return SIM800_ERROR;
    }

    if (wait_for_state(handle, SIM800_ReceivedStatus, index) == SIM800_TIMEOUT)
    {
        remove_expected_code(handle, index);
        return SIM800_TIMEOUT;
    }

    if (validate_message(handle->expected_codes[index].data) == SIM800_ERROR)
    {
        remove_expected_code(handle, index);
        return SIM800_ERROR;
    }

    remove_expected_code(handle, index);

    return SIM800_OK;
}


/**
 * @brief   Deletes all SMS messages from the SIM800 module.
 *
 * This function sends the "AT+CMGD=1,4" command to the SIM800 module to delete all SMS messages stored on it.
 * It waits for a response, validates the received message, and returns the status.
 *
 * @param   *handle: Pointer to the SIM800 handle structure.
 * @retval  SIM800_OK on success, SIM800_ERROR on failure, SIM800_TIMEOUT on timeout.
 * @return  Execution status. For details, refer to the SIM800_Status_t enumeration.
 */
SIM800_Status_t SIM800_DeleteAllSMSMessages(SIM800_Handle_t *handle)
{
	char cmd[] = "AT+CMGD=1,4\r\n";
	uint8_t index = add_pending_message(handle, "+CMGD", NULL);

	if (send_command(cmd) == SIM800_ERROR)
	{
		remove_expected_code(handle, index);
		return SIM800_ERROR;
	}

	if (wait_for_state(handle, SIM800_ReceivedStatus, index) == SIM800_TIMEOUT)
	{
		remove_expected_code(handle, index);
		return SIM800_TIMEOUT;
	}

	if (validate_message(handle->expected_codes[index].data) == SIM800_ERROR)
	{
		remove_expected_code(handle, index);
		return SIM800_ERROR;
	}

	remove_expected_code(handle, index);

	return SIM800_OK;
}


/**
 * @brief   Sends an SMS message via the SIM800 module.
 *
 * This function sends an SMS message to the specified destination number using the SIM800 module.
 * It constructs the appropriate AT command, sends the message, and waits for a response.
 * The expected code is removed after the operation.
 *
 * @param   *handle: Pointer to the SIM800 handle structure.
 * @param   *destination: Destination phone number.
 * @param   *message: SMS message to be sent.
 * @retval  SIM800_OK on success, SIM800_ERROR on failure, SIM800_TIMEOUT on timeout.
 * @return  Execution status. For details, refer to the SIM800_Status_t enumeration.
 */
SIM800_Status_t SIM800_SendSMSMessage(SIM800_Handle_t *handle, char *destination, char *message)
{
    char buff[SMS_TX_MAX_LEN];
    // Add an expected code for the +CMGS code
    uint8_t index = add_pending_message(handle, "+CMGS", NULL);

    if (strlen(destination) > (SMS_TX_MAX_LEN - 3) || strlen(message) > (SMS_TX_MAX_LEN - 3))
    {
        // Remove the expected code if the data is too large
        remove_expected_code(handle, index);
        return SIM800_ERROR;
    }

    // Prepare the AT command for sending an SMS
    strcpy(buff, "AT+CMGS=\"");
    strcat(buff, destination);
    strcat(buff, "\"\r\n");

    // Send the command
    if (send_command(buff) == SIM800_ERROR)
    {
        // Remove the expected code in case of failure
        remove_expected_code(handle, index);
        return SIM800_ERROR;
    }

    // Wait after sending the first command
    HAL_Delay(500);

    // Prepare the message for sending and add an end character
    strcpy(buff, message);
    strcat(buff, "\032");

    // Send the SMS message
    if (send_command(buff) == SIM800_ERROR)
    {
        // Send the end character in case of failure
        send_command("\032");
        // Remove the expected code
        remove_expected_code(handle, index);
        return SIM800_ERROR;
    }

    // Wait for a response to the sent SMS
    if (wait_for_state(handle, SIM800_ReceivedStatus, index) == SIM800_TIMEOUT)
    {
        // Send the end character in case of a timeout
        send_command("\032");
        // Remove the expected code
        remove_expected_code(handle, index);
        return SIM800_TIMEOUT;
    }

    // Validate the response message
    if (validate_message(handle->expected_codes[index].data) == SIM800_ERROR)
    {
        // Send the end character in case of an error
        send_command("\032");
        // Remove the expected code
        remove_expected_code(handle, index);
        return SIM800_ERROR;
    }

    // Remove the expected code since the SMS is sent
    remove_expected_code(handle, index);
    return SIM800_OK;
}


/**
 * @brief   Manages SMS notifications from the SIM800 module.
 *
 * This function enables or disables the reception and handling of SMS notifications from the SIM800 module.
 * When enabled, it adds an expected code for incoming SMS notifications to the pending messages.
 * When disabled, it removes the expected code.
 *
 * @param   *handle: Pointer to the SIM800 handle structure.
 * @param   enordi: ENABLE to enable SMS notifications, DISABLE to disable SMS notifications.
 * @retval  SIM800_OK on success, SIM800_ERROR on failure.
 * @return  Execution status. For details, refer to the SIM800_Status_t enumeration.
 */
SIM800_Status_t SIM800_ManageSMSNotifications(SIM800_Handle_t *handle, uint8_t enordi)
{
    static uint32_t index = 0xFFFF;

    if (enordi == ENABLE)
    {
        if (index == 0xFFFF)
        {
            // Add an expected code for incoming SMS notifications
            index = add_pending_message(handle, "+CMTI", &cmti_handler);
            return SIM800_OK;
        }
        return SIM800_ERROR;
    }
    else
    {
        if (index != 0xFFFF)
        {
            // Remove the expected code for incoming SMS notifications
            remove_expected_code(handle, index);
            index = 0xFFFF;
        }
    }

    return SIM800_OK;
}


/**
 * @brief   Requests an SMS message from the SIM800 module.
 *
 * This function sends an AT command to the SIM800 module to request an SMS message by its index.
 *
 * @param   *handle: Pointer to the SIM800 handle structure.
 * @param   sms_index: Index of the SMS message to request.
 * @retval  SIM800_OK on success, SIM800_ERROR on failure.
 */
SIM800_Status_t SIM800_RequestSMSMessage(SIM800_Handle_t *handle, uint32_t sms_index)
{
    char cmd[strlen("AT+CMGR=") + 12];
    char str_sms_index[10];

    // Add an expected +CMGR code and associate it with the cmgr_handler function
    uint8_t index = add_pending_message(handle, "+CMGR", &cmgr_handler);

    // Prepare the AT command to request the SMS message
    strcpy(cmd, "AT+CMGR=");
    itoa(sms_index, str_sms_index, 10);
    strcat(cmd, str_sms_index);
    strcat(cmd, "\r\n");

    // Send the command to request the SMS message
    if (send_command(cmd) == SIM800_ERROR)
    {
        remove_expected_code(handle, index);
        return SIM800_ERROR;
    }

    return SIM800_OK;
}


/**
 * @brief   Handles incoming messages from the SIM800 module.
 *
 * This function is responsible for processing incoming characters from the SIM800 module.
 * It collects received characters and checks for specific responses or expected codes.
 * If an "OK" or "ERROR" response is received, it appends the data to the expected code buffer,
 * sets the received status to SIM800_ReceivedStatus, and invokes the associated handler if available.
 * If a known expected code is received, it stores the data in the corresponding buffer and
 * sets the received status to SIM800_Received. It also invokes the associated handler if available.
 * If none of the expected codes match, it appends the received characters to the current expected code buffer.
 * The function is designed to work with asynchronous UART communication.
 *
 * @param   *handle: Pointer to the SIM800 handle structure.
 */
void SIM800_MessageHandler(SIM800_Handle_t *handle)
{
    uint32_t counter = 0;
    uint8_t flag = 1; // Flag to check if the first part of a response (e.g., +CODE) is received
    size_t data_str_len;

    if (handle->rcvdByte != '\n')
    {
        // Store the received character in the buffer(if buffer is not full)
        if (handle->rxCounter < RX_BUFFER_LENGTH)
        {
            handle->rxBuffer[handle->rxCounter] = handle->rcvdByte;
            handle->rxCounter++;
        }

        // Continue receiving characters
        HAL_UART_Receive_IT(SIM800_UART, (uint8_t *)&handle->rcvdByte, 1);
        return;
    }

    // Append the newline character and terminate the buffer
    handle->rxBuffer[handle->rxCounter] = handle->rcvdByte;
    handle->rxCounter++;
    handle->rxBuffer[handle->rxCounter] = '\0';

    if (!strncmp(handle->rxBuffer, "OK", 2) || !strncmp(handle->rxBuffer, "ERROR", 2))
    {
        // Check if the received response is "OK" or "ERROR"
        data_str_len = strlen(handle->expected_codes[handle->curProccesPacket_index].data);

        // Append the data to the expected code buffer
        strncpy(&handle->expected_codes[handle->curProccesPacket_index].data[data_str_len],
                handle->rxBuffer,
                RECEIVE_DATA_MAX_LENGTH - data_str_len);

        // Set the received status to SIM800_ReceivedStatus
        handle->expected_codes[handle->curProccesPacket_index].state = SIM800_ReceivedStatus;

        // Invoke the associated handler if available
        if (handle->expected_codes[handle->curProccesPacket_index].handle != NULL)
        {
            handle->expected_codes[handle->curProccesPacket_index].handle(handle, handle->curProccesPacket_index);
        }
    }
    else
    {
        for (uint32_t i = 0; i < EXPECTED_CODES_MAX_COUNT; i++)
        {
            if (handle->expected_codes[i].state == SIM800_WaitingFor)
            {
                if (!strncmp(handle->rxBuffer, handle->expected_codes[i].code, handle->expected_codes[i].code_length))
                {
                    // Store the data in the corresponding buffer
                    strcpy(handle->expected_codes[i].data, handle->rxBuffer);

                    // Update the current processed packet index
                    handle->curProccesPacket_index = i;

                    // Set the received status to SIM800_Received
                    handle->expected_codes[i].state = SIM800_Received;

                    flag = 0;

                    // Invoke the associated handler if available
                    if (handle->expected_codes[i].handle != NULL)
                    {
                        handle->expected_codes[i].handle(handle, i);
                    }
                }

                counter++;
                if (counter >= handle->expected_codes_count)
                {
                    break; // Exit the loop when all expected codes have been processed
                }
            }
        }

        if (flag && handle->expected_codes[handle->curProccesPacket_index].state == SIM800_Received &&
            strncmp(handle->expected_codes[handle->curProccesPacket_index].data, "\r\n", 2))
        {
            data_str_len = strlen(handle->expected_codes[handle->curProccesPacket_index].data);

            // Append the received characters to the current expected code buffer
            strncpy(&handle->expected_codes[handle->curProccesPacket_index].data[data_str_len],
                    handle->rxBuffer,
                    RECEIVE_DATA_MAX_LENGTH - data_str_len);
        }
    }

    // Reset the receive counter
    handle->rxCounter = 0;

    // If still in receive status, continue to receive characters
    if (handle->recStatus == SIM800_Receives)
    {
        HAL_UART_Receive_IT(SIM800_UART, (uint8_t *)&handle->rcvdByte, 1);
    }
}


/*********************************************************************************************
 *										Static helpful functions
 ********************************************************************************************/

/**
 * @brief   Adds an expected message code to the SIM800 module handler.
 *
 * This function adds an expected message code to the list of codes that the SIM800 module handler should watch for.
 * It also allows specifying a custom message handler function that will be called when the expected code is received.
 * If the maximum number of expected codes is reached or if the provided code is too long, the function returns 0xFF,
 * indicating an error.
 *
 * @param   *handle: Pointer to the SIM800 handle structure.
 * @param   *code: Expected message code to watch for (e.g., "+CMGS").
 * @param   messageHandler: Custom message handler function to call upon receiving the expected code.
 * @retval  The index under which the code is added or 0xFF on error.
 */
static uint8_t add_pending_message(SIM800_Handle_t *handle, char *code, void (*messageHandler)(void*, uint32_t))
{
    size_t code_len = strlen(code);

    if (code_len > CODE_MAX_LENGTH)
    {
        return 0xFF; // Error: Code is too long
    }

    for (uint32_t i = 0; i < EXPECTED_CODES_MAX_COUNT; i++)
    {
        if (handle->expected_codes[i].state == SIM800_DoesntExpects)
        {
            // Store the code, its length, and the associated message handler (if any)
            strcpy(handle->expected_codes[i].code, code);
            handle->expected_codes[i].code_length = code_len;
            handle->expected_codes[i].handle = messageHandler;
            handle->expected_codes[i].state = SIM800_WaitingFor;

            // Increase the count of expected codes
            handle->expected_codes_count++;

            // Set the current processed packet index
            handle->curProccesPacket_index = i;

            return i; // Return the index under which the code is added
        }
    }

    return 0xFF; // Error: Maximum expected codes count reached
}


/**
 * @brief   Removes an expected message code from the SIM800 module handler.
 *
 * This function removes an expected message code from the list of codes that the SIM800 module handler watches for.
 * It takes the index of the code to remove and clears the corresponding data structure.
 * Additionally, it decreases the count of expected codes.
 *
 * @param   *handle: Pointer to the SIM800 handle structure.
 * @param   index: The index of the expected code to remove.
 */
static void remove_expected_code(SIM800_Handle_t *handle, uint8_t index)
{
    memset(&handle->expected_codes[index], 0, sizeof(handle->expected_codes[index]));
    handle->expected_codes_count--;
}


/**
 * @brief   Sends an AT command to the SIM800 module.
 *
 * This function sends an AT command to the SIM800 module via UART communication.
 * It takes the command string `cmd` and transmits it to the SIM800 module.
 * The function uses an asynchronous UART transmission and returns SIM800_OK on success
 * or SIM800_ERROR in case of a transmission failure.
 *
 * @param   *cmd: Pointer to the AT command string to be sent.
 * @retval  SIM800_OK on success, SIM800_ERROR on failure.
 */
static SIM800_Status_t send_command(char *cmd)
{
    if (HAL_UART_Transmit_IT(SIM800_UART, (uint8_t *)cmd, strlen(cmd)) != HAL_OK)
    {
        return SIM800_ERROR;
    }

    return SIM800_OK;
}


/**
 * @brief   Validates an AT command response message.
 *
 * This function validates an AT command response message received from the SIM800 module.
 * It checks if the response message contains the "OK" string, indicating a successful command execution.
 *
 * @param   *message: Pointer to the response message to be validated.
 * @retval  SIM800_OK if the message contains "OK," SIM800_ERROR otherwise.
 */
static SIM800_Status_t validate_message(char *message)
{
    if (strstr(message, "OK"))
    {
        return SIM800_OK;
    }

    return SIM800_ERROR;
}


/**
 * @brief   Waits for a specific state in the pending message.
 *
 * This function waits for the specified state in the pending message associated with the given index.
 * It uses a timeout mechanism to prevent infinite waiting.
 *
 * @param   *handle: Pointer to the SIM800 handle structure.
 * @param   state: The expected state to wait for in the pending message.
 * @param   index: The index of the expected message code to check.
 * @retval  SIM800_OK if the expected state is reached, SIM800_TIMEOUT if the timeout is reached.
 */
static SIM800_Status_t wait_for_state(SIM800_Handle_t *handle, SIM800_ExpectedCodeState_t state, uint8_t index)
{
    uint32_t tickStart = HAL_GetTick();

    while (handle->expected_codes[index].state != state)
    {
        if (HAL_GetTick() - tickStart > SIM800_MAX_DELAY)
        {
            return SIM800_TIMEOUT;
        }
    }

    return SIM800_OK;
}

/**
 * @brief   Handles the +CMTI notification.
 *
 * This function is called when a +CMTI notification is received from the SIM800 module.
 * It extracts the SMS index from the notification and triggers the corresponding callback function.
 *
 * @param   *handle_ptr: Pointer to the SIM800 handle structure.
 * @param   msg_index: Index of the pending message associated with the +CMTI notification.
 */
static void cmti_handler(void *handle_ptr, uint32_t msg_index)
{
    /*
     * +CMTI: <mem3>,<index>
     */
    SIM800_Handle_t *handle = (SIM800_Handle_t *)handle_ptr;
    char *ptr;
    uint32_t sms_index;

    ptr = strchr(handle->expected_codes[msg_index].data, ',');
    sms_index = strtol(++ptr, NULL, 10);

    handle->expected_codes[msg_index].state = SIM800_WaitingFor;

    SIM800_NewSMSNotificationCallBack(handle, sms_index);
}

/**
 * @brief   Handles the +CMGR response.
 *
 * This function is called when a +CMGR response is received from the SIM800 module.
 * It parses the response message and triggers the corresponding callback function for SMS handling.
 *
 * @param   *handle_ptr: Pointer to the SIM800 handle structure.
 * @param   msg_index: Index of the pending message associated with the +CMGR response.
 */
static void cmgr_handler(void *handle_ptr, uint32_t msg_index)
{
    SIM800_Handle_t *handle = (SIM800_Handle_t *)handle_ptr;
    SIM800_SMSMessage_t message = {0};

    if (handle->expected_codes[msg_index].state != SIM800_ReceivedStatus)
    {
        return;
    }

    cmgr_parser(&message, handle->expected_codes[msg_index].data);

    SIM800_RcvdSMSCallBack(handle, &message);

    remove_expected_code(handle, msg_index);
}


/**
 * @brief   Parses the +CBC response to obtain battery information.
 *
 * This function is called when a +CBC response is received from the SIM800 module.
 * It extracts battery information, including charge status, connection level, and battery level.
 *
 * @param   *batt: Pointer to the SIM800_Battery_t structure to store the battery information.
 * @param   *message: The response message containing battery information.
 * @retval  SIM800_OK if the message is successfully parsed, SIM800_ERROR if an error occurs.
 */
static SIM800_Status_t cbc_parser(SIM800_Battery_t *batt, char *message)
{
    /*
     * +CBC: <bcs>,<bcl>,<voltage>
     */
    char *ptr;

    if (validate_message(message) == SIM800_ERROR)
        return SIM800_ERROR;

    ptr = strchr(message, ':');
    batt->charge_status = strtol(++ptr, NULL, 10);

    ptr = strchr(ptr, ',');
    batt->conection_level = strtol(++ptr, NULL, 10);

    ptr = strchr(ptr, ',');
    batt->battery_level = strtol(++ptr, NULL, 10);

    return SIM800_OK;
}

/**
 * @brief   Parses the +CMGR response to extract SMS message details.
 *
 * This function is called when a +CMGR response is received from the SIM800 module.
 * It extracts SMS message details, including sender information and the message body.
 *
 * @param   *sms_message: Pointer to the SIM800_SMSMessage_t structure to store the SMS message details.
 * @param   *message: The response message containing SMS message details.
 */
static void cmgr_parser(SIM800_SMSMessage_t *sms_message, char *message)
{
    /*
     *  +CMGR: "REC UNREAD","+8613918186089","","02/01/30,20:40:31+00"
     *	This is a test
     *	OK
     */
    char *ptr1, *ptr2;
    size_t len;

    if (validate_message(message) == SIM800_ERROR)
        return;

    ptr1 = strchr(message, '+');
    ptr1 = strchr(++ptr1, '+');

    ptr2 = strchr(ptr1, '"');
    ptr2--;

    len = ptr2 - ptr1;

    strncpy(sms_message->sender, ptr1, len+1);

    ptr1 = strchr(message, '\n');
    ptr1++;

    ptr2 = strchr(ptr1, '\n');

    len = ptr2 - ptr1 - 1;

    strncpy(sms_message->text, ptr1, len);
}


/*********************************************************************************************
 *										Callback functions
 ********************************************************************************************/

/**
 * @brief   User-defined callback for handling new SMS notifications.
 *
 * This function is called when a new SMS notification is received from the SIM800 module.
 * You can override this function to define custom behavior when new SMS notifications arrive.
 *
 * @param   *handle: Pointer to the SIM800 handle structure.
 * @param   sms_index: Index of the new SMS notification.
 */
__weak void SIM800_NewSMSNotificationCallBack(SIM800_Handle_t *handle, uint32_t sms_index)
{
    // Your custom code for handling new SMS notifications can be added here.
}


/**
 * @brief   User-defined callback for handling received SMS messages.
 *
 * This function is called when a new SMS message is successfully received and parsed.
 * You can override this function to define custom behavior for processing received SMS messages.
 *
 * @param   *handle: Pointer to the SIM800 handle structure.
 * @param   *message: Pointer to the parsed SMS message details.
 */
__weak void SIM800_RcvdSMSCallBack(SIM800_Handle_t *handle, SIM800_SMSMessage_t *message)
{
    // Your custom code for handling received SMS messages can be added here.
}
