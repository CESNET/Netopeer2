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

    /* setup environment necessary for installing module */
    NP_GLOB_SETUP_ENV_FUNC;
    assert_int_equal(setenv_rv, 0);

    /* connect to server and install test modules */
    assert_int_equal(sr_connect(SR_CONN_DEFAULT, &conn), SR_ERR_OK);
    assert_int_equal(sr_install_module(conn, module1, NULL, features),
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
    assert_int_equal(sr_disconnect(conn), SR_ERR_OK);

    /* close netopeer2 server */
    return np_glob_teardown(state);
}

/* TODO: Add more modules to test filter on */

/* TODO: Test subpath filtering */

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

int
main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_xpath_basic),
    };

    nc_verbosity(NC_VERB_WARNING);
    return cmocka_run_group_tests(tests, local_setup, local_teardown);
}
