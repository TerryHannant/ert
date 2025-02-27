/*
   Copyright (C) 2012  Equinor ASA, Norway.

   The file 'workflow.c' is part of ERT - Ensemble based Reservoir Tool.

   ERT is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   ERT is distributed in the hope that it will be useful, but WITHOUT ANY
   WARRANTY; without even the implied warranty of MERCHANTABILITY or
   FITNESS FOR A PARTICULAR PURPOSE.

   See the GNU General Public License at <http://www.gnu.org/licenses/gpl.html>
   for more details.
*/

#include <filesystem>

#include <stdlib.h>

#include <ert/res_util/subst_list.hpp>
#include <ert/util/int_vector.hpp>
#include <ert/util/stringlist.hpp>
#include <ert/util/vector.hpp>

#include <ert/config/config_parser.hpp>

#include <ert/job_queue/workflow.hpp>

namespace fs = std::filesystem;

#define CMD_TYPE_ID 66153
#define WORKFLOW_TYPE_ID 6762081
#define WORKFLOW_COMMENT_STRING "--"
#define WORKFLOW_INCLUDE "INCLUDE"

typedef struct cmd_struct cmd_type;

struct cmd_struct {
    UTIL_TYPE_ID_DECLARATION;
    const workflow_job_type *workflow_job;
    stringlist_type *arglist;
};

struct workflow_struct {
    UTIL_TYPE_ID_DECLARATION;
    time_t compile_time;
    bool compiled;
    char *src_file;
    vector_type *cmd_list;
    workflow_joblist_type *joblist;
    config_error_type *last_error;
    vector_type *stack;
};

static cmd_type *cmd_alloc(const workflow_job_type *workflow_job,
                           const stringlist_type *arglist) {
    cmd_type *cmd = (cmd_type *)util_malloc(sizeof *cmd);
    UTIL_TYPE_ID_INIT(cmd, CMD_TYPE_ID);
    cmd->workflow_job = workflow_job;
    cmd->arglist = stringlist_alloc_deep_copy(arglist);
    return cmd;
}

static UTIL_SAFE_CAST_FUNCTION(cmd, CMD_TYPE_ID);

static void cmd_free(cmd_type *cmd) {
    stringlist_free(cmd->arglist);
    free(cmd);
}

static void cmd_free__(void *arg) {
    cmd_type *cmd = cmd_safe_cast(arg);
    cmd_free(cmd);
}

static void workflow_add_cmd(workflow_type *workflow, cmd_type *cmd) {
    vector_append_owned_ref(workflow->cmd_list, cmd, cmd_free__);
}

static void workflow_clear(workflow_type *workflow) {
    vector_clear(workflow->cmd_list);
}

static void workflow_store_error(workflow_type *workflow,
                                 const config_error_type *error) {
    if (workflow->last_error)
        config_error_free(workflow->last_error);

    if (error)
        workflow->last_error = config_error_alloc_copy(error);
    else
        workflow->last_error = NULL;
}

bool workflow_try_compile(workflow_type *script,
                          const subst_list_type *context) {
    if (fs::exists(script->src_file)) {
        const char *src_file = script->src_file;
        char *tmp_file = NULL;
        bool update = false;
        if (context != NULL) {
            tmp_file = util_alloc_tmp_file("/tmp", "ert-workflow", false);
            update =
                subst_list_filter_file(context, script->src_file, tmp_file);
            if (update) {
                script->compiled = false;
                src_file = tmp_file;
            } else {
                remove(tmp_file);
                free(tmp_file);
                tmp_file = NULL;
            }
        }

        {
            time_t src_mtime = util_file_mtime(script->src_file);
            if (script->compiled) {
                if (util_difftime_seconds(src_mtime, script->compile_time) > 0)
                    return true;
                else {
                    // Script has been compiled succesfully, but then changed afterwards.
                    // We try to recompile; if that fails we are left with 'nothing'.
                }
            }
        }

        {
            // Try to compile
            config_parser_type *config_compiler =
                workflow_joblist_get_compiler(script->joblist);
            script->compiled = false;
            workflow_clear(script);
            {
                config_content_type *content =
                    config_parse(config_compiler, src_file,
                                 WORKFLOW_COMMENT_STRING, WORKFLOW_INCLUDE,
                                 NULL, NULL, CONFIG_UNRECOGNIZED_ERROR, true);

                if (config_content_is_valid(content)) {
                    int cmd_line;
                    for (cmd_line = 0;
                         cmd_line < config_content_get_size(content);
                         cmd_line++) {
                        const config_content_node_type *node =
                            config_content_iget_node(content, cmd_line);
                        const char *jobname = config_content_node_get_kw(node);
                        const workflow_job_type *job =
                            workflow_joblist_get_job(script->joblist, jobname);
                        cmd_type *cmd = cmd_alloc(
                            job, config_content_node_get_stringlist(node));

                        workflow_add_cmd(script, cmd);
                    }
                    script->compiled = true;
                } else
                    workflow_store_error(script,
                                         config_content_get_errors(content));

                config_content_free(content);
            }
        }

        if (tmp_file != NULL) {
            if (script->compiled)
                remove(tmp_file);
            free(tmp_file);
        }
    }

    // It is legal to remove the script after successful compilation but
    // then the context will not be applied at subsequent invocations.
    return script->compiled;
}

bool workflow_run(workflow_type *workflow, void *self, bool verbose,
                  const subst_list_type *context) {
    vector_clear(workflow->stack);
    workflow_try_compile(workflow, context);

    if (workflow->compiled) {
        int icmd;
        for (icmd = 0; icmd < vector_get_size(workflow->cmd_list); icmd++) {
            const cmd_type *cmd =
                (const cmd_type *)vector_iget_const(workflow->cmd_list, icmd);
            void *return_value = workflow_job_run(cmd->workflow_job, self,
                                                  verbose, cmd->arglist);
            vector_push_front_ref(workflow->stack, return_value);
        }
        return true;
    } else
        return false;
}

int workflow_get_stack_size(const workflow_type *workflow) {
    return vector_get_size(workflow->stack);
}

void *workflow_iget_stack_ptr(const workflow_type *workflow, int index) {
    return vector_iget(workflow->stack, index);
}

void *workflow_pop_stack(workflow_type *workflow) {
    return vector_pop_front(workflow->stack);
}

workflow_type *workflow_alloc(const char *src_file,
                              workflow_joblist_type *joblist) {
    workflow_type *script = (workflow_type *)util_malloc(sizeof *script);
    UTIL_TYPE_ID_INIT(script, WORKFLOW_TYPE_ID);

    script->src_file = util_alloc_string_copy(src_file);
    script->joblist = joblist;
    script->cmd_list = vector_alloc_new();
    script->compiled = false;
    script->last_error = NULL;
    script->stack = vector_alloc_new();

    workflow_try_compile(script, NULL);
    return script;
}

static UTIL_SAFE_CAST_FUNCTION(workflow, WORKFLOW_TYPE_ID)
    UTIL_IS_INSTANCE_FUNCTION(workflow, WORKFLOW_TYPE_ID)

        void workflow_free(workflow_type *workflow) {
    free(workflow->src_file);
    vector_free(workflow->cmd_list);
    vector_free(workflow->stack);

    if (workflow->last_error)
        config_error_free(workflow->last_error);

    free(workflow);
}

void workflow_free__(void *arg) {
    workflow_type *workflow = workflow_safe_cast(arg);
    workflow_free(workflow);
}

const config_error_type *
workflow_get_last_error(const workflow_type *workflow) {
    return workflow->last_error;
}

int workflow_size(const workflow_type *workflow) {
    return vector_get_size(workflow->cmd_list);
}

const workflow_job_type *workflow_iget_job(const workflow_type *workflow,
                                           int index) {
    const cmd_type *cmd =
        (const cmd_type *)vector_iget_const(workflow->cmd_list, index);
    return cmd->workflow_job;
}

stringlist_type *workflow_iget_arguments(const workflow_type *workflow,
                                         int index) {
    const cmd_type *cmd =
        (const cmd_type *)vector_iget_const(workflow->cmd_list, index);
    return cmd->arglist;
}

extern "C" PY_USED const char *
worflow_get_src_file(const workflow_type *workflow) {
    return workflow->src_file;
}
