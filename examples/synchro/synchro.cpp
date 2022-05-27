/*
 * Copyright (C) 2016 Orange
 *
 * This software is distributed under the terms and conditions of the
 * 'BSD-3-Clause'
 * license which can be found in the file 'LICENSE.txt' in this package
 * distribution
 * or at 'https://opensource.org/licenses/BSD-3-Clause'.
 */

/**
 * @file  basic.c
 * @brief A simple user application using all available LiveObjects
 * iotsotbox-mqtt features
 */

#include <iostream>
#include <pthread.h>
#include <thread>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <unistd.h>

/* Raspberry Pi GPIO */
#include <wiringPi.h>

#include "config/liveobjects_dev_params.h"
#include "liveobjects_iotsoftbox_api.h"
#include "../../mqtt_live_objects/LiveObjects-iotSoftbox-mqtt-core/liveobjects-client/LiveObjectsClient_Defs.h"

/* Default LiveObjects device settings : name space and device identifier*/
#define LOC_CLIENT_DEV_NAME_SPACE            "Langevin-Wallon"

#define LOC_CLIENT_DEV_ID                    "NewLinuxPi"

/** Here, set your LiveObject Apikey. It is mandatory to run the application
 *
 * C_LOC_CLIENT_DEV_API_KEY_P1 must be the first sixteen char of the ApiKey
 * C_LOC_CLIENT_DEV_API_KEY_P1 must be the last sixteen char of the ApiKey
 *
 * If your APIKEY is 0123456789abcdeffedcba9876543210 then
 * it should look like this :
 *
 * #define C_LOC_CLIENT_DEV_API_KEY_P1			0x0123456789abcdef
 * #define C_LOC_CLIENT_DEV_API_KEY_P2			0xfedcba9876543210
 *
 * */

#define C_LOC_CLIENT_DEV_API_KEY_P1            0x0408221d2fd3423a
#define C_LOC_CLIENT_DEV_API_KEY_P2            0x8b3b3547b40368b2

#define DBG_DFT_MAIN_LOG_LEVEL 0
#define DBG_DFT_LOMC_LOG_LEVEL 0

// set 0x0F to have all message dump (text+hexa)
#define DBG_DFT_MSG_DUMP         0xF

#define APPV_VERSION "LINUX BASIC SAMPLE V01.2"
#define LOM_BUILD_TAG "BUILD LiveObjects IoT Basic 1.1"

#define LECTURE      0
#define COMMANDE     2
#define READY        3

uint8_t appv_log_level = DBG_DFT_MAIN_LOG_LEVEL;

// ==========================================================
//
// Live Objects IoT Client object (using iotsoftbox-mqtt library)
//
// - status information at connection
// - collected data to send
// - supported configuration parameters
// - supported commands
// - resources declaration (firmware, text file, etc.)

// ----------------------------------------------------------
// STATUS data
//

int32_t appv_status_counter = 0;
char appv_status_message[150] = "READY";

/// Set of status
LiveObjectsD_Data_t appv_set_status[] = {
        {LOD_TYPE_STRING_C, "sample_version", (void *) APPV_VERSION, 1},
        {LOD_TYPE_INT32,    "sample_counter", &appv_status_counter,  1},
        {LOD_TYPE_STRING_C, "sample_message", appv_status_message,   1}
};
#define SET_STATUS_NB (sizeof(appv_set_status) / sizeof(LiveObjectsD_Data_t))

int appv_hdl_status = -1;

// ----------------------------------------------------------
// 'COLLECTED DATA'
//
#define STREAM_PREFIX 0

uint8_t appv_measures_enabled = 1;

// contains a counter incremented after each data sent
uint32_t appvCounter = 0;


double appvTSApres = 0;
double appvTSAvant = 0;


/// Set of Collected data (published on a data stream)
LiveObjectsD_Data_t appv_set_measures[] = {
        {LOD_TYPE_UINT8,  "counter",         &appvCounter, 1},
        {LOD_TYPE_DOUBLE, "Timestamp Avant", &appvTSAvant, 1},
        {LOD_TYPE_DOUBLE, "Timestamp Apres", &appvTSApres, 1}
};

#define SET_MEASURES_NB (sizeof(appv_set_measures) / sizeof(LiveObjectsD_Data_t))


int appv_hdl_data = -1;

// ----------------------------------------------------------
// CONFIGURATION data
//
uint32_t appv_cfg_timeout = 10;

// a structure containing various kind of parameters (char[], int and float)
struct conf_s {
    char name[20];
    int32_t threshold;
    float gain;
} appv_conf = {"TICTAC", -3, 1.05};

// definition of identifier for each kind of parameters
#define PARM_IDX_NAME 1
#define PARM_IDX_TIMEOUT 2
#define PARM_IDX_THRESHOLD 3
#define PARM_IDX_GAIN 4

/// Set of configuration parameters
LiveObjectsD_Param_t appv_set_param[] = {
        {PARM_IDX_NAME,      {LOD_TYPE_STRING_C, "name",      appv_conf.name,             1}},
        {PARM_IDX_TIMEOUT,   {LOD_TYPE_UINT32,   "timeout",   (void *) &appv_cfg_timeout, 1}},
        {PARM_IDX_THRESHOLD, {LOD_TYPE_INT32,    "threshold", &appv_conf.threshold,       1}},
        {PARM_IDX_GAIN,      {LOD_TYPE_FLOAT,    "gain",      &appv_conf.gain,            1}}
};
#define SET_PARAM_NB (sizeof(appv_set_param) / sizeof(LiveObjectsD_Param_t))

// ----------------------------------------------------------
// COMMANDS
// All the data are simulated
// Digital output to change the status of the RED LED
int32_t app_led_user = 0;

/// counter used to postpone the LED command response
static int cmd_cnt = 0;

#define CMD_IDX_RESET 1
#define CMD_IDX_LED 2

/// set of commands
LiveObjectsD_Command_t appv_set_commands[] = {
        {CMD_IDX_RESET, "RESET", 0},
        {CMD_IDX_LED,   "LED",   0}
};
#define SET_COMMANDS_NB (sizeof(appv_set_commands) / sizeof(LiveObjectsD_Command_t))

// ----------------------------------------------------------
// RESOURCE data
//
char appv_rsc_image[5 * 1024] = "";

char appv_rv_message[10] = "01.00";
char appv_rv_image[10] = "01.00";

#define RSC_IDX_MESSAGE 1
#define RSC_IDX_IMAGE 2

/// Set of resources
LiveObjectsD_Resource_t appv_set_resources[] = {
        {RSC_IDX_MESSAGE, "message", appv_rv_message,
                                                    sizeof(appv_rv_message) -
                                                    1},  // resource used to update appv_status_message
        {RSC_IDX_IMAGE,   "image",   appv_rv_image, sizeof(appv_rv_image) - 1}
};
#define SET_RESOURCES_NB (sizeof(appv_set_resources) / sizeof(LiveObjectsD_Resource_t))

// variables used to process the current resource transfer
uint32_t appv_rsc_size = 0;
uint32_t appv_rsc_offset = 0;

// ==========================================================
// IotSoftbox-mqtt callback functions (in 'C' api)

LiveObjectsD_ResourceRespCode_t main_cb_rsc_ntfy(uint8_t state, const LiveObjectsD_Resource_t *rsc_ptr,
                                                 const char *version_old, const char *version_new, uint32_t size) {
    LiveObjectsD_ResourceRespCode_t ret = RSC_RSP_OK;  // OK to update the resource

    //printf("*** rsc_ntfy: ...\r\n");
    std::cout << "*** rsc_ntfy: ..." << std::endl;

    if ((rsc_ptr) && (rsc_ptr->rsc_uref > 0) && (rsc_ptr->rsc_uref <= SET_RESOURCES_NB)) {
        /*printf("***   user ref     = %d\r\n", rsc_ptr->rsc_uref);
		printf("***   name         = %s\r\n", rsc_ptr->rsc_name);
		printf("***   version_old  = %s\r\n", version_old);
		printf("***   version_new  = %s\r\n", version_new);
		printf("***   size         = %"PRIu32"\r\n", size);*/
        std::cout << "***   user ref     = " << rsc_ptr->rsc_uref << std::endl;
        std::cout << "***   name     = " << rsc_ptr->rsc_name << std::endl;
        std::cout << "***   version_old     = " << version_old << std::endl;
        std::cout << "***   version_new     = " << version_new << std::endl;
        std::cout << "***   size     = " << size << std::endl;

        if (state) {
            if (state == 1) {  // Completed without error
                /*printf("***   state        = COMPLETED without error\r\n");*/
                std::cout << "***   state        = COMPLETED without error" << std::endl;
                // Update version
                /*printf(" ===> UPDATE - version %s to %s\r\n", rsc_ptr->rsc_version_ptr, version_new);*/
                std::cout << " ===> UPDATE - version " << rsc_ptr->rsc_version_ptr << " to " << version_new
                          << std::endl;
                strncpy((char *) rsc_ptr->rsc_version_ptr, version_new, rsc_ptr->rsc_version_sz);

                if (rsc_ptr->rsc_uref == RSC_IDX_IMAGE) {
                    /*printf("\r\n\r\n");
                    printf("%s", appv_rsc_image);
                    printf("\r\n\r\n");*/
                    std::cout << std::endl << std::endl << appv_rsc_image << std::endl << std::endl;

                }
            }
            else {
                /*printf("***   state        = COMPLETED with error !!!!\r\n");*/
                std::cout << "***   state        = COMPLETED with error" << std::endl;
                // Roll back ?
            }
            appv_rsc_offset = 0;
            appv_rsc_size = 0;

            // Push Status (message has been updated or not)
            LiveObjectsClient_PushStatus(appv_hdl_status);
        }
        else {
            appv_rsc_offset = 0;
            ret = RSC_RSP_ERR_NOT_AUTHORIZED;
            switch (rsc_ptr->rsc_uref) {
                case RSC_IDX_MESSAGE:
                    if (size < (sizeof(appv_status_message) - 1)) {
                        ret = RSC_RSP_OK;
                    }
                    break;
                case RSC_IDX_IMAGE:
                    if (size < (sizeof(appv_rsc_image) - 1)) {
                        ret = RSC_RSP_OK;
                    }
                    break;
            }
            if (ret == RSC_RSP_OK) {
                appv_rsc_size = size;
/*				printf("***   state        = START - ACCEPTED\r\n");*/
                std::cout << "***   state        = START - ACCEPTED" << std::endl;
            }
            else {
                appv_rsc_size = 0;
                /*printf("***   state        = START - REFUSED\r\n");*/
                std::cout << "***   state        = START - REFUSED" << std::endl;
            }
        }
    }
    else {
        /*printf("***  UNKNOWN USER REF (x%p %d)  in state=%d\r\n", rsc_ptr, rsc_ptr->rsc_uref, state);*/
        std::cout << "***  UNKNOWN USER REF (x" << rsc_ptr << " " << rsc_ptr->rsc_uref << ")  in state=" << state
                  << std::endl;
        ret = RSC_RSP_ERR_INVALID_RESOURCE;
    }
    return ret;
}

/**
 * Called (by the LiveObjects thread) to request the user
 * to read data from current resource transfer.
 */
int main_cb_rsc_data(const LiveObjectsD_Resource_t *rsc_ptr, uint32_t offset) {
    int ret;

    if (appv_log_level > 1) {
        /*printf("*** rsc_data: rsc[%d]='%s' offset=%"PRIu32" - data ready ...\r\n", rsc_ptr->rsc_uref, rsc_ptr->rsc_name,offset);*/
        std::cout << "*** rsc_data: rsc[" << rsc_ptr->rsc_uref << "]='" << rsc_ptr->rsc_name << "' offset=" << offset
                  << " - data ready ..." << std::endl;
    }

    if (rsc_ptr->rsc_uref == RSC_IDX_MESSAGE) {
        char buf[40];
        if (offset > (sizeof(appv_status_message) - 1)) {
            /*	printf("*** rsc_data: rsc[%d]='%s' offset=%"PRIu32" > %zu - OUT OF ARRAY\r\n", rsc_ptr->rsc_uref, rsc_ptr->rsc_name,offset, sizeof(appv_status_message) - 1);*/
            std::cout << "*** rsc_data: rsc[" << rsc_ptr->rsc_uref << "]='" << rsc_ptr->rsc_name << "' offset="
                      << offset << " > " << sizeof(appv_status_message) - 1 << " - OUT OF ARRAY" << std::endl;
            return -1;
        }
        ret = LiveObjectsClient_RscGetChunck(rsc_ptr, buf, sizeof(buf) - 1);
        if (ret > 0) {
            if ((offset + ret) > (sizeof(appv_status_message) - 1)) {
                /*printf("*** rsc_data: rsc[%"PRIu32"]='%s' offset=%"PRIu32" - read=%d => %"PRIu32" > %zu - OUT OF ARRAY\r\n", rsc_ptr->rsc_uref, rsc_ptr->rsc_name, offset, ret, offset + ret,sizeof(appv_status_message) - 1);*/
                std::cout << "*** rsc_data: rsc[" << rsc_ptr->rsc_uref << "]='" << rsc_ptr->rsc_name << "' offset="
                          << offset << " - read=" << ret << " => " << offset + ret << " > "
                          << sizeof(appv_status_message) - 1 << " - OUT OF ARRAY" << std::endl;

                return -1;
            }
            appv_rsc_offset += ret;
            memcpy(&appv_status_message[offset], buf, ret);
            appv_status_message[offset + ret] = 0;
            /*printf("*** rsc_data: rsc[%d]='%s' offset=%"PRIu32" - read=%d/%zu '%s'\r\n", rsc_ptr->rsc_uref, rsc_ptr->rsc_name,offset, ret, sizeof(buf) - 1, appv_status_message);*/
            std::cout << "*** rsc_data: rsc[" << rsc_ptr->rsc_uref << "]='" << rsc_ptr->rsc_name << "' offset="
                      << offset << " - read=" << ret << "/" << sizeof(buf) - 1 << " '" << appv_status_message << "'"
                      << std::endl;
        }
    }
    else if (rsc_ptr->rsc_uref == RSC_IDX_IMAGE) {
        if (offset > (sizeof(appv_rsc_image) - 1)) {
            /*printf("*** rsc_data: rsc[%d]='%s' offset=%"PRIu32" > %zu - OUT OF ARRAY\r\n", rsc_ptr->rsc_uref, rsc_ptr->rsc_name,offset, sizeof(appv_rsc_image) - 1);*/
            std::cout << "*** rsc_data: rsc[" << rsc_ptr->rsc_uref << "]='" << rsc_ptr->rsc_name << "offset=" << offset
                      << " > " << sizeof(appv_rsc_image) - 1 << " - OUT OF ARRAY" << std::endl;
            return -1;
        }
        int data_len = sizeof(appv_rsc_image) - offset - 1;
        ret = LiveObjectsClient_RscGetChunck(rsc_ptr, &appv_rsc_image[offset], data_len);
        if (ret > 0) {
            if ((offset + ret) > (sizeof(appv_rsc_image) - 1)) {
                /*printf("*** rsc_data: rsc[%d]='%s' offset=%"PRIu32" - read=%d => %"PRIu32" > %zu - OUT OF ARRAY\r\n", rsc_ptr->rsc_uref, rsc_ptr->rsc_name, offset, ret, offset + ret,sizeof(appv_rsc_image) - 1);*/
                std::cout << "*** rsc_data: rsc[" << rsc_ptr->rsc_uref << "]='" << rsc_ptr->rsc_name << "' offset="
                          << offset << " - read=" << ret << " => " << offset + ret << " > %zu - OUT OF ARRAY"
                          << std::endl;
                return -1;
            }
            appv_rsc_offset += ret;
            if (appv_log_level > 0) {
                /*printf("*** rsc_data: rsc[%d]='%s' offset=%"PRIu32" - read=%d/%d - %"PRIu32"/%"PRIu32"\r\n", rsc_ptr->rsc_uref,rsc_ptr->rsc_name, offset, ret, data_len, appv_rsc_offset, appv_rsc_size);*/
                std::cout << "*** rsc_data: rsc[" << rsc_ptr->rsc_uref << "]='" << rsc_ptr->rsc_name << "' offset="
                          << offset << " - read=" << ret << "/" << data_len << " - " << appv_rsc_offset << "/"
                          << appv_rsc_size << std::endl;
            }

        }
        else {
            /*printf("*** rsc_data: rsc[%d]='%s' offset=%"PRIu32" - read error (%d) - %"PRIu32"/%"PRIu32"\r\n", rsc_ptr->rsc_uref,rsc_ptr->rsc_name, offset, ret, appv_rsc_offset, appv_rsc_size);*/
            std::cout << "*** rsc_data: rsc[" << rsc_ptr->rsc_uref << "]='" << rsc_ptr->rsc_name << "' offset="
                      << offset << " - read error (" << ret << ") - " << appv_rsc_offset << "/" << appv_rsc_size
                      << std::endl;
        }
    }
    else {
        ret = -1;
    }

    return ret;
}

// ----------------------------------------------------------
// COMMAND Callback Functions

static int
main_cmd_doSystemReset(LiveObjectsD_CommandRequestBlock_t *pCmdReqBlk);

static int main_cmd_doLED(LiveObjectsD_CommandRequestBlock_t *pCmdReqBlk);

/// Called (by the LiveObjects thread) to perform an 'attached/registered'
/// command
int main_cb_command(LiveObjectsD_CommandRequestBlock_t *pCmdReqBlk) {
    int ret;
    const LiveObjectsD_Command_t *cmd_ptr;

    if ((pCmdReqBlk == NULL) || (pCmdReqBlk->hd.cmd_ptr == NULL) || (pCmdReqBlk->hd.cmd_cid == 0)) {
/*		printf("**** COMMAND : ERROR, Invalid parameter\r\n");*/
        std::cout << "**** COMMAND : ERROR, Invalid parameter" << std::endl;
        return -1;
    }

    cmd_ptr = pCmdReqBlk->hd.cmd_ptr;
/*	printf("**** COMMAND %d %s - cid=%d\r\n", cmd_ptr->cmd_uref, cmd_ptr->cmd_name, pCmdReqBlk->hd.cmd_cid);*/
    std::cout << "**** COMMAND " << cmd_ptr->cmd_uref << " " << cmd_ptr->cmd_name << " - cid=" << pCmdReqBlk->hd.cmd_cid
              << std::endl;

    int i;
    /*printf("**** ARGS %d : \r\n", pCmdReqBlk->hd.cmd_args_nb);*/
    std::cout << "**** ARGS " << pCmdReqBlk->hd.cmd_args_nb << std::endl;
    for (i = 0; i < pCmdReqBlk->hd.cmd_args_nb; i++) {
        /*printf("**** ARG [%d] (%d) :  %s %s\r\n", i, pCmdReqBlk->args_array[i].arg_type,pCmdReqBlk->args_array[i].arg_name, pCmdReqBlk->args_array[i].arg_value);*/
        std::cout << "**** ARG [" << i << "] (" << pCmdReqBlk->args_array[i].arg_type << ") :  "
                  << pCmdReqBlk->args_array[i].arg_name << " " << pCmdReqBlk->args_array[i].arg_value << std::endl;
    }

    switch (cmd_ptr->cmd_uref) {
        case CMD_IDX_RESET:  // RESET
            /*printf("main_callbackCommand: command[%d] %s\r\n", cmd_ptr->cmd_uref, cmd_ptr->cmd_name);*/
            std::cout << "main_callbackCommand: command[" << cmd_ptr->cmd_uref << "] " << cmd_ptr->cmd_name
                      << std::endl;
            ret = main_cmd_doLED(pCmdReqBlk); // main_cmd_doSystemReset(pCmdReqBlk);
            break;

        case CMD_IDX_LED:  // LED
            /*printf("main_callbackCommand: command[%d] %s\r\n", cmd_ptr->cmd_uref, cmd_ptr->cmd_name);*/
            std::cout << "main_callbackCommand: command[" << cmd_ptr->cmd_uref << "] " << cmd_ptr->cmd_name
                      << std::endl;
            ret = main_cmd_doLED(pCmdReqBlk);
            break;
        default:
            /*printf("main_callbackCommand: ERROR, unknown command %d\r\n", cmd_ptr->cmd_uref);*/
            std::cout << "main_callbackCommand: ERROR, unknown command " << cmd_ptr->cmd_uref << std::endl;
            ret = -4;
    }
    return ret;
}

// ----------------------------------------------------------
/// Board reset
static void main_SystemReset(void) {
/*	printf("SYSTEM REBOOT\n");*/
    std::cout << "SYSTEM REBOOT" << std::endl;

}

// ----------------------------------------------------------
/// do a RESET command
/*
static int main_cmd_doSystemReset(LiveObjectsD_CommandRequestBlock_t *pCmdReqBlk) {
	if (LiveObjectsClient_Stop()) {
		printf("doSystemReset: not running => wait 500 ms and reset ...\r\n");
        WAIT_MS(200);

		main_SystemReset();
	}
	return 1;  // response = OK
}*/

// ----------------------------------------------------------
/// do a LED command
static int main_cmd_doLED(LiveObjectsD_CommandRequestBlock_t *pCmdReqBlk) {
    int ret;
    // switch on the Red LED
    app_led_user = 0;

    if (pCmdReqBlk->hd.cmd_args_nb == 0) {
        /*printf("main_cmd_doLED: No ARG\r\n");*/
        std::cout << "main_cmd_doLED: No ARG" << std::endl;
        app_led_user = !app_led_user;
        ret = 1;  // Response OK
        cmd_cnt = 0;
    }
    else {
        int i;
        for (i = 0; i < pCmdReqBlk->hd.cmd_args_nb; i++) {
            if (strncasecmp("ticks", pCmdReqBlk->args_array[i].arg_name, 5)
                && pCmdReqBlk->args_array[i].arg_type == 0) {
                int cnt = atoi(pCmdReqBlk->args_array[i].arg_value);
                if ((cnt >= 0) || (cnt <= 3)) {
                    cmd_cnt = cnt;
                }
                else {
                    cmd_cnt = 0;
                }
                /*printf("main_cmd_doLED: cmd_cnt = %di (%d)\r\n", cmd_cnt, cnt);*/
                std::cout << "main_cmd_doLED: cmd_cnt =" << cmd_cnt << " (" << cnt << ")" << std::endl;
            }
        }
    }

    if (cmd_cnt == 0) {
        app_led_user = !app_led_user;
        ret = 1;  // Response OK
    }
    else {
        LiveObjectsD_Command_t *cmd_ptr = (LiveObjectsD_Command_t *) (pCmdReqBlk->hd.cmd_ptr);
        app_led_user = 0;
        /*printf("main_cmd_doLED: ccid=%d (%d)\r\n", pCmdReqBlk->hd.cmd_cid, cmd_ptr->cmd_cid);*/
        std::cout << "main_cmd_doLED: ccid=" << pCmdReqBlk->hd.cmd_cid << " (" << cmd_ptr->cmd_cid << ")" << std::endl;
        cmd_ptr->cmd_cid = pCmdReqBlk->hd.cmd_cid;
        ret = 0;  // pending
    }
    return ret;  // response = OK
}

// ----------------------------------------------------------
// CONFIGURATION PARAMETERS Callback function
// Check value in range, and copy string parameters


/// Called (by the LiveObjects thread) to update configuration parameters.
int main_cb_param_udp(const LiveObjectsD_Param_t *param_ptr, const void *value, int len) {

    if (param_ptr == NULL) {
        /*printf("UPDATE  ERROR - invalid parameter x%p\r\n", param_ptr);*/
        std::cout << "UPDATE  ERROR - invalid parameter x" << param_ptr << std::endl;

        return -1;
    }
    /*printf("UPDATE user_ref=%d %s ....\r\n", param_ptr->parm_uref, param_ptr->parm_data.data_name);*/
    std::cout << "UPDATE user_ref=" << param_ptr->parm_uref << " " << param_ptr->parm_data.data_name << " ...."
              << std::endl;
    switch (param_ptr->parm_uref) {
        case PARM_IDX_NAME: {
            /*printf("update name = %.*s\r\n", len, (const char *) value);*/
            std::cout << "update name = " << (const char *) value << std::endl;
            if ((len > 0) && (len < (sizeof(appv_conf.name) - 1))) {
                // Only c-string parameter must be updated by the user application (to
                // check the string length)
                strncpy(appv_conf.name, (const char *) value, len);
                appv_conf.name[len] = 0;
                return 0;  // OK.
            }
        }
            break;
        case PARM_IDX_TIMEOUT: {
            uint32_t timeout = *((const uint32_t *) value);
            /*printf("update timeout = %" PRIu32 "\r\n", timeout);*/
            std::cout << "update timeout = " << timeout << " " << std::endl;
            if ((timeout > 0) && (timeout <= 120) && (timeout != appv_cfg_timeout)) {
                return 0;  // primitive parameter is updated by library
            }
        }
            break;
        case PARM_IDX_THRESHOLD: {
            int32_t threshold = *((const int32_t *) value);
            /*printf("update threshold = %" PRIi32 "\r\n", threshold);*/
            std::cout << "update threshold = " << threshold << std::endl;
            if ((threshold >= -10) && (threshold <= 10) && (threshold != appv_conf.threshold)) {
                return 0;  // primitive parameter is updated by library
            }
        }
            break;
        case PARM_IDX_GAIN: {
            float gain = *((const float *) value);
            /*printf("update gain = %f\r\n", gain);*/
            std::cout << "update gain = " << gain << std::endl;
            if ((gain > 0.0) && (gain < 10.0) && (gain != appv_conf.gain)) {
                return 0;  // primitive parameter is updated by library
            }
        }
            break;
    }
    /*printf("ERROR to update param[%d] %s !!!\r\n", param_ptr->parm_uref, param_ptr->parm_data.data_name);*/
    std::cout << "ERROR to update param[" << param_ptr->parm_uref << "] " << param_ptr->parm_data.data_name << " !!!"
              << std::endl;
    return -1;
}

// ----------------------------------------------------------

uint32_t loop_cnt = 0;

void appli_sched(void) {
    ++loop_cnt;
    if (appv_log_level > 1) {
        /*printf("thread_appli: %"PRIu32"\r\n", loop_cnt);*/
        std::cout << "thread_appli: " << loop_cnt << std::endl;
    }


    if (appv_log_level > 2) {
        /*printf("thread_appli: %"PRIu32" - %s PUBLISH - volt=%2.2f temp=%d\r\n", loop_cnt,appv_measures_enabled ? "DATA" : "NO", appv_measures_volt, appvTimestamp);*/
        std::cout << "thread_appli: " << loop_cnt << " - " << (appv_measures_enabled ? "DATA" : "NO")
                  << " PUBLISH - temp=" << appvTimestamp << std::endl;
    }

    if (appv_measures_enabled) {
        std::cout << "LiveObjectsClient_PushData..." << std::endl;
        LiveObjectsClient_PushData(appv_hdl_data);
        appvCounter++;
    }
}

// ----------------------------------------------------------

bool mqtt_start(void *ctx) {
    int ret;

    LiveObjectsClient_SetDbgLevel((lotrace_level_t) appv_log_level);
    LiveObjectsClient_SetDevId(LOC_CLIENT_DEV_ID);
    LiveObjectsClient_SetNameSpace(LOC_CLIENT_DEV_NAME_SPACE);

    unsigned long long apikey_p1 = C_LOC_CLIENT_DEV_API_KEY_P1;
    unsigned long long apikey_p2 = C_LOC_CLIENT_DEV_API_KEY_P2;

//	printf("mqtt_start: LiveObjectsClient_Init ...\n");
//    std::cout << "mqtt_start: LiveObjectsClient_Init en cpp..." << std::endl;
    ret = LiveObjectsClient_Init(NULL, apikey_p1, apikey_p2);
    if (ret) {
        //printf("mqtt_start: ERROR returned by LiveObjectsClient_Init\n");
        //      std::cout << "mqtt_start: ERROR returned by LiveObjectsClient_Init" << std::endl;
        return false;
    }

    // Attach my local RESOURCES to the LiveObjects Client instance
    // ------------------------------------------------------------
    ret = LiveObjectsClient_AttachResources(appv_set_resources, SET_RESOURCES_NB, main_cb_rsc_ntfy, main_cb_rsc_data);
    if (ret) { ;//       std::cout << " !!! ERROR (" << ret << ") to attach RESOURCES !" << std::endl;
    }
    else { ;//      std::cout << "mqtt_start: LiveObjectsClient_AttachResources -> OK" << std::endl;
    }

    // Attach my local Configuration Parameters to the LiveObjects Client instance
    // ----------------------------------------------------------------------------
    ret = LiveObjectsClient_AttachCfgParams(appv_set_param, SET_PARAM_NB, main_cb_param_udp);
    if (ret) { ;//      std::cout << " !!! ERROR (" << ret << ") to attach Config Parameters !" << std::endl;
    }
    else { ;//       std::cout << "mqtt_start: LiveObjectsClient_AttachCfgParams -> OK" << std::endl;
    }

    // Attach my local STATUS data to the LiveObjects Client instance
    // --------------------------------------------------------------
    appv_hdl_status = LiveObjectsClient_AttachStatus(appv_set_status, SET_STATUS_NB);
    if (appv_hdl_status);//      std::cout << " !!! ERROR (" << appv_hdl_status << ") to attach status !" << std::endl;
    else;//      std::cout << "mqtt_start: LiveObjectsClient_AttachStatus -> OK" << std::endl;

    // Attach one set of collected data to the LiveObjects Client instance
    // --------------------------------------------------------------------
    appv_hdl_data = LiveObjectsClient_AttachData(STREAM_PREFIX, "MesureLatence", "mV1", "\"Test\"", NULL,
                                                 appv_set_measures, SET_MEASURES_NB);
    if (appv_hdl_data <
        0) { ;//     std::cout << " !!! ERROR (" << appv_hdl_data << ") to attach a collected data stream !" << std::endl;
    }
    else { ;//      std::cout << "mqtt_start: LiveObjectsClient_AttachData -> OK " << std::endl;
    }

    // Attach a set of commands to the LiveObjects Client instance
    // -----------------------------------------------------------
    ret = LiveObjectsClient_AttachCommands(appv_set_commands, SET_COMMANDS_NB, main_cb_command);
    if (ret < 0) { ;//    std::cout << " !!! ERROR (" << ret << ") to attach a set of commands !" << std::endl;
    }
    else { ;//    std::cout << "mqtt_start: LiveObjectsClient_AttachCommands -> OK" << std::endl;
    }

    // Enable the receipt of commands
    ret = LiveObjectsClient_ControlCommands(true);
    if (ret < 0) { ;//    std::cout << " !!! ERROR (" << ret << ") to enable the receipt of commands !" << std::endl;
    }

    // Enable the receipt of resource update requests
    ret = LiveObjectsClient_ControlResources(true);
    if (ret <
        0) { ;//     std::cout << " !!! ERROR (" << ret << ") to enable the receipt of resource update request " << std::endl;
    }

    // Connect to the LiveObjects Platform
    // -----------------------------------
    std::cout << "mqtt_start: LiveObjectsClient_Connect ..." << std::endl;
    ret = LiveObjectsClient_Connect();
    if (ret) { ;//    std::cout << "mqtt_start: ERROR returned by LiveObjectsClient_Connect" << std::endl;
        return false;
    }

    ret = LiveObjectsClient_PushStatus(appv_hdl_status);
    if (ret) { ;//    std::cout << "mqtt_start: ERROR returned by LiveObjectsClient_PushStatus" << std::endl;
    }

    std::cout << "mqtt_start: OK" << std::endl;
    return true;
}

// ----------------------------------------------------------
/// Entry point to the program
int main() {

    auto compteur = std::chrono::milliseconds(15000);
    auto fin = std::chrono::milliseconds::zero();
    wiringPiSetup();

/* pull pin down and wait the start signal */
    pinMode(COMMANDE, OUTPUT);
    pinMode(READY, INPUT);
    pinMode(LECTURE, INPUT);

    digitalWrite(COMMANDE, HIGH);
    std::cout << "Raspi prete. Attend Sodaq" << std::endl;

    /**
     * Temporise 5s avant de vérifier si la carte Sodaq est prête
     * Evite le riste qu'un état bas "faux positif" puisse appraitre
     * au démarrage de la carte
     */
    std::this_thread::sleep_for(std::chrono::milliseconds(5000));

    if (mqtt_start(NULL)) {
        while (digitalRead(READY));   //attend LOW

        digitalWrite(COMMANDE, LOW);
        auto pointMesure1 = std::chrono::system_clock::now();    // premier point de mesure
        std::cout << "Début chronometre !" << std::endl;
        while (digitalRead(LECTURE));   //attend LOW fin de transmission
        //  auto pointMesure2 = std::chrono::system_clock::now();    // deuxieme point de mesure
        //  auto TimestampAvant = std::chrono::duration<double>(pointMesure2 - pointMesure1);    // défini la durée du traitement
        auto TimestampAvant = std::chrono::duration<double>(
                pointMesure1.time_since_epoch());    // défini la durée du traitement

        appvTSAvant = TimestampAvant.count();
        auto toutDeSuite = std::chrono::system_clock::now();
        auto convertion = std::chrono::duration<double>(toutDeSuite.time_since_epoch());

        appvTSApres = convertion.count();
        appvTSAvant = TimestampAvant.count();

        appli_sched();
        LiveObjectsClient_Cycle(1);

        std::cout << "Timestamp Avant : " << std::fixed << appvTSAvant << std::endl;
        std::cout << "Timestamp Apres: " << std::fixed << appvTSApres << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(5000));
        std::cout << "Fin programme : " << std::endl;
    }
    return 0;
}
