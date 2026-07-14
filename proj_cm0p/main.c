#include "cyhal.h"
#include "cybsp.h"
#include "cy_retarget_io.h"

#include "cyabs_rtos.h"
#include <FreeRTOS.h>
#include <task.h>

#include "tcp_server.h"


/*******************************************************************************
* Macros
********************************************************************************/
/* RTOS related macros for TCP server task. */
#define TCP_SERVER_TASK_STACK_SIZE                (1024 * 5)
#define TCP_SERVER_TASK_PRIORITY                  (1)

/* Queue lengths of message queues used in this project */
#define SINGLE_ELEMENT_QUEUE                      (1u)

/********************************************************************************
* Global Variables
********************************************************************************/
/* Queue handler */
cy_queue_t led_command_q;

/* This enables RTOS aware debugging. */
volatile int uxTopUsedPriority;

int main()
{
    cy_rslt_t result;

    /* This enables RTOS aware debugging in OpenOCD. */
    uxTopUsedPriority = configMAX_PRIORITIES - 1;

    result = cybsp_init() ;
    CY_ASSERT(result == CY_RSLT_SUCCESS) ;

    /* Enable global interrupts. */
    __enable_irq();

    // Redirect stdin and stdout to the debug UART port
    cy_retarget_io_init(CYBSP_DEBUG_UART_TX, CYBSP_DEBUG_UART_RX,
        CY_RETARGET_IO_BAUDRATE);

    /* \x1b[2J\x1b[;H - ANSI ESC sequence to clear screen. */
    printf("\x1b[2J\x1b[;H");
    printf("===============================================================\n");
    printf("CE229153 - Connectivity Example: TCP Server\n");
    printf("===============================================================\n\n");

    /* Initialize a queue to receive command. */
    cy_rtos_queue_init(&led_command_q, SINGLE_ELEMENT_QUEUE, sizeof(uint8_t));

    xTaskCreate(tcp_server_task, "Network task", TCP_SERVER_TASK_STACK_SIZE, NULL,
               TCP_SERVER_TASK_PRIORITY, NULL);

    printf("Enabling CM4 at %x\n", CY_CORTEX_M4_APPL_ADDR);
    Cy_SysEnableCM4(CY_CORTEX_M4_APPL_ADDR);

    vTaskStartScheduler();

    /* Should never get here. */
    CY_ASSERT(0);
}

/*
Custom power management callback for CM0+ core which is registered by cybsp_init().
This avoids conflict with default power management callback registered on CM4 core.
*/
cy_rslt_t cybsp_register_custom_sysclk_pm_callback(void)
{
    cy_rslt_t result = CY_RSLT_SUCCESS;

    return result;
}

 /* [] END OF FILE */
