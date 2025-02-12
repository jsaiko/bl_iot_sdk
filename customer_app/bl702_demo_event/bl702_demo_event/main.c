/*
 * Copyright (c) 2020 Bouffalolab.
 *
 * This file is part of
 *     *** Bouffalolab Software Dev Kit ***
 *      (see www.bouffalolab.com).
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *   1. Redistributions of source code must retain the above copyright notice,
 *      this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright notice,
 *      this list of conditions and the following disclaimer in the documentation
 *      and/or other materials provided with the distribution.
 *   3. Neither the name of Bouffalo Lab nor the names of its contributors
 *      may be used to endorse or promote products derived from this software
 *      without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <FreeRTOS.h>
#include <task.h>
#include <timers.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <vfs.h>
#include <device/vfs_uart.h>
#include <aos/kernel.h>
#include <aos/yloop.h>
#include <event_device.h>
#include <cli.h>

#include <bl_sys.h>
#include <bl_chip.h>
#include <bl_wireless.h>
#include <bl_irq.h>
#include <bl_sec.h>
#include <bl_rtc.h>
#include <bl_uart.h>
#include <bl_gpio.h>
#include <bl_flash.h>
#include <bl_timer.h>
#include <bl_wdt.h>
#include <hal_boot2.h>
#include <hal_board.h>
#include <hosal_uart.h>
#include <hosal_gpio.h>
#include <hal_gpio.h>
#include <hal_button.h>
#include <hal_hwtimer.h>
#include <hal_pds.h>
#include <hal_tcal.h>
#include <FreeRTOS.h>
#include <timers.h>

#ifdef CFG_ETHERNET_ENABLE
#include <lwip/netif.h>
#include <lwip/etharp.h>
#include <lwip/udp.h>
#include <lwip/ip.h>
#include <lwip/init.h>
#include <lwip/ip_addr.h>
#include <lwip/tcpip.h>
#include <lwip/dhcp.h>
#include <lwip/netifapi.h>

#include <bl_sys_ota.h>
#include <bl_emac.h>
#include <bl702_glb.h>
#include <bl702_common.h>
#include <bflb_platform.h>
#include <eth_bd.h>
#include <netutils/netutils.h>

#include <netif/ethernet.h>
#endif /* CFG_ETHERNET_ENABLE */
#include <easyflash.h>
#include <libfdt.h>
#include <utils_log.h>
#include <blog.h>

#ifdef EASYFLASH_ENABLE
#include <easyflash.h>
#endif
#include <utils_string.h>
#if defined(CONFIG_AUTO_PTS)
#include "bttester.h"
#include "autopts_uart.h"
#endif

#if defined(CFG_BLE_ENABLE)
#include "bluetooth.h"
#include "ble_cli_cmds.h"
#include <hci_driver.h>
#include "ble_lib_api.h"

#if defined(CONFIG_BLE_TP_SERVER)
#include "ble_tp_svc.h"
#endif

#if defined(CONFIG_BT_MESH)
#include "mesh_cli_cmds.h"
#endif
#endif

#if defined(CFG_ZIGBEE_ENABLE)
#include "zb_common.h"
#include "zb_stack_cli.h"
#include "zigbee_app.h"
//#include "zb_bdb.h"
#endif
#if defined(CONFIG_ZIGBEE_PROV)
#include "blsync_ble_app.h"
#endif
#if defined(CFG_USE_PSRAM)
#include "bl_psram.h"
#endif /* CFG_USE_PSRAM */

#include "bl_flash.h"
#if defined(CFG_ZIGBEE_HBN)
#include "bl_hbn.h"
#endif

#ifdef CFG_ETHERNET_ENABLE
//extern err_t ethernetif_init(struct netif *netif);
extern err_t eth_init(struct netif *netif);

#define ETH_USE_DHCP 1

#if ETH_USE_DHCP
void lwip_init_netif(void)
{
    ip_addr_t ipaddr, netmask, gw;
    IP4_ADDR(&ipaddr, 0,0,0,0);
    IP4_ADDR(&netmask, 0,0,0,0);
    IP4_ADDR(&gw, 0,0,0,0);
    netif_add(&eth_mac, &ipaddr, &netmask, &gw, NULL, eth_init, ethernet_input);
    //netif_set_default(&eth_mac);
    //netif_set_up(&eth_mac);
    //printf("start dhcp....\r\n");
    //dhcp_start(&eth_mac);
}
#else
void lwip_init_netif(void)
{
    ip_addr_t ipaddr, netmask, gw;

    IP4_ADDR(&gw, 192,168,99,1);
    IP4_ADDR(&ipaddr, 192,168,99,150);
    IP4_ADDR(&netmask, 255,255,255,0);

    netif_add(&eth_mac, &ipaddr, &netmask, &gw, NULL, eth_init, ethernet_input);
    netif_set_default(&eth_mac);
    netif_set_up(&eth_mac);
}
#endif /* ETH_USE_DHCP */
#endif /* CFG_ETHERNET_ENABLE */

HOSAL_UART_DEV_DECL(uart_stdio, 0, 14, 15, 2000000);

extern uint8_t _heap_start;
extern uint8_t _heap_size; // @suppress("Type cannot be resolved")
extern uint8_t _heap2_start;
extern uint8_t _heap2_size; // @suppress("Type cannot be resolved")
static HeapRegion_t xHeapRegions[] =
{
    { &_heap_start,  (unsigned int) &_heap_size}, //set on runtime
    { &_heap2_start, (unsigned int) &_heap2_size },            
    { NULL, 0 }, /* Terminates the array. */
    { NULL, 0 } /* Terminates the array. */
};
#if defined(CFG_USE_PSRAM)
extern uint8_t _heap3_start;
extern uint8_t _heap3_size; // @suppress("Type cannot be resolved")
static HeapRegion_t xHeapRegionsPsram[] =  
{
    { &_heap3_start, (unsigned int) &_heap3_size },
    { NULL, 0 }, /* Terminates the array. */
    { NULL, 0 } /* Terminates the array. */
};
#endif /* CFG_USE_PSRAM */

bool pds_start = false;
bool wfi_disable = false;
static void bl702_low_power_config(void);
static void cmd_start_pds(char *buf, int len, int argc, char **argv)
{
    pds_start = true;
}

#if defined(CFG_ZIGBEE_HBN)
bool hbn_start = false;
static void cmd_start_hbn(char *buf, int len, int argc, char **argv)
{
    hbn_start = true;
}
#endif

static void cmd_lowpower_config(char *buf, int len, int argc, char **argv)
{
    bl702_low_power_config();
}

typedef enum {
    TEST_OP_GET32 = 0,
    TEST_OP_GET16,
    TEST_OP_SET32 = 256,
    TEST_OP_SET16,
    TEST_OP_MAX = 0x7FFFFFFF
} test_op_t;
static __attribute__ ((noinline)) uint32_t misaligned_acc_test(void *ptr, test_op_t op, uint32_t v)
{
    uint32_t res = 0;

    switch (op) {
        case TEST_OP_GET32:
            res = *(volatile uint32_t *)ptr;
            break;
        case TEST_OP_GET16:
            res = *(volatile uint16_t *)ptr;
            break;
        case TEST_OP_SET32:
            *(volatile uint32_t *)ptr = v;
            break;
        case TEST_OP_SET16:
            *(volatile uint16_t *)ptr = v;
            break;
        default:
            break;
    }

    return res;
}

//uint32_t bl_timer_now_us(void){return 0;}
void test_align(uint32_t buf)
{
    volatile uint32_t testv[4] = {0};
    uint32_t t1 = 0;
    uint32_t t2 = 0;
    uint32_t t3 = 0;
    uint32_t i = 0;
    volatile uint32_t reg = buf;

    portDISABLE_INTERRUPTS();

    /* test get 32 */
    __asm volatile ("nop":::"memory");
    t1 = *(volatile uint32_t*)0x4000A52C;
    // 3*n + 5
    testv[0] = *(volatile uint32_t *)(reg + 0 * 8 + 1);
    t2 = *(volatile uint32_t*)0x4000A52C;
    // 3*n + 1
    testv[1] = *(volatile uint32_t *)(reg + 1 * 8 + 0);
    t3 = *(volatile uint32_t*)0x4000A52C;
    log_info("testv[0] = %08lx, testv[1] = %08lx\r\n", testv[0], testv[1]);
    log_info("time_us = %ld & %ld ---> %d\r\n", (t2 - t1), (t3 - t2), (t2 - t1)/(t3 - t2));

    /* test get 16 */
    __asm volatile ("nop":::"memory");
    t1 = bl_timer_now_us();
    for (i = 0; i < 1 * 1000 * 1000; i++) {
        testv[0] = misaligned_acc_test((void *)(reg + 2 * 8 + 1), TEST_OP_GET16, 0);
    }
    t2 = bl_timer_now_us();
    for (i = 0; i < 1 * 1000 * 1000; i++) {
        testv[1] = misaligned_acc_test((void *)(reg + 3 * 8 + 0), TEST_OP_GET16, 0);
    }
    t3 = bl_timer_now_us();
    log_info("testv[0] = %08lx, testv[1] = %08lx\r\n", testv[0], testv[1]);
    log_info("time_us = %ld & %ld ---> %d\r\n", (t2 - t1), (t3 - t2), (t2 - t1)/(t3 - t2));

    /* test set 32 */
    __asm volatile ("nop":::"memory");
    t1 = bl_timer_now_us();
    for (i = 0; i < 1 * 1000 * 1000; i++) {
        misaligned_acc_test((void *)(reg + 4 * 8 + 1), TEST_OP_SET32, 0x44332211);
    }
    t2 = bl_timer_now_us();
    for (i = 0; i < 1 * 1000 * 1000; i++) {
        misaligned_acc_test((void *)(reg + 5 * 8 + 0), TEST_OP_SET32, 0x44332211);
    }
    t3 = bl_timer_now_us();
    log_info("time_us = %ld & %ld ---> %d\r\n", (t2 - t1), (t3 - t2), (t2 - t1)/(t3 - t2));

    /* test set 16 */
    __asm volatile ("nop":::"memory");
    t1 = bl_timer_now_us();
    for (i = 0; i < 1 * 1000 * 1000; i++) {
        misaligned_acc_test((void *)(reg + 6 * 8 + 1), TEST_OP_SET16, 0x6655);
    }
    t2 = bl_timer_now_us();
    for (i = 0; i < 1 * 1000 * 1000; i++) {
        misaligned_acc_test((void *)(reg + 7 * 8 + 0), TEST_OP_SET16, 0x6655);
    }
    t3 = bl_timer_now_us();
    log_info("time_us = %ld & %ld ---> %d\r\n", (t2 - t1), (t3 - t2), (t2 - t1)/(t3 - t2));

    portENABLE_INTERRUPTS();
}

void test_misaligned_access(void) __attribute__((optimize("O0")));
void test_misaligned_access(void)// __attribute__((optimize("O0")))
{
#define TEST_V_LEN         (32)
    __attribute__ ((aligned(16))) volatile unsigned char test_vector[TEST_V_LEN] = {0};
    int i = 0;
    volatile uint32_t v = 0;
    uint32_t addr = (uint32_t)test_vector;
    volatile char *pb = (volatile char *)test_vector;
    register float a asm("fa0") = 0.0f;
    register float b asm("fa1") = 0.5f;

    for (i = 0; i < TEST_V_LEN; i ++)
        test_vector[i] = i;

    addr += 1; // offset 1
    __asm volatile ("nop");
    v = *(volatile uint16_t *)(addr); // 0x0201
    __asm volatile ("nop");
    printf("%s: v=%8lx, should be 0x0201\r\n", __func__, v);
    __asm volatile ("nop");
    *(volatile uint16_t *)(addr) = 0x5aa5;
    __asm volatile ("nop");
    __asm volatile ("nop");
    v = *(volatile uint16_t *)(addr); // 0x5aa5
    __asm volatile ("nop");
    printf("%s: v=%8lx, should be 0x5aa5\r\n", __func__, v);

    addr += 4; // offset 5
    __asm volatile ("nop");
    v = *(volatile uint32_t *)(addr); //0x08070605
    __asm volatile ("nop");
    printf("%s: v=%8lx, should be 0x08070605\r\n", __func__, v);
    __asm volatile ("nop");
    *(volatile uint32_t *)(addr) = 0xa5aa55a5;
    __asm volatile ("nop");
    __asm volatile ("nop");
    v = *(volatile uint32_t *)(addr); // 0xa5aa55a5
    __asm volatile ("nop");
    printf("%s: v=%8lx, should be 0xa5aa55a5\r\n", __func__, v);

    pb[0x11] = 0x00;
    pb[0x12] = 0x00;
    pb[0x13] = 0xc0;
    pb[0x14] = 0x3f;

    addr += 12; // offset 0x11
    __asm volatile ("nop");
    a = *(float *)(addr);
    __asm volatile ("nop");
    v = a * 4.0f; /* should be 6 */
    __asm volatile ("nop");
    __asm volatile ("nop");
    printf("%s: v=%8lx, should be 0x6\r\n", __func__, v);
    b = v / 12.0f;
    __asm volatile ("nop");
    addr += 4; // offset 0x15
    *(float *)(addr) = b;
    __asm volatile ("nop");
    v = *(volatile uint32_t *)(addr); // 0x3f000000
    __asm volatile ("nop");
    printf("%s: v=%8lx, should be 0x3f000000\r\n", __func__, v);
}

static void cmd_align(char *buf, int len, int argc, char **argv)
{
    char *testbuf = NULL;
    int i = 0;

    log_info("align test start.\r\n");
    test_misaligned_access();

    testbuf = aos_malloc(1024);
    if (!testbuf) {
        log_error("mem error.\r\n");
    }
 
    memset(testbuf, 0xEE, 1024);
    for (i = 0; i < 32; i++) {
        testbuf[i] = i;
    }
    test_align((uint32_t)(testbuf));

    log_buf(testbuf, 64);
    aos_free(testbuf);

    log_info("align test end.\r\n");
}
#if defined(CONFIG_ZIGBEE_PROV)
static void cmd_blsync_blezb_start(void)
{
    blsync_ble_start();
}
#endif

static void cmd_wdt_set(char *buf, int len, int argc, char **argv)
{
    if(argc != 2){
        log_info("Number of parameters error.\r\n");
        return;
    }
    if(strcmp(argv[1], "enable") == 0){
        bl_wdt_init(4000);
    }
    else if(strcmp(argv[1], "disable") == 0){
        bl_wdt_disable();
    }
    else{
        log_info("Second parameter error.\r\n");
    }
}


const static struct cli_command cmds_user[] STATIC_CLI_CMD_ATTRIBUTE = { 
        #if defined(CFG_ZIGBEE_PDS) || (CFG_BLE_PDS)
        {"pds_start", "enable pds", cmd_start_pds},
        #endif
        {"lw_cfg", "lowpower configuration for active current test", cmd_lowpower_config},
        #if defined(CFG_ZIGBEE_HBN)
        {"hbn_start", "enable hbn", cmd_start_hbn},
        #endif
        {"aligntc", "align case test", cmd_align},
        #if defined(CONFIG_ZIGBEE_PROV)
        { "blsync_blezb_start", "start zigbee provisioning via ble", cmd_blsync_blezb_start},
        #endif
        { "wdt_set", "enable or disable pds", cmd_wdt_set},
};

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName )
{
    puts("Stack Overflow checked\r\n");
	if(pcTaskName){
		printf("Stack name %s\r\n", pcTaskName);
	}
    while (1) {
        /*empty here*/
    }
}

void vApplicationMallocFailedHook(void)
{
    printf("Memory Allocate Failed. Current left size is %d bytes\r\n"
#if defined(CFG_USE_PSRAM)
        "Current psram left size is %d bytes\r\n"
#endif /*CFG_USE_PSRAM*/
        ,xPortGetFreeHeapSize()
#if defined(CFG_USE_PSRAM)
        ,xPortGetFreeHeapSizePsram()
#endif /*CFG_USE_PSRAM*/
    );
    while (1) {
        /*empty here*/
    }
}

void vApplicationIdleHook(void)
{
    bl_wdt_feed();
    bool bWFI_disable =  false;
    #if defined (CFG_BLE_PDS)
    bWFI_disable = wfi_disable;
    #else
    bWFI_disable = pds_start;
    #endif
    if(!bWFI_disable){
        __asm volatile(
                "   wfi     "
        );
        /*empty*/
    }
}

#if ( configUSE_TICKLESS_IDLE != 0 )
#if !defined(CFG_BLE_PDS) && !defined(CFG_ZIGBEE_PDS)&& !defined(CFG_ZIGBEE_HBN)
void vApplicationSleep( TickType_t xExpectedIdleTime )
{
    
}
#endif
#endif

static void bl702_low_power_config(void)
{
#if !defined(CFG_USB_CDC_ENABLE)
    // Power off DLL
    GLB_Power_Off_DLL();
#endif
    
    // Disable secure engine
    Sec_Eng_Trng_Disable();
    SEC_Eng_Turn_Off_Sec_Ring();
    
#if !defined(CFG_BLE_ENABLE)
    // if ble is not enabled, Disable BLE clock
    GLB_Set_BLE_CLK(0);
#endif
#if !defined(CFG_ZIGBEE_ENABLE)
    // if zigbee is not enabled, Disable Zigbee clock
    GLB_Set_MAC154_ZIGBEE_CLK(0);
#endif
    
    // Gate peripheral clock
    BL_WR_REG(GLB_BASE, GLB_CGEN_CFG1, 0x00214BC3);
}

#if ( configUSE_TICK_HOOK != 0 )
void vApplicationTickHook( void )
{
#if defined(CFG_USB_CDC_ENABLE)
    extern void usb_cdc_monitor(void);
    usb_cdc_monitor();
#endif
#if defined(CFG_ZIGBEE_ENABLE)
	extern void ZB_MONITOR(void);
	ZB_MONITOR();
#endif
}
#endif

void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize)
{
    /* If the buffers to be provided to the Idle task are declared inside this
    function then they must be declared static - otherwise they will be allocated on
    the stack and so not exists after this function exits. */
    static StaticTask_t xIdleTaskTCB;
    //static StackType_t uxIdleTaskStack[ configMINIMAL_STACK_SIZE ];
    static StackType_t uxIdleTaskStack[256];

    /* Pass out a pointer to the StaticTask_t structure in which the Idle task's
    state will be stored. */
    *ppxIdleTaskTCBBuffer = &xIdleTaskTCB;

    /* Pass out the array that will be used as the Idle task's stack. */
    *ppxIdleTaskStackBuffer = uxIdleTaskStack;

    /* Pass out the size of the array pointed to by *ppxIdleTaskStackBuffer.
    Note that, as the array is necessarily of type StackType_t,
    configMINIMAL_STACK_SIZE is specified in words, not bytes. */
    //*pulIdleTaskStackSize = configMINIMAL_STACK_SIZE; 
    *pulIdleTaskStackSize = 256;//size 256 words is For ble pds mode, otherwise stack overflow of idle task will happen.
}

/* configSUPPORT_STATIC_ALLOCATION and configUSE_TIMERS are both set to 1, so the
application must provide an implementation of vApplicationGetTimerTaskMemory()
to provide the memory that is used by the Timer service task. */
void vApplicationGetTimerTaskMemory(StaticTask_t **ppxTimerTaskTCBBuffer, StackType_t **ppxTimerTaskStackBuffer, uint32_t *pulTimerTaskStackSize)
{
    /* If the buffers to be provided to the Timer task are declared inside this
    function then they must be declared static - otherwise they will be allocated on
    the stack and so not exists after this function exits. */
    static StaticTask_t xTimerTaskTCB;
    static StackType_t uxTimerTaskStack[ configTIMER_TASK_STACK_DEPTH ];

    /* Pass out a pointer to the StaticTask_t structure in which the Timer
    task's state will be stored. */
    *ppxTimerTaskTCBBuffer = &xTimerTaskTCB;

    /* Pass out the array that will be used as the Timer task's stack. */
    *ppxTimerTaskStackBuffer = uxTimerTaskStack;

    /* Pass out the size of the array pointed to by *ppxTimerTaskStackBuffer.
    Note that, as the array is necessarily of type StackType_t,
    configTIMER_TASK_STACK_DEPTH is specified in words, not bytes. */
    *pulTimerTaskStackSize = configTIMER_TASK_STACK_DEPTH;
}

void user_vAssertCalled(void) __attribute__ ((weak, alias ("vAssertCalled")));
void vAssertCalled(void)
{
    taskDISABLE_INTERRUPTS();
    printf("vAssertCalled\r\n");
    abort();
}


static void _cli_init()
{
    /*Put CLI which needs to be init here*/
#if defined(CFG_EFLASH_LOADER_ENABLE)
    extern int helper_eflash_loader_cli_init(void);
    helper_eflash_loader_cli_init();
#endif

#if defined(CFG_RFPHY_CLI_ENABLE)
    extern int helper_rfphy_cli_init(void);
    helper_rfphy_cli_init();
#endif
#ifdef CFG_ETHERNET_ENABLE
    /*Put CLI which needs to be init here*/
    network_netutils_iperf_cli_register();
    network_netutils_ping_cli_register();
    bl_sys_ota_cli_init();
#endif /* CFG_ETHERNET_ENABLE */
}

static int get_dts_addr(const char *name, uint32_t *start, uint32_t *off)
{
    uint32_t addr = hal_board_get_factory_addr();
    const void *fdt = (const void *)addr;
    uint32_t offset;

    if (!name || !start || !off) {
        return -1;
    }

    offset = fdt_subnode_offset(fdt, 0, name);
    if (offset <= 0) {
       log_error("%s NULL.\r\n", name);
       return -1;
    }

    *start = (uint32_t)fdt;
    *off = offset;

    return 0;
}

#if defined(CFG_BLE_ENABLE)
void ble_init(void)
{
	extern void ble_stack_start(void);
	ble_stack_start();
}
#endif

#if defined(CFG_ZIGBEE_ENABLE)
void zigbee_init(void)
{
    zbRet_t status;
    status = zb_stackInit();
    if (status != ZB_SUCC)
    {
        printf("BL Zbstack Init fail : 0x%08x\r\n", status);
        //ASSERT(false);
    }
    else
    {
        printf("BL Zbstack Init Success\r\n");
    }

    zb_cli_register();

    register_zb_cb();
    
    zb_app_startup();

    //zb_bdb_init();
}
#endif

void event_cb_key_event(input_event_t *event, void *private_data)
{
    switch (event->code) {
        case KEY_1:
        {
            printf("[KEY_1] [EVT] INIT DONE %lld\r\n", aos_now_ms());
            printf("short press \r\n");
        }
        break;
        case KEY_2:
        {
            printf("[KEY_2] [EVT] INIT DONE %lld\r\n", aos_now_ms());
            printf("long press \r\n");
        }
        break;
        case KEY_3:
        {
            printf("[KEY_3] [EVT] INIT DONE %lld\r\n", aos_now_ms());
            printf("longlong press \r\n");
        }
        break;
        default:
        {
            printf("[KEY] [EVT] Unknown code %u, %lld\r\n", event->code, aos_now_ms());
            /*nothing*/
        }
    }
}

static void aos_loop_proc(void *pvParameters)
{
    int fd_console;
    uint32_t fdt = 0, offset = 0;

#ifdef EASYFLASH_ENABLE
    easyflash_init();
#endif

    vfs_init();
    vfs_device_init();

    /* uart */
    const char *uart_node[] = {
        "uart@4000A000",
        "uart@4000A100",
    };

    if (0 == get_dts_addr("uart", &fdt, &offset)) {
        vfs_uart_init(fdt, offset, uart_node, 2);
    }

#ifndef CFG_ETHERNET_ENABLE
    /* gpio */
    if (0 == get_dts_addr("gpio", &fdt, &offset)) {
        hal_gpio_init_from_dts(fdt, offset);
        fdt_button_module_init((const void *)fdt, (int)offset);
    }

#endif /* CFG_ETHERNET_ENABLE */

    aos_loop_init();

    fd_console = aos_open("/dev/ttyS0", 0);
    if (fd_console >= 0) {
        printf("Init CLI with event Driven\r\n");
        aos_cli_init(0);
        aos_poll_read_fd(fd_console, aos_cli_event_cb_read_get(), (void*)0x12345678);
        _cli_init();
    }

#if defined(CFG_USB_CDC_ENABLE)
    extern void usb_cdc_start(int fd_console);
    usb_cdc_start(fd_console);
#endif

    hal_hwtimer_init();

#if defined(CFG_BLE_ENABLE)
    #if defined(CONFIG_AUTO_PTS)
    pts_uart_init(1,115200,8,1,0,0);
    // Initialize BLE controller
    ble_controller_init(configMAX_PRIORITIES - 1);
    extern int hci_driver_init(void);
    // Initialize BLE Host stack
    hci_driver_init();

    tester_send(BTP_SERVICE_ID_CORE, CORE_EV_IUT_READY, BTP_INDEX_NONE,
            NULL, 0);
    #else
    ble_init();
    #endif
#endif

#if defined(CFG_ZIGBEE_ENABLE)
    zigbee_init();
#endif

    aos_register_event_filter(EV_KEY, event_cb_key_event, NULL);

    aos_loop_run();

    puts("------------------------------------------\r\n");
    puts("+++++++++Critical Exit From Loop++++++++++\r\n");
    puts("******************************************\r\n");
    vTaskDelete(NULL);
}

#if 0
static void proc_hellow_entry(void *pvParameters)
{
    vTaskDelay(500);

    while (1) {
        printf("%s: RISC-V rv32imafc\r\n", __func__);
        vTaskDelay(10000);
    }
    vTaskDelete(NULL);
}
#endif

static void _dump_boot_info(void)
{
    char chip_feature[40];
    const char *banner;

    puts("Booting BL702 Chip...\r\n");

    /*Display Banner*/
    if (0 == bl_chip_banner(&banner)) {
//        puts(banner);
    }
    puts("\r\n");
    /*Chip Feature list*/
    puts("\r\n");
    puts("------------------------------------------------------------\r\n");
    puts("RISC-V Core Feature:");
    bl_chip_info(chip_feature);
    puts(chip_feature);
    puts("\r\n");

    puts("Build Version: ");
    puts(BL_SDK_VER); // @suppress("Symbol is not resolved")
    puts("\r\n");

    puts("Std BSP Driver Version: ");
    puts(BL_SDK_STDDRV_VER); // @suppress("Symbol is not resolved")
    puts("\r\n");

    puts("Std BSP Common Version: ");
    puts(BL_SDK_STDCOM_VER); // @suppress("Symbol is not resolved")
    puts("\r\n");

    puts("RF Version: ");
    puts(BL_SDK_RF_VER); // @suppress("Symbol is not resolved")
    puts("\r\n");

#if defined(CFG_BLE_ENABLE)
    puts("BLE Controller LIB Version: ");
    puts(ble_controller_get_lib_ver());
    puts("\r\n");
#endif

#if defined(CFG_ZIGBEE_ENABLE)
    puts("Zigbee LIB Version: ");
    puts(zb_getLibVer());
    puts("\r\n");
#endif

    puts("Build Date: ");
    puts(__DATE__);
    puts("\r\n");
    puts("Build Time: ");
    puts(__TIME__);
    puts("\r\n");
    puts("------------------------------------------------------------\r\n");

}

static void system_init(void)
{
    blog_init();
    bl_irq_init();
    #if defined(CONFIG_HW_SEC_ENG_DISABLE)
    //if sec engine is disabled, use software rand in bl_rand
    int seed = bl_timer_get_current_time();
    srand(seed);
    #endif
#if defined(CFG_ETHERNET_ENABLE) || defined(CFG_BLE_ENABLE) || defined(CFG_ZIGBEE_ENABLE)
	bl_sec_init();
#endif /*CFG_ETHERNET_ENABLE*/
    bl_rtc_init();
    hal_boot2_init();

    /* board config is set after system is init*/
    hal_board_cfg(0);
    hal_pds_init();
    hal_tcal_init();

    //bl_wdt_init(4000);

#if defined(CFG_ZIGBEE_HBN)
    if(bl_sys_rstinfo_get() == BL_RST_POR){
        bl_hbn_fastboot_init();
        extern struct _zbRxPacket rxPktStoredInHbnRam;
        memset(&rxPktStoredInHbnRam, 0, sizeof(struct _zbRxPacket));
    }
#endif

#if defined(CFG_USE_PSRAM)
    bl_psram_init();
    vPortDefineHeapRegionsPsram(xHeapRegionsPsram);
#endif /*CFG_USE_PSRAM*/
}

static void system_thread_init()
{
    /*nothing here*/
}

void rf_reset_done_callback(void)
{
#if !defined(CFG_ZIGBEE_HBN)
    hal_tcal_restart();
#endif
}

void setup_heap()
{
    bl_sys_em_config();

    // Invoked during system boot via start.S
    vPortDefineHeapRegions(xHeapRegions);
}

void bl702_main()
{
    static StackType_t aos_loop_proc_stack[1024];
    static StaticTask_t aos_loop_proc_task;
    //static StackType_t proc_hellow_stack[512];
    //static StaticTask_t proc_hellow_task;

    bl_sys_early_init();

    /*Init UART In the first place*/
    hosal_uart_init(&uart_stdio);
    puts("Starting bl702 now....\r\n");

    bl_sys_init();

    _dump_boot_info();

    printf("Heap %u@%p, %u@%p"
#if defined(CFG_USE_PSRAM)
            ", %u@%p"
#endif /*CFG_USE_PSRAM*/
            "\r\n",
            (unsigned int)&_heap_size, &_heap_start,
            (unsigned int)&_heap2_size, &_heap2_start
#if defined(CFG_USE_PSRAM)
            ,(unsigned int)&_heap3_size, &_heap3_start
#endif /*CFG_USE_PSRAM*/
    );

    system_init();
    system_thread_init();

    //puts("[OS] Starting proc_hellow_entry task...\r\n");
    //xTaskCreateStatic(proc_hellow_entry, (char*)"hellow", sizeof(proc_hellow_stack)/4, NULL, 15, proc_hellow_stack, &proc_hellow_task);
    puts("[OS] Starting aos_loop_proc task...\r\n");

#if defined(CONFIG_AUTO_PTS)
    tester_init();
#endif
    xTaskCreateStatic(aos_loop_proc, (char*)"event_loop", sizeof(aos_loop_proc_stack)/4, NULL, 15, aos_loop_proc_stack, &aos_loop_proc_task);

#ifdef CFG_ETHERNET_ENABLE
    tcpip_init(NULL, NULL);
    lwip_init_netif();
#endif /*CFG_ETHERNET_ENABLE*/
    puts("[OS] Starting OS Scheduler...\r\n");
    vTaskStartScheduler();
}
