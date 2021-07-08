/**
 * @file np_test.h
 * @author Michal Vasko <mvasko@cesnet.cz>
 * @brief base header for netopeer2 testing
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

#ifndef _NP_TEST_H_
#define _NP_TEST_H_

#include <string.h>
#include <unistd.h>

#include <nc_client.h>

/* global setpu for environment variables for sysrepo*/
#define NP_GLOB_SETUP_ENV_FUNC \
    char file[64]; \
    int setenv_rv; \
\
    strcpy(file, __FILE__); \
    file[strlen(file) - 2] = '\0'; \
    setenv_rv = setup_setenv_sysrepo(strrchr(file, '/') + 1);

#define FREE_TEST_VARS(state)                   \
    nc_rpc_free(state->rpc);                    \
    lyd_free_tree(state->envp);                 \
    lyd_free_tree(state->op);                   \
    if(st->str)                                 \
    {                                           \
        free(st->str);                          \
    }                                           \
    st->str = NULL;

#define ASSERT_OK_REPLY(state)                                                 \
    state->msgtype = nc_recv_reply(state->nc_sess, state->rpc, state->msgid,   \
                                   2000, &state->envp, &state->op);            \
    assert_int_equal(state->msgtype, NC_MSG_REPLY);                            \
    assert_null(state->op);                                                    \
    assert_string_equal(LYD_NAME(lyd_child(state->envp)), "ok");

#define ASSERT_OK_REPLY_SESS2(state)                                           \
    state->msgtype = nc_recv_reply(state->nc_sess2, state->rpc, state->msgid,  \
                                   2000, &state->envp, &state->op);            \
    assert_int_equal(state->msgtype, NC_MSG_REPLY);                            \
    assert_null(state->op);                                                    \
    assert_string_equal(LYD_NAME(lyd_child(state->envp)), "ok");

#define ASSERT_RPC_ERROR(state)                                                \
    state->msgtype = nc_recv_reply(state->nc_sess, state->rpc, state->msgid,   \
                                   2000, &state->envp, &state->op);            \
    assert_int_equal(state->msgtype, NC_MSG_REPLY);                            \
    assert_null(state->op);                                                    \
    assert_string_equal(LYD_NAME(lyd_child(state->envp)), "rpc-error");

#define ASSERT_RPC_ERROR_SESS2(state)                                          \
    state->msgtype = nc_recv_reply(state->nc_sess2, state->rpc, state->msgid,  \
                                   2000, &state->envp, &state->op);            \
    assert_int_equal(state->msgtype, NC_MSG_REPLY);                            \
    assert_null(state->op);                                                    \
    assert_string_equal(LYD_NAME(lyd_child(state->envp)), "rpc-error");

/* test state structure */
struct np_test {
    pid_t server_pid;
    struct nc_session *nc_sess;
    struct nc_session *nc_sess2;
    struct nc_rpc *rpc;
    NC_MSG_TYPE msgtype;
    uint64_t msgid;
    struct lyd_node *envp, *op;
    char *str;
};

int np_glob_setup_np2(void **state);

int setup_setenv_sysrepo(const char *test_name);

int np_glob_teardown(void **state);

#endif /* _NP_TEST_H_ */
