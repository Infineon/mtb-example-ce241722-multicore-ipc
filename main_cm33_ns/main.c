/*****************************************************************************
* File Name        : main.c
*
* Description      : This is the source code for the non-secure main CM33 core
*                    (main_cm33_ns) of the Multicore IPC example. It initializes
*                    the debug UART, sets up IPC interrupts on
*                    PPCA_IPC_INTR_STRUCT1, boots PPCA Core 0 and Core 1 from
*                    flash, and idles while the PPCA cores perform IPC
*                    communication.
*
* Related Document : See README.md
*
********************************************************************************
* (c) 2026, Infineon Technologies AG, or an affiliate of Infineon
* Technologies AG. All rights reserved.
* This software, associated documentation and materials ("Software") is
* owned by Infineon Technologies AG or one of its affiliates ("Infineon")
* and is protected by and subject to worldwide patent protection, worldwide
* copyright laws, and international treaty provisions. Therefore, you may use
* this Software only as provided in the license agreement accompanying the
* software package from which you obtained this Software. If no license
* agreement applies, then any use, reproduction, modification, translation, or
* compilation of this Software is prohibited without the express written
* permission of Infineon.
*
* Disclaimer: UNLESS OTHERWISE EXPRESSLY AGREED WITH INFINEON, THIS SOFTWARE
* IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
* INCLUDING, BUT NOT LIMITED TO, ALL WARRANTIES OF NON-INFRINGEMENT OF
* THIRD-PARTY RIGHTS AND IMPLIED WARRANTIES SUCH AS WARRANTIES OF FITNESS FOR A
* SPECIFIC USE/PURPOSE OR MERCHANTABILITY.
* Infineon reserves the right to make changes to the Software without notice.
* You are responsible for properly designing, programming, and testing the
* functionality and safety of your intended application of the Software, as
* well as complying with any legal requirements related to its use. Infineon
* does not guarantee that the Software will be free from intrusion, data theft
* or loss, or other breaches ("Security Breaches"), and Infineon shall have
* no liability arising out of any Security Breaches. Unless otherwise
* explicitly approved by Infineon, the Software may not be used in any
* application where a failure of the Product or any consequences of the use
* thereof can reasonably be expected to result in personal injury.
*******************************************************************************/

/******************************************************************************
 * Header Files
 *****************************************************************************/
#include "cy_pdl.h"
#include "cycfg.h"
#include "cy_system_ppca_init.h"
#include "cybsp.h"
#include <stdio.h>
#include "cy_retarget_io.h"

/*******************************************************************************
* Macros
*******************************************************************************/
/* These are the addresses where the core0 and core1 images are located. */
/* Flash memory addresses where PPCA core images are stored */
#define CORE0_IMAGE_ADDRESS   CYMEM_CM33_0_m33ns_ppca0_nvm_C_START
#define CORE1_IMAGE_ADDRESS   CYMEM_CM33_0_m33ns_ppca1_nvm_C_START

#define PPCA0_IMAGE_SIZE      CYMEM_CM33_0_ppca0_code_SIZE
#define PPCA1_IMAGE_SIZE      CYMEM_CM33_0_ppca1_code_SIZE

#define IPC_LOCK_STRUCT            (IPC_STRUCT_Type *)PPCA_IPC_STRUCT0
#define IPC_INTR_STRUCT            (IPC_INTR_STRUCT_Type *)PPCA_IPC_INTR_STRUCT1
#define PPCA_RAM_M4_ADDR        0x43050800
#define MAIN_CORE_MSG           0xAA
#define REL_MASK                1
#define NOTIFY_MASK             1
#define REL_BIT_POS             1 << 0
#define NOTIFY_BIT_POS          1 << 16
#define NOTIFY_CORE0            1 << 0

/*******************************************************************************
* Global Variables
*******************************************************************************/
/* Debug UART variables */
static cy_stc_scb_uart_context_t    DEBUG_UART_context; /* DEBUG_UART context */
static mtb_hal_uart_t               DEBUG_UART_hal_obj; /* Debug DEBUG_UART HAL object */
volatile int aquired_ipc = 0, released_ipc = 1, is_busy = 1, ipc_notify=0;

cy_stc_sysint_t ipc_irq_config =
{
        .intrSrc = ppca_ipc_1_IRQn, //(IRQn_Type)ppca_irq_ppca_48_IRQn,
        .intrPriority = 3U
};

/*******************************************************************************
* Function Prototypes
*******************************************************************************/

/*******************************************************************************
* Function Definitions
*******************************************************************************/
void ipc_irq_handler(void)
{
    if(Cy_IPC_Drv_GetInterruptStatus(IPC_INTR_STRUCT) & (REL_BIT_POS))
        released_ipc = 1;

    if(Cy_IPC_Drv_GetInterruptStatus(IPC_INTR_STRUCT) & (NOTIFY_BIT_POS))
        ipc_notify = 1;

    Cy_IPC_Drv_ClearInterrupt(IPC_INTR_STRUCT, REL_MASK, NOTIFY_MASK);

}
/*******************************************************************************
* Function Name: main
********************************************************************************
* Summary:
*  Entry point for the non-secure CM33 main core. Performs the following:
*    1. Initializes board peripherals (cybsp_init) and the debug UART.
*    2. Enables global interrupts and configures the IPC interrupt handler
*       on PPCA_IPC_INTR_STRUCT1 for lock-release and notify events.
*    3. Boots PPCA Core 0 and Core 1 from their flash images.
*    4. Idles in an infinite loop while the PPCA cores handle IPC
*       communication.
*
* Parameters:
*  void
*
* Return:
*  int
*
*******************************************************************************/
int main(void)
{
    cy_rslt_t result;
    /* Initialize the device and board peripherals */
    result = cybsp_init();

    /* Board init failed. Stop program execution */
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    Cy_SCB_UART_Init(DEBUG_UART_HW, &DEBUG_UART_config, &DEBUG_UART_context);
    Cy_SCB_UART_Enable(DEBUG_UART_HW);

    /* Setup the HAL DEBUG_UART */
    result = mtb_hal_uart_setup(&DEBUG_UART_hal_obj, &DEBUG_UART_hal_config,
            &DEBUG_UART_context, NULL);

    /* HAL DEBUG_UART init failed. Stop program execution */
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    /* Initialize redirecting of low level IO */
    result = cy_retarget_io_init(&DEBUG_UART_hal_obj);

     /* HAL retarget_io init failed. Stop program execution */
     if (result != CY_RSLT_SUCCESS)
     {
          CY_ASSERT(0);
     }

    /* Transmit header to the terminal */
    /* \x1b[2J\x1b[;H - ANSI ESC sequence for clear screen */
    printf("\x1b[2J\x1b[;H");
    printf("************************************************************\r\n");
    printf("PSOC Control C3M/P8: Multicore IPC\r\n");
    printf("************************************************************\r\n\n");

    /* enable interrupts */
    __enable_irq();

    Cy_SysInt_Init(&ipc_irq_config, &ipc_irq_handler);
    NVIC_ClearPendingIRQ((IRQn_Type)ipc_irq_config.intrSrc);
    NVIC_EnableIRQ((IRQn_Type)ipc_irq_config.intrSrc);

    Cy_IPC_Drv_SetInterruptMask (IPC_INTR_STRUCT, REL_MASK, NOTIFY_MASK);
    Cy_System_Init_CPU0((void*)CORE0_IMAGE_ADDRESS,PPCA0_IMAGE_SIZE);
    Cy_System_Init_CPU1((void*)CORE1_IMAGE_ADDRESS,PPCA1_IMAGE_SIZE);

    for (;;)
    {


    }
}
