/**
****************************************************************************************
* @file user_peripheral.c
* @brief Peripheral project source code.
****************************************************************************************
*/
/**
****************************************************************************************
* @addtogroup APP
* @{
****************************************************************************************
*/
//#include "arch_console.h"
#include "user_peripheral.h"
#include "user_periph_setup.h"
#include "user_custs1_impl.h"
#include "user_custs1_def.h"
#include "user_wakeup.h"
#include "user_xl_driver.h"

/* Duration of timer for connection parameter update request */
#define APP_PARAM_UPDATE_REQUEST_TO         (1000)   // 1000*10ms = 10sec, The maximum allowed value is 41943sec (4194300 * 10ms)

/* Advertising data update timer */
#define APP_ADV_DATA_UPDATE_TO              (1)   // 3000*10ms = 30sec, The maximum allowed value is 41943sec (4194300 * 10ms)

#define XL_AVERAGE_NUM_SAMPLES              (16) //8

/* Manufacturer specific data constants */
#define APP_AD_MSD_COMPANY_ID               (0x00)
#define APP_AD_MSD_COMPANY_ID_LEN           (1)
#define APP_AD_MSD_DATA_NUM_BYTES            5
#define APP_AD_MSD_DATA_LEN                 (APP_AD_MSD_DATA_NUM_BYTES*sizeof(uint8_t))

volatile int8_t xDataArray[XL_AVERAGE_NUM_SAMPLES];
volatile int8_t yDataArray[XL_AVERAGE_NUM_SAMPLES];
volatile int8_t zDataArray[XL_AVERAGE_NUM_SAMPLES];

uint8_t xlDataArrayCounter;
bool gestureLockedOut = false;
bool gestureDisplayReset = false;
volatile uint8_t gestureCounter = 0;
bool continueUpdatingAdvertisementData = true;
bool deviceWokeUpStartCountdownToSleep = true;

// Manufacturer Specific Data ADV structure type
struct mnf_specific_data_ad_structure {
    uint8_t ad_structure_size;
    uint8_t ad_structure_type;
    // uint8_t company_id[APP_AD_MSD_COMPANY_ID_LEN];
    uint8_t proprietary_data[3];
};

// GLOBAL VARIABLE DEFINITIONS
uint8_t app_connection_idx                       __SECTION_ZERO("retention_mem_area0");
timer_hnd app_adv_data_update_timer_used         __SECTION_ZERO("retention_mem_area0");
timer_hnd app_param_update_request_timer_used    __SECTION_ZERO("retention_mem_area0");
ke_msg_id_t timer_used                           __SECTION_ZERO("retention_mem_area0"); //@RETENTION MEMORY
timer_hnd app_gesture_direction_timer_used      __SECTION_ZERO("retention_mem_area0"); //@RETENTION MEMORY
timer_hnd app_gesture_display_reset_timer_used  __SECTION_ZERO("retention_mem_area0"); //@RETENTION MEMORY
timer_hnd app_motor_on_timer_used  				__SECTION_ZERO("retention_mem_area0"); //@RETENTION MEMORY

// Retained variables
struct mnf_specific_data_ad_structure mnf_data   __SECTION_ZERO("retention_mem_area0"); //@RETENTION MEMORY
// Index of manufacturer data in advertising data or scan response data (when MSB is 1)
uint8_t mnf_data_index                           __SECTION_ZERO("retention_mem_area0"); //@RETENTION MEMORY
uint8_t stored_adv_data_len                      __SECTION_ZERO("retention_mem_area0"); //@RETENTION MEMORY
uint8_t stored_scan_rsp_data_len                 __SECTION_ZERO("retention_mem_area0"); //@RETENTION MEMORY
uint8_t stored_adv_data[ADV_DATA_LEN]            __SECTION_ZERO("retention_mem_area0"); //@RETENTION MEMORY
uint8_t stored_scan_rsp_data[SCAN_RSP_DATA_LEN]  __SECTION_ZERO("retention_mem_area0"); //@RETENTION MEMORY

/*
 * FUNCTION DEFINITIONS
 ****************************************************************************************
*/

/**
 ****************************************************************************************
 * @brief Gesture display reset timer callback function.
 ****************************************************************************************
*/
static void gesture_display_reset_timer_cb()
{
	//arch_printf("RESETTING THE GESTURE COUNTER AND DISPLAY\n\r");	
	for(int i=0; i<(APP_AD_MSD_DATA_NUM_BYTES-2);i++)
	{
		mnf_data.proprietary_data[i] = 0;
	}
	gestureCounter = 0;	
	gestureDisplayReset = false;
	//arch_printf("gesture display reset called\n\r");
	user_app_going_to_sleep();
}

/**
 ****************************************************************************************
 * @brief Lockout the gesture recognition timer callback function.
 ****************************************************************************************
*/
static void gesture_lockout_timer_cb()
{
	gestureLockedOut = false;
}

/**
****************************************************************************************
* @brief Initialize Manufacturer Specific Data
****************************************************************************************
*/
static void mnf_data_init() {
    mnf_data.ad_structure_size = sizeof(struct mnf_specific_data_ad_structure ) - sizeof(uint8_t); // minus the size of the ad_structure_size field
    mnf_data.ad_structure_type = GAP_AD_TYPE_MANU_SPECIFIC_DATA;
    // mnf_data.company_id[0] = APP_AD_MSD_COMPANY_ID & 0xFF; // LSB
    // mnf_data.company_id[1] = (APP_AD_MSD_COMPANY_ID >> 8 )& 0xFF; // MSB
    mnf_data.proprietary_data[0] = 0;
    mnf_data.proprietary_data[1] = 0;
    mnf_data.proprietary_data[2] = 0;
    gestureCounter = 0;
    xlDataArrayCounter = 0;
}

/**
****************************************************************************************
* @brief Update Manufacturer Specific Data
****************************************************************************************
*/
static void mnf_data_update() {
    // uint16_t data;
    // data = mnf_data.proprietary_data[0] | (mnf_data.proprietary_data[1] << 8);
    // data += 1;
    // mnf_data.proprietary_data[0] = data & 0xFF;
    // mnf_data.proprietary_data[1] = (data >> 8) & 0xFF;

    // if (data == 0xFFFF) {
    //     mnf_data.proprietary_data[0] = 0;
    //     mnf_data.proprietary_data[1] = 0;
    // }
    
    int xData = 0;
    int	yData = 0;
    int zData = 0;

    for(int imCounting=0; imCounting<XL_AVERAGE_NUM_SAMPLES;imCounting++)
    {
        xDataArray[imCounting] *= 0.9;
        yDataArray[imCounting] *= 0.9;
        zDataArray[imCounting] *= 0.9;
    }

    xDataArray[xlDataArrayCounter] = (i2c_XL_Read_X()>>8);
    yDataArray[xlDataArrayCounter] = (i2c_XL_Read_Y()>>8);
    zDataArray[xlDataArrayCounter] = (i2c_XL_Read_Z()>>8);

    
    for(int i=0; i<XL_AVERAGE_NUM_SAMPLES;i++)
    {
        xData += xDataArray[i];
        yData += yDataArray[i];
        zData += zDataArray[i];
    }
    
    xData = (xData/XL_AVERAGE_NUM_SAMPLES);
    yData = (yData/XL_AVERAGE_NUM_SAMPLES);
    zData = (zData/XL_AVERAGE_NUM_SAMPLES);

    if(xlDataArrayCounter<XL_AVERAGE_NUM_SAMPLES)
    {
        xlDataArrayCounter++;
    }
    else
    {
        xlDataArrayCounter = 0;
    }
    int8_t deltaData[3];
    volatile int8_t threshholdValue = 15;
    deltaData[0] = xData;
    deltaData[1] = yData;	
    volatile int8_t minMaxValue = 0;
    volatile int8_t largestDelta = 0;
    for(int i=0;i<2;i++)
    {		
        if(abs(deltaData[i])>minMaxValue)
        {
            minMaxValue = abs(deltaData[i]);
            largestDelta = i;
        }
    }
    deltaData[2] = largestDelta;
    if((abs(deltaData[0])>threshholdValue)||(abs(deltaData[1])>threshholdValue))
    {
        if(!gestureLockedOut)
        {
            volatile uint8_t newGestureData = 0x00;
            switch(deltaData[2])
            {
                case 0:
                    // arch_printf("\n\r*****\n\r**X** x:%d, y:%d, z:%d, gestureSlot:%d\n\r*****\n\r\n\r",deltaData[0],deltaData[1],deltaData[2],gestureCounter);
                    newGestureData = ((deltaData[0]>0)?0x4C:0x52);
                break;
                case 1:
                    // arch_printf("\n\r*****\n\r**Y** x:%d, y:%d, z:%d, gestureSlot:%d\n\r*****\n\r\n\r",deltaData[0],deltaData[1],deltaData[2],gestureCounter);
                    newGestureData = ((deltaData[1]>0)?0x44:0x55);
                break;
                default:
                break;
            }
            mnf_data.proprietary_data[gestureCounter] = newGestureData;
            if(gestureCounter<(APP_AD_MSD_DATA_NUM_BYTES-1))
            {
                gestureCounter++;
            }
            else
            {
                gestureCounter = 0;
            }
            gestureLockedOut = true;
            app_gesture_direction_timer_used = app_easy_timer(50, gesture_lockout_timer_cb);
            if(gestureDisplayReset)
            {
                app_easy_timer_cancel(app_gesture_display_reset_timer_used);
            }
            app_gesture_display_reset_timer_used = app_easy_timer(300, gesture_display_reset_timer_cb);
            gestureDisplayReset = true;
        }
    }
    if(deviceWokeUpStartCountdownToSleep)
    {		
            if(gestureDisplayReset)
            {
                    app_easy_timer_cancel(app_gesture_display_reset_timer_used);
            }
            app_gesture_display_reset_timer_used = app_easy_timer(300, gesture_display_reset_timer_cb);
            gestureDisplayReset = true;
            deviceWokeUpStartCountdownToSleep = false;
    }
}

/**
****************************************************************************************
* @brief Add an AD structure in the Advertising or Scan Response Data of the
*        GAPM_START_ADVERTISE_CMD parameter struct.
* @param[in] cmd               GAPM_START_ADVERTISE_CMD parameter struct
* @param[in] ad_struct_data    AD structure buffer
* @param[in] ad_struct_len     AD structure length
* @param[in] adv_connectable   Connectable advertising event or not. It controls whether
*                              the advertising data use the full 31 bytes length or only
*                              28 bytes (Document CCSv6 - Part 1.3 Flags).
****************************************************************************************
*/
static void app_add_ad_struct(struct gapm_start_advertise_cmd *cmd, void *ad_struct_data, uint8_t ad_struct_len, uint8_t adv_connectable) {
    uint8_t adv_data_max_size = (adv_connectable) ? (ADV_DATA_LEN - 3) : (ADV_DATA_LEN);

    if ((adv_data_max_size - cmd->info.host.adv_data_len) >= ad_struct_len) {
        // Append manufacturer data to advertising data
        memcpy(&cmd->info.host.adv_data[cmd->info.host.adv_data_len], ad_struct_data, ad_struct_len);

        // Update Advertising Data Length
        cmd->info.host.adv_data_len += ad_struct_len;

        // Store index of manufacturer data which are included in the advertising data
        mnf_data_index = cmd->info.host.adv_data_len - sizeof(struct mnf_specific_data_ad_structure);
    }
    else if ((SCAN_RSP_DATA_LEN - cmd->info.host.scan_rsp_data_len) >= ad_struct_len) {
        // Append manufacturer data to scan response data
        memcpy(&cmd->info.host.scan_rsp_data[cmd->info.host.scan_rsp_data_len], ad_struct_data, ad_struct_len);

        // Update Scan Response Data Length
        cmd->info.host.scan_rsp_data_len += ad_struct_len;

        // Store index of manufacturer data which are included in the scan response data
        mnf_data_index = cmd->info.host.scan_rsp_data_len - sizeof(struct mnf_specific_data_ad_structure);
        // Mark that manufacturer data is in scan response and not advertising data
        mnf_data_index |= 0x80;
    }
    else {
        // Manufacturer Specific Data do not fit in either Advertising Data or Scan Response Data
        ASSERT_WARNING(0);
    }
    stored_adv_data_len = cmd->info.host.adv_data_len;                                    // Store advertising data length
    memcpy(stored_adv_data, cmd->info.host.adv_data, stored_adv_data_len);                // Store advertising data
    stored_scan_rsp_data_len = cmd->info.host.scan_rsp_data_len;                          // Store scan response data length
    memcpy(stored_scan_rsp_data, cmd->info.host.scan_rsp_data, stored_scan_rsp_data_len); // Store scan_response data
}

/**
****************************************************************************************
* @brief Advertisement data update timer callback function.
****************************************************************************************
*/
static void adv_data_update_timer_cb() {
    // If mnd_data_index has MSB set, manufacturer data is stored in scan response
    uint8_t *mnf_data_storage = (mnf_data_index & 0x80) ? stored_scan_rsp_data : stored_adv_data;

    // Update manufacturer data
    mnf_data_update();

    // Update the selected fields of the advertising data (manufacturer data)
    memcpy(mnf_data_storage + (mnf_data_index & 0x7F), &mnf_data, sizeof(struct mnf_specific_data_ad_structure));

    // Update advertising data on the fly
    app_easy_gap_update_adv_data(stored_adv_data, stored_adv_data_len, stored_scan_rsp_data, stored_scan_rsp_data_len);

    // Restart timer for the next advertising update
    // app_adv_data_update_timer_used = app_easy_timer(APP_ADV_DATA_UPDATE_TO, adv_data_update_timer_cb);
    // Restart timer for the next advertising update
	if(continueUpdatingAdvertisementData)
	{
		app_adv_data_update_timer_used = app_easy_timer(APP_ADV_DATA_UPDATE_TO, adv_data_update_timer_cb);
	}
}

/**
****************************************************************************************
* @brief Parameter update request timer callback function.
****************************************************************************************
*/
static void param_update_request_timer_cb() {
    app_easy_gap_param_update_start(app_connection_idx);
    app_param_update_request_timer_used = EASY_TIMER_INVALID_TIMER;
}

void app_adcval1_timer_cb_handler() 
{
    char data[65];
    struct bd_addr mac_addr;
    
    // if (nvds_get(NVDS_TAG_BD_ADDRESS, mac_addr.addr, NULL) == NVDS_OK)
    // {
    //     sprintf(data, "{\"device_id\": \"%02X:%02X:%02X:%02X:%02X:%02X\", \"gesture_id\": \"swand_001\"}", 
    //             mac_addr.addr[5], mac_addr.addr[4], mac_addr.addr[3], 
    //             mac_addr.addr[2], mac_addr.addr[1], mac_addr.addr[0]);
    // }
    // else
    // {
    //     sprintf(data, "{\"device_id\": \"UNKNOWN\", \"gesture_id\": \"swand_001\"}");
    // }
    if (nvds_get(NVDS_TAG_BD_ADDRESS, mac_addr.addr, NULL) == NVDS_OK)
    {
        sprintf(data, "{\"device_id\": \"%02X:%02X:%02X:%02X:%02X:%02X\", \"gesture_id\": \"%02X\"}", 
                mac_addr.addr[5], mac_addr.addr[4], mac_addr.addr[3], 
                mac_addr.addr[2], mac_addr.addr[1], mac_addr.addr[0], mnf_data.proprietary_data[gestureCounter]);
    }
    else
    {
        sprintf(data, "{\"device_id\": \"UNKNOWN\", \"gesture_id\": \"%02X\"}", mnf_data.proprietary_data[gestureCounter]);
    }

    struct custs1_val_ntf_ind_req *req = KE_MSG_ALLOC_DYN(CUSTS1_VAL_NTF_REQ,
                                                            prf_get_task_from_id(TASK_ID_CUSTS1),
                                                            TASK_APP,
                                                            custs1_val_ntf_ind_req,
                                                            strlen(data));

    req->handle = SVC1_IDX_ADC_VAL_1_VAL;      // Location to send it to
    req->length = strlen(data);
    req->notification = true;
    memcpy(req->value, data, strlen(data));
    ke_msg_send(req);

    if (ke_state_get(TASK_APP) == APP_CONNECTED) {timer_used = app_easy_timer(100, app_adcval1_timer_cb_handler);};
}

void user_app_init(void)
{
    app_param_update_request_timer_used = EASY_TIMER_INVALID_TIMER;
    continueUpdatingAdvertisementData = true;
    user_wakeup_example_init();
    mnf_data_init();
    memcpy(stored_adv_data, USER_ADVERTISE_DATA, USER_ADVERTISE_DATA_LEN); 
    stored_adv_data_len = USER_ADVERTISE_DATA_LEN;
    memcpy(stored_scan_rsp_data, USER_ADVERTISE_SCAN_RESPONSE_DATA, USER_ADVERTISE_SCAN_RESPONSE_DATA_LEN);
    stored_scan_rsp_data_len = USER_ADVERTISE_SCAN_RESPONSE_DATA_LEN;
    default_app_on_init();
    i2c_XL_initialize();
}

void user_app_adv_start(void) {
    // Schedule the next advertising data update
    app_adv_data_update_timer_used = app_easy_timer(APP_ADV_DATA_UPDATE_TO, adv_data_update_timer_cb);

    struct gapm_start_advertise_cmd* cmd;
    cmd = app_easy_gap_undirected_advertise_get_active();

    // Add manufacturer data to initial advertising or scan response data, if there is enough space
    app_add_ad_struct(cmd, &mnf_data, sizeof(struct mnf_specific_data_ad_structure), 1);
    app_easy_gap_undirected_advertise_start();
}

void user_app_connection(uint8_t connection_idx, struct gapc_connection_req_ind const *param) {
    if (app_env[connection_idx].conidx != GAP_INVALID_CONIDX) {
        app_connection_idx = connection_idx;
        app_easy_timer_cancel(app_adv_data_update_timer_used);             // Stop advertising data update timer
        if ((param->con_interval < user_connection_param_conf.intv_min) || // Check if parameters are preferred ones.
            (param->con_interval > user_connection_param_conf.intv_max) || // If not, schedule connect. param. update req.
            (param->con_latency != user_connection_param_conf.latency)  ||
            (param->sup_to != user_connection_param_conf.time_out)) {
            app_param_update_request_timer_used = app_easy_timer(APP_PARAM_UPDATE_REQUEST_TO, param_update_request_timer_cb);
        }
    }
    else {user_app_adv_start();} // No connection has been established, restart advertising
    default_app_on_connection(connection_idx, param);             // Default app callback on connection
    timer_used = app_easy_timer(100, app_adcval1_timer_cb_handler); // Begin collection of ADC readings
}

// If advertising was canceled then update advertising data and start advertising again
void user_app_adv_undirect_complete(uint8_t status) {if (status == GAP_ERR_CANCELED) {user_app_adv_start();}}


void user_app_disconnect(struct gapc_disconnect_ind const *param) {
    if (app_param_update_request_timer_used != EASY_TIMER_INVALID_TIMER) { // Cancel the parameter update request timer
        app_easy_timer_cancel(app_param_update_request_timer_used);
        app_param_update_request_timer_used = EASY_TIMER_INVALID_TIMER;
    }
    mnf_data_update();       // Update manufacturer data for the next advertsing event
    user_app_adv_start();    // Restart Advertising
}

void user_catch_rest_hndl(ke_msg_id_t const msgid,
                          void const *param,
                          ke_task_id_t const dest_id,
                          ke_task_id_t const src_id) {
    switch(msgid) {
        case CUSTS1_VAL_WRITE_IND: {
            struct custs1_val_write_ind const *msg_param = (struct custs1_val_write_ind const *) (param);
            switch (msg_param->handle) {
                case SVC1_IDX_CONTROL_POINT_VAL:
                    user_svc1_ctrl_wr_ind_handler(msgid, msg_param, dest_id, src_id);
                    break;
                default:
                    break;
            }
        } break;
        case GAPC_PARAM_UPDATED_IND: {
            // Cast the "param" pointer to the appropriate message structure
            struct gapc_param_updated_ind const *msg_param = (struct gapc_param_updated_ind const *)(param);
            // Check if updated Conn Params filled to preferred ones
            if ((msg_param->con_interval >= user_connection_param_conf.intv_min) &&
                (msg_param->con_interval <= user_connection_param_conf.intv_max) &&
                (msg_param->con_latency == user_connection_param_conf.latency) &&
                (msg_param->sup_to == user_connection_param_conf.time_out))
            {
            }
        } break;
        case CUSTS1_VALUE_REQ_IND:
        {
            struct custs1_value_req_ind const *msg_param = (struct custs1_value_req_ind const *) param;

            switch (msg_param->att_idx) {
                case SVC3_IDX_READ_4_VAL: {user_svc3_read_non_db_val_handler(msgid, msg_param, dest_id, src_id);} break;
                default: {
                    // Send Error message
                    struct custs1_value_req_rsp *rsp = KE_MSG_ALLOC(CUSTS1_VALUE_REQ_RSP,
                                                                    src_id,
                                                                    dest_id,
                                                                    custs1_value_req_rsp);
                    rsp->conidx  = app_env[msg_param->conidx].conidx;
                    rsp->att_idx = msg_param->att_idx;
                    rsp->length = 0;
                    rsp->status  = ATT_ERR_APP_ERROR;
                    ke_msg_send(rsp);
                } break;
             }
        } break;
        default: break;
    }
}

/**
 ****************************************************************************************
 * @brief Gesture display reset timer callback function.
 ****************************************************************************************
*/
static void motor_on_timer_cb()
{
	// arch_printf("Motor stopping\n\r");	
	GPIO_SetInactive(MOTOR_PORT, MOTOR_PIN);
	continueUpdatingAdvertisementData = true;
	app_adv_data_update_timer_used = app_easy_timer(APP_ADV_DATA_UPDATE_TO, adv_data_update_timer_cb);
	deviceWokeUpStartCountdownToSleep = true;
}

void user_app_waking_from_sleep(void)
{
	i2c_XL_initialize();
	// arch_printf("Motor starting\n\r");	
	app_motor_on_timer_used = app_easy_timer(100, motor_on_timer_cb);
	GPIO_SetActive(MOTOR_PORT, MOTOR_PIN);
}
void user_app_going_to_sleep(void)
{
	continueUpdatingAdvertisementData = false;
	i2c_XL_Sleep_Mode();
	// arch_printf("\n\rSystem going to sleep",NULL);

	app_easy_gap_advertise_stop();

	arch_set_sleep_mode(ARCH_EXT_SLEEP_ON);				
	arch_ble_ext_wakeup_on();
	user_reset_event_counter(); // When callback is triggerd the event counter is not set 0 for the 531, that is why this function is called. 		
}  

/// @} APP
