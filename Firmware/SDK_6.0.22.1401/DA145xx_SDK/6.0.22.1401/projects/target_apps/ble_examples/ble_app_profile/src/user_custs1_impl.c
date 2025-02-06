/**
 ****************************************************************************************
 * @file user_custs1_impl.c
 * @brief Peripheral project Custom1 Server implementation source code.
 ****************************************************************************************
*/

#include "user_custs1_def.h"
#include "user_custs1_impl.h"

uint16_t indication_counter __SECTION_ZERO("retention_mem_area0"); //@RETENTION MEMORY
uint16_t non_db_val_counter __SECTION_ZERO("retention_mem_area0"); //@RETENTION MEMORY

/*
 * FUNCTION DEFINITIONS
 ****************************************************************************************
 */
void user_svc1_ctrl_wr_ind_handler(ke_msg_id_t const msgid,
                                   struct custs1_val_write_ind const *param,
                                   ke_task_id_t const dest_id,
                                   ke_task_id_t const src_id) {
    uint8_t val = 0;
    memcpy(&val, &param->value[0], param->length);
    SetBits16(SYS_CTRL_REG, SW_RESET, 1);
}

void user_svc3_read_non_db_val_handler(ke_msg_id_t const msgid,
                                       struct custs1_value_req_ind const *param,
                                       ke_task_id_t const dest_id,
                                       ke_task_id_t const src_id) {
    // Increase value by one
    non_db_val_counter++;

    struct custs1_value_req_rsp *rsp = KE_MSG_ALLOC_DYN(CUSTS1_VALUE_REQ_RSP,
                                                        prf_get_task_from_id(TASK_ID_CUSTS1),
                                                        TASK_APP,
                                                        custs1_value_req_rsp,
                                                        DEF_SVC3_READ_VAL_4_CHAR_LEN);
    rsp->conidx  = app_env[param->conidx].conidx;
    rsp->att_idx = param->att_idx;
    rsp->length  = sizeof(non_db_val_counter);
    rsp->status  = ATT_ERR_NO_ERROR;
    memcpy(&rsp->value, &non_db_val_counter, rsp->length);
    ke_msg_send(rsp);
}
