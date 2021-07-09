/******************************************************************************
* File Name:   main.c
*
* Description: This is the source code for the XMC MCU: DMA PWM Example for
*              ModusToolbox. This example demonstrates how to use DMA double
*              buffering with the PWM Block.
*
* Related Document: See README.md
*
*******************************************************************************
*
* Copyright (c) 2015 - 2021, Infineon Technologies AG
* All rights reserved.
*
* Boost Software License - Version 1.0 - August 17th, 2003
*
* Permission is hereby granted, free of charge, to any person or organization
* obtaining a copy of the software and accompanying documentation covered by
* this license (the "Software") to use, reproduce, display, distribute,
* execute, and transmit the Software, and to prepare derivative works of the
* Software, and to permit third-parties to whom the Software is furnished to
* do so, all subject to the following:
*
* The copyright notices in the Software and this entire statement, including
* the above license grant, this restriction and the following disclaimer,
* must be included in all copies of the Software, in whole or in part, and
* all derivative works of the Software, unless such copies or derivative
* works are solely in the form of machine-executable object code generated by
* a source language processor.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
* SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
* FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
* ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
* DEALINGS IN THE SOFTWARE.
*
*****************************************************************************/

/*******************************************************************************
 * Include header files
 ******************************************************************************/
#include "cybsp.h"
#include "cy_utils.h"
#include "xmc_dma.h"
#include "xmc_ccu4.h"


/*******************************************************************************
* Macros
*******************************************************************************/
#define BLOCK_SIZE 48
#define GPDMA_CHANNEL0 0    /* DMA Channel 0 */
#define GPDMA_CHANNEL1 1    /* DMA Channel 1 */
#define TIMER_PERIOD 65535
#define COMPARE_BLOCK TIMER_PERIOD/BLOCK_SIZE


/*******************************************************************************
* Variables
*******************************************************************************/
uint32_t shadow_transfer_enable;
uint32_t duty_cycles[2][BLOCK_SIZE];

/* DMA linked list to transfer the data from memory to CCU4 peripheral. 
 * - The block size must be set to the transfer block size defined by the 
 *   macro BLOCK_SIZE
 * - Source address is the RAM buffer duty_cycle, and destination is the CCU4 
 *   compare register CRS.
 * - The linked list pointer points to the next DMA linked list to be 
 *   transferred.
 * - The source and destination transfer width must be 32 to enable 32 bit 
 *   transfers.
 * - Source address count mode should be configured to increment after every 
 *   data transfer and the destination count mode should not change as data 
 *   is written to the same register.
 * - Transfer flow is memory to peripheral (RAM to CCU4).
 */
__attribute__((aligned(32))) XMC_DMA_LLI_t dma_ll[2] =
{
  {
    .block_size = BLOCK_SIZE,                                            /* Transfer Block size */
    .src_addr = (uint32_t)&duty_cycles[0][0],                            /* Source address */
    .dst_addr = (uint32_t)&PWM_0_HW->CRS,                                /* Destination address */
    .llp = &dma_ll[1],                                                   /* Linked list pointer of type XMC_DMA_LLI_t */
    .src_transfer_width = XMC_DMA_CH_TRANSFER_WIDTH_32,                  /* Source transfer width */
    .dst_transfer_width = XMC_DMA_CH_TRANSFER_WIDTH_32,                  /* Destination transfer width */
    .src_address_count_mode = XMC_DMA_CH_ADDRESS_COUNT_MODE_INCREMENT,   /* Source address count mode */
    .dst_address_count_mode = XMC_DMA_CH_ADDRESS_COUNT_MODE_NO_CHANGE,   /* Destination address count mode */
    .src_burst_length = XMC_DMA_CH_BURST_LENGTH_1,                       /* Source burst length */
    .dst_burst_length = XMC_DMA_CH_BURST_LENGTH_1,                       /* Destination burst length */
    .transfer_flow = XMC_DMA_CH_TRANSFER_FLOW_M2P_DMA,                   /* DMA Transfer flow */
    .enable_src_linked_list = true,                                      /* Enable source linked list? */
    .enable_dst_linked_list = true,                                      /* Enable destination linked list? */
  },
  {
    .block_size = BLOCK_SIZE,                                            /* Transfer Block size */
    .src_addr = (uint32_t)&duty_cycles[1][0],                            /* Source address */
    .dst_addr = (uint32_t)&PWM_0_HW->CRS,                                /* Destination address */
    .llp = &dma_ll[0],                                                   /* Linked list pointer of type XMC_DMA_LLI_t */
    .src_transfer_width = XMC_DMA_CH_TRANSFER_WIDTH_32,                  /* Source transfer width */
    .dst_transfer_width = XMC_DMA_CH_TRANSFER_WIDTH_32,                  /* Destination transfer width */
    .src_address_count_mode = XMC_DMA_CH_ADDRESS_COUNT_MODE_INCREMENT,   /* Source address count mode */
    .dst_address_count_mode = XMC_DMA_CH_ADDRESS_COUNT_MODE_NO_CHANGE,   /* Destination address count mode */
    .src_burst_length = XMC_DMA_CH_BURST_LENGTH_1,                       /* Source burst length */
    .dst_burst_length = XMC_DMA_CH_BURST_LENGTH_1,                       /* Destination burst length */
    .transfer_flow = XMC_DMA_CH_TRANSFER_FLOW_M2P_DMA,                   /* DMA Transfer flow */
    .enable_src_linked_list = true,                                      /* Enable source linked list? */
    .enable_dst_linked_list = true,                                      /* Enable destination linked list? */
  }
};

/* DMA channel configuration used to transfer duty cycles using a linked list to
 * achieve double buffering.
 * - The linked list pointer points to the created DMA linked list, dma_ll in 
 *   this case.
 * - Transfer type is source address linked destination address linked
 * - The destination address handshaking is hardware as the CCU4 peripheral 
 *   instructs the GPDMA to transfer data.
 * - The destination peripheral request is mapped to CCU40
 * - Priority level is set to low priority in this case - lower number equals
 *   lower priority.
 */
XMC_DMA_CH_CONFIG_t dma_ch0_config =
{
  .block_size = BLOCK_SIZE,                                                             /* Transfer Block size */
  .linked_list_pointer = (XMC_DMA_LLI_t *)&dma_ll[0],                                   /* Linked list pointer */
  .transfer_flow = XMC_DMA_CH_TRANSFER_FLOW_M2P_DMA,                                    /* DMA Transfer flow */
  .transfer_type = XMC_DMA_CH_TRANSFER_TYPE_MULTI_BLOCK_SRCADR_LINKED_DSTADR_LINKED,    /* DMA transfer type */
  .src_handshaking = XMC_DMA_CH_SRC_HANDSHAKING_SOFTWARE,                               /* Source handshaking */
  .dst_handshaking = XMC_DMA_CH_DST_HANDSHAKING_HARDWARE,                               /* Destination handshaking */
  .dst_peripheral_request = DMA0_PERIPHERAL_REQUEST_CCU40_SR0_0,                        /* Destination peripheral request */
  .priority = XMC_DMA_CH_PRIORITY_0,                                                    /* Priority level */
};

/* DMA channel used to transfer the shadow transfer request. The shadow transfer
 * register needs to be set after every write to shadow register for the compare 
 * value to be updated.
 * - The source address is the variable to set the shadow transfer bit in the CCU4
 *   register and the destination is the GCSS register of the CCU40 peripheral.
 * - The source and destination count mode requires no change since the same 
 *   data location is written everytime.
 * - The transfer type in this case is reload since we need the same 
 *   configuration after every DMA transfer. 
 * - The destination handshake is hardware and the peripheral request is mapped
 *   to CCU40 peripheral. 
 */
XMC_DMA_CH_CONFIG_t dma_ch1_config =
{
  .block_size = BLOCK_SIZE,                                                             /* Transfer Block size */
  .src_addr = (uint32_t)&shadow_transfer_enable,                                        /* Source address */
  .dst_addr = (uint32_t)&CCU40->GCSS,                                                   /* Destination address */
  .src_transfer_width = XMC_DMA_CH_TRANSFER_WIDTH_32,                                   /* Source transfer width */
  .dst_transfer_width = XMC_DMA_CH_TRANSFER_WIDTH_32,                                   /* Destination transfer width */
  .src_address_count_mode = XMC_DMA_CH_ADDRESS_COUNT_MODE_NO_CHANGE,                    /* Source address count mode */
  .dst_address_count_mode = XMC_DMA_CH_ADDRESS_COUNT_MODE_NO_CHANGE,                    /* Destination address count mode */
  .src_burst_length = XMC_DMA_CH_BURST_LENGTH_1,                                        /* Source burst length */
  .dst_burst_length = XMC_DMA_CH_BURST_LENGTH_1,                                        /* Destination burst length */
  .transfer_flow = XMC_DMA_CH_TRANSFER_FLOW_M2P_DMA,                                    /* DMA Transfer flow */
  .transfer_type = XMC_DMA_CH_TRANSFER_TYPE_MULTI_BLOCK_SRCADR_RELOAD_DSTADR_RELOAD,    /* DMA transfer type */
  .src_handshaking = XMC_DMA_CH_SRC_HANDSHAKING_SOFTWARE,                               /* Source handshaking */
  .dst_handshaking = XMC_DMA_CH_DST_HANDSHAKING_HARDWARE,                               /* Destination handshaking */
  .dst_peripheral_request = DMA0_PERIPHERAL_REQUEST_CCU40_SR0_0,                        /* Destination peripheral request */
  .priority = XMC_DMA_CH_PRIORITY_0                                                     /* Priority level */
};

/*******************************************************************************
* Function Name: main
********************************************************************************
* Summary:
* This is the main function. This function sets up the CCU4 peripheral and 
* initializes the GPDMA peripheral.
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
    
    /*Initialize the device and board peripherals */
    result = cybsp_init();
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }
    for(int i = 0; i < BLOCK_SIZE; i++)
    {
        duty_cycles[0][i] = COMPARE_BLOCK * i ;
        duty_cycles[1][i] = (COMPARE_BLOCK * (BLOCK_SIZE - 1)) - (COMPARE_BLOCK * i);
    }

    /* Set the shadow transfer enable variable that sets the shadow transfer
     * bit in the CCU40 GCSS register
     */
    shadow_transfer_enable = CCU4_GCSS_S0SE_Msk;
    
    /* Initialize and enable the GPDMA peripheral */
    XMC_DMA_Init(XMC_DMA0);

    /* Initialize the DMA channels with provided channel configuration */
    XMC_DMA_CH_Init(XMC_DMA0, GPDMA_CHANNEL0, &dma_ch0_config);
    XMC_DMA_CH_Init(XMC_DMA0, GPDMA_CHANNEL1, &dma_ch1_config);

    /* Enable the DMA channel to initiate transfer */
    XMC_DMA_CH_Enable(XMC_DMA0, GPDMA_CHANNEL0);
    XMC_DMA_CH_Enable(XMC_DMA0, GPDMA_CHANNEL1);
  
    /* Start the PWM */
    XMC_CCU4_SLICE_StartTimer(PWM_0_HW);
  
    /* Placeholder for user application code. The while loop below can be 
     * replaced with user application code. 
     */
    while(1U);
}

/* [] END OF FILE */