/**
 * @file jtok_object.c
 * @author Carl Mattatall (cmattatall2@gmail.com)
 * @brief Source module to handle jtok object operations
 * @version 0.1
 * @date 2020-12-25
 *
 * @copyright Copyright (c) 2020 Carl Mattatall
 *
 */

#include <assert.h>
#include <limits.h>

#include "inc/jtok_object.h"
#include "inc/jtok_array.h"
#include "inc/jtok_primitive.h"
#include "inc/jtok_string.h"
#include "inc/jtok_shared.h"


JTOK_PARSE_STATUS_t jtok_parse_object(jtok_parser_t *parser, int depth)
{
    JTOK_PARSE_STATUS_t status = JTOK_PARSE_STATUS_OK;

    int         start  = parser->pos;
    const char *json   = parser->json;
    int         len    = parser->json_len;
    jtok_tkn_t *tokens = parser->tkn_pool;

    if (depth > JTOK_MAX_RECURSE_DEPTH)
    {
        status = JTOK_PARSE_STATUS_NEST_DEPTH_EXCEEDED;
        return status;
    }

    enum
    {
        OBJECT_KEY,
        OBJECT_COLON,
        OBJECT_VALUE,
        OBJECT_COMMA,
    } expecting = OBJECT_KEY;

    if (tokens == NULL) /* Check for caller API error */
    {
        return status;
    }
    else if (json[parser->pos] != '{')
    {
        return JTOK_PARSE_STATUS_NON_OBJECT;
    }

    jtok_tkn_t *token = jtok_alloc_token(parser);
    if (token == NULL)
    {
        /*
         * Do not reset parser->pos because we want
         * caller to see which token maxed out the
         * pool
         */
        status = JTOK_PARSE_STATUS_NOMEM;
        return status;
    }

    /* If the object has a parent key, increase that key's size */
    token->parent = parser->toksuper;

    /* new superior token becomes the one we JUST processed */
    parser->toksuper = parser->toknext - 1;

    /* Index of current top level token is the current superior token */
    /* We need to preserve the index of top level token in the currenet
     * stack frame because parsing a sub-object changes the value of
     * parser->toksuper
     */
    int object_token_index = parser->toksuper;

    /* end of token will be populated when we find the closing brace */
    jtok_fill_token(token, JTOK_OBJECT, parser->pos, INVALID_ARRAY_INDEX);

    /* go inside the object */
    parser->pos++;

    /* all objects start with no children (since they can be empty) */
    parser->last_child = NO_CHILD_IDX;

    for (; parser->pos < len && json[parser->pos] != '\0' &&
           status == JTOK_PARSE_STATUS_OK;
         parser->pos++)
    {
        switch (json[parser->pos])
        {
            case '{':
            {
                switch (expecting)
                {
                    case OBJECT_KEY:
                    {
                        status = JTOK_PARSE_STATUS_OBJ_NOKEY;
                    }
                    break;
                    case OBJECT_COLON:
                    {
                        status = JTOK_PARSE_STATUS_VAL_NO_COLON;
                    }
                    break;
                    case OBJECT_VALUE: /* Enter and parse the sub-object */
                    {
                        /* Index of the key that owns this object */
                        int key_idx = parser->toksuper;


                        status = jtok_parse_object(parser, depth + 1);
                        if (status == JTOK_PARSE_STATUS_OK)
                        {
                            if (key_idx != NO_PARENT_IDX)
                            {
                                tokens[key_idx].size++;
                            }
                            parser->toksuper   = key_idx;
                            expecting          = OBJECT_COMMA;
                            parser->last_child = key_idx;
                        }
                    }
                    break;
                    case OBJECT_COMMA:
                    default:
                    {
                        status = JTOK_PARSE_STATUS_INVAL;
                    }
                    break;
                }
            }
            break;
            case '[':
            {
                switch (expecting)
                {
                    case OBJECT_KEY:
                    {
                        status = JTOK_PARSE_STATUS_OBJ_NOKEY;
                    }
                    break;
                    case OBJECT_COLON:
                    {
                        status = JTOK_PARSE_STATUS_VAL_NO_COLON;
                    }
                    break;
                    case OBJECT_VALUE:
                    {
                        /* Index of key that "owns" the array */
                        int key_idx = parser->toksuper;
                        status      = jtok_parse_array(parser, depth + 1);

                        if (status == JTOK_PARSE_STATUS_OK)
                        {
                            if (key_idx != NO_PARENT_IDX)
                            {
                                tokens[key_idx].size++;
                            }
                            else
                            {
                                /* Keys must have a parent token */
                                status = JTOK_PARSE_STATUS_INVALID_PARENT;
                            }
                            parser->last_child = key_idx;

                            expecting = OBJECT_COMMA;
                        }
                    }
                    break;
                    case OBJECT_COMMA:
                    {
                        status = JTOK_PARSE_STATUS_INVAL;
                    }
                    break;
                    default:
                    {
                        status = JTOK_PARSE_STATUS_INVAL;
                    }
                    break;
                }
            }
            break;
            case '}':
            {
                switch (expecting)
                {
                    /* Technically we should be checking if this is the very
                     * first token in the object, because
                     * {"key1" : "value1", "key2" : "value2",} is invalid
                     * from the trailing comma
                     * (transition to expecting==key only occurs when finding
                     * comma or when we START parsing the object)
                     *
                     * So in cases where we have transitioned from a comma,
                     * if we find '}' then it means we have a trailing
                     * comma inside the object
                     */

                    /********************************
                     * Case where we find end of    *
                     * object instead of key        *
                     * (aka: empty object)          *
                     *                              *
                     * eg: {     }                  *
                     *           ^ Right here       *
                     *******************************/
                    case OBJECT_KEY:
                    {
                        jtok_tkn_t *parent_obj = &tokens[object_token_index];
                        if (parent_obj->type != JTOK_OBJECT ||
                            parser->toknext == 0)
                        {
                            parser->pos = start;
                            status      = JTOK_PARSE_STATUS_INVAL;
                        }
                        else
                        {
                            parent_obj->end  = parser->pos + 1;
                            parser->toksuper = parent_obj->parent;

                            /* Don't have to update children->sibling link
                             * because there are no children in the object */
                        }
                        return status;
                    }
                    break;

                    /****************************************************
                     * Case wherein, instead of comma,                  *
                     * we find end of object '}'                        *
                     * eg : {\"key\":true, \"blah\":false   }           *
                     *                                      ^           *
                     *                                      Right here  *
                     ***************************************************/
                    case OBJECT_COMMA:
                    {
                        jtok_tkn_t *parent_obj = &tokens[object_token_index];
                        if (parent_obj->type != JTOK_OBJECT ||
                            parser->toknext == 0)
                        {
                            parser->pos = start;
                            status      = JTOK_PARSE_STATUS_INVAL;
                        }
                        else
                        {
                            parent_obj->end = parser->pos + 1;

                            /* Update superior token to the key that owns
                             * the current object */
                            parser->toksuper = parent_obj->parent;

                            /* Final item in object has no sibling key */
                            if (parser->last_child != NO_CHILD_IDX)
                            {
                                tokens[parser->last_child].sibling =
                                    NO_SIBLING_IDX;
                            }

                            /* Update last child */
                            parser->last_child = NO_CHILD_IDX;
                        }
                        return status;
                    }
                    break;
                    case OBJECT_COLON:
                    {
                        status = JTOK_PARSE_STATUS_KEY_NO_VAL;
                    }
                    break;
                    case OBJECT_VALUE:
                    {
                        status = JTOK_PARSE_STATUS_KEY_NO_VAL;
                    }
                    break;
                    default:
                    {
                        status = JTOK_PARSE_STATUS_INVAL;
                    }
                    break;
                }
            }
            break;
            case '\"':
            {
                switch (expecting)
                {
                    case OBJECT_KEY:
                    {
                        jtok_tkn_t *parent_obj = &tokens[parser->toksuper];
                        if (parent_obj->type == JTOK_OBJECT)
                        {
                            status = jtok_parse_string(parser);
                            if (status == JTOK_PARSE_STATUS_OK)
                            {
                                if (parser->last_child != NO_CHILD_IDX)
                                {
                                    /* Link previous child to current child */
                                    tokens[parser->last_child].sibling =
                                        parser->toknext - 1;
                                }

                                /* Update last child and increase parent size */
                                parser->last_child = parser->toknext - 1;
                                parent_obj->size++;
                            }
                            expecting = OBJECT_COLON;
                        }
                        else
                        {
                            status = JTOK_PARSE_STATUS_INVALID_PARENT;
                        }
                    }
                    break;
                    case OBJECT_VALUE:
                    {
                        jtok_tkn_t *key_tkn = &tokens[parser->toksuper];
                        if (key_tkn->type == JTOK_STRING)
                        {
                            if (key_tkn->size != 0)
                            {
                                /* an object key can only have 1 value */
                                status = JTOK_PARSE_STATUS_KEY_MULTIPLE_VAL;
                            }
                            else
                            {
                                status = jtok_parse_string(parser);
                                if (status == JTOK_PARSE_STATUS_OK)
                                {
                                    key_tkn->size++;
                                }
                                expecting = OBJECT_COMMA;
                            }
                        }
                        else
                        {
                            status = JTOK_PARSE_STATUS_INVALID_PARENT;
                        }
                    }
                    break;
                    case OBJECT_COLON: /* found " when expecting ':' */
                    {
                        status = JTOK_PARSE_STATUS_VAL_NO_COLON;
                    }
                    break;
                    case OBJECT_COMMA: /* found " when expecting ',' */
                    {
                        status = JTOK_PARSE_STATUS_VAL_NO_COMMA;
                    }
                    break;
                    default:
                    {
                        status = JTOK_PARSE_STATUS_INVAL;
                    }
                    break;
                }
            }
            break;
            case '\t':
            case '\r':
            case '\n':
            case ' ':
                continue; /* skip whitespce */
            case ':':
            {
                if (expecting == OBJECT_COLON)
                {
                    expecting = OBJECT_VALUE;

                    /* Superior token becomes the key we just processed */
                    parser->toksuper = parser->toknext - 1;
                }
                else
                {
                    parser->pos = start;
                    status      = JTOK_PARSE_STATUS_INVAL;
                }
            }
            break;
            case ',':
            {
                if (expecting == OBJECT_COMMA)
                {
                    expecting              = OBJECT_KEY;
                    jtok_tkn_t *parent_key = &tokens[parser->toksuper];
                    parser->toksuper       = parent_key->parent;
                }
                else
                {
                    status = JTOK_PARSE_STATUS_OBJ_NOKEY;
                }
            }
            break;
            case '+':
            case '-':
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
            case 't':
            case 'f':
            case 'n':
            {
                /* We must be expecting a value */
                if (expecting == OBJECT_VALUE)
                {
                    /* We're at the start of a primitive so validate parent type
                     */
                    jtok_tkn_t *parent = &tokens[parser->toksuper];
                    switch (parent->type)
                    {
                        case JTOK_OBJECT:
                        {
                            /* primitives cannot be keys (they are not
                             * quoted)
                             */
                            parser->pos = start;
                            status      = JTOK_PARSE_STATUS_INVAL;
                        }
                        break;
                        case JTOK_STRING:
                        {
                            if (parent->size != 0)
                            {
                                /* an object key can only have 1 value */
                                parser->pos = start;
                                status      = JTOK_PARSE_STATUS_INVAL;
                            }
                        }
                        break;
                        default:
                        {
                            /*
                             * If we're inside parse_object,
                             * other types cannot be parent tokens
                             */
                            status = JTOK_PARSE_STATUS_INVAL;
                        }
                        break;
                    }

                    if (status == JTOK_PARSE_STATUS_OK)
                    {
                        status = jtok_parse_primitive(parser);
                        if (status == JTOK_PARSE_STATUS_OK)
                        {
                            if (parser->toksuper != NO_PARENT_IDX)
                            {
                                tokens[parser->toksuper].size++;
                            }
                            expecting = OBJECT_COMMA;
                        }
                    }
                }
                else
                {
                    /* move pos to start of the key that's missing the value */
                    parser->pos = tokens[parser->toksuper].start;
                    status      = JTOK_PARSE_STATUS_KEY_NO_VAL;
                }
            }
            break;
            default: /* unexpected character */
            {
                parser->pos = start;
                status      = JTOK_PARSE_STATUS_INVAL;
            }
            break;
        } /* end of character switch statement */
    }

    if (status == JTOK_PARSE_STATUS_OK)
    {
        /* If we didnt find the } to close current object, we have partial JSON
         */
        parser->pos = start;
        status      = JTOK_PARSE_STATUS_PARTIAL_TOKEN;
    }

    return status;
}


bool jtok_toktokcmp_object(const jtok_tkn_t *obj1, const jtok_tkn_t *obj2)
{
    const jtok_tkn_t *const pool1 = obj1->pool;
    assert(pool1 != NULL);
    assert(pool1->type == JTOK_OBJECT);
    assert(pool1->json == obj1->json);
    if (pool1 != obj1)
    {
        /* Greater than 1 because if it's non-empty, it must AT LEAST
         * have a key inside it as well as the value for that key */
        assert(pool1->size > 1);
    }

    const jtok_tkn_t *const pool2 = obj2->pool;
    assert(pool2 != NULL);
    assert(pool2->type == JTOK_OBJECT);
    assert(pool2->json == obj2->json);
    if (pool2 != obj2)
    {
        /* Greater than 1 because if it's non-empty, it must AT LEAST
         * have a key inside it as well as the value for that key */
        assert(pool2->size > 1);
    }


    bool is_equal = true;
    if (obj1->type != JTOK_OBJECT || obj2->type != JTOK_OBJECT)
    {
        is_equal = false;
    }
    else
    {
        /* Easy preliminary check */
        if (obj1->size != obj2->size)
        {
            is_equal = false;
        }
        else if (obj1->size > 0)
        {
            /*
             * 2 empty objects are trivally equal
             * so only perform child comparison if the objects actaully
             * have children
             */
            int i;
            int j;

            /* When object not empty, its first child is token after object */
            jtok_tkn_t *child1 = (jtok_tkn_t *)&obj1[1];
            assert(child1->type == JTOK_STRING);
            for (i = 0; i < obj1->size && is_equal; i++)
            {
                jtok_tkn_t *child2 = (jtok_tkn_t *)&obj2[1];
                assert(child2->type == JTOK_STRING);

                /* Try to find matching key in second object */
                for (j = 0; j < obj2->size && is_equal; j++)
                {
                    if (jtok_toktokcmp(child1, child2))
                    {
                        /* If key matches, compare values */
                        jtok_tkn_t *val1 = &child1[1];
                        jtok_tkn_t *val2 = &child2[1];
                        if (jtok_toktokcmp(val1, val2))
                        {
                            /* value of key in obj1 matches value of key in obj2
                             * exit search loop for key in obj2 */
                            break;
                        }
                        else
                        {
                            /*
                             * Since there cannot be duplicate keys
                             * in a json object, if we find the key in obj2
                             * but it's value isn't equal to the value of the
                             * SAME key in obj1, then the objects cannot be
                             * equal.
                             */
                            is_equal = false;
                        }
                    }
                    else
                    {
                        /* Curerent key in obj2 doesnt match current key in
                         * obj1, but the objects may have keys ordered
                         * differently. So we check if we can explore other
                         * child keys and if yes, we repeat the process until
                         * all of obj2's child keys are exhausted */

                        if (child2->sibling == NO_SIBLING_IDX)
                        {
                            /* We've ran out of children in obj2 to check
                             * against current key in obj1 */
                            is_equal = false;
                            break;
                        }
                        else
                        {
                            /* Go to next key */
                            child2 = (jtok_tkn_t *)&pool2[child2->sibling];
                        }
                    }
                } /* END J LOOP */

                if (j == obj2->size)
                {
                    /* Did not find key in obj2 matching current key in obj1
                     */
                    is_equal = false;
                }
                else
                {
                    /* We matched current key in obj1,  */
                    /* so go to next key to match.      */
                    child1 = (jtok_tkn_t *)&pool1[child1->sibling];
                }

            } /* END I LOOP */
        }
    }
    return is_equal;
}
