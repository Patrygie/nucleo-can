/*
 * can_config.c
 *
 *   Created on: 27.02.2022
 *  Modified on: 30.07.2022
 *       Author: Krystian Sosin
 *      Version: 1.0.2
 *  Last change: Fix minor bugs and add CAN_AcknowledgeWriteMessage() function.
 */

#include "main.h"
#include "can_config.h"
#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_can.h"
#include "stdbool.h"


/* Variables and arrays needed to proper work of CAN */
CAN_FilterTypeDef    sFilterConfig;
CAN_TxHeaderTypeDef  TxHeader;
CAN_RxHeaderTypeDef  RxHeader;
uint8_t				 TxData[8];
uint8_t				 RxData[8];
uint32_t			 TxMailbox;

/* USER CODE BEGIN Externs */
/* Externs used in configs */
extern bool Write_State1;
extern bool Write_State2;
extern bool Write_State3;
extern uint8_t Write_Data1;

extern uint8_t Read_Data1;
extern uint8_t Read_Data2;
extern uint8_t Read_Data3;
extern uint8_t Read_Data4;
/* USER CODE END Externs */

/** 
 * CAN READ MESSAGE FRAME
 *
 * | RxID  |  2  | 0x3D | ADDR  |
 * | StdID | DLC | READ | RegID |
 *
 *
 */
// Enum of ReadRegs defined in can_config.h. Nothing needs to be configured here.

/* USER CODE BEGIN ResponseMessage */

/**
 *  CAN RESPONSE MESSAGE FRAME
 *
 * | TxID  | DLC | ADDR  | VALUE  | ... | VALUE  |
 * | StdID | DLC | RegID | DATA_1 | ... | DATA_N |
 *
 **/
ResponseMessageFrame ResponseMessage[NUMBER_OF_READ_REGS] =
{
	{
		.Response_DLC        = 2u,                         // Data length of response message
		.Read_ReactionHandler = ReadReactionHandler1,       // Handler of reaction to read request from MCU
		.Response_RegID      = Read_MeaningfulRegName1_ID, // Address of regs which response refers
		//.Response_Data1      = &Read_Data1                 // Returned data to MCU
	},
	{
		.Response_DLC        = 2u,
		// Here should be another Read_ReactionHandler for this RegID
		.Response_RegID      = Read_MeaningfulRegName2_ID,
		//.Response_Data1      = &Read_Data2
	},
	{
		.Response_DLC   	 = 3u,
		// Like above
		.Response_RegID 	 = Read_MeaningfulRegNameN_ID,
		//.Response_Data1 	 = &Read_Data3,
       // .Response_Data2 	 = &Read_Data4
	}
};
/* USER CODE END ResponseMessage */

/* USER CODE BEGIN WriteMessage */

/**
 *  CAN WRITE MESSAGE FRAME
 *
 * | RxID  | DLC | ADDR  | VALUE  | ... | VALUE  |
 * | StdID | DLC | WRITE | DATA_1 | ... | DATA_N |
 *
 **/
WriteMessageFrame WriteMessage[NUMBER_OF_WRITE_REGS] =
{
    // Replace by some more meaningful name of reg
	{
		.Write_RegID           = Write_MeaningfulRegName1_ID, // Reg which should be written by MCU command
		.Write_ReactionHandler = WriteReactionHandler1,       // Handler of reaction to write request from MCU
		//.Write_State           = &Write_State1,               // If this MCU command should change state of sth this pointer should point to variable which regards this state eg. if MCU want to light up brake light, this structure element should point to variable which contain the state of brake lights
       // .Write_Data1           = &Write_Data1                 // Like above - If MCU would like to change sth with specific value eg. passing some date which sould be displayed on steering wheal - this pointer should point to variable which store this specific displayed data on steering wheal
	},
    // Replace by some more meaningful name of reg
	{
		.Write_RegID           = Write_MeaningfulRegName2_ID,
		//.Write_State           = &Write_State2
	},
    // Replace by some more meaningful name of reg
	{
		.Write_RegID           = Write_MeaningfulRegNameN_ID,
		//.Write_State           = &Write_State3,
	}
};
/* USER CODE END WriteMessage */
/**
 *  CAN ERROR MESSAGE FRAME
 *
 * | TxID  |  2  | 0x1D  |    ID   |
 * | StdID | DLC | ERROR | ErrorID |
 *
 */
// Enum of ErrorRegs defined in can_config.h. Nothing needs to be configured here.

/** CAN_Init
 * @brief Function to ensure proper work of CAN interface
          - configuration of filter and calling essantial functions of CAN initialization
          Filter configured in accordance with E&S Team Project Guidlines.
 *
 * @retval None.
 **/
void CAN_Init(void)
{
	sFilterConfig.FilterBank = 1;
	sFilterConfig.FilterMode = CAN_FILTERMODE_IDMASK;
	sFilterConfig.FilterScale = CAN_FILTERSCALE_32BIT;
	sFilterConfig.FilterIdHigh = Rx_ID << 5;
	sFilterConfig.FilterIdLow = 0x0000;
	sFilterConfig.FilterMaskIdHigh = 0xFFFF << 5;
	sFilterConfig.FilterMaskIdLow = 0x0000;
	sFilterConfig.FilterFIFOAssignment = CAN_RX_FIFO0;
	sFilterConfig.FilterActivation = ENABLE;
	sFilterConfig.SlaveStartFilterBank = 14;

	if (HAL_CAN_ConfigFilter(&hcan1, &sFilterConfig) != HAL_OK)
	{
		/* Filter configuration Error */
		Error_Handler();
	}
	if (HAL_CAN_Start(&hcan1) != HAL_OK)
	{
		/* Start Error */
		Error_Handler();
	}
	if (HAL_CAN_ActivateNotification(&hcan1, CAN_IT_RX_FIFO0_MSG_PENDING | CAN_IT_TX_MAILBOX_EMPTY) != HAL_OK)
	{
		/* Notification Error */
		Error_Handler();
	}

	TxHeader.StdId = Tx_ID;
	TxHeader.ExtId = 0x0000;
	TxHeader.RTR = CAN_RTR_DATA;
	TxHeader.IDE = CAN_ID_STD;
	TxHeader.DLC = 8;
	TxHeader.TransmitGlobalTime = DISABLE;
}

/** HAL_CAN_RxFifo0MsgPendingCallback
 * @brief HAL Callback to handle interuption from CAN new message
 * 
 * @param hcan pointer to a CAN_HandleTypeDef structure that contains
 *         the configuration information for the specified CAN.
 *
 * @retval None 
 **/
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
	CAN_Receive(hcan, &RxHeader, RxData);
	CAN_On_Receive(RxData);
}

/** CAN_On_Receive
 * @brief Function to procces received message - checking if message is read or write command and call specific handler
 * 
 * @param RxData pointer to uint8_t array which stores received data
 * 
 * @retval None
 **/
void CAN_On_Receive(uint8_t *RxData)
{
	if(Read_RequestMessage == RxData[ReadMessage])
	{
		CAN_Respond();
	}
	else
	{
		CAN_ProcessWriteCommand();
	}
}

/** CAN_Receive
 * @brief Function to receive data via CAN and, if neccessary, report error of CAN connection
 * 
 * @param CANPointerpointer to a CAN_HandleTypeDef structure that contains
 *                          the configuration information for the specified CAN.
 * @param RxHeader CAN_RxHeaderTypeDef pointer to structure that contains RxHeader configuration.
 * @param RxData uint8_t pointer to array that will contain received data.
 * 
 * @retval None.
 **/
void CAN_Receive(CAN_HandleTypeDef *CANPointer, CAN_RxHeaderTypeDef *RxHeader, uint8_t *RxData)
{
	if(HAL_CAN_GetRxMessage(CANPointer, CAN_RX_FIFO0, RxHeader, RxData) != HAL_OK)
	{
		CANBUS_Error_Handler();
	}
};

/** CAN_Transmit
 * @brief Function to transmit data via CAN and, if neccessary, report error of CAN connection
 * 
 * @param TxHeader CAN_TxHeaderTypeDef pointer to structure that contains TxHeader configuration.
 * @param TxDLC uint8_t variable which contains Date Length of CAN message.
 * @param TxData uint8_t pointer to array that contains data to transmit.
 * @param TxMailbox uint32_t pointer to array that contains whole CAN message to transmit.
 * 
 * @retval None.
 **/
void CAN_Transmit(CAN_TxHeaderTypeDef *TxHeader, uint8_t TxDLC, uint8_t *TxData, uint32_t *TxMailbox)
{
	TxHeader->DLC = TxDLC;
	if(HAL_CAN_AddTxMessage(&hcan1, TxHeader, TxData, TxMailbox) != HAL_OK)
	{
		CANBUS_Error_Handler();
	}
}

/** CAN_Respond
 * @brief Function to respond in connection with read request from MCU
 * 
 * @retval None.
 **/
void CAN_Respond(void)
{
	for (int i = FIRST_ARRAY_ELEMENT; i < NUMBER_OF_READ_REGS; i++)
	{
		if (ResponseMessage[i].Response_RegID == RxData[ReadRegID])
		{
			ResponseMessage[i].Read_ReactionHandler();
		}
	}
}

/** CAN_ProcessWriteCommand
 * @brief Function to process write command
 * 
 * @retval None.
 **/
void CAN_ProcessWriteCommand(void)
{
	for (int i = FIRST_ARRAY_ELEMENT; i < NUMBER_OF_WRITE_REGS; i++)
	{
		if (WriteMessage[i].Write_RegID == RxData[WriteMessage_reg])
		{
			CAN_AcknowledgeWriteMessage(WriteMessage[i].Write_RegID);
			WriteMessage[i].Write_ReactionHandler();
		}
	}
}

/** CAN_AcknowledgeWriteMessage
 * @brief Function to send acknowledment received write instruction via CAN
 * 
 * @param WriteReqID ID of received write instruction
 * 
 * @retval None.
 **/
void CAN_AcknowledgeWriteMessage(WriteRegsID WriteReqID)
{
	TxData[AcknowledgmentMessage_reg] = Write_AcknowledgmentMessage; // 1st Data Byte: Standard Write Acknowledgment instruction 
	TxData[WriteRegID] = WriteReqID;                                 // 2nd Data Byte: Acknowledged Received Write Command ReqID
	CAN_Transmit(&TxHeader, ACKNOWLEDMENT_DLC, TxData, &TxMailbox);  // Transmit Data
}

/** CAN_ReportError
 * @brief Function to report error via CAN
 * 
 * @param ErrorID ID of reported Error Register
 * 
 * @retval None.
 **/
void CAN_ReportError(ErrorRegsID ErrorID)
{
	TxData[ErrorMessage_reg] = Error_ReportMessage;         // 1st Data Byte: Standard Error Report instruction 
	TxData[ErrorRegID] = ErrorID;                           // 2nd Data Byte: Reported Error ID
	CAN_Transmit(&TxHeader, ERROR_DLC, TxData, &TxMailbox); // Transmit Data
}

/* USER CODE BEGIN CANBUS_Error_Handler */

/** CANBUS_Error_Handler
 * @brief General error handler of CAN connection and communication
 * 
 * @retval None.
 * */
void CANBUS_Error_Handler(void)
{
	__disable_irq();
	/*
	Put here behaviour of ECU when error will be occured.
	*/
}
/* USER CODE END ReadReactionHandlers */

/* USER CODE BEGIN ReadReactionHandlers */

/** Add function name
 * Add brief
 **/
void ReadReactionHandler1(void)
{
	// Here you should write the reation to Read request from MCU, e.g. prepare TxData[] with specific values
}
/* USER CODE END ReadReactionHandlers */

/* USER CODE BEGIN WriteReactionHandlers */

/** Add function name
 * Add brief
 **/
void WriteReactionHandler1(void)
{
	// Here you should write the reation to Write request from MCU
}
/* USER CODE END WriteReactionHandlers */
