/**
 * @file test_notif.c
 * @author Michal Vasko <mvasko@cesnet.cz>
 * @brief tests for notifications
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
    struct np_test *st;
    sr_conn_ctx_t *conn;
    const char *features[] = {NULL};
    const char *module1 = NP_TEST_MODULE_DIR "/notif1.yang";
    int rv;

    /* setup environment necessary for installing module */
    NP_GLOB_SETUP_ENV_FUNC;
    assert_int_equal(setenv_rv, 0);

    /* connect to server and install test modules */
    assert_int_equal(sr_connect(SR_CONN_DEFAULT, &conn), SR_ERR_OK);
    assert_int_equal(sr_install_module(conn, module1, NULL, features),
            SR_ERR_OK);
    assert_int_equal(sr_disconnect(conn), SR_ERR_OK);

    /* setup netopeer2 server */
    if (!(rv = np_glob_setup_np2(state))) {
        /* state is allocated in np_glob_setup_np2 have to set here */
        st = *state;
        /* Open connection to start a session for the tests */
        assert_int_equal(sr_connect(SR_CONN_DEFAULT, &st->conn), SR_ERR_OK);
        assert_int_equal(
                sr_session_start(st->conn, SR_DS_RUNNING, &st->sr_sess),
                SR_ERR_OK);
        assert_non_null(st->ctx = sr_get_context(st->conn));
    }
    return rv;
}

static int
local_teardown(void **state)
{
    struct np_test *st = *state;
    sr_conn_ctx_t *conn;

    /* Close the session and connection needed for tests */
    assert_int_equal(sr_session_stop(st->sr_sess), SR_ERR_OK);
    assert_int_equal(sr_disconnect(st->conn), SR_ERR_OK);

    /* connect to server and remove test modules */
    assert_int_equal(sr_connect(SR_CONN_DEFAULT, &conn), SR_ERR_OK);
    assert_int_equal(sr_remove_module(conn, "notif1"), SR_ERR_OK);
    assert_int_equal(sr_disconnect(conn), SR_ERR_OK);

    /* close netopeer2 server */
    return np_glob_teardown(state);
}

static void
test_all_notif(void **state)
{
    struct np_test *st = *state;
    const char *data;
    struct ly_in *in;

    st->rpc = nc_rpc_subscribe(NULL, NULL, NULL, NULL, NC_PARAMTYPE_CONST);

    st->msgtype = nc_send_rpc(st->nc_sess, st->rpc, 1000, &st->msgid);
    assert_int_equal(NC_MSG_RPC, st->msgtype);

    /* check reply */
    st->msgtype = nc_recv_reply(st->nc_sess, st->rpc, st->msgid, 1000,
            &st->envp, &st->op);
    assert_int_equal(NC_MSG_REPLY, st->msgtype);
    assert_null(st->op);
    assert_string_equal(LYD_NAME(lyd_child(st->envp)), "ok");
    FREE_TEST_VARS(st);

    /* Parse notification into lyd_node */
    data =
            "<n1 xmlns=\"n1\">\n"                   \
            "  <first>Test</first>\n"               \
            "</n1>\n";

    ly_in_new_memory(data, &in);

    assert_int_equal(LY_SUCCESS,
            lyd_parse_op(st->ctx, NULL, in, LYD_XML,
            LYD_TYPE_NOTIF_YANG, &st->node, NULL));

    /* Send the notification */
    sr_event_notif_send_tree(st->sr_sess, st->node, 1000, 1);
    ly_in_free(in, 0);

    /* Receive the notification and test the contents */
    st->msgtype = nc_recv_notif(st->nc_sess, 1000, &st->envp, &st->op);
    assert_int_equal(NC_MSG_NOTIF, st->msgtype);
    assert_int_equal(lyd_print_mem(&st->str, st->op, LYD_XML, 0),
            LY_SUCCESS);

    assert_string_equal(data, st->str);

    FREE_TEST_VARS(st);
}

int
main(int argc, char **argv)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_all_notif),
    };

    nc_verbosity(NC_VERB_WARNING);
    parse_arg(argc, argv);
    return cmocka_run_group_tests(tests, local_setup, local_teardown);
}
