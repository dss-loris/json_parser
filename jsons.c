/**
 * @file jsons.c
 * @author Carl Mattatall (cmattatall2@gmail.com)
 * @brief JSON PDU parsing file for EPS firmware.
 *    For PDU related questions contact Brandon Allen.
 *    (ballen@dal.ca)
 * @version 0.1
 * @date 2020-12-09
 *
 * @copyright Copyright (c) 2020 DSS - LORIS project
 *
 */

#include <stdint.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "JTOK/inc/jtok.h"
#include "jsons.h"

#define BASE_10 10
#define JSON_TKN_CNT 20
#define JSON_HANDLER_RETVAL_ERROR NULL

typedef uint_fast16_t token_index_t;

typedef void *         json_handler_retval;
typedef token_index_t *json_handler_args;
typedef json_handler_retval (*json_handler)(json_handler_args);

typedef struct
{
    char         key[25];
    json_handler handler;
} json_parse_table_item;


static jtok_tkn_t tkns[JSON_TKN_CNT];
static char       value_holder[50];


/* JSON HANDLER DECLARATIONS */
//static void *parse_hardware_json(json_handler_args args);

/* JSON PARSE TABLE */
/* clang-format off */
static const json_parse_table_item json_parse_table[] = {

  //  {.key = "fwVersion",  .handler = parse_firmware_json},
  
  };
/* clang-format on */


int json_parse(uint8_t *json)
{
    CONFIG_ASSERT(json != NULL);

    int json_parse_status = 0;

    int jtok_retval = jtok_parse((char *)json, tkns, JSON_TKN_CNT);

    if (jtok_retval != JTOK_PARSE_STATUS_OK)
    {
        json_parse_status = jtok_retval;
        memset(tkns, 0, sizeof(tkns));
    }
    else
    {

        token_index_t t; /* token index */
        token_index_t k; /* key index for json table */
        t = 0;
        if (isValidJson(tkns, JSON_TKN_CNT))
        {
            /* Go through command table and check if we have a registered
             * command for the key */
            t++;

            unsigned int k_max =
                sizeof(json_parse_table) / sizeof(*json_parse_table);
            for (k = 0; k < k_max; k++)
            {
                /*
                 * If we have a command for the current key,
                 * execute the command handler
                 */
                if (jtok_tokcmp(json_parse_table[k].key, &tkns[t]))
                {
                    if (NULL != json_parse_table[k].handler)
                    {
                        json_handler_retval retval;
                        retval = json_parse_table[k].handler(&t);
                        if (retval == JSON_HANDLER_RETVAL_ERROR)
                        {
                            json_parse_status = -1;
                        }
                    }
                    break;
                }
            }

            /* No match with supported json keys */
            if (k >= k_max)
            {
                json_parse_status = -1;
            }
        }
        else
        {
            json_parse_status = 1;
        }
    }
    return json_parse_status;
}
