/******************************************************************************
* File Name:   main.c
*
* Description: This is the source code for CM4 in the the Dual CPU IPC Pipes 
*              Application for ModusToolbox.
*
* Related Document: See README.md
*
*
*******************************************************************************
* Copyright 2020-2024, Cypress Semiconductor Corporation (an Infineon company) or
* an affiliate of Cypress Semiconductor Corporation.  All rights reserved.
*
* This software, including source code, documentation and related
* materials ("Software") is owned by Cypress Semiconductor Corporation
* or one of its affiliates ("Cypress") and is protected by and subject to
* worldwide patent protection (United States and foreign),
* United States copyright laws and international treaty provisions.
* Therefore, you may use this Software only as provided in the license
* agreement accompanying the software package from which you
* obtained this Software ("EULA").
* If no EULA applies, Cypress hereby grants you a personal, non-exclusive,
* non-transferable license to copy, modify, and compile the Software
* source code solely for use in connection with Cypress's
* integrated circuit products.  Any reproduction, modification, translation,
* compilation, or representation of this Software except as specified
* above is prohibited without the express written permission of Cypress.
*
* Disclaimer: THIS SOFTWARE IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, NONINFRINGEMENT, IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. Cypress
* reserves the right to make changes to the Software without notice. Cypress
* does not assume any liability arising out of the application or use of the
* Software or any product or circuit described in the Software. Cypress does
* not authorize its products for use in any products where a malfunction or
* failure of the Cypress product may reasonably be expected to result in
* significant property damage, injury or death ("High Risk Product"). By
* including Cypress's product in a High Risk Product, the manufacturer
* of such system or application assumes all risk of such use and in doing
* so agrees to indemnify Cypress against all liability.
*******************************************************************************/

#include "cy_pdl.h"
#include "cyhal.h"
#include "cybsp.h"
#include "cy_retarget_io.h"

#include "ipc_communication.h"

#include <stdio.h>

/****************************************************************************
* Constants
*****************************************************************************/
#define BTN_MSG_START   1u
#define BTN_MSG_STOP    2u

#define SEND_IPC_MSG(x) ipc_msg.cmd = x; \
                        Cy_IPC_Pipe_SendMessage(USER_IPC_PIPE_EP_ADDR_CM0, \
                                                USER_IPC_PIPE_EP_ADDR_CM4, \
                                                (void *) &ipc_msg, 0);     

/****************************************************************************
* Functions Prototypes
*****************************************************************************/
static void cm4_msg_callback(uint32_t *msg);
static void rx_dma_complete();

/****************************************************************************
* Global Variables
*****************************************************************************/
/* IPC structure to be sent to CM0+ */
static ipc_msg_t ipc_msg = {
    .client_id  = IPC_CM4_TO_CM0_CLIENT_ID,
    .cpu_status = 0,
    .intr_mask  = USER_IPC_PIPE_INTR_MASK,
    .cmd        = IPC_CMD_INIT,
};

/* Message variables */
static volatile bool msg_flag = false;
static volatile uint32_t msg_value;
static volatile uint32_t button_flag;
static volatile bool button_pressed = false;
static volatile bool rx_dma_done = false;

/* MCWDT Object [used by CM0+] */
static cyhal_resource_inst_t mcwdt_0 =
{
    .type = CYHAL_RSC_LPTIMER,
    .block_num = 0
};

#define UART_INTR_NUM        (scb_6_interrupt_IRQn)
#define UART_INTR_PRIORITY   (7U)
cy_stc_scb_uart_context_t uartContext;
const cy_stc_sysint_t uartIntrConfig = {
    .intrSrc = UART_INTR_NUM,
    .intrPriority = UART_INTR_PRIORITY          // Priority level (1 - 3)
};
void UART_Isr(void)
{
    Cy_SCB_UART_Interrupt(SCB6, &uartContext);
}

#ifdef COMPONENT_MW_MTB_HAL_CAT1
#endif
    
int main(void)
{
    cy_rslt_t result;
    cy_en_ipc_pipe_status_t ipc_status;

    /* Init the IPC communication for CM4 */
    setup_ipc_communication_cm4();

    /* Initialize the device and board peripherals */
    result = cybsp_init() ;
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);   
    }

    /* Reserve the MCWDT used by the CM0+ to avoid conflicts if a LPTIMER is 
       to be added in the CM4 */
    result = cyhal_hwmgr_reserve(&mcwdt_0);
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    /* Enable global interrupts */
    __enable_irq();

    result = cy_retarget_io_init(P12_1, P12_0, CY_RETARGET_IO_BAUDRATE);
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }
    printf("i'm ALIVEEEE\r\n");

    const cy_stc_sysint_t intRxDma_cfg =
    {
       .intrSrc      = rx_dma_IRQ,
       .intrPriority = 3u
    };

    Cy_SysInt_Init(&intRxDma_cfg, (cy_israddress) &rx_dma_complete);
    NVIC_EnableIRQ(intRxDma_cfg.intrSrc);

    while (true) {
        printf("Rx DMA done? %x\r\n", rx_dma_done);
        Cy_SysLib_Delay(1000);
    }

}

/*******************************************************************************
* Function Name: cm4_msg_callback
********************************************************************************
* Summary:
*   Callback function to execute when receiving a message from CM0+ to CM4.
*
* Parameters:
*   msg: message received
*
*******************************************************************************/
static void cm4_msg_callback(uint32_t *msg)
{
    ipc_msg_t *ipc_recv_msg;

    if (msg != NULL)
    {
        /* Cast received message to the IPC message structure */
        ipc_recv_msg = (ipc_msg_t *) msg;

        /* Extract the message value */
        msg_value = ipc_recv_msg->value;

        /* Set message flag */
        msg_flag = true;
    }
    
}

void rx_dma_complete()
{
    rx_dma_done = true;
    /* Scenario: Inside the interrupt service routine for block DW0 channel 23: */
    if (CY_DMA_INTR_MASK == Cy_DMA_Channel_GetInterruptStatusMasked(rx_dma_HW, rx_dma_CHANNEL)) {
        Cy_DMA_Channel_ClearInterrupt(rx_dma_HW, rx_dma_CHANNEL);
    }
}

/* [] END OF FILE */
