/** @file */
/*****************************************************************************
* Copyright (c) 2016-2017, Cisco Systems, Inc.
* All rights reserved.

* Redistribution and use in source and binary forms, with or without modification,
* are permitted provided that the following conditions are met:
*
* 1. Redistributions of source code must retain the above copyright notice,
*    this list of conditions and the following disclaimer.
*
* 2. Redistributions in binary form must reproduce the above copyright notice,
*    this list of conditions and the following disclaimer in the documentation
*    and/or other materials provided with the distribution.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
* FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
* DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
* SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
* CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
* OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
* USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*****************************************************************************/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "acvp.h"
#include "acvp_lcl.h"
#include "parson.h"

static ACVP_RESULT acvp_cmac_init_tc (ACVP_CTX *ctx,
                                      ACVP_CMAC_TC *stc,
                                      unsigned int tc_id,
                                      char *msg,
                                      unsigned int msg_len,
                                      unsigned int key_len,
                                      char *key,
                                      char *key2,
                                      char *key3,
                                      int direction_verify,
                                      char *mac,
                                      unsigned int mac_len,
                                      ACVP_CIPHER alg_id) {
    ACVP_RESULT rv;

    if (!ctx) {
        return ACVP_NO_CTX;
    }
    if (alg_id != ACVP_CMAC_TDES && alg_id != ACVP_CMAC_AES) {
        return ACVP_INVALID_ARG;
    }
    if (!msg || !stc || !tc_id || !key) {
        return ACVP_INVALID_ARG;
    }
    if (alg_id == ACVP_CMAC_TDES && (!key2 || !key3)) {
        return ACVP_INVALID_ARG;
    }
    if (direction_verify) {
        if (!mac_len || !mac) {
            return ACVP_INVALID_ARG;
        }
    }
    
    memset(stc, 0x0, sizeof(ACVP_CMAC_TC));

    stc->msg = calloc(1, ACVP_CMAC_MSG_MAX);
    if (!stc->msg) { return ACVP_MALLOC_FAIL; }
    
    stc->mac = calloc(ACVP_CMAC_MAC_MAX, sizeof(char));
    if (!stc->mac) { return ACVP_MALLOC_FAIL; }
    stc->key = calloc(1, ACVP_CMAC_KEY_MAX);
    if (!stc->key) { return ACVP_MALLOC_FAIL; }
    
    stc->key2 = calloc(1, ACVP_CMAC_KEY_MAX);
    if (!stc->key2) { return ACVP_MALLOC_FAIL; }
    stc->key3 = calloc(1, ACVP_CMAC_KEY_MAX);
    if (!stc->key3) { return ACVP_MALLOC_FAIL; }

    rv = acvp_hexstr_to_bin(msg, stc->msg, ACVP_CMAC_MSG_MAX, NULL);
    if (rv != ACVP_SUCCESS) {
        ACVP_LOG_ERR("Hex converstion failure (msg)");
        return rv;
    }
    
    if (alg_id == ACVP_CMAC_AES) {
        rv = acvp_hexstr_to_bin(key, stc->key, ACVP_CMAC_KEY_MAX, (int *)&(stc->key_len));
        if (rv != ACVP_SUCCESS) {
            ACVP_LOG_ERR("Hex converstion failure (key)");
            return rv;
        }
    } else if (alg_id == ACVP_CMAC_TDES) {
        rv = acvp_hexstr_to_bin(key, stc->key, ACVP_CMAC_KEY_MAX, NULL);
        if (rv != ACVP_SUCCESS) {
            ACVP_LOG_ERR("Hex converstion failure (key1)");
            return rv;
        }
        rv = acvp_hexstr_to_bin(key2, stc->key2, ACVP_CMAC_KEY_MAX, NULL);
        if (rv != ACVP_SUCCESS) {
            ACVP_LOG_ERR("Hex converstion failure (key2)");
            return rv;
        }
        rv = acvp_hexstr_to_bin(key3, stc->key3, ACVP_CMAC_KEY_MAX, NULL);
        if (rv != ACVP_SUCCESS) {
            ACVP_LOG_ERR("Hex converstion failure (key3)");
            return rv;
        }
    }

    if (direction_verify) {
        strncpy(stc->direction, "ver", 4);
        stc->mac_len = mac_len;
        strncpy((char *)stc->mac, (const char *)mac, stc->mac_len * 2);
    } else {
        strncpy(stc->direction, "gen", 4);
    }

    stc->tc_id = tc_id;
    stc->msg_len = msg_len;
    stc->cipher = alg_id;
    
    return ACVP_SUCCESS;
}

/*
 * After the test case has been processed by the DUT, the results
 * need to be JSON formated to be included in the vector set results
 * file that will be uploaded to the server.  This routine handles
 * the JSON processing for a single test case.
 */
static ACVP_RESULT acvp_cmac_output_tc (ACVP_CTX *ctx, ACVP_CMAC_TC *stc, JSON_Object *tc_rsp) {
    ACVP_RESULT rv = ACVP_SUCCESS;
    char *tmp = NULL;

    tmp = calloc(ACVP_CMAC_MAC_MAX+1, sizeof(char));
    if (!tmp) {
        ACVP_LOG_ERR("Unable to malloc in acvp_cmac_output_tc");
        return ACVP_MALLOC_FAIL;
    }
    
    if (strncmp(stc->direction, "ver", 3) == 0) {
        json_object_set_string(tc_rsp, "result", stc->ver_disposition);
    } else {
        rv = acvp_bin_to_hexstr(stc->mac, stc->mac_len, tmp, ACVP_CMAC_MAC_MAX);
        if (rv != ACVP_SUCCESS) {
            ACVP_LOG_ERR("hex conversion failure (mac)");
            goto end;
        }
        json_object_set_string(tc_rsp, "mac", tmp);
    }

end:
    if (tmp) free(tmp);

    return rv;
}

/*
 * This function simply releases the data associated with
 * a test case.
 */
static ACVP_RESULT acvp_cmac_release_tc (ACVP_CMAC_TC *stc) {
    free(stc->msg);
    free(stc->mac);
    free(stc->key);
    free(stc->key2);
    free(stc->key3);
    memset(stc, 0x0, sizeof(ACVP_CMAC_TC));

    return ACVP_SUCCESS;
}

ACVP_RESULT acvp_cmac_kat_handler (ACVP_CTX *ctx, JSON_Object *obj) {
    unsigned int tc_id, msglen, keyLen = 0, keyingOption = 0, maclen, verify = 0;
    char *msg = NULL, *key1 = NULL, *key2 = NULL, *key3 = NULL, *mac = NULL;
    JSON_Value *groupval;
    JSON_Object *groupobj = NULL;
    JSON_Value *testval;
    JSON_Object *testobj = NULL;
    JSON_Array *groups;
    JSON_Array *tests;

    JSON_Value *reg_arry_val = NULL;
    JSON_Object *reg_obj = NULL;
    JSON_Array *reg_arry = NULL;

    int i, g_cnt;
    int j, t_cnt;

    JSON_Value *r_vs_val = NULL;
    JSON_Object *r_vs = NULL;
    JSON_Array *r_tarr = NULL; /* Response testarray */
    JSON_Value *r_tval = NULL; /* Response testval */
    JSON_Object *r_tobj = NULL; /* Response testobj */
    ACVP_CAPS_LIST *cap;
    ACVP_CMAC_TC stc;
    ACVP_TEST_CASE tc;
    ACVP_RESULT rv;
    const char *alg_str = json_object_get_string(obj, "algorithm");
    ACVP_CIPHER alg_id;
    char *json_result, *direction = NULL;
    int key1_len, key2_len, key3_len, json_msglen;

    if (!ctx) {
        ACVP_LOG_ERR("No ctx for handler operation");
        return (ACVP_NO_CTX);
    }

    if (!obj) {
        ACVP_LOG_ERR("No obj for handler operation");
        return (ACVP_MALFORMED_JSON);
    }

    if (!alg_str) {
        ACVP_LOG_ERR("ERROR: unable to parse 'algorithm' from JSON");
        return (ACVP_MALFORMED_JSON);
    }

    /*
     * Get a reference to the abstracted test case
     */
    tc.tc.cmac = &stc;

    /*
     * Get the crypto module handler for this hash algorithm
     */
    alg_id = acvp_lookup_cipher_index(alg_str);
    if (alg_id < ACVP_CIPHER_START) {
        ACVP_LOG_ERR("ERROR: unsupported algorithm (%s)", alg_str);
        return (ACVP_UNSUPPORTED_OP);
    }
    cap = acvp_locate_cap_entry(ctx, alg_id);
    if (!cap) {
        ACVP_LOG_ERR("ERROR: ACVP server requesting unsupported capability");
        return (ACVP_UNSUPPORTED_OP);
    }

    /*
     * Create ACVP array for response
     */
    rv = acvp_create_array(&reg_obj, &reg_arry_val, &reg_arry);
    if (rv != ACVP_SUCCESS) {
        ACVP_LOG_ERR("ERROR: Failed to create JSON response struct. ");
        return (rv);
    }

    /*
     * Start to build the JSON response
     * TODO: This code will likely be common to all the algorithms, need to move this
     */
    if (ctx->kat_resp) {
        json_value_free(ctx->kat_resp);
    }
    ctx->kat_resp = reg_arry_val;
    r_vs_val = json_value_init_object();
    r_vs = json_value_get_object(r_vs_val);

    json_object_set_number(r_vs, "vsId", ctx->vs_id);
    json_object_set_string(r_vs, "algorithm", alg_str);
    json_object_set_value(r_vs, "testResults", json_value_init_array());
    r_tarr = json_object_get_array(r_vs, "testResults");

    groups = json_object_get_array(obj, "testGroups");
    g_cnt = json_array_get_count(groups);
    for (i = 0; i < g_cnt; i++) {
        groupval = json_array_get_value(groups, i);
        groupobj = json_value_get_object(groupval);
        
        if (alg_id == ACVP_CMAC_AES) {
            keyLen = (unsigned int) json_object_get_number(groupobj, "keyLen");
            if (!keyLen) {
                ACVP_LOG_ERR("keylen missing from cmac aes json");
                return (ACVP_MISSING_ARG);
            }
        } else if (alg_id == ACVP_CMAC_TDES) {
            keyingOption = (unsigned int) json_object_get_number(groupobj, "keyingOption");
            if (keyingOption <= ACVP_CMAC_TDES_KEYING_OPTION_MIN ||
                keyingOption >= ACVP_CMAC_TDES_KEYING_OPTION_MAX) {
                ACVP_LOG_ERR("keyingOption missing or wrong from cmac tdes json");
                return (ACVP_INVALID_ARG);
            }
        }
        
        direction = (char *)json_object_get_string(groupobj, "direction");
        if (!direction) {
            ACVP_LOG_ERR("Unable to parse 'direction' from JSON.");
            return ACVP_MALFORMED_JSON;
        }
        if (strncmp((const char *)direction, "ver", 3) == 0) {
            verify = 1;
        } else if (strncmp((const char *)direction, "gen", 3) != 0) {
            ACVP_LOG_ERR("'direction' should be 'gen' or 'ver'");
            return ACVP_UNSUPPORTED_OP;
        }
        msglen = (unsigned int) json_object_get_number(groupobj, "msgLen") / 8;

        maclen = (unsigned int) json_object_get_number(groupobj, "macLen") / 8;
        if (!maclen) {
            ACVP_LOG_ERR("Server JSON missing 'macLen'");
            return ACVP_MISSING_ARG;
        }
    
        ACVP_LOG_INFO("\n\n    Test group: %d", i);

        tests = json_object_get_array(groupobj, "tests");
        t_cnt = json_array_get_count(tests);
        for (j = 0; j < t_cnt; j++) {
            ACVP_LOG_INFO("Found new cmac test vector...");
            testval = json_array_get_value(tests, j);
            testobj = json_value_get_object(testval);

            tc_id = (unsigned int) json_object_get_number(testobj, "tcId");
            msg = (char *) json_object_get_string(testobj, "msg");
            
            /* msg can be null if msglen is 0 */
            if (msg) {
                json_msglen = strnlen(msg, ACVP_CMAC_MSG_MAX + 1);
                if (json_msglen > ACVP_CMAC_MSG_MAX) {
                    ACVP_LOG_ERR("'msg' too long");
                    return ACVP_INVALID_ARG;
    
                }
                if (!msglen && json_msglen > 0) {
                    ACVP_LOG_ERR("Server JSON missing 'msgLen'");
                    return ACVP_MISSING_ARG;
                }
            } else if (msglen) {
                ACVP_LOG_ERR("msglen is nonzero, expected 'msg' in json");
                return ACVP_MISSING_ARG;
            }

            if (alg_id == ACVP_CMAC_AES) {
                key1 = (char *) json_object_get_string(testobj, "key");
                if (!key1) {
                    ACVP_LOG_ERR("Server JSON missing 'key'");
                    return ACVP_MISSING_ARG;
                }
                key1_len = strnlen(key1, ACVP_CMAC_KEY_MAX + 1);
                if (key1_len > ACVP_CMAC_KEY_MAX) {
                    ACVP_LOG_ERR("Invalid length for 'key' attribute in CMAC-AES test");
                    return ACVP_INVALID_ARG;
                }
            } else if (alg_id == ACVP_CMAC_TDES) {
                key1 = (char *) json_object_get_string(testobj, "key1");
                key2 = (char *) json_object_get_string(testobj, "key2");
                key3 = (char *) json_object_get_string(testobj, "key3");
                if (!key1 || !key2 || !key3) {
                    ACVP_LOG_ERR("Server JSON missing 'key(1,2,3)' value");
                    return ACVP_MISSING_ARG;
                }
                key1_len = strnlen(key1, ACVP_CMAC_KEY_MAX + 1);
                key2_len = strnlen(key2, ACVP_CMAC_KEY_MAX + 1);
                key3_len = strnlen(key3, ACVP_CMAC_KEY_MAX + 1);
                if (key1_len > ACVP_CMAC_KEY_MAX ||
                    key2_len > ACVP_CMAC_KEY_MAX ||
                    key3_len > ACVP_CMAC_KEY_MAX) {
                    ACVP_LOG_ERR("Invalid length for 'key(1|2|3)' attribute in CMAC-TDES test");
                    return ACVP_INVALID_ARG;
                }
            }
            
            if (verify) {
                mac = (char *) json_object_get_string(testobj, "mac");
                if (!mac) {
                    ACVP_LOG_ERR("Server JSON missing 'mac'");
                    return ACVP_MISSING_ARG;
                }
            }

            ACVP_LOG_INFO("\n        Test case: %d", j);
            ACVP_LOG_INFO("             tcId: %d", tc_id);
            ACVP_LOG_INFO("        direction: %s", direction);

            ACVP_LOG_INFO("           msgLen: %d", msglen);
            ACVP_LOG_INFO("              msg: %s", msg);
            if (alg_id == ACVP_CMAC_AES) {
                ACVP_LOG_INFO("           keyLen: %d", keyLen);
                ACVP_LOG_INFO("              key: %s", key1);
            } else if (alg_id == ACVP_CMAC_TDES) {
                ACVP_LOG_INFO("     keyingOption: %d", keyingOption);
                ACVP_LOG_INFO("             key1: %s", key1);
                ACVP_LOG_INFO("             key2: %s", key2);
                ACVP_LOG_INFO("             key3: %s", key3);
            }
    
            if (verify) {
                ACVP_LOG_INFO("              mac: %s", mac);
            }
            
            /*
             * Create a new test case in the response
             */
            r_tval = json_value_init_object();
            r_tobj = json_value_get_object(r_tval);

            json_object_set_number(r_tobj, "tcId", tc_id);

            /*
             * Setup the test case data that will be passed down to
             * the crypto module.
             * TODO: this does mallocs, we can probably do the mallocs once for
             *       the entire vector set to be more efficient
             */
            acvp_cmac_init_tc(ctx, &stc, tc_id, msg, msglen, keyLen, key1, key2, key3,
                              verify, mac, maclen, alg_id);

            /* Process the current test vector... */
            rv = (cap->crypto_handler)(&tc);
            if (rv != ACVP_SUCCESS) {
                ACVP_LOG_ERR("ERROR: crypto module failed the operation");
                return ACVP_CRYPTO_MODULE_FAIL;
            }

            /*
             * Output the test case results using JSON
             */
            rv = acvp_cmac_output_tc(ctx, &stc, r_tobj);
            if (rv != ACVP_SUCCESS) {
                ACVP_LOG_ERR("ERROR: JSON output failure in hash module");
                return rv;
            }
            /*
             * Release all the memory associated with the test case
             */
            acvp_cmac_release_tc(&stc);

            /* Append the test response value to array */
            json_array_append_value(r_tarr, r_tval);
        }
    }

    json_array_append_value(reg_arry, r_vs_val);

    json_result = json_serialize_to_string_pretty(ctx->kat_resp);
    if (ctx->debug == ACVP_LOG_LVL_VERBOSE) {
        printf("\n\n%s\n\n", json_result);
    } else {
        ACVP_LOG_INFO("\n\n%s\n\n", json_result);
    }
    json_free_serialized_string(json_result);

    return ACVP_SUCCESS;
}
