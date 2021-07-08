/**
 * @file test_edit.c
 * @author Michal Vasko <mvasko@cesnet.cz>
 * @brief tests for the edit-config rpc
 *
 * @copyright
 * Copyright 2021 Deutsche Telekom AG.
 * Copyright 2021 CESNET, z.s.p.o.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define _GNU_SOURCE

#include <setjmp.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <cmocka.h>
#include <libyang/libyang.h>
#include <nc_client.h>
#include <sysrepo.h>

#include "np_test.h"
#include "np_test_config.h"

/* TODO: Move some macros to np_test.h? */

#define GET_CONFIG_FILTER(filter)                                       \
    rpc = nc_rpc_getconfig(NC_DATASTORE_RUNNING, filter,                \
                           NC_WD_ALL, NC_PARAMTYPE_CONST);              \
    msgtype = nc_send_rpc(st->nc_sess, rpc, 1000, &msgid);              \
    assert_int_equal(NC_MSG_RPC, msgtype);                              \
    msgtype = nc_recv_reply(st->nc_sess, rpc, msgid, 2000, &envp, &op); \
    assert_int_equal(msgtype, NC_MSG_REPLY);                            \
    assert_non_null(op);                                                \
    assert_non_null(envp);                                              \
    assert_string_equal(LYD_NAME(lyd_child(op)), "data");               \
    assert_int_equal(LY_SUCCESS, lyd_print_mem(&str, op, LYD_XML, 0));  \
    FREE_TEST_VARS;

#define GET_CONFIG GET_CONFIG_FILTER(NULL);

#define SEND_EDIT_RPC(module)                                               \
    rpc = nc_rpc_edit(NC_DATASTORE_RUNNING, NC_RPC_EDIT_DFLTOP_MERGE,       \
                      NC_RPC_EDIT_TESTOPT_SET, NC_RPC_EDIT_ERROPT_ROLLBACK, \
                      module , NC_PARAMTYPE_CONST);                         \
    msgtype = nc_send_rpc(st->nc_sess, rpc, 1000, &msgid);                  \
    assert_int_equal(NC_MSG_RPC, msgtype);

#define EMPTY_GETCONFIG                                                 \
    "<get-config xmlns=\"urn:ietf:params:xml:ns:netconf:base:1.0\">\n"  \
    "  <data/>\n"                                                       \
    "</get-config>\n"

#define ASSERT_EMPTY_CONFIG                     \
    GET_CONFIG;                                 \
    assert_string_equal(str, EMPTY_GETCONFIG);  \
    free(str);

#define EDIT_1_VALID_DATA "<first xmlns=\"ed1\">TestFirst</first>"

#define EDIT_2_VALID_DATA                       \
    "<top xmlns=\"ed2\">"                       \
    "  <name>TestSecond</name>"                 \
    "  <num>123</num>"                          \
    "</top>"

#define EDIT_2_PARTIAL_DATA                     \
    "<top xmlns=\"ed2\">"                       \
    "  <name>TestSecond</name>"                 \
    "</top>"

#define EDIT_2_ALT_DATA                         \
    "<top xmlns=\"ed2\">"                       \
    "  <name>TestSecond</name>"                 \
    "  <num>456</num>"                          \
    "</top>"

#define EDIT_2_INVALID_DATA_NUM                 \
    "<top xmlns=\"ed2\">"                       \
    "  <name>TestSecond</name>"                 \
    "  <num>ClearlyNotANumericValue</num>"      \
    "</top>"

#define EDIT_3_VALID_DATA                       \
    "<top xmlns=\"ed3\">"                       \
    "  <name>TestThird</name>"                  \
    "  <num>123</num>"                          \
    "</top>"

#define EDIT_3_ALT_DATA                         \
    "<top xmlns=\"ed3\">"                       \
    "  <name>TestThird</name>"                  \
    "  <num>456</num>"                          \
    "</top>"

#define EDIT_1_DELETE                                       \
    "<first xmlns=\"ed1\""                                  \
    "xmlns:xc=\"urn:ietf:params:xml:ns:netconf:base:1.0\""  \
    "xc:operation=\"delete\">TestFirst</first>"

#define EDIT_2_DELETE                                       \
    "<top xmlns=\"ed2\""                                    \
    "xmlns:xc=\"urn:ietf:params:xml:ns:netconf:base:1.0\""  \
    "xc:operation=\"delete\">"                              \
    "  <name>TestSecond</name>"                             \
    "  <num>123</num>"                                      \
    "</top>"

#define EDIT_3_DELETE                                       \
    "<top xmlns=\"ed3\""                                    \
    "xmlns:xc=\"urn:ietf:params:xml:ns:netconf:base:1.0\""  \
    "xc:operation=\"delete\">"                              \
    "  <name>TestThird</name>"                              \
    "  <num>123</num>"                                      \
    "  <num>456</num>"                                      \
    "</top>"

#define EDIT_3_ALT_DELETE                                   \
    "<top xmlns=\"ed3\""                                    \
    "xmlns:xc=\"urn:ietf:params:xml:ns:netconf:base:1.0\""  \
    "xc:operation=\"delete\">"                              \
    "  <name>TestThird</name>"                              \
    "  <num>456</num>"                                      \
    "</top>"

#define EDIT_1_REMOVE                                       \
    "<first xmlns=\"ed1\""                                  \
    "xmlns:xc=\"urn:ietf:params:xml:ns:netconf:base:1.0\""  \
    "xc:operation=\"remove\">TestFirst</first>"

#define EDIT_1_CREATE                                       \
    "<first xmlns=\"ed1\""                                  \
    "xmlns:xc=\"urn:ietf:params:xml:ns:netconf:base:1.0\""  \
    "xc:operation=\"create\">TestFirst</first>"

#define EDIT_2_REPLACE                                      \
    "<top xmlns=\"ed2\""                                    \
    "xmlns:xc=\"urn:ietf:params:xml:ns:netconf:base:1.0\""  \
    "xc:operation=\"replace\">"                             \
    "  <name>TestSecond</name>"                             \
    "  <num>456</num>"                                      \
    "</top>"

#define EDIT_3_REPLACE                                      \
    "<top xmlns=\"ed3\""                                    \
    "xmlns:xc=\"urn:ietf:params:xml:ns:netconf:base:1.0\""  \
    "xc:operation=\"replace\">"                             \
    "  <name>TestThird</name>"                              \
    "  <num>123</num>"                                      \
    "</top>"

#define EDIT_3_ALT_REPLACE                                  \
    "<top xmlns=\"ed3\""                                    \
    "xmlns:xc=\"urn:ietf:params:xml:ns:netconf:base:1.0\""  \
    "xc:operation=\"replace\">"                             \
    "  <name>TestThird</name>"                              \
    "  <num>456</num>"                                      \
    "</top>"

#define RFC1_VALID_DATA                         \
    "<top xmlns=\"rfc1\">"                      \
    "  <interface>"                             \
    "    <name>Ethernet0/0</name>"              \
    "    <mtu>1500</mtu>"                       \
    "  </interface>"                            \
    "</top>"

#define RFC1_VALID_DATA2                        \
    "<top xmlns=\"rfc1\">"                      \
    "  <interface operation=\"replace\">"       \
    "    <name>Ethernet0/0</name>"              \
    "    <mtu>1500</mtu>"                       \
    "    <address>"                             \
    "      <name>192.0.2.4</name>"              \
    "      <prefix-length>24</prefix-length>"   \
    "    </address>"                            \
    "  </interface>"                            \
    "</top>"

#define RFC1_DELETE                                         \
    "<top xmlns=\"rfc1\""                                   \
    "xmlns:xc=\"urn:ietf:params:xml:ns:netconf:base:1.0\">" \
    "  <interface xc:operation=\"delete\">"                 \
    "    <name>Ethernet0/0</name>"                          \
    "  </interface>"                                        \
    "</top>"

#define RFC2_VALID_DATA                         \
    "<top xmlns=\"rfc2\">"                      \
    "  <protocols>"                             \
    "    <ospf>"                                \
    "      <area>"                              \
    "        <name>0.0.0.0</name>"              \
    "        <interfaces>"                      \
    "          <interface>"                     \
    "            <name>192.0.2.4</name>"        \
    "          </interface>"                    \
    "        </interfaces>"                     \
    "      </area>"                             \
    "    </ospf>"                               \
    "  </protocols>"                            \
    "</top>"

#define RFC2_VALID_DATA2                        \
    "<top xmlns=\"rfc2\">"                      \
    "  <protocols>"                             \
    "    <ospf>"                                \
    "      <area>"                              \
    "        <name>0.0.0.0</name>"              \
    "        <interfaces>"                      \
    "          <interface>"                     \
    "            <name>192.0.2.1</name>"        \
    "          </interface>"                    \
    "        </interfaces>"                     \
    "      </area>"                             \
    "    </ospf>"                               \
    "  </protocols>"                            \
    "</top>"

#define RFC2_DELETE_DATA                                                \
    "<top xmlns=\"rfc2\">"                                              \
    "  <protocols>"                                                     \
    "    <ospf>"                                                        \
    "      <area>"                                                      \
    "        <name>0.0.0.0</name>"                                      \
    "        <interfaces"                                               \
    "         xmlns:xc=\"urn:ietf:params:xml:ns:netconf:base:1.0\">"    \
    "          <interface xc:operation=\"delete\">"                     \
    "            <name>192.0.2.4</name>"                                \
    "          </interface>"                                            \
    "        </interfaces>"                                             \
    "      </area>"                                                     \
    "    </ospf>"                                                       \
    "  </protocols>"                                                    \
    "</top>"

#define RFC2_DELETE_REST                                        \
    "<top xmlns=\"rfc2\""                                       \
    "     xmlns:xc=\"urn:ietf:params:xml:ns:netconf:base:1.0\"" \
    "     xc:operation=\"delete\">"                             \
    "  <protocols>"                                             \
    "    <ospf>"                                                \
    "      <area>"                                              \
    "        <name>0.0.0.0</name>"                              \
    "        <interfaces>"                                      \
    "          <interface>"                                     \
    "            <name>192.0.2.1</name>"                        \
    "          </interface>"                                    \
    "        </interfaces>"                                     \
    "      </area>"                                             \
    "    </ospf>"                                               \
    "  </protocols>"                                            \
    "</top>"

#define RFC2_COMPLEX_DATA                       \
    "<top xmlns=\"rfc2\">"                      \
    "  <protocols>"                             \
    "    <ospf>"                                \
    "      <area>"                              \
    "        <name>0.0.0.0</name>"              \
    "        <interfaces>"                      \
    "          <interface>"                     \
    "            <name>192.0.2.1</name>"        \
    "          </interface>"                    \
    "          <interface>"                     \
    "            <name>192.0.2.4</name>"        \
    "          </interface>"                    \
    "        </interfaces>"                     \
    "      </area>"                             \
    "      <area>"                              \
    "        <name>192.168.0.0</name>"          \
    "        <interfaces>"                      \
    "          <interface>"                     \
    "            <name>192.168.0.1</name>"      \
    "          </interface>"                    \
    "          <interface>"                     \
    "            <name>192.168.0.12</name>"     \
    "          </interface>"                    \
    "          <interface>"                     \
    "            <name>192.168.0.25</name>"     \
    "          </interface>"                    \
    "        </interfaces>"                     \
    "      </area>"                             \
    "    </ospf>"                               \
    "  </protocols>"                            \
    "</top>"

#define RFC2_FILTER_AREA1                                               \
    "<get-config xmlns=\"urn:ietf:params:xml:ns:netconf:base:1.0\">\n"  \
    "  <data>\n"                                                        \
    "    <top xmlns=\"rfc2\">\n"                                        \
    "      <protocols>\n"                                               \
    "        <ospf>\n"                                                  \
    "          <area>\n"                                                \
    "            <name>0.0.0.0</name>\n"                                \
    "            <interfaces>\n"                                        \
    "              <interface>\n"                                       \
    "                <name>192.0.2.1</name>\n"                          \
    "              </interface>\n"                                      \
    "              <interface>\n"                                       \
    "                <name>192.0.2.4</name>\n"                          \
    "              </interface>\n"                                      \
    "            </interfaces>\n"                                       \
    "          </area>\n"                                               \
    "        </ospf>\n"                                                 \
    "      </protocols>\n"                                              \
    "    </top>\n"                                                      \
    "  </data>\n"                                                       \
    "</get-config>\n"

#define RFC2_DELETE_ALL                                     \
    "<top xmlns=\"rfc2\""                                   \
    "xmlns:xc=\"urn:ietf:params:xml:ns:netconf:base:1.0\""  \
    "xc:operation=\"delete\">"                              \
    "</top>"

static int
local_setup(void **state)
{
    sr_conn_ctx_t *conn;
    const char *features[] = {NULL};
    const char *module1 = NP_TEST_MODULE_DIR "/edit1.yang";
    const char *module2 = NP_TEST_MODULE_DIR "/edit2.yang";
    const char *module3 = NP_TEST_MODULE_DIR "/edit3.yang";
    const char *module4 = NP_TEST_MODULE_DIR "/rfc1.yang";
    const char *module5 = NP_TEST_MODULE_DIR "/rfc2.yang";

    /* setup environment necessary for installing module */
    NP_GLOB_SETUP_ENV_FUNC;
    assert_int_equal(setenv_rv, 0);

    /* connect to server and install test modules */
    assert_int_equal(sr_connect(SR_CONN_DEFAULT, &conn), SR_ERR_OK);
    assert_int_equal(sr_install_module(conn, module1, NULL, features), SR_ERR_OK);
    assert_int_equal(sr_install_module(conn, module2, NULL, features), SR_ERR_OK);
    assert_int_equal(sr_install_module(conn, module3, NULL, features), SR_ERR_OK);
    assert_int_equal(sr_install_module(conn, module4, NULL, features), SR_ERR_OK);
    assert_int_equal(sr_install_module(conn, module5, NULL, features), SR_ERR_OK);
    assert_int_equal(sr_disconnect(conn), SR_ERR_OK);

    /* setup netopeer2 server */
    return np_glob_setup_np2(state);
}

static int
local_teardown(void **state)
{
    sr_conn_ctx_t *conn;

    /* connect to server and remove test modules */
    assert_int_equal(sr_connect(SR_CONN_DEFAULT, &conn), SR_ERR_OK);
    assert_int_equal(sr_remove_module(conn, "edit1"), SR_ERR_OK);
    assert_int_equal(sr_remove_module(conn, "edit2"), SR_ERR_OK);
    assert_int_equal(sr_remove_module(conn, "edit3"), SR_ERR_OK);
    assert_int_equal(sr_remove_module(conn, "rfc1"), SR_ERR_OK);
    assert_int_equal(sr_remove_module(conn, "rfc2"), SR_ERR_OK);
    assert_int_equal(sr_disconnect(conn), SR_ERR_OK);

    /* close netopeer2 server */
    return np_glob_teardown(state);
}

static void
test_merge(void **state)
{
    struct np_test *st = *state;
    struct nc_rpc *rpc;
    NC_MSG_TYPE msgtype;
    uint64_t msgid;
    struct lyd_node *envp, *op;
    char *str;

    /* Send rpc editing module edit1 */
    SEND_EDIT_RPC(EDIT_1_VALID_DATA);

    /* Receive a reply, should succeed*/
    ASSERT_OK_REPLY;

    FREE_TEST_VARS;

    /* Check if added to config */
    GET_CONFIG;
    assert_non_null(strstr(str, "TestFirst"));
    free(str);

    /* Send rpc editing module edit2 */
    SEND_EDIT_RPC(EDIT_2_VALID_DATA);

    /* Receive a reply, should succeed */
    ASSERT_OK_REPLY;

    FREE_TEST_VARS;

    /* Check if added to config */
    GET_CONFIG;
    assert_non_null(strstr(str, "TestSecond"));
    free(str);

    /* Send invalid rpc editing module edit2 */
    SEND_EDIT_RPC(EDIT_2_INVALID_DATA_NUM);

    /* Receive a reply, should fail */
    ASSERT_RPC_ERROR;

    FREE_TEST_VARS;
}

static void
test_delete(void **state)
{
    struct np_test *st = *state;
    struct nc_rpc *rpc;
    NC_MSG_TYPE msgtype;
    uint64_t msgid;
    struct lyd_node *envp, *op;
    char *str;

    /* Check if the config for both is present */
    GET_CONFIG;
    assert_non_null(strstr(str, "TestFirst"));
    assert_non_null(strstr(str, "TestSecond"));
    free(str);

    /* Send rpc deleting config in module edit1 */
    SEND_EDIT_RPC(EDIT_1_DELETE);

    /* Receive a reply, should suceed */
    ASSERT_OK_REPLY;

    FREE_TEST_VARS;

    /* Check if the config was deleted */
    GET_CONFIG;
    assert_null(strstr(str, "TestFirst"));
    free(str);

    /* Send rpc deleting config in module edit2 */
    SEND_EDIT_RPC(EDIT_2_DELETE);

    /* Receive a reply, should succeed */
    ASSERT_OK_REPLY;

    FREE_TEST_VARS;

    /* Check if the config was deleted */
    ASSERT_EMPTY_CONFIG;

    /* Try deleting a non-existent config */
    SEND_EDIT_RPC(EDIT_1_DELETE);

    /* Receive a reply, should fail */
    ASSERT_RPC_ERROR;

    FREE_TEST_VARS;
}

static void
test_merge_advanced(void **state)
{
    struct np_test *st = *state;
    struct nc_rpc *rpc;
    NC_MSG_TYPE msgtype;
    uint64_t msgid;
    struct lyd_node *envp, *op;
    char *str;

    /* Check if config empty */
    ASSERT_EMPTY_CONFIG;

    /* Merge a partial config */
    SEND_EDIT_RPC(EDIT_2_PARTIAL_DATA);

    /* Recieve a reply, should succeed */
    ASSERT_OK_REPLY;

    FREE_TEST_VARS;

    /* Check if merged*/
    GET_CONFIG;
    assert_non_null(strstr(str, "TestSecond"));
    assert_null(strstr(str, "123"));
    free(str);

    /* Merge a full config */
    SEND_EDIT_RPC(EDIT_2_VALID_DATA);

    /* Recieve a reply, should succeed */
    ASSERT_OK_REPLY;

    FREE_TEST_VARS;

    /* Check if merged */
    GET_CONFIG;
    assert_non_null(strstr(str, "TestSecond"));
    assert_non_null(strstr(str, "123"));
    free(str);

    /* Empty the config */
    SEND_EDIT_RPC(EDIT_2_DELETE);

    /* Recieve a reply, should succeed */
    ASSERT_OK_REPLY;

    FREE_TEST_VARS;

    /* Check if config empty */
    ASSERT_EMPTY_CONFIG;

    /* Send rpc to merge into edit3 config */
    SEND_EDIT_RPC(EDIT_3_VALID_DATA);

    /* Recieve a reply, should succeed */
    ASSERT_OK_REPLY;

    FREE_TEST_VARS;

    /* Check if merged */
    GET_CONFIG;
    assert_non_null(strstr(str, "TestThird"));
    assert_non_null(strstr(str, "123"));
    assert_null(strstr(str, "456"));
    free(str);

    /* Send rpc to merge alternate edit3 config */
    SEND_EDIT_RPC(EDIT_3_ALT_DATA);

    /* Recieve a reply, should succeed */
    ASSERT_OK_REPLY;

    FREE_TEST_VARS;

    /* Check if merged, should now contain both since merging a leaf-list */
    GET_CONFIG;
    assert_non_null(strstr(str, "TestThird"));
    assert_non_null(strstr(str, "123"));
    assert_non_null(strstr(str, "456"));
    free(str);

    /* Empty the config */
    SEND_EDIT_RPC(EDIT_3_DELETE);

    /* Recieve a reply, should succeed */
    ASSERT_OK_REPLY;

    FREE_TEST_VARS;

    /* Check if config empty */
    ASSERT_EMPTY_CONFIG;
}

static void
test_replace(void **state)
{
    struct np_test *st = *state;
    struct nc_rpc *rpc;
    NC_MSG_TYPE msgtype;
    uint64_t msgid;
    struct lyd_node *envp, *op;
    char *str;

    /* Send rpc to replace in an empty config, should create */
    SEND_EDIT_RPC(EDIT_3_REPLACE);

    /* Receive a reply, should succeed */
    ASSERT_OK_REPLY;

    FREE_TEST_VARS;

    /* Check if correct config */
    GET_CONFIG;
    assert_non_null(strstr(str, "TestThird"));
    assert_non_null(strstr(str, "123"));
    assert_null(strstr(str, "456"));
    free(str);

    /* Send rpc to replace the original config */
    SEND_EDIT_RPC(EDIT_3_ALT_REPLACE);

    /* Receive a reply, should succeed */
    ASSERT_OK_REPLY;

    FREE_TEST_VARS;

    /* Check if replaced correctly */
    GET_CONFIG;
    assert_non_null(strstr(str, "TestThird"));
    assert_non_null(strstr(str, "456"));
    assert_null(strstr(str, "123"));
    free(str);

    /* Empty the config */
    SEND_EDIT_RPC(EDIT_3_ALT_DELETE);

    /* Recieve a reply, should succeed */
    ASSERT_OK_REPLY;

    FREE_TEST_VARS;

    /* Check if config empty */
    ASSERT_EMPTY_CONFIG;
}

static void
test_create(void **state)
{
    struct np_test *st = *state;
    struct nc_rpc *rpc;
    NC_MSG_TYPE msgtype;
    uint64_t msgid;
    struct lyd_node *envp, *op;
    char *str;

    /* Send rpc creating config in module edit1 */
    SEND_EDIT_RPC(EDIT_1_CREATE);

    /* Receive a reply, should succeed */
    ASSERT_OK_REPLY;

    FREE_TEST_VARS;

    /* Check if config config now contains edit1 */
    GET_CONFIG;
    assert_non_null(strstr(str, "TestFirst"));
    free(str);

    /* Send rpc creating the same module */
    SEND_EDIT_RPC(EDIT_1_CREATE);

    /* Receive a reply, should fail */
    ASSERT_RPC_ERROR;

    FREE_TEST_VARS;

    /* remove to get an empty config */
    SEND_EDIT_RPC(EDIT_1_REMOVE);

    /* Receive a reply, should succeed */
    ASSERT_OK_REPLY;

    FREE_TEST_VARS;

    /* check if empty config */
    ASSERT_EMPTY_CONFIG;
}

static void
test_remove(void **state)
{
    struct np_test *st = *state;
    struct nc_rpc *rpc;
    NC_MSG_TYPE msgtype;
    uint64_t msgid;
    struct lyd_node *envp, *op;
    char *str;

    /* Send rpc editing module edit1 */
    SEND_EDIT_RPC(EDIT_1_VALID_DATA);

    /* Receive a reply, should succeed*/
    ASSERT_OK_REPLY;

    FREE_TEST_VARS;

    /* Check if config was merged */
    GET_CONFIG;
    assert_string_not_equal(str, EMPTY_GETCONFIG);
    free(str);

    /* Try removing the merged config */
    SEND_EDIT_RPC(EDIT_1_REMOVE);

    /* Receive a reply, should succeed */
    ASSERT_OK_REPLY;

    FREE_TEST_VARS;

    /* Check if config is now empty */
    ASSERT_EMPTY_CONFIG;

    /* Try removing the from empty config */
    SEND_EDIT_RPC(EDIT_1_REMOVE);

    /* Receive a reply, should succeed */
    ASSERT_OK_REPLY;

    FREE_TEST_VARS;

    /* Check if config is still empty */
    ASSERT_EMPTY_CONFIG;
}

static void
test_rfc1(void **state)
{
    /* First example for edit-config from rfc 6241 section 7.2 */
    struct np_test *st = *state;
    struct nc_rpc *rpc;
    NC_MSG_TYPE msgtype;
    uint64_t msgid;
    struct lyd_node *envp, *op;
    char *str;

    /* Send rpc editing module rfc1 */
    SEND_EDIT_RPC(RFC1_VALID_DATA);

    /* Receive a reply, should succeed */
    ASSERT_OK_REPLY;

    FREE_TEST_VARS;

    /* Check if there is no address element */
    GET_CONFIG;
    assert_null(strstr(str, "address"));
    free(str);

    /* Send rpc replacing module rfc1 */
    SEND_EDIT_RPC(RFC1_VALID_DATA2);

    /* Receive a reply, should succeed*/
    ASSERT_OK_REPLY;

    FREE_TEST_VARS;

    /* Check if the address element is now present */
    GET_CONFIG;
    assert_non_null(strstr(str, "address"));
    free(str);

    /* Send rpc deleting config in module rfc1 */
    SEND_EDIT_RPC(RFC1_DELETE);

    /* Receive a reply, should succeed*/
    ASSERT_OK_REPLY;

    FREE_TEST_VARS;

    /* Check if empty config */
    ASSERT_EMPTY_CONFIG;
}

static void
test_rfc2(void **state)
{
    /* Second example for edit-config from rfc 6241 section 7.2 */
    struct np_test *st = *state;
    struct nc_rpc *rpc;
    NC_MSG_TYPE msgtype;
    uint64_t msgid;
    struct lyd_node *envp, *op;
    char *str;

    /* Need to have some running config first */

    /* Send rpc editing module rfc2 */
    SEND_EDIT_RPC(RFC2_VALID_DATA);

    /* Receive a reply, should succeed*/
    ASSERT_OK_REPLY;

    FREE_TEST_VARS;

    /* Send another rpc editing module rfc2 */
    SEND_EDIT_RPC(RFC2_VALID_DATA2);

    /* Receive a reply, should succeed */
    ASSERT_OK_REPLY;

    FREE_TEST_VARS;

    /* Send rpc deleting part of the data from module rfc2 */
    SEND_EDIT_RPC(RFC2_DELETE_DATA);

    /* Receive a reply, should succeed */
    ASSERT_OK_REPLY;

    FREE_TEST_VARS;

    /* Check if the config was patrialy deleted */
    GET_CONFIG;
    assert_null(strstr(str, "192.0.2.4"));
    assert_non_null(strstr(str, "192.0.2.1"));
    free(str);

    /* Send rpc deleting part of the data from module rfc2 */
    SEND_EDIT_RPC(RFC2_DELETE_REST);

    /* Receive a reply, should succeed */
    ASSERT_OK_REPLY;

    FREE_TEST_VARS;

    /* Check if empty config */
    ASSERT_EMPTY_CONFIG;
}

static void
test_filter(void **state)
{
    /* TODO: Move since this is not edit? */
    struct np_test *st = *state;
    struct nc_rpc *rpc;
    NC_MSG_TYPE msgtype;
    uint64_t msgid;
    struct lyd_node *envp, *op;
    char *str;

    /* Send rpc editing rfc2 */
    SEND_EDIT_RPC(RFC2_COMPLEX_DATA);

    /* Receive a reply, should succeed */
    ASSERT_OK_REPLY;

    FREE_TEST_VARS;

    /* TODO: Add more modules to test filter on */

    /* Filter by xpath */
    GET_CONFIG_FILTER("/top/protocols/ospf/area[1]");
    assert_string_equal(str, RFC2_FILTER_AREA1);
    free(str);

    /* since there are two last()-1 should be same as 1 */
    GET_CONFIG_FILTER("/top/protocols/ospf/area[last()-1]");
    assert_string_equal(str, RFC2_FILTER_AREA1);
    free(str);

    /* filter by area name same as the two before */
    GET_CONFIG_FILTER("/top/protocols/ospf/area[name='0.0.0.0']");
    assert_string_equal(str, RFC2_FILTER_AREA1);
    free(str);

    /* Send rpc deleting part of the data from module rfc2 */
    SEND_EDIT_RPC(RFC2_DELETE_ALL);

    /* Receive a reply, should succeed */
    ASSERT_OK_REPLY;

    FREE_TEST_VARS;

    /* Check if empty config */
    ASSERT_EMPTY_CONFIG;
}

int
main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_merge),
        cmocka_unit_test(test_delete),
        cmocka_unit_test(test_merge_advanced),
        cmocka_unit_test(test_replace),
        cmocka_unit_test(test_create),
        cmocka_unit_test(test_remove),
        cmocka_unit_test(test_rfc1),
        cmocka_unit_test(test_rfc2),
        cmocka_unit_test(test_filter),
    };

    nc_verbosity(NC_VERB_WARNING);
    return cmocka_run_group_tests(tests, local_setup, local_teardown);
}
