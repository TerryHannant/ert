/*
   copyright (c) 2011  equinor asa, norway.

   the file 'ensemble_config.c' is part of ert - ensemble based reservoir tool.

   ert is free software: you can redistribute it and/or modify
   it under the terms of the gnu general public license as published by
   the free software foundation, either version 3 of the license, or
   (at your option) any later version.

   ert is distributed in the hope that it will be useful, but without any
   warranty; without even the implied warranty of merchantability or
   fitness for a particular purpose.

   see the gnu general public license at <http://www.gnu.org/licenses/gpl.html>
   for more details.
*/

#include <filesystem>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

#include <pthread.h> /* must have rw locking on the config_nodes ... */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <ert/util/hash.h>
#include <ert/util/util.h>

#include <ert/res_util/string.hpp>

#include <ert/ecl/ecl_grid.h>

#include <ert/job_queue/ext_joblist.hpp>

#include <ert/config/config_parser.hpp>

#include <ert/enkf/config_keys.hpp>
#include <ert/enkf/enkf_config_node.hpp>
#include <ert/enkf/enkf_defaults.hpp>
#include <ert/enkf/enkf_obs.hpp>
#include <ert/enkf/ensemble_config.hpp>
#include <ert/enkf/gen_kw_config.hpp>
#include <ert/logging.hpp>

namespace fs = std::filesystem;

#define ENSEMBLE_CONFIG_TYPE_ID 8825306

static auto logger = ert::get_logger("ensemble_config");

namespace {

std::unordered_map<std::string, std::string>
create_opt_map(const config_content_node_type *node, int offset) {
    std::unordered_map<std::string, std::string> options;
    for (int i = offset; i < config_content_node_get_size(node); i++) {
        const char *key_value = config_content_node_iget(node, i);
        char *value = NULL;
        char *key = NULL;

        util_binary_split_string(key_value, ":", true, &key, &value);
        if (value)
            options[key] = value;

        free(key);
        free(value);
    }

    return options;
}

const char *
get_string(const std::unordered_map<std::string, std::string> &opt_map,
           const char *key) {
    const auto &iter = opt_map.find(key);
    if (iter == opt_map.end())
        return NULL;

    return iter->second.c_str();
}

} // namespace

struct ensemble_config_struct {
    UTIL_TYPE_ID_DECLARATION;
    pthread_mutex_t mutex;
    char *
        gen_kw_format_string; /* format string used when creating gen_kw search/replace strings. */
    std::map<std::string, enkf_config_node_type *>
        config_nodes; /* a hash of enkf_config_node instances - which again conatin pointers to e.g. field_config objects.  */
    field_trans_table_type *
        field_trans_table; /* a table of the transformations which are available to apply on fields. */
    bool have_forward_init;
    summary_key_matcher_type *summary_key_matcher;
};

UTIL_IS_INSTANCE_FUNCTION(ensemble_config, ENSEMBLE_CONFIG_TYPE_ID)
UTIL_SAFE_CAST_FUNCTION(ensemble_config, ENSEMBLE_CONFIG_TYPE_ID)

/**
   setting the format string used to 'mangle' the string in the gen_kw
   template files. consider the following example:

      parameter file
      --------------
      multpv   logunif  0.0001 0.10


      template file
      -------------
      box
         1  10  1 10  1 5 /

      multpv  500*__multpv__

   here the parameter file defines a parameter named 'multpv', and the
   template file uses the marker string '__multpv__' which should be
   replaced with a numerical value. for the current example the
   gen_kw_format_string should have the value '__%s__'.

   there are no rules for the format string, but it _must_ contain a
   '%s' placeholder which will be replaced with the parameter name
   (this is not checked for). the function call creating a search
   string from a parameter name is:

      tagged_string = util_alloc_sprintf( gen_kw_format_string , parameter_name );

*/
void ensemble_config_set_gen_kw_format(ensemble_config_type *ensemble_config,
                                       const char *gen_kw_format_string) {
    if (!util_string_equal(gen_kw_format_string,
                           ensemble_config->gen_kw_format_string)) {
        stringlist_type *gen_kw_keys =
            ensemble_config_alloc_keylist_from_impl_type(ensemble_config,
                                                         GEN_KW);
        int i;
        ensemble_config->gen_kw_format_string = util_realloc_string_copy(
            ensemble_config->gen_kw_format_string, gen_kw_format_string);
        for (i = 0; i < stringlist_get_size(gen_kw_keys); i++) {
            enkf_config_node_type *config_node = ensemble_config_get_node(
                ensemble_config, stringlist_iget(gen_kw_keys, i));
            gen_kw_config_update_tag_format(
                (gen_kw_config_type *)enkf_config_node_get_ref(config_node),
                gen_kw_format_string); // CXX_CAST_ERROR
        }
        stringlist_free(gen_kw_keys);
    }
}

field_trans_table_type *
ensemble_config_get_trans_table(const ensemble_config_type *ensemble_config) {
    return ensemble_config->field_trans_table;
}

static ensemble_config_type *ensemble_config_alloc_empty(void) {
    ensemble_config_type *ensemble_config = new ensemble_config_type();

    UTIL_TYPE_ID_INIT(ensemble_config, ENSEMBLE_CONFIG_TYPE_ID);
    ensemble_config->field_trans_table = field_trans_table_alloc();
    ensemble_config->gen_kw_format_string =
        util_alloc_string_copy(DEFAULT_GEN_KW_TAG_FORMAT);
    ensemble_config->have_forward_init = false;
    ensemble_config->summary_key_matcher = summary_key_matcher_alloc();
    pthread_mutex_init(&ensemble_config->mutex, NULL);

    return ensemble_config;
}

ensemble_config_type *
ensemble_config_alloc_full(const char *gen_kw_format_string) {
    ensemble_config_type *ensemble_config = ensemble_config_alloc_empty();
    ensemble_config->field_trans_table = field_trans_table_alloc();
    ensemble_config_set_gen_kw_format(ensemble_config, gen_kw_format_string);
    ensemble_config->summary_key_matcher = summary_key_matcher_alloc();
    pthread_mutex_init(&ensemble_config->mutex, NULL);
    return ensemble_config;
}

ensemble_config_type *ensemble_config_alloc_load(const char *user_config_file,
                                                 ecl_grid_type *grid,
                                                 const ecl_sum_type *refcase) {
    config_parser_type *config_parser = config_alloc();
    config_content_type *config_content = NULL;
    if (user_config_file)
        config_content =
            model_config_alloc_content(user_config_file, config_parser);

    ensemble_config_type *ensemble_config =
        ensemble_config_alloc(config_content, grid, refcase);

    config_content_free(config_content);
    config_free(config_parser);

    return ensemble_config;
}

// forward declaration -> implementation further down
static void ensemble_config_init(ensemble_config_type *ensemble_config,
                                 const config_content_type *config,
                                 ecl_grid_type *grid,
                                 const ecl_sum_type *refcase);

ensemble_config_type *
ensemble_config_alloc(const config_content_type *config_content,
                      ecl_grid_type *grid, const ecl_sum_type *refcase) {

    ensemble_config_type *ensemble_config = ensemble_config_alloc_empty();

    if (config_content)
        ensemble_config_init(ensemble_config, config_content, grid, refcase);

    return ensemble_config;
}

void ensemble_config_free(ensemble_config_type *ensemble_config) {
    field_trans_table_free(ensemble_config->field_trans_table);
    summary_key_matcher_free(ensemble_config->summary_key_matcher);
    free(ensemble_config->gen_kw_format_string);

    for (auto &config_pair : ensemble_config->config_nodes)
        enkf_config_node_free(config_pair.second);

    delete ensemble_config;
}

bool ensemble_config_has_key(const ensemble_config_type *ensemble_config,
                             const char *key) {
    return ensemble_config->config_nodes.count(key) > 0;
}

enkf_config_node_type *
ensemble_config_get_node(const ensemble_config_type *ensemble_config,
                         const char *key) {
    const auto node_it = ensemble_config->config_nodes.find(key);
    if (node_it != ensemble_config->config_nodes.end()) {
        enkf_config_node_type *node = node_it->second;
        return node;
    } else {
        util_abort("%s: ens node:\"%s\" does not exist \n", __func__, key);
        return NULL; /* compiler shut up */
    }
}

enkf_config_node_type *ensemble_config_get_or_create_summary_node(
    ensemble_config_type *ensemble_config, const char *key) {
    if (ensemble_config->config_nodes.count(key) == 0)
        ensemble_config_add_summary(ensemble_config, key, LOAD_FAIL_SILENT);

    return ensemble_config_get_node(ensemble_config, key);
}

bool ensemble_config_have_forward_init(
    const ensemble_config_type *ensemble_config) {
    return ensemble_config->have_forward_init;
}

void ensemble_config_add_node(ensemble_config_type *ensemble_config,
                              enkf_config_node_type *node) {
    if (node) {
        const char *key = enkf_config_node_get_key(node);
        if (ensemble_config_has_key(ensemble_config, key))
            util_abort("%s: a configuration object:%s has already been added - "
                       "aborting \n",
                       __func__, key);

        ensemble_config->config_nodes[key] = node;
        ensemble_config->have_forward_init |=
            enkf_config_node_use_forward_init(node);
    } else
        util_abort("%s: internal error - tried to add NULL node to ensemble "
                   "configuration \n",
                   __func__);
}

void ensemble_config_add_obs_key(ensemble_config_type *ensemble_config,
                                 const char *key, const char *obs_key) {
    enkf_config_node_type *node = ensemble_config->config_nodes.at(key);
    enkf_config_node_add_obs_key(node, obs_key);
}

void ensemble_config_clear_obs_keys(ensemble_config_type *ensemble_config) {
    for (auto &config_pair : ensemble_config->config_nodes) {
        enkf_config_node_type *config_node = config_pair.second;
        enkf_config_node_clear_obs_keys(config_node);
    }
}

void ensemble_config_add_config_items(config_parser_type *config) {
    config_schema_item_type *item;

    /*
      the two fault types are just added to the config object only to
      be able to print suitable messages before exiting.
  */

    item = config_add_schema_item(config, "HAVANA_FAULT", false);
    config_schema_item_set_argc_minmax(item, 2, 2);

    item = config_add_schema_item(config, "MULTFLT", false);
    config_schema_item_set_argc_minmax(item, 3, 3);
    config_schema_item_iset_type(item, 2, CONFIG_EXISTING_PATH);

    item = config_add_schema_item(config, GEN_KW_KEY, false);
    config_schema_item_set_argc_minmax(item, 4, 6);
    config_schema_item_iset_type(item, 1, CONFIG_EXISTING_PATH);
    config_schema_item_iset_type(item, 2, CONFIG_PATH);
    config_schema_item_iset_type(item, 3, CONFIG_EXISTING_PATH);

    item = config_add_key_value(config, GEN_KW_TAG_FORMAT_KEY, false,
                                CONFIG_STRING);
    item = config_add_schema_item(config, SCHEDULE_PREDICTION_FILE_KEY, false);
    /* scedhule_prediction_file   filename  <parameters:> <init_files:> */
    config_schema_item_set_argc_minmax(item, 1, 3);
    config_schema_item_iset_type(item, 0, CONFIG_EXISTING_PATH);

    enkf_config_node_add_GEN_PARAM_config_schema(config);
    enkf_config_node_add_GEN_DATA_config_schema(config);

    item = config_add_schema_item(
        config, SUMMARY_KEY,
        false); /* can have several summary keys on each line. */
    config_schema_item_set_argc_minmax(item, 1, CONFIG_DEFAULT_ARG_MAX);

    item = config_add_schema_item(
        config, CONTAINER_KEY,
        false); /* can have several summary keys on each line. */
    config_schema_item_set_argc_minmax(item, 2, CONFIG_DEFAULT_ARG_MAX);

    item = config_add_schema_item(config, SURFACE_KEY, false);
    config_schema_item_set_argc_minmax(item, 4, 5);

    /*
     the way config info is entered for fields is unfortunate because
     it is difficult/impossible to let the config system handle run
     time validation of the input.
  */

    item = config_add_schema_item(config, FIELD_KEY, false);
    config_schema_item_set_argc_minmax(item, 2, CONFIG_DEFAULT_ARG_MAX);
    config_schema_item_add_required_children(
        item, GRID_KEY); /* if you are using a field - you must have a grid. */
}

static void ensemble_config_init_GEN_DATA(ensemble_config_type *ensemble_config,
                                          const config_content_type *config) {
    if (config_content_has_item(config, GEN_DATA_KEY)) {
        const config_content_item_type *item =
            config_content_get_item(config, GEN_DATA_KEY);
        int i;
        for (i = 0; i < config_content_item_get_size(item); i++) {
            const config_content_node_type *node =
                config_content_item_iget_node(item, i);
            enkf_config_node_type *config_node =
                enkf_config_node_alloc_GEN_DATA_from_config(node);
            if (config_node)
                ensemble_config_add_node(ensemble_config, config_node);
        }
    }
}

void ensemble_config_init_GEN_PARAM(ensemble_config_type *ensemble_config,
                                    const config_content_type *config) {
    /* gen_param  - should be unified with the gen_data*/
    if (config_content_has_item(config, GEN_PARAM_KEY)) {
        const config_content_item_type *item =
            config_content_get_item(config, GEN_PARAM_KEY);
        for (int i = 0; i < config_content_item_get_size(item); i++) {
            const config_content_node_type *node =
                config_content_item_iget_node(item, i);
            enkf_config_node_type *config_node =
                enkf_config_node_alloc_GEN_PARAM_from_config(node);
            if (config_node)
                ensemble_config_add_node(ensemble_config, config_node);
        }
    }
}

static void ensemble_config_init_GEN_KW(ensemble_config_type *ensemble_config,
                                        const config_content_type *config) {
    if (config_content_has_item(config, GEN_KW_KEY)) {
        const config_content_item_type *gen_kw_item =
            config_content_get_item(config, GEN_KW_KEY);
        int i;
        for (i = 0; i < config_content_item_get_size(gen_kw_item); i++) {
            config_content_node_type *node =
                config_content_item_iget_node(gen_kw_item, i);

            const char *key = config_content_node_iget(node, 0);
            const char *template_file =
                config_content_node_iget_as_abspath(node, 1);
            const char *enkf_outfile = config_content_node_iget(node, 2);
            const char *parameter_file =
                config_content_node_iget_as_abspath(node, 3);

            auto opt_map = create_opt_map(node, 4);
            {
                const char *forward_string =
                    get_string(opt_map, FORWARD_INIT_KEY);
                enkf_config_node_type *config_node;
                bool forward_init = false;

                if (forward_string) {
                    if (!util_sscanf_bool(forward_string, &forward_init))
                        fprintf(stderr,
                                "** Warning: parsing %s as bool failed - using "
                                "FALSE \n",
                                forward_string);
                }

                config_node = ensemble_config_add_gen_kw(ensemble_config, key,
                                                         forward_init);
                enkf_config_node_update_gen_kw(
                    config_node, enkf_outfile, template_file, parameter_file,
                    get_string(opt_map, MIN_STD_KEY),
                    get_string(opt_map, INIT_FILES_KEY));
            }
        }
    }
}

static void ensemble_config_init_SURFACE(ensemble_config_type *ensemble_config,
                                         const config_content_type *config) {
    if (config_content_has_item(config, SURFACE_KEY)) {
        const config_content_item_type *item =
            config_content_get_item(config, SURFACE_KEY);
        int i;
        for (i = 0; i < config_content_item_get_size(item); i++) {
            const config_content_node_type *node =
                config_content_item_iget_node(item, i);
            const char *key = config_content_node_iget(node, 0);
            {
                auto opt_map = create_opt_map(node, 1);
                {
                    const char *init_file_fmt =
                        get_string(opt_map, INIT_FILES_KEY);
                    const char *output_file =
                        get_string(opt_map, OUTPUT_FILE_KEY);
                    const char *base_surface =
                        get_string(opt_map, BASE_SURFACE_KEY);
                    const char *min_std_file = get_string(opt_map, MIN_STD_KEY);
                    const char *forward_string =
                        get_string(opt_map, FORWARD_INIT_KEY);
                    bool forward_init = false;

                    if (forward_string) {
                        if (!util_sscanf_bool(forward_string, &forward_init))
                            fprintf(stderr,
                                    "** Warning: parsing %s as bool failed - "
                                    "using FALSE \n",
                                    forward_string);
                    }

                    if ((init_file_fmt == NULL) || (output_file == NULL) ||
                        (base_surface == NULL)) {
                        fprintf(stderr, "** error: when entering a surface you "
                                        "must provide arguments:\n");
                        fprintf(stderr, "**   %s:/path/to/input/files%%d  \n",
                                INIT_FILES_KEY);
                        fprintf(stderr, "**   %s:name_of_output_file\n",
                                OUTPUT_FILE_KEY);
                        fprintf(stderr, "**   %s:base_surface_file\n",
                                BASE_SURFACE_KEY);
                        exit(1);
                    }

                    {
                        enkf_config_node_type *config_node =
                            ensemble_config_add_surface(ensemble_config, key,
                                                        forward_init);
                        enkf_config_node_update_surface(
                            config_node, base_surface, init_file_fmt,
                            output_file, min_std_file);
                    }
                }
            }
        }
    }
}

static void ensemble_config_init_SUMMARY(ensemble_config_type *ensemble_config,
                                         const config_content_type *config,
                                         const ecl_sum_type *refcase) {
    if (config_content_has_item(config, SUMMARY_KEY)) {
        const config_content_item_type *item =
            config_content_get_item(config, SUMMARY_KEY);
        int i;
        for (i = 0; i < config_content_item_get_size(item); i++) {
            const config_content_node_type *node =
                config_content_item_iget_node(item, i);
            int j;
            for (j = 0; j < config_content_node_get_size(node); j++) {
                const char *key = config_content_node_iget(node, j);
                summary_key_matcher_add_summary_key(
                    ensemble_config->summary_key_matcher, key);
                ensemble_config_init_SUMMARY_full(ensemble_config, key,
                                                  refcase);
            }
        }
    }
}

void ensemble_config_init_SUMMARY_full(ensemble_config_type *ensemble_config,
                                       const char *key,
                                       const ecl_sum_type *refcase) {
    summary_key_matcher_add_summary_key(ensemble_config->summary_key_matcher,
                                        key);
    if (util_string_has_wildcard(key)) {
        if (refcase != NULL) {
            stringlist_type *keys = stringlist_alloc_new();
            ecl_sum_select_matching_general_var_list(
                refcase, key,
                keys); /* expanding the wildcard notation with help of the refcase. */
            int k;
            for (k = 0; k < stringlist_get_size(keys); k++)
                ensemble_config_add_summary(ensemble_config,
                                            stringlist_iget(keys, k),
                                            LOAD_FAIL_SILENT);

            stringlist_free(keys);
        }
    } else {
        ensemble_config_add_summary(ensemble_config, key, LOAD_FAIL_SILENT);
    }
}

static void ensemble_config_init_FIELD(ensemble_config_type *ensemble_config,
                                       const config_content_type *config,
                                       ecl_grid_type *grid) {
    if (config_content_has_item(config, FIELD_KEY)) {
        const config_content_item_type *item =
            config_content_get_item(config, FIELD_KEY);
        int i;
        for (i = 0; i < config_content_item_get_size(item); i++) {
            const config_content_node_type *node =
                config_content_item_iget_node(item, i);
            const char *key = config_content_node_iget(node, 0);
            const char *var_type_string = config_content_node_iget(node, 1);
            enkf_config_node_type *config_node;

            {
                int truncation = TRUNCATE_NONE;
                double value_min = -1;
                double value_max = -1;

                auto opt_map = create_opt_map(node, 2);
                if (opt_map.count(MIN_KEY) > 0) {
                    truncation |= TRUNCATE_MIN;
                    value_min = atof(get_string(opt_map, MIN_KEY));
                }

                if (opt_map.count(MAX_KEY) > 0) {
                    truncation |= TRUNCATE_MAX;
                    value_max = atof(get_string(opt_map, MAX_KEY));
                }

                if (strcmp(var_type_string, PARAMETER_KEY) == 0) {
                    const char *ecl_file = config_content_node_iget(node, 2);
                    const char *init_file_fmt =
                        get_string(opt_map, INIT_FILES_KEY);
                    const char *init_transform =
                        get_string(opt_map, INIT_TRANSFORM_KEY);
                    const char *output_transform =
                        get_string(opt_map, OUTPUT_TRANSFORM_KEY);
                    const char *min_std_file = get_string(opt_map, MIN_STD_KEY);
                    const char *forward_string =
                        get_string(opt_map, FORWARD_INIT_KEY);
                    bool forward_init = false;

                    if (forward_string) {
                        if (!util_sscanf_bool(forward_string, &forward_init))
                            fprintf(stderr,
                                    "** Warning: parsing %s as bool failed - "
                                    "using FALSE \n",
                                    forward_string);
                    }
                    config_node = ensemble_config_add_field(
                        ensemble_config, key, grid, forward_init);
                    enkf_config_node_update_parameter_field(
                        config_node, ecl_file, init_file_fmt, min_std_file,
                        truncation, value_min, value_max, init_transform,
                        output_transform);
                } else if (strcmp(var_type_string, GENERAL_KEY) == 0) {
                    /* General - not really interesting .. */
                    const char *ecl_file = config_content_node_iget(node, 2);
                    const char *enkf_infile = config_content_node_iget(node, 3);
                    const char *init_file_fmt =
                        get_string(opt_map, INIT_FILES_KEY);
                    const char *init_transform =
                        get_string(opt_map, INIT_TRANSFORM_KEY);
                    const char *output_transform =
                        get_string(opt_map, OUTPUT_TRANSFORM_KEY);
                    const char *input_transform =
                        get_string(opt_map, INPUT_TRANSFORM_KEY);
                    const char *min_std_file = get_string(opt_map, MIN_STD_KEY);
                    const char *forward_string =
                        get_string(opt_map, FORWARD_INIT_KEY);
                    bool forward_init = false;

                    if (forward_string) {
                        if (!util_sscanf_bool(forward_string, &forward_init))
                            fprintf(stderr,
                                    "** Warning: parsing %s as bool failed - "
                                    "using FALSE \n",
                                    forward_string);
                    }

                    config_node = ensemble_config_add_field(
                        ensemble_config, key, grid, forward_init);
                    enkf_config_node_update_general_field(
                        config_node, ecl_file, enkf_infile, init_file_fmt,
                        min_std_file, truncation, value_min, value_max,
                        init_transform, input_transform, output_transform);

                } else
                    util_abort("%s: field type: %s is not recognized\n",
                               __func__, var_type_string);
            }
        }
    }
}

static void ensemble_config_init_PRED(ensemble_config_type *ensemble_config,
                                      const config_content_type *content) {
    if (!config_content_has_item(content, SCHEDULE_PREDICTION_FILE_KEY))
        return;

    const config_content_item_type *pred_item =
        config_content_get_item(content, SCHEDULE_PREDICTION_FILE_KEY);

    config_content_node_type *pred_node =
        config_content_item_get_last_node(pred_item);
    const char *template_file = config_content_node_iget_as_path(pred_node, 0);
    if (!template_file)
        return;

    hash_type *opt_hash = hash_alloc();
    config_content_node_init_opt_hash(pred_node, opt_hash, 1);
    const char *parameters =
        (const char *)hash_safe_get(opt_hash, PARAMETER_KEY);
    const char *min_std = (const char *)hash_safe_get(opt_hash, MIN_STD_KEY);
    const char *init_files =
        (const char *)hash_safe_get(opt_hash, INIT_FILES_KEY);
    hash_free(opt_hash);

    char *base;
    char *ext;
    util_alloc_file_components(template_file, NULL, &base, &ext);
    char *target_file = util_alloc_filename(NULL, base, ext);
    free(base);
    free(ext);

    enkf_config_node_type *config_node =
        ensemble_config_add_gen_kw(ensemble_config, PRED_KEY, false);
    enkf_config_node_update_gen_kw(config_node, target_file, template_file,
                                   parameters, min_std, init_files);

    free(target_file);
}

/**
   observe that if the user has not given a refcase with the refcase
   key the refcase pointer will be NULL. in that case it will be
   impossible to use wildcards when expanding summary variables.
*/
static void ensemble_config_init(ensemble_config_type *ensemble_config,
                                 const config_content_type *config,
                                 ecl_grid_type *grid,
                                 const ecl_sum_type *refcase) {

    if (config_content_has_item(config, GEN_KW_TAG_FORMAT_KEY)) {
        ensemble_config_set_gen_kw_format(
            ensemble_config,
            config_content_iget(config, GEN_KW_TAG_FORMAT_KEY, 0, 0));
    }

    ensemble_config_init_GEN_PARAM(ensemble_config, config);
    ensemble_config_init_GEN_DATA(ensemble_config, config);
    ensemble_config_init_GEN_KW(ensemble_config, config);
    ensemble_config_init_SURFACE(ensemble_config, config);
    ensemble_config_init_SUMMARY(ensemble_config, config, refcase);
    ensemble_config_init_FIELD(ensemble_config, config, grid);
    ensemble_config_init_PRED(ensemble_config, config);

    /* Containers - this must come last, to ensure that the other nodes have been added. */
    {
        for (int i = 0;
             i < config_content_get_occurences(config, CONTAINER_KEY); i++) {
            const stringlist_type *container_kw_list =
                config_content_iget_stringlist_ref(config, CONTAINER_KEY, i);
            const char *container_key = stringlist_iget(container_kw_list, 0);
            enkf_config_node_type *container_node =
                ensemble_config_add_container(ensemble_config, container_key);

            for (int j = 1; j < stringlist_get_size(container_kw_list); j++) {
                const char *child_key = stringlist_iget(container_kw_list, j);
                enkf_config_node_update_container(
                    container_node,
                    ensemble_config_get_node(ensemble_config, child_key));
            }
        }
    }
}

/**
   @brief Retrieve enkf_config_node by key string in an ensemble_config.

   this function takes a string like this: "pressure:1,4,7" - it
   splits the string on ":" and tries to lookup a config object with
   that key. for the general string a:b:c:d it will try consecutively
   the keys: a, a:b, a:b:c, a:b:c:d. if a config object is found it is
   returned, otherwise NULL is returned.

   the last argument is the pointer to a string which will be updated
   with the node-spesific part of the full key. so for instance with
   the example "pressure:1,4,7", the index_key will contain
   "1,4,7". if the full full_key is used to find an object index_key
   will be NULL, that also applies if no object is found.

   @param[in] config The ensemble_config to retrieve a config_node from.
   @param[in] full_key a string on the form "pressure:1,4,7"
   @param[out] index_key the index part of the key, ie. "1,4,7", NULL if no
    node is found.
   @return The enkf_config_node if found, NULL otherwise.

*/
const enkf_config_node_type *
ensemble_config_user_get_node(const ensemble_config_type *config,
                              const char *full_key, char **index_key) {
    *index_key = NULL;
    auto key_list = ert::split(full_key, USER_KEY_JOIN_STRING[0]);

    std::string current_key;
    auto it =
        std::find_if(key_list.begin(), key_list.end(), [&](auto key) -> bool {
            if (!current_key.empty()) {
                current_key.push_back(USER_KEY_JOIN_STRING[0]);
            }
            current_key += key;
            return ensemble_config_has_key(config, current_key.c_str());
        });
    if (it == key_list.end())
        return nullptr;

    if (current_key.size() < strlen(full_key))
        *index_key = strdup(&full_key[current_key.size() + 1]);
    return ensemble_config_get_node(config, current_key.c_str());
}

stringlist_type *
ensemble_config_alloc_keylist(const ensemble_config_type *config) {
    stringlist_type *s = stringlist_alloc_new();
    for (const auto &config_pair : config->config_nodes)
        stringlist_append_copy(s, config_pair.first.c_str());
    return s;
}

/*
   observe that var_type here is an integer - naturally written as a
   sum of enkf_var_type values:

     ensemble_config_keylist_from_var_type( config , parameter + dynamic_state);

*/

std::vector<std::string>
ensemble_config_keylist_from_var_type(const ensemble_config_type *config,
                                      int var_mask) {
    std::vector<std::string> key_list;

    for (const auto &config_pair : config->config_nodes) {
        const std::string key = config_pair.first;
        enkf_var_type var_type =
            enkf_config_node_get_var_type(config_pair.second);

        if (var_type & var_mask)
            key_list.push_back(key);
    }

    return key_list;
}

stringlist_type *
ensemble_config_alloc_keylist_from_impl_type(const ensemble_config_type *config,
                                             ert_impl_type impl_type) {
    stringlist_type *key_list = stringlist_alloc_new();

    for (const auto &config_pair : config->config_nodes) {
        const char *key = config_pair.first.c_str();
        if (impl_type == enkf_config_node_get_impl_type(config_pair.second))
            stringlist_append_copy(key_list, key);
    }

    return key_list;
}

bool ensemble_config_has_impl_type(const ensemble_config_type *config,
                                   const ert_impl_type impl_type) {
    for (const auto &config_pair : config->config_nodes) {
        if (impl_type == enkf_config_node_get_impl_type(config_pair.second))
            return true;
    }

    return false;
}

bool ensemble_config_require_summary(const ensemble_config_type *ens_config) {
    return ensemble_config_has_impl_type(ens_config, SUMMARY);
}

/**
   this function will look up the user_key in the ensemble_config. if
   the corresponding config_node can not be found 0 will be returned,
   otherwise enkf_config_node functions will be invoked.
*/
int ensemble_config_get_observations(const ensemble_config_type *config,
                                     enkf_obs_type *enkf_obs,
                                     const char *user_key, int obs_count,
                                     time_t *obs_time, double *y, double *std) {
    int num_obs = 0;
    char *index_key;
    const enkf_config_node_type *config_node =
        ensemble_config_user_get_node(config, user_key, &index_key);
    if (config_node != NULL) {
        num_obs = enkf_config_node_load_obs(config_node, enkf_obs, index_key,
                                            obs_count, obs_time, y, std);
        free(index_key);
    }
    return num_obs;
}

/*
   the ensemble_config_add_xxx() functions below will create a new xxx
   instance and add it to the ensemble_config; the return value from
   the functions is the newly created config_node instances.

   the newly created enkf_config_node instances are __not__ fully
   initialized, and a subsequent call to enkf_config_node_update_xxx()
   is essential for proper operation.
*/

enkf_config_node_type *ensemble_config_add_field(ensemble_config_type *config,
                                                 const char *key,
                                                 ecl_grid_type *ecl_grid,
                                                 bool forward_init) {
    enkf_config_node_type *config_node = enkf_config_node_alloc_field(
        key, ecl_grid, config->field_trans_table, forward_init);
    ensemble_config_add_node(config, config_node);
    return config_node;
}

enkf_config_node_type *ensemble_config_add_gen_kw(ensemble_config_type *config,
                                                  const char *key,
                                                  bool forward_init) {
    enkf_config_node_type *config_node = enkf_config_node_new_gen_kw(
        key, config->gen_kw_format_string, forward_init);
    ensemble_config_add_node(config, config_node);
    return config_node;
}

/**
   this function ensures that object contains a node with 'key' and
   type == summary.

   if the @refcase pointer is different from NULL the key will be
   validated. keys which do not exist in the refcase will be ignored,
   a warning will be printed on stderr and the function will return
   NULL.
*/
enkf_config_node_type *
ensemble_config_add_summary(ensemble_config_type *ensemble_config,
                            const char *key, load_fail_type load_fail) {
    enkf_config_node_type *config_node = NULL;

    const auto node_it = ensemble_config->config_nodes.find(key);
    if (node_it != ensemble_config->config_nodes.end()) {
        config_node = node_it->second;
        if (enkf_config_node_get_impl_type(config_node) != SUMMARY) {
            util_abort("%s: ensemble key:%s already exists - but it is not of "
                       "summary type\n",
                       __func__, key);
        }

        summary_config_type *summary_config =
            (summary_config_type *)enkf_config_node_get_ref(config_node);
        summary_config_update_load_fail_mode(summary_config, load_fail);

    } else {
        config_node = enkf_config_node_alloc_summary(key, load_fail);
        ensemble_config_add_node(ensemble_config, config_node);
    }

    return config_node;
}

enkf_config_node_type *
ensemble_config_add_summary_observation(ensemble_config_type *ensemble_config,
                                        const char *key,
                                        load_fail_type load_fail) {
    enkf_config_node_type *config_node =
        ensemble_config_add_summary(ensemble_config, key, load_fail);

    summary_key_matcher_add_summary_key(ensemble_config->summary_key_matcher,
                                        key);

    return config_node;
}

enkf_config_node_type *
ensemble_config_add_surface(ensemble_config_type *ensemble_config,
                            const char *key, bool forward_init) {
    enkf_config_node_type *config_node =
        enkf_config_node_new_surface(key, forward_init);
    ensemble_config_add_node(ensemble_config, config_node);
    return config_node;
}

/**
  If key == NULL the function will create a random key.
*/
enkf_config_node_type *
ensemble_config_add_container(ensemble_config_type *ensemble_config,
                              const char *key) {
    std::string local_key;
    bool random_key = false;
    if (key == NULL) {
        local_key = std::string("ensemble_config_random") +
                    std::to_string(random() % 10000000);
        random_key = true;
    } else {
        local_key = std::string(key);
    }

    enkf_config_node_type *config_node =
        enkf_config_node_new_container(local_key.c_str());
    ensemble_config_add_node(ensemble_config, config_node);
    return config_node;
}

const summary_key_matcher_type *ensemble_config_get_summary_key_matcher(
    const ensemble_config_type *ensemble_config) {
    return ensemble_config->summary_key_matcher;
}

int ensemble_config_get_size(const ensemble_config_type *ensemble_config) {
    return ensemble_config->config_nodes.size();
}

std::pair<fw_load_status, std::string>
ensemble_config_forward_init(const ensemble_config_type *ens_config,
                             const run_arg_type *run_arg) {

    auto result = LOAD_SUCCESSFUL;
    std::string error_msg;
    if (run_arg_get_step1(run_arg) == 0) {
        int iens = run_arg_get_iens(run_arg);
        for (auto &config_pair : ens_config->config_nodes) {
            enkf_config_node_type *config_node = config_pair.second;
            if (enkf_config_node_use_forward_init(config_node)) {
                enkf_node_type *node = enkf_node_alloc(config_node);
                enkf_fs_type *sim_fs = run_arg_get_sim_fs(run_arg);
                node_id_type node_id = {.report_step = 0, .iens = iens};

                /*
           Will not reinitialize; i.e. it is essential that the
           forward model uses the state given from the stored
           instance, and not from the current run of e.g. RMS.
        */

                if (!enkf_node_has_data(node, sim_fs, node_id)) {
                    if (enkf_node_forward_init(
                            node, run_arg_get_runpath(run_arg), iens))
                        enkf_node_store(node, sim_fs, node_id);
                    else {
                        char *init_file = enkf_config_node_alloc_initfile(
                            enkf_node_get_config(node),
                            run_arg_get_runpath(run_arg), iens);

                        if (init_file && !fs::exists(init_file))
                            error_msg = fmt::format(
                                "File not found: {} - failed to initialize "
                                "node: {}\n",
                                init_file, enkf_node_get_key(node));
                        else
                            error_msg =
                                fmt::format("Failed to initialize node: {}\n",
                                            enkf_node_get_key(node));

                        free(init_file);
                        result = LOAD_FAILURE;
                    }
                }
                enkf_node_free(node);
            }
        }
    }
    return {result, error_msg};
}
