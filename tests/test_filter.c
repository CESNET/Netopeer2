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

static int
local_setup(void **state)
{
    sr_conn_ctx_t *conn;
    const char *features[] = {NULL};
    const char *module1 = NP_TEST_MODULE_DIR "/rfc2.yang";
    const char *module2 = NP_TEST_MODULE_DIR "/filter1.yang";

    /* setup environment necessary for installing module */
    NP_GLOB_SETUP_ENV_FUNC;
    assert_int_equal(setenv_rv, 0);

    /* connect to server and install test modules */
    assert_int_equal(sr_connect(SR_CONN_DEFAULT, &conn), SR_ERR_OK);
    assert_int_equal(sr_install_module(conn, module1, NULL, features),
            SR_ERR_OK);
    assert_int_equal(sr_install_module(conn, module2, NULL, features),
            SR_ERR_OK);
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
    assert_int_equal(sr_remove_module(conn, "rfc2"), SR_ERR_OK);
    assert_int_equal(sr_remove_module(conn, "filter1"), SR_ERR_OK);
    assert_int_equal(sr_disconnect(conn), SR_ERR_OK);

    /* close netopeer2 server */
    return np_glob_teardown(state);
}

/* TODO: Add more modules to test filter on */

static void
test_xpath_basic(void **state)
{
    struct np_test *st = *state;
    const char *RFC2_COMPLEX_DATA, *RFC2_FILTER_AREA1, *RFC2_DELETE_ALL;

    RFC2_COMPLEX_DATA =
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
            "</top>";

    /* Send rpc editing rfc2 */
    SEND_EDIT_RPC(st, RFC2_COMPLEX_DATA);

    /* Receive a reply, should succeed */
    ASSERT_OK_REPLY(st);

    FREE_TEST_VARS(st);

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

    RFC2_DELETE_ALL =
            "<top xmlns=\"rfc2\""                                   \
            "xmlns:xc=\"urn:ietf:params:xml:ns:netconf:base:1.0\""  \
            "xc:operation=\"delete\">"                              \
            "</top>";

    /* Send rpc deleting part of the data from module rfc2 */
    SEND_EDIT_RPC(st, RFC2_DELETE_ALL);

    /* Receive a reply, should succeed */
    ASSERT_OK_REPLY(st);

    FREE_TEST_VARS(st);

    /* Check if empty config */
    ASSERT_EMPTY_CONFIG(st);
}

static void
test_subtree_basic(void **state)
{
    /* TODO: Check with multiple configs without namespace */
    struct np_test *st = *state;
    const char *RFC2_COMPLEX_DATA, *RFC2_DELETE_ALL, *RFC2_FILTER_AREA1,
            *subtree1, *F1_DATA, *F1_DELETE_ALL, *F1_SELECTION_NODE_TEST,
            *F1_SELECTION_NODE_RESULT, *NO_NAMESPACE_FILTER;

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

    /* TODO: Returns empty data on "", is that expected? */
    /* TODO: Similar test is already in test_rpc.c but for get, is it needed? */
    GET_CONFIG_FILTER(st, NULL);

    const char *RFC2_EMPTY_FILTER =
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
            "          <area>\n"                                               \
            "            <name>192.168.0.0</name>\n"                           \
            "            <interfaces>\n"                                       \
            "              <interface>\n"                                      \
            "                <name>192.168.0.1</name>\n"                       \
            "              </interface>\n"                                     \
            "              <interface>\n"                                      \
            "                <name>192.168.0.12</name>\n"                      \
            "              </interface>\n"                                     \
            "              <interface>\n"                                      \
            "                <name>192.168.0.25</name>\n"                      \
            "              </interface>\n"                                     \
            "            </interfaces>\n"                                      \
            "          </area>\n"                                              \
            "        </ospf>\n"                                                \
            "      </protocols>\n"                                             \
            "    </top>\n"                                                     \
            "  </data>\n"                                                      \
            "</get-config>\n";

    assert_string_equal(st->str, RFC2_EMPTY_FILTER);

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

    NO_NAMESPACE_FILTER =
            "<top/>";

    GET_CONFIG_FILTER(st, NO_NAMESPACE_FILTER);

    /* TODO: Change this once wildcards work */
    printf("||%s||", st->str);

    FREE_TEST_VARS(st);

    RFC2_DELETE_ALL =
            "<top xmlns=\"rfc2\""                                   \
            "xmlns:xc=\"urn:ietf:params:xml:ns:netconf:base:1.0\""  \
            "xc:operation=\"delete\">"                              \
            "</top>";

    /* Send rpc deleting part of the data from module rfc2 */
    SEND_EDIT_RPC(st, RFC2_DELETE_ALL);

    /* Receive a reply, should succeed */
    ASSERT_OK_REPLY(st);

    FREE_TEST_VARS(st);

    F1_DELETE_ALL =
            "<top xmlns=\"f1\""                                         \
            "xmlns:xc=\"urn:ietf:params:xml:ns:netconf:base:1.0\""      \
            "xc:operation=\"delete\">"                                  \
            "</top>";

    /* Send rpc deleting part of the data from module f1 */
    SEND_EDIT_RPC(st, F1_DELETE_ALL);

    /* Receive a reply, should succeed */
    ASSERT_OK_REPLY(st);

    FREE_TEST_VARS(st);

    /* Check if empty config */
    ASSERT_EMPTY_CONFIG(st);
}

int
main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_xpath_basic),
        cmocka_unit_test(test_subtree_basic),
    };

    nc_verbosity(NC_VERB_WARNING);
    return cmocka_run_group_tests(tests, local_setup, local_teardown);
}
