/*****************************************************************************
 * File Name        : main.c
 *
 * Description      : This is the source code for the CM33 secure application
 *                    of the Multicore IPC example. It configures the Flash MPC
 *                    to partition memory into secure and non-secure regions,
 *                    initializes the BSP, and launches the non-secure
 *                    application (main_cm33_ns).
 *
 * Related Document : See README.md
 *
 *******************************************************************************
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

#include <arm_cmse.h>
#include "cy_pdl.h"
#include "cybsp.h"
#include "partition_ARMCM33.h"
#include "partition_psc3.h"
#include "cy_mpc.h"
#include "cy_ppc.h"

/*******************************************************************************
 * Macros
 *******************************************************************************/
/* Memory addresses for cm33 */
#define CM33_NS_APP_BOOT_ADDR (CYMEM_CM33_0_S_m33ns_nvm_C_S_START)

#define CM33_NS_STACK_POINTER ((uint32_t)(*((uint32_t*)CM33_NS_APP_BOOT_ADDR)))
#define CM33_NS_RESET_HANDLER ((funcptr_void)(*((uint32_t*)(CM33_NS_APP_BOOT_ADDR + 4))))

/*******************************************************************************
 * Global Variables
 *******************************************************************************/

/*******************************************************************************
 * Function Prototype
 *******************************************************************************/

/* Entry point for non-secure image */
typedef void (*funcptr_void)(void) __attribute__((cmse_nonsecure_call));
cy_en_mpc_status_t config_mpc(void);

/*******************************************************************************
 * Function Definition
 *******************************************************************************/

/*******************************************************************************
 * Function Name: config_mpc
 ********************************************************************************
 * Summary: Configures Flash MPC.
 * This function configures the FLASH memory protection controller (FLASH MPC) to
 * partition the memory in to secure and non-secure worlds. The memory which are 
 * unused by secure project is configured as non-secure.
 *
 * Parameters:
 *  void
 *
 * Return:
 *  The status cy_en_mpc_status_t.
 *
 *******************************************************************************/
cy_en_mpc_status_t config_mpc(void)
{
    cy_stc_mpc_cfg_t config;
    cy_en_mpc_status_t mpcStatus;

    config.secure = CY_MPC_NON_SECURE;

    /* Internal flash config */
    mpcStatus = Cy_Mpc_ConfigMpcStruct((MPC_Type *)FLASHC_MPC0,
        CYMEM_CM33_0_S_m33ns_nvm_OFFSET,
        CYMEM_CM33_0_S_m33ns_nvm_SIZE + CYMEM_CM33_0_S_m33ns_ppca0_nvm_SIZE + CYMEM_CM33_0_S_m33ns_ppca1_nvm_SIZE,
        &config);
    if (mpcStatus != CY_MPC_SUCCESS)
    {
        return mpcStatus;
    }

    return mpcStatus;
}


/*******************************************************************************
* Function Name: main
********************************************************************************
* Summary:
* This is the main function of the CM33 Secure application.
*
* It configures the Flash MPC to partition memory into secure and non-secure
* regions, initializes the device and board peripherals, enables interrupts,
* and then sets up the non-secure stack pointer before jumping to the
* non-secure reset handler (main_cm33_ns). This core does not return after
* launching the non-secure application.
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
    cy_rslt_t result;
    cy_en_mpc_status_t mpc_status;
    uint32_t ns_stack = CM33_NS_STACK_POINTER;
    funcptr_void NonSecure_ResetHandler = CM33_NS_RESET_HANDLER;

    /* Config Flash MPC */
    mpc_status = config_mpc();
    if (mpc_status != CY_MPC_SUCCESS)
    {
        CY_ASSERT(0);
    }

    /* Initialize the device and board peripherals */
    result = cybsp_init();
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    __enable_irq();

    /* Set-up Stack pointer for non-secure application*/
    __TZ_set_MSP_NS(ns_stack);

    NonSecure_ResetHandler();

    /* Non-secure software does not return, this code is not executed */
    while (true);
}

