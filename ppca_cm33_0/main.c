/******************************************************************************
* File Name:   main.c
*
* Description: This is the source code for PPCA Core 0 of the Multicore IPC
*              example. Core 0 initiates the IPC ping-pong sequence by
*              acquiring the shared IPC lock first, writing CORE0_MSG (0xBB)
*              to shared RAM and the IPC data register, and notifying Core 1
*              and the main core before releasing the lock.
*
* Related Document: See README.md
*
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

/*******************************************************************************
* Header Files
********************************************************************************/

#include "cy_pdl.h"
#include "cycfg.h"
#include <stdio.h>

/*******************************************************************************
 * Macros
 *******************************************************************************/
#define PPCA_CORE_GPIO_PORT0_MAPPING 0x68410000
#define PPCA_CORE_GPIO_SCB2_UART_MAPPING 0x68850000

#define IPC_LOCK_STRUCT            (IPC_STRUCT_Type *)PPCA_IPC_STRUCT0
#define IPC_INTR_STRUCT            (IPC_INTR_STRUCT_Type *)PPCA_IPC_INTR_STRUCT0
#define PPCA_RAM_M4_ADDR        0x20040800
#define CORE0_MSG               0xBB
#define REL_MASK                1
#define NOTIFY_MASK             1
#define REL_BIT_POS             1 << 0
#define NOTIFY_BIT_POS          1 << 16
#define NOTIFY_CORE1            1 << 2
#define NOTIFY_CORE_MAIN        1 << 1

/*******************************************************************************
 * Global Variables
 *******************************************************************************/
volatile int aquired_ipc = 0, released_ipc = 1, is_busy = 0, ipc_notify = 0;
cy_stc_sysint_t ipc_irq_config =
{
        .intrSrc = ppca_ipc_0_IRQn, //(IRQn_Type)47,
        .intrPriority = 3U
};

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
* This is the main function for PPCA Core 0.
*
* It enables interrupts and configures the IPC interrupt handler to listen for
* lock-release and notify events on PPCA_IPC_INTR_STRUCT0. Core 0 initiates
* the IPC ping-pong sequence by acquiring the shared IPC lock (PPCA_IPC_STRUCT0)
* first. Once acquired, it reads the shared RAM and IPC data register, writes
* CORE0_MSG (0xBB), prints the before/after values over UART, and notifies
* Core 1 and the main core before releasing the lock.
*
* Parameters:
*  none
*
* Return:
*  int
*
*******************************************************************************/
int main(void)
{
    char buff[50]={'\0'};
    /* enable interrupts */
    __enable_irq();

    Cy_SysInt_Init(&ipc_irq_config, &ipc_irq_handler);
    NVIC_ClearPendingIRQ((IRQn_Type)ipc_irq_config.intrSrc);
    NVIC_EnableIRQ((IRQn_Type)ipc_irq_config.intrSrc);
    Cy_IPC_Drv_SetInterruptMask (IPC_INTR_STRUCT, REL_MASK, NOTIFY_MASK);

    for(;;)
    {
        CyDelay(1000);

        is_busy = Cy_IPC_Drv_IsLockAcquired(IPC_LOCK_STRUCT);
        if(is_busy)
            continue;

        if(released_ipc && !(is_busy))
        {
            if(Cy_IPC_Drv_LockAcquire(IPC_LOCK_STRUCT) == CY_IPC_DRV_SUCCESS)
            {
                aquired_ipc = 1;
            }
        }
        Cy_SCB_UART_PutString(DEBUG_UART_HW, "\r\n ********************************************");
        if(aquired_ipc)
        {
            Cy_SCB_UART_PutString(DEBUG_UART_HW, "\r\nCore0 Acquired lock");
            if(ipc_notify)
            {
                Cy_SCB_UART_PutString(DEBUG_UART_HW, "\r\nCore0 got notified");
                ipc_notify = 0;
            }
            sprintf(buff,"\r\n Memory before update from core0= %X",(unsigned int)CY_GET_REG32(PPCA_RAM_M4_ADDR)) ;
            Cy_SCB_UART_PutString(DEBUG_UART_HW, buff);

            sprintf(buff,"\r\n DATA register before update from core0 = %X",(unsigned int)Cy_IPC_Drv_ReadDataValue(IPC_LOCK_STRUCT)) ;
            Cy_SCB_UART_PutString(DEBUG_UART_HW, buff);

            CY_SET_REG32(PPCA_RAM_M4_ADDR, CORE0_MSG);
            Cy_IPC_Drv_WriteDataValue(IPC_LOCK_STRUCT, CY_GET_REG32(PPCA_RAM_M4_ADDR));

            sprintf(buff,"\r\n Memory after update from core0 = %X",(unsigned int)CY_GET_REG32(PPCA_RAM_M4_ADDR)) ;
            Cy_SCB_UART_PutString(DEBUG_UART_HW, buff);

            sprintf(buff,"\r\n DATA register after update from core0 = %X",(unsigned int)Cy_IPC_Drv_ReadDataValue(IPC_LOCK_STRUCT)) ;
            Cy_SCB_UART_PutString(DEBUG_UART_HW, buff);

            Cy_IPC_Drv_AcquireNotify(IPC_LOCK_STRUCT, (NOTIFY_CORE_MAIN) | (NOTIFY_CORE1));
            Cy_IPC_Drv_ReleaseNotify(IPC_LOCK_STRUCT, (NOTIFY_CORE_MAIN) | (NOTIFY_CORE1));

            aquired_ipc = 0;
            released_ipc = 0;
        }
    }
}
