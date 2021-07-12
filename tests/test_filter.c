/**
 * @file test_filter.c
 * @author Michal Vasko <mvasko@cesnet.cz>
 * @brief tests for filter in get and get-config rpc
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

static void
setup_data(void **state)
{
    /*
     * TODO: Change np2 edit-config rpc calls into direct sr calls
     * TODO: add return value
     */
    struct np_test *st = *state;
    char *RFC2_COMPLEX_DATA, *F1_DATA, *I1_DATA;

    RFC2_COMPLEX_DATA =
            "<top xmlns=\"rfc2\">\n"                      \
            "  <protocols>\n"                             \
            "    <ospf>\n"                                \
            "      <area>\n"                              \
            "        <name>0.0.0.0</name>\n"              \
            "        <interfaces>\n"                      \
            "          <interface>\n"                     \
            "            <name>192.0.2.1</name>\n"        \
            "          </interface>\n"                    \
            "          <interface>\n"                     \
            "            <name>192.0.2.4</name>\n"        \
            "          </interface>\n"                    \
            "        </interfaces>\n"                     \
            "      </area>\n"                             \
            "      <area>\n"                              \
            "        <name>192.168.0.0</name>\n"          \
            "        <interfaces>\n"                      \
            "          <interface>\n"                     \
            "            <name>192.168.0.1</name>\n"      \
            "          </interface>\n"                    \
            "          <interface>\n"                     \
            "            <name>192.168.0.12</name>\n"     \
            "          </interface>\n"                    \
            "          <interface>\n"                     \
            "            <name>192.168.0.25</name>\n"     \
            "          </interface>\n"                    \
            "        </interfaces>\n"                     \
            "      </area>\n"                             \
            "    </ospf>\n"                               \
            "  </protocols>\n"                            \
            "</top>\n";

    /* Send rpc editing rfc2 */
    SEND_EDIT_RPC(st, RFC2_COMPLEX_DATA);

    /* Receive a reply, should succeed */
    ASSERT_OK_REPLY(st);

    FREE_TEST_VARS(st);

    I1_DATA =
        "<hardware xmlns=\"i1\">\n"             \
        "  <component>\n"                       \
        "    <name>ComponentName</name>\n"      \
        "    <class>O-RAN-RADIO</class>\n"      \
        "  </component>\n"                      \
        "</hardware>\n";

    SEND_EDIT_RPC(st, I1_DATA);

    ASSERT_OK_REPLY(st);

    FREE_TEST_VARS(st);

    F1_DATA =
            "<top xmlns=\"f1\">\n"                           \
            "  <devices>\n"                                  \
            "    <servers>\n"                                \
            "      <server>\n"                               \
            "        <name>First</name>\n"                   \
            "        <address>192.168.0.4</address>\n"       \
            "        <port>80</port>\n"                      \
            "      </server>\n"                              \
            "      <server>\n"                               \
            "        <name>Second</name>\n"                  \
            "        <address>192.168.0.12</address>\n"      \
            "        <port>80</port>\n"                      \
            "      </server>\n"                              \
            "      <server>\n"                               \
            "        <name>Fourth</name>\n"                  \
            "        <address>192.168.0.50</address>\n"      \
            "        <port>22</port>\n"                      \
            "      </server>\n"                              \
            "      <server>\n"                               \
            "        <name>Fifth</name>\n"                   \
            "        <address>192.168.0.50</address>\n"      \
            "        <port>443</port>\n"                     \
            "      </server>\n"                              \
            "      <server>\n"                               \
            "        <name>Sixth</name>\n"                   \
            "        <address>192.168.0.102</address>\n"     \
            "        <port>22</port>\n"                      \
            "      </server>\n"                              \
            "    </servers>\n"                               \
            "    <desktops>\n"                               \
            "      <desktop>\n"                              \
            "        <name>Seventh</name>\n"                 \
            "        <address>192.168.0.130</address>\n"     \
            "      </desktop>\n"                             \
            "      <desktop>\n"                              \
            "        <name>Sixth</name>\n"                   \
            "        <address>192.168.0.142</address>\n"     \
            "      </desktop>\n"                             \
            "    </desktops>\n"                              \
            "  </devices>\n"                                 \
            "</top>\n";

    /* Send rpc merging data into f1 */
    SEND_EDIT_RPC(st, F1_DATA);

    /* Receive a reply, should succeed */
    ASSERT_OK_REPLY(st);

    FREE_TEST_VARS(st);
}

static int
local_setup(void **state)
{
    struct np_test *st;
    sr_conn_ctx_t *conn;
    const char *features[] = {NULL};
    const char *module1 = NP_TEST_MODULE_DIR "/rfc2.yang";
    const char *module2 = NP_TEST_MODULE_DIR "/filter1.yang";
    const char *module3 = NP_TEST_MODULE_DIR "/issue1.yang";
    int rv;

    /* setup environment necessary for installing module */
    NP_GLOB_SETUP_ENV_FUNC;
    assert_int_equal(setenv_rv, 0);

    /* connect to server and install test modules */
    assert_int_equal(sr_connect(SR_CONN_DEFAULT, &conn), SR_ERR_OK);
    assert_int_equal(sr_install_module(conn, module1, NULL, features),
            SR_ERR_OK);
    assert_int_equal(sr_install_module(conn, module2, NULL, features),
            SR_ERR_OK);
    assert_int_equal(sr_install_module(conn, module3, NULL, features),
            SR_ERR_OK);
    assert_int_equal(sr_disconnect(conn), SR_ERR_OK);

    /* setup netopeer2 server */
    if (!(rv = np_glob_setup_np2(state)))
    {
        /* state is allocated in np_glob_setup_np2 have to set here */
        st = *state;
        /* Open connection to start a session for the tests */
        assert_int_equal(sr_connect(SR_CONN_DEFAULT, &st->conn), SR_ERR_OK);
        assert_int_equal(sr_session_start(st->conn, SR_DS_RUNNING, &st->sr_sess),
                SR_ERR_OK);
        setup_data(state);
    }
    return rv;
}

static void
teardown_data(void **state)
{
    struct np_test *st = *state;
    char *RFC2_REMOVE_ALL, *I1_REMOVE_ALL, *F1_REMOVE_ALL;

    RFC2_REMOVE_ALL =
            "<top xmlns=\"rfc2\""                                   \
            "xmlns:xc=\"urn:ietf:params:xml:ns:netconf:base:1.0\""  \
            "xc:operation=\"remove\">"                              \
            "</top>";

    /* Send rpc removing part of the data from module rfc2 */
    SEND_EDIT_RPC(st, RFC2_REMOVE_ALL);

    /* Receive a reply, should succeed */
    ASSERT_OK_REPLY(st);

    FREE_TEST_VARS(st);

    RFC2_REMOVE_ALL =
            "<top xmlns=\"rfc2\""                                   \
            "xmlns:xc=\"urn:ietf:params:xml:ns:netconf:base:1.0\""  \
            "xc:operation=\"remove\">"                              \
            "</top>";

    /* Send rpc deleting part of the data from module rfc2 */
    SEND_EDIT_RPC(st, RFC2_REMOVE_ALL);

    /* Receive a reply, should succeed */
    ASSERT_OK_REPLY(st);

    FREE_TEST_VARS(st);

    F1_REMOVE_ALL =
            "<top xmlns=\"f1\""                                         \
            "xmlns:xc=\"urn:ietf:params:xml:ns:netconf:base:1.0\""      \
            "xc:operation=\"remove\">"                                  \
            "</top>";

    /* Send rpc deleting part of the data from module f1 */
    SEND_EDIT_RPC(st, F1_REMOVE_ALL);

    /* Receive a reply, should succeed */
    ASSERT_OK_REPLY(st);

    FREE_TEST_VARS(st);

    I1_REMOVE_ALL =
        "<hardware xmlns=\"i1\""                                        \
        "xmlns:xc=\"urn:ietf:params:xml:ns:netconf:base:1.0\""          \
        "xc:operation=\"remove\">"                                      \
        "</hardware>\n";

    SEND_EDIT_RPC(st, I1_REMOVE_ALL);

    ASSERT_OK_REPLY(st);

    FREE_TEST_VARS(st);
}

static int
local_teardown(void **state)
{
    /*
     * TODO: Change np2 edit-config rpc calls into direct sr calls
     * TODO: add return value
     */
    struct np_test *st = *state;
    sr_conn_ctx_t *conn;

    teardown_data(state);
    /* Check if empty config */
    ASSERT_EMPTY_CONFIG(st);

    /* Close the session and connection needed for tests */
    assert_int_equal(sr_session_stop(st->sr_sess), SR_ERR_OK);
    assert_int_equal(sr_disconnect(st->conn), SR_ERR_OK);

    /* connect to server and remove test modules */
    assert_int_equal(sr_connect(SR_CONN_DEFAULT, &conn), SR_ERR_OK);
    assert_int_equal(sr_remove_module(conn, "rfc2"), SR_ERR_OK);
    assert_int_equal(sr_remove_module(conn, "filter1"), SR_ERR_OK);
    assert_int_equal(sr_remove_module(conn, "issue1"), SR_ERR_OK);
    assert_int_equal(sr_disconnect(conn), SR_ERR_OK);

    /* close netopeer2 server */
    return np_glob_teardown(state);
}

static void
test_xpath_basic(void **state)
{
    struct np_test *st = *state;
    const char *RFC2_FILTER_AREA1;

    /* TODO: Test operators (union mostly) */

    RFC2_FILTER_AREA1 =
            "<get-config xmlns=\"urn:ietf:params:xml:ns:netconf:base:1.0\">\n" \
            "  <data>\n"                                                       \
            "    <top xmlns=\"rfc2\">\n"                                       \
            "      <protocols>\n"                                              \
            "        <ospf>\n"                                                 \
            "          <area>\n"                                               \
            "            <name>0.0.0.0</name>\n"                               \
            "            <interfaces>\n"                                       \
            "              <interface>\n"                                      \
            "                <name>192.0.2.1</name>\n"                         \
            "              </interface>\n"                                     \
            "              <interface>\n"                                      \
            "                <name>192.0.2.4</name>\n"                         \
            "              </interface>\n"                                     \
            "            </interfaces>\n"                                      \
            "          </area>\n"                                              \
            "        </ospf>\n"                                                \
            "      </protocols>\n"                                             \
            "    </top>\n"                                                     \
            "  </data>\n"                                                      \
            "</get-config>\n";

    /* Filter by xpath */
    GET_CONFIG_FILTER(st, "/top/protocols/ospf/area[1]");
    assert_string_equal(st->str, RFC2_FILTER_AREA1);
    FREE_TEST_VARS(st);

    /* since there are two last()-1 should be same as 1 */
    GET_CONFIG_FILTER(st, "/top/protocols/ospf/area[last()-1]");
    assert_string_equal(st->str, RFC2_FILTER_AREA1);
    FREE_TEST_VARS(st);

    /* filter by area name same as the two before */
    GET_CONFIG_FILTER(st, "/top/protocols/ospf/area[name='0.0.0.0']");
    assert_string_equal(st->str, RFC2_FILTER_AREA1);
    FREE_TEST_VARS(st);
}

static void
test_subtree_basic(void **state)
{
    struct np_test *st = *state;
    const char *RFC2_FILTER_AREA1, *subtree1, *F1_SELECTION_NODE_TEST,
            *F1_SELECTION_NODE_RESULT;

    GET_CONFIG(st);

    FREE_TEST_VARS(st);

    subtree1 =
            "<top xmlns=\"rfc2\">\n"              \
            "  <protocols>\n"                     \
            "    <ospf>\n"                        \
            "      <area>\n"                      \
            "        <name>0.0.0.0</name>\n"      \
            "      </area>\n"                     \
            "    </ospf>\n"                       \
            "  </protocols>\n"                    \
            "</top>\n";

    GET_CONFIG_FILTER(st, subtree1);

    RFC2_FILTER_AREA1 =
            "<get-config xmlns=\"urn:ietf:params:xml:ns:netconf:base:1.0\">\n" \
            "  <data>\n"                                                       \
            "    <top xmlns=\"rfc2\">\n"                                       \
            "      <protocols>\n"                                              \
            "        <ospf>\n"                                                 \
            "          <area>\n"                                               \
            "            <name>0.0.0.0</name>\n"                               \
            "            <interfaces>\n"                                       \
            "              <interface>\n"                                      \
            "                <name>192.0.2.1</name>\n"                         \
            "              </interface>\n"                                     \
            "              <interface>\n"                                      \
            "                <name>192.0.2.4</name>\n"                         \
            "              </interface>\n"                                     \
            "            </interfaces>\n"                                      \
            "          </area>\n"                                              \
            "        </ospf>\n"                                                \
            "      </protocols>\n"                                             \
            "    </top>\n"                                                     \
            "  </data>\n"                                                      \
            "</get-config>\n";

    assert_string_equal(st->str, RFC2_FILTER_AREA1);

    FREE_TEST_VARS(st);

    F1_SELECTION_NODE_TEST =
            "<top xmlns=\"f1\">\n"                  \
            "  <devices>\n"                         \
            "    <servers/>\n"                      \
            "  </devices>\n"                        \
            "</top>\n";

    GET_CONFIG_FILTER(st, F1_SELECTION_NODE_TEST);

    F1_SELECTION_NODE_RESULT =
            "<get-config xmlns=\"urn:ietf:params:xml:ns:netconf:base:1.0\">\n" \
            "  <data>\n"                                                       \
            "    <top xmlns=\"f1\">\n"                                         \
            "      <devices>\n"                                                \
            "        <servers>\n"                                              \
            "          <server>\n"                                             \
            "            <name>First</name>\n"                                 \
            "            <address>192.168.0.4</address>\n"                     \
            "            <port>80</port>\n"                                    \
            "          </server>\n"                                            \
            "          <server>\n"                                             \
            "            <name>Second</name>\n"                                \
            "            <address>192.168.0.12</address>\n"                    \
            "            <port>80</port>\n"                                    \
            "          </server>\n"                                            \
            "          <server>\n"                                             \
            "            <name>Fourth</name>\n"                                \
            "            <address>192.168.0.50</address>\n"                    \
            "            <port>22</port>\n"                                    \
            "          </server>\n"                                            \
            "          <server>\n"                                             \
            "            <name>Fifth</name>\n"                                 \
            "            <address>192.168.0.50</address>\n"                    \
            "            <port>443</port>\n"                                   \
            "          </server>\n"                                            \
            "          <server>\n"                                             \
            "            <name>Sixth</name>\n"                                 \
            "            <address>192.168.0.102</address>\n"                   \
            "            <port>22</port>\n"                                    \
            "          </server>\n"                                            \
            "        </servers>\n"                                             \
            "      </devices>\n"                                               \
            "    </top>\n"                                                     \
            "  </data>\n"                                                      \
            "</get-config>\n";

    assert_string_equal(st->str, F1_SELECTION_NODE_RESULT);

    FREE_TEST_VARS(st);
}

static int
change_serial_num(sr_session_ctx_t *session, uint32_t sub_id,
                      const char *module_name, const char *path,
                      const char *request_xpath, uint32_t request_id,
                      struct lyd_node **parent, void *private_data)
{
    (void) session; (void) sub_id; (void) module_name; (void) path;
    (void) request_xpath; (void) request_id; (void) private_data;
    if(!lyd_new_path(*parent, NULL, "serial-num", "1234", 0, NULL))
    {
        return SR_ERR_OK;
    }
    else
    {
        return SR_ERR_LY;
    }
}

static void
test_get(void **state)
{
    struct np_test *st = *state;

    /* TODO: move to state, setup and teardown*/
    sr_subscription_ctx_t *sub = NULL;

    /* TODO: Remove */
    /* sr_log_stderr(SR_LL_DBG); */
    assert_int_equal(SR_ERR_OK,
        sr_oper_get_items_subscribe(st->sr_sess, "issue1",
            "/issue1:hardware/component/serial-num",
            change_serial_num, NULL,
            SR_SUBSCR_DEFAULT, &sub));

    sr_unsubscribe(sub);
}

int
main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_xpath_basic),
        cmocka_unit_test(test_subtree_basic),
        cmocka_unit_test(test_get),
    };

    nc_verbosity(NC_VERB_WARNING);
    return cmocka_run_group_tests(tests, local_setup, local_teardown);
}
