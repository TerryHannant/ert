/*
   Copyright (C) 2011  Equinor ASA, Norway.

   The file 'field.c' is part of ERT - Ensemble based Reservoir Tool.

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

#include <Eigen/Dense>
#include <cmath>
#include <stdlib.h>
#include <string.h>

#include <ert/res_util/file_utils.hpp>
#include <ert/util/buffer.h>
#include <ert/util/rng.h>
#include <ert/util/util.h>

#include <ert/ecl/ecl_endian_flip.h>
#include <ert/ecl/ecl_kw.h>
#include <ert/ecl/fortio.h>

#include <ert/rms/rms_file.hpp>
#include <ert/rms/rms_util.hpp>

#include <ert/enkf/field.hpp>

namespace fs = std::filesystem;

GET_DATA_SIZE_HEADER(field);

/**
  The field data type contains for "something" which is distributed
  over the full grid, i.e. permeability or pressure. All configuration
  information is stored in the config object, which is of type
  field_config_type. Observe the following:

  * The field **only** contains the active cells - the config object
    has a reference to actnum information.

  * The data is stored in a char pointer; the real underlying data can
    be (at least) of the types int, float and double.
*/
struct field_struct {
    int __type_id;
    /** The field config object - containing information of active cells++ */
    const field_config_type *config;
    /** The actual storage for the field - suitabley casted to int/float/double on use*/
    char *data;

    /** If the data is shared - i.e. managed (xalloc & free) from another scope. */
    bool shared_data;
    /** The size of the shared buffer (if it is shared). */
    int shared_byte_size;
    /** IFF an output transform should be applied this pointer will hold the
     * transformed data. */
    char *export_data;
    /** IFF an output transform, this pointer will hold the original data
     * during the transform and export. */
    char *__data;
};

#define EXPORT_MACRO                                                           \
    {                                                                          \
        int nx, ny, nz;                                                        \
        field_config_get_dims(field->config, &nx, &ny, &nz);                   \
        int i, j, k;                                                           \
        for (k = 0; k < nz; k++) {                                             \
            for (j = 0; j < ny; j++) {                                         \
                for (i = 0; i < nx; i++) {                                     \
                    bool active_cell =                                         \
                        field_config_active_cell(config, i, j, k);             \
                    bool use_initial_value = false;                            \
                                                                               \
                    if (init_file && !active_cell)                             \
                        use_initial_value = true;                              \
                                                                               \
                    int source_index = 0;                                      \
                    if (use_initial_value)                                     \
                        source_index =                                         \
                            field_config_global_index(config, i, j, k);        \
                    else                                                       \
                        source_index =                                         \
                            field_config_active_index(config, i, j, k);        \
                                                                               \
                    int target_index;                                          \
                    if (rms_index_order)                                       \
                        target_index = rms_util_global_index_from_eclipse_ijk( \
                            nx, ny, nz, i, j, k);                              \
                    else                                                       \
                        target_index = i + j * nx + k * nx * ny;               \
                                                                               \
                    if (use_initial_value)                                     \
                        target_data[target_index] =                            \
                            initial_src_data[source_index];                    \
                    else if (active_cell)                                      \
                        target_data[target_index] = src_data[source_index];    \
                    else                                                       \
                        memcpy(&target_data[target_index], fill_value,         \
                               sizeof_ctype_target);                           \
                }                                                              \
            }                                                                  \
        }                                                                      \
    }

void field_export3D(const field_type *field, void *_target_data,
                    bool rms_index_order, ecl_data_type target_data_type,
                    void *fill_value, const char *init_file) {
    const field_config_type *config = field->config;
    ecl_data_type data_type = field_config_get_ecl_data_type(config);
    int sizeof_ctype_target = ecl_type_get_sizeof_ctype(target_data_type);

    field_type *initial_field = NULL;
    field_config_type *initial_field_config = NULL;
    if (init_file) {
        ecl_grid_type *grid = field_config_get_grid(config);
        bool global_size = true;
        initial_field_config = field_config_alloc_empty(
            field_config_get_key(config), grid, NULL, global_size);
        initial_field = field_alloc(initial_field_config);

        field_fload_keep_inactive(initial_field, init_file);
    }
    switch (ecl_type_get_type(data_type)) {
    case (ECL_DOUBLE_TYPE): {
        const double *src_data = (const double *)field->data;
        const double *initial_src_data =
            initial_field ? (const double *)initial_field->data : NULL;

        if (ecl_type_is_float(target_data_type)) {
            float *target_data = (float *)_target_data;
            EXPORT_MACRO;
        } else if (ecl_type_is_double(target_data_type)) {
            double *target_data = (double *)_target_data;
            EXPORT_MACRO;
        } else {
            fprintf(stderr,
                    "%s: double field can only export to double/float\n",
                    __func__);
            abort();
        }
    } break;
    case (ECL_FLOAT_TYPE): {
        const float *src_data = (const float *)field->data;
        const float *initial_src_data =
            initial_field ? (const float *)initial_field->data : NULL;
        if (ecl_type_is_float(target_data_type)) {
            float *target_data = (float *)_target_data;
            EXPORT_MACRO;
        } else if (ecl_type_is_double(target_data_type)) {
            double *target_data = (double *)_target_data;
            EXPORT_MACRO;
        } else {
            fprintf(stderr, "%s: float field can only export to double/float\n",
                    __func__);
            abort();
        }
    } break;
    case (ECL_INT_TYPE): {
        const int *src_data = (const int *)field->data;
        const int *initial_src_data =
            initial_field ? (const int *)initial_field->data : NULL;
        if (ecl_type_is_float(target_data_type)) {
            float *target_data = (float *)_target_data;
            EXPORT_MACRO;
        } else if (ecl_type_is_double(target_data_type)) {
            double *target_data = (double *)_target_data;
            EXPORT_MACRO;
        } else if (ecl_type_is_int(target_data_type)) {
            int *target_data = (int *)_target_data;
            EXPORT_MACRO;
        } else {
            fprintf(stderr,
                    "%s: int field can only export to int/double/float\n",
                    __func__);
            abort();
        }
    } break;
    default:
        fprintf(stderr, "%s: Sorry field has unexportable type ... \n",
                __func__);
        break;
    }

    if (initial_field) {
        field_config_free(initial_field_config);
        field_free(initial_field);
    }
}
#undef EXPORT_MACRO

#define IMPORT_MACRO                                                           \
    {                                                                          \
        int i, j, k;                                                           \
        int nx, ny, nz;                                                        \
        field_config_get_dims(field->config, &nx, &ny, &nz);                   \
        for (k = 0; k < nz; k++) {                                             \
            for (j = 0; j < ny; j++) {                                         \
                for (i = 0; i < nx; i++) {                                     \
                    int target_index =                                         \
                        keep_inactive_cells                                    \
                            ? field_config_global_index(config, i, j, k)       \
                            : field_config_active_index(config, i, j, k);      \
                                                                               \
                    if (target_index >= 0) {                                   \
                        int source_index;                                      \
                        if (rms_index_order)                                   \
                            source_index =                                     \
                                rms_util_global_index_from_eclipse_ijk(        \
                                    nx, ny, nz, i, j, k);                      \
                        else                                                   \
                            source_index = i + j * nx + k * nx * ny;           \
                                                                               \
                        target_data[target_index] = src_data[source_index];    \
                    }                                                          \
                }                                                              \
            }                                                                  \
        }                                                                      \
    }

/**
   The main function of the field_import3D and field_export3D
   functions are to skip the inactive cells (field_import3D) and
   distribute inactive cells (field_export3D).
   When the flag keep_inactive_cells is set for field_import3D,
   the values for the inactive cells are kept. The field argument
   must have been allocated with flag global_size = true for this
   to work.
   When field_export3D is called with argument INIT_FILE, the
   exported values for inactive cells are read from the INIT_FILE.

   In addition we can reorganize input/output according to the
   RMS Roff index convention, and also perform float <-> double
   conversions.

   Observe that these functions only import/export onto memory
   buffers, the actual reading and writing of files is done in other
   functions (calling these).
*/
static void field_import3D(field_type *field, const void *_src_data,
                           bool rms_index_order, bool keep_inactive_cells,
                           ecl_data_type src_type) {
    const field_config_type *config = field->config;
    ecl_data_type data_type = field_config_get_ecl_data_type(config);

    switch (ecl_type_get_type(data_type)) {
    case (ECL_DOUBLE_TYPE): {
        double *target_data = (double *)field->data;
        if (ecl_type_is_float(src_type)) {
            float *src_data = (float *)_src_data;
            IMPORT_MACRO;
        } else if (ecl_type_is_double(src_type)) {
            double *src_data = (double *)_src_data;
            IMPORT_MACRO;
        } else if (ecl_type_is_int(src_type)) {
            int *src_data = (int *)_src_data;
            IMPORT_MACRO;
        } else {
            fprintf(stderr,
                    "%s: double field can only import from int/double/float\n",
                    __func__);
            abort();
        }
    } break;
    case (ECL_FLOAT_TYPE): {
        float *target_data = (float *)field->data;
        if (ecl_type_is_float(src_type)) {
            float *src_data = (float *)_src_data;
            IMPORT_MACRO;
        } else if (ecl_type_is_double(src_type)) {
            double *src_data = (double *)_src_data;
            IMPORT_MACRO;
        } else if (ecl_type_is_int(src_type)) {
            int *src_data = (int *)_src_data;
            IMPORT_MACRO;
        } else {
            fprintf(stderr,
                    "%s: double field can only import from int/double/float\n",
                    __func__);
            abort();
        }
    } break;
    case (ECL_INT_TYPE): {
        int *target_data = (int *)field->data;
        if (ecl_type_is_int(src_type)) {
            int *src_data = (int *)_src_data;
            IMPORT_MACRO;
        } else {
            fprintf(stderr, "%s: int field can only import from int\n",
                    __func__);
            abort();
        }
    } break;
    default:
        fprintf(stderr, "%s: Sorry field has unimportable type ... \n",
                __func__);
        break;
    }
}
#undef IMPORT_MACRO

#define CLEAR_MACRO(d, s)                                                      \
    {                                                                          \
        int k;                                                                 \
        for (k = 0; k < (s); k++)                                              \
            (d)[k] = 0;                                                        \
    }
C_USED void field_clear(field_type *field) {
    const ecl_data_type data_type =
        field_config_get_ecl_data_type(field->config);
    const int data_size = field_config_get_data_size(field->config);

    switch (data_type.type) {
    case (ECL_DOUBLE_TYPE): {
        double *data = (double *)field->data;
        CLEAR_MACRO(data, data_size);
        break;
    }
    case (ECL_FLOAT_TYPE): {
        float *data = (float *)field->data;
        CLEAR_MACRO(data, data_size);
        break;
    }
    case (ECL_INT_TYPE): {
        int *data = (int *)field->data;
        CLEAR_MACRO(data, data_size);
        break;
    }
    default:
        util_abort("%s: not implemeneted for data_type: %d \n", __func__,
                   data_type.type);
    }
}
#undef CLEAR_MACRO

static field_type *__field_alloc(const field_config_type *field_config,
                                 void *shared_data, int shared_byte_size) {
    field_type *field = (field_type *)util_malloc(sizeof *field);
    field->config = field_config;
    if (shared_data == NULL) {
        field->shared_data = false;
        field->data = (char *)util_calloc(
            field_config_get_byte_size(field->config), sizeof *field->data);
    } else {
        field->data = (char *)shared_data;
        field->shared_data = true;
        field->shared_byte_size = shared_byte_size;
        if (shared_byte_size < field_config_get_byte_size(field->config))
            util_abort("%s: the shared buffer is to small to hold the input "
                       "field - aborting \n",
                       __func__);
    }
    field->export_data =
        NULL; /* This NULL is checked for in the revert_output_transform() */
    field->__type_id = FIELD;
    return field;
}

int field_get_size(const field_type *field) {
    return field_config_get_data_size(field->config);
}

field_type *field_alloc(const field_config_type *field_config) {
    return __field_alloc(field_config, NULL, 0);
}

C_USED void field_copy(const field_type *src, field_type *target) {
    if (src->config == target->config)
        memcpy(target->data, src->data,
               field_config_get_byte_size(src->config));
    else
        util_abort("%s: instances do not share config \n", __func__);
}

void field_read_from_buffer(field_type *field, buffer_type *buffer,
                            enkf_fs_type *fs, int report_step) {
    int byte_size = field_config_get_byte_size(field->config);
    enkf_util_assert_buffer_type(buffer,
                                 FIELD); // FIXME flaky runpath_list test
    buffer_fread_compressed(buffer, buffer_get_remaining_size(buffer),
                            field->data, byte_size);
}

static void *__field_alloc_3D_data(const field_type *field, int data_size,
                                   bool rms_index_order,
                                   ecl_data_type data_type,
                                   ecl_data_type target_data_type,
                                   const char *init_file) {
    void *data = (void *)util_calloc(
        data_size, ecl_type_get_sizeof_ctype(target_data_type));
    if (ecl_type_is_double(data_type)) {
        double fill;
        if (rms_index_order)
            fill = RMS_INACTIVE_DOUBLE;
        else
            fill = 0;
        field_export3D(field, data, rms_index_order, target_data_type, &fill,
                       init_file);
    } else if (ecl_type_is_float(data_type)) {
        float fill;
        if (rms_index_order)
            fill = RMS_INACTIVE_FLOAT;
        else
            fill = 0;
        field_export3D(field, data, rms_index_order, target_data_type, &fill,
                       init_file);
    } else if (ecl_type_is_int(data_type)) {
        int fill;
        if (rms_index_order)
            fill = RMS_INACTIVE_INT;
        else
            fill = 0;
        field_export3D(field, data, rms_index_order, target_data_type, &fill,
                       init_file);
    } else
        util_abort(
            "%s: trying to export type != int/float/double - aborting \n",
            __func__);
    return data;
}

/*
   A general comment about writing fields to disk:

   The writing of fields to disk can be done in **MANY** different ways:

   o The native function field_fwrite() will save the field in the
     format most suitable for use with enkf. This function will only
     save the active cells, and compress the field if the variable
     write_compressed is true. Most of the configuration information
     is with the field_config object, and not saved with the field.

   o Export as ECLIPSE input. This again has three subdivisions:

     * The function field_ecl_grdecl_export() will write the field to
       disk in a format suitable for ECLIPSE INCLUDE statements. This
       means that both active and inactive cells are written, with a
       zero fill for the inactive. If the argument init_file is set,
       the value for the inactive cells are read from this file.

     * The functions field_xxxx_fortio() writes the field in the
       ECLIPSE restart format. The function field_ecl_write3D_fortio()
       writes all the cells - with zero filling for inactive
       cells. This is suitable for IMPORT of e.g. PORO.

       The function field_ecl_write1D_fortio() will write only the
       active cells in an ECLIPSE restart file. This is suitable for
       e.g. the pressure.

       Observe that the function field_ecl_write() should get config
       information and automatically select the right way to export to
       eclipse format.

   o Export in RMS ROFF format.
*/

/**
    This function exports *one* field instance to the rms_file
    instance. It is the responsibility of the field_ROFF_export()
    function to initialize and close down the rms_file instance.
*/
static void field_ROFF_export__(const field_type *field,
                                rms_file_type *rms_file,
                                const char *init_file) {
    const int data_size = field_config_get_volume(field->config);
    const ecl_data_type target_type = field_config_get_ecl_data_type(
        field->config); /* Could/should in principle be input */
    const ecl_data_type data_type =
        field_config_get_ecl_data_type(field->config);

    void *data = __field_alloc_3D_data(field, data_size, true, data_type,
                                       target_type, init_file);
    rms_tagkey_type *data_key = rms_tagkey_alloc_complete(
        "data", data_size, rms_util_convert_ecl_type(target_type), data, true);
    rms_tag_fwrite_parameter(field_config_get_ecl_kw_name(field->config),
                             data_key, rms_file_get_FILE(rms_file));
    rms_tagkey_free(data_key);
    free(data);
}

static rms_file_type *field_init_ROFF_export(const field_type *field,
                                             const char *filename) {
    rms_file_type *rms_file = rms_file_alloc(filename, false);
    rms_file_fopen_w(rms_file);
    rms_file_init_fwrite(rms_file, "parameter"); /* Version / byteswap ++ */
    {
        int nx, ny, nz;
        field_config_get_dims(field->config, &nx, &ny, &nz);
        rms_tag_fwrite_dimensions(
            nx, ny, nz, rms_file_get_FILE(rms_file)); /* Dimension header */
    }
    return rms_file;
}

static void field_complete_ROFF_export(const field_type *field,
                                       rms_file_type *rms_file) {
    rms_file_complete_fwrite(rms_file);
    rms_file_fclose(rms_file);
    rms_file_free(rms_file);
}

/**
    This function exports the data of a field as a parameter to an RMS
    roff file. The export process is divided in three parts:

    1. The rms_file is opened, and initialized with some basic data
       for dimensions++
    2. The field is written to file.
    3. The file is completed / closed.

    The reason for doing it like this is that it should be easy to
    export several fields (of the same dimension+++) with repeated
    calls to 2 (i.e. field_ROFF_export__()) - that is currently not
    implemented.
*/
void field_ROFF_export(const field_type *field, const char *export_filename,
                       const char *init_file) {
    rms_file_type *rms_file = field_init_ROFF_export(field, export_filename);
    field_ROFF_export__(
        field, rms_file,
        init_file); /* Should now be possible to several calls to field_ROFF_export__() */
    field_complete_ROFF_export(field, rms_file);
}

bool field_write_to_buffer(const field_type *field, buffer_type *buffer,
                           int report_step) {
    int byte_size = field_config_get_byte_size(field->config);
    buffer_fwrite_int(buffer, FIELD);
    buffer_fwrite_compressed(buffer, field->data, byte_size);
    return true;
}

void field_ecl_write1D_fortio(const field_type *field, fortio_type *fortio) {
    const int data_size = field_config_get_data_size(field->config);
    const ecl_data_type data_type =
        field_config_get_ecl_data_type(field->config);

    ecl_kw_fwrite_param_fortio(fortio,
                               field_config_get_ecl_kw_name(field->config),
                               data_type, data_size, field->data);
}

void field_ecl_write3D_fortio(const field_type *field, fortio_type *fortio,
                              const char *init_file) {
    const int data_size = field_config_get_volume(field->config);
    const ecl_data_type target_type = field_config_get_ecl_data_type(
        field->config); /* Could/should in principle be input */
    const ecl_data_type data_type =
        field_config_get_ecl_data_type(field->config);
    void *data = __field_alloc_3D_data(field, data_size, false, data_type,
                                       target_type, init_file);

    ecl_kw_fwrite_param_fortio(fortio,
                               field_config_get_ecl_kw_name(field->config),
                               data_type, data_size, data);
    free(data);
}

static ecl_kw_type *field_alloc_ecl_kw_wrapper__(const field_type *field,
                                                 void *data) {
    const int data_size = field_config_get_volume(field->config);
    const ecl_data_type target_type = field_config_get_ecl_data_type(
        field->config); /* Could/should in principle be input */

    ecl_kw_type *ecl_kw =
        ecl_kw_alloc_new_shared(field_config_get_ecl_kw_name(field->config),
                                data_size, target_type, data);

    return ecl_kw;
}

void field_ecl_grdecl_export(const field_type *field, FILE *stream,
                             const char *init_file) {
    const int data_size = field_config_get_volume(field->config);
    const ecl_data_type target_type = field_config_get_ecl_data_type(
        field->config); /* Could/should in principle be input */
    const ecl_data_type data_type =
        field_config_get_ecl_data_type(field->config);
    void *data = __field_alloc_3D_data(field, data_size, false, data_type,
                                       target_type, init_file);
    ecl_kw_type *ecl_kw = field_alloc_ecl_kw_wrapper__(field, data);
    ecl_kw_fprintf_grdecl(ecl_kw, stream);
    ecl_kw_free(ecl_kw);
    free(data);
}

static void field_apply(field_type *field, field_func_type *func) {
    field_config_assert_unary(field->config, __func__);
    {
        const int data_size = field_config_get_data_size(field->config);
        const ecl_data_type data_type =
            field_config_get_ecl_data_type(field->config);

        if (ecl_type_is_float(data_type)) {
            float *data = (float *)field->data;
            for (int i = 0; i < data_size; i++)
                data[i] = func(data[i]);
        } else if (ecl_type_is_double(data_type)) {
            double *data = (double *)field->data;
            for (int i = 0; i < data_size; i++)
                data[i] = func(data[i]);
        }
    }
}

static bool field_check_finite(const field_type *field) {
    const int data_size = field_config_get_data_size(field->config);
    const ecl_data_type data_type =
        field_config_get_ecl_data_type(field->config);
    bool ok = true;

    if (ecl_type_is_float(data_type)) {
        float *data = (float *)field->data;
        for (int i = 0; i < data_size; i++)
            if (!std::isfinite(data[i]))
                ok = false;
    } else if (ecl_type_is_double(data_type)) {
        double *data = (double *)field->data;
        for (int i = 0; i < data_size; i++)
            if (!std::isfinite(data[i]))
                ok = false;
    }
    return ok;
}

void field_inplace_output_transform(field_type *field) {
    field_func_type *output_transform =
        field_config_get_output_transform(field->config);
    if (output_transform != NULL)
        field_apply(field, output_transform);
}

#define TRUNCATE_MACRO(s, d, t, min, max)                                      \
    for (int i = 0; i < s; i++) {                                              \
        if (t & TRUNCATE_MIN)                                                  \
            if (d[i] < min)                                                    \
                d[i] = min;                                                    \
        if (t & TRUNCATE_MAX)                                                  \
            if (d[i] > max)                                                    \
                d[i] = max;                                                    \
    }

static void field_apply_truncation(field_type *field) {
    int truncation = field_config_get_truncation_mode(field->config);
    if (truncation != TRUNCATE_NONE) {
        double min_value = field_config_get_truncation_min(field->config);
        double max_value = field_config_get_truncation_max(field->config);

        const int data_size = field_config_get_data_size(field->config);
        const ecl_data_type data_type =
            field_config_get_ecl_data_type(field->config);
        if (ecl_type_is_float(data_type)) {
            float *data = (float *)field->data;
            TRUNCATE_MACRO(data_size, data, truncation, min_value, max_value);
        } else if (ecl_type_is_double(data_type)) {
            double *data = (double *)field->data;
            TRUNCATE_MACRO(data_size, data, truncation, min_value, max_value);
        } else
            util_abort("%s: Field type not supported for truncation \n",
                       __func__);
    }
}

/**
    Does both the explicit output transform *AND* the truncation.
*/
static void field_output_transform(field_type *field) {
    field_func_type *output_transform =
        field_config_get_output_transform(field->config);
    int truncation = field_config_get_truncation_mode(field->config);
    if ((output_transform != NULL) || (truncation != TRUNCATE_NONE)) {
        field->export_data = (char *)util_alloc_copy(
            field->data, field_config_get_byte_size(field->config));
        field->__data =
            field->data; /* Storing a pointer to the original data. */
        field->data = field->export_data;

        if (output_transform != NULL)
            field_inplace_output_transform(field);

        field_apply_truncation(field);
    }
}

static void field_revert_output_transform(field_type *field) {
    if (field->export_data != NULL) {
        free(field->export_data);
        field->export_data = NULL;
        field->data = field->__data; /* Recover the original pointer. */
    }
}

/**
  This is the generic "export field to eclipse" function. It will
  check up the config object to determine how to export the field,
  and then call the appropriate function. The alternatives are:

  * Restart format - only active cells (field_ecl_write1D_fortio).
  * Restart format - all cells         (field_ecl_write3D_fortio).
  * GRDECL  format                     (field_ecl_grdecl_export)

  Observe that the output transform is hooked in here, that means
  that if you call e.g. the ROFF export function directly, the output
  transform will *NOT* be applied.
*/
void field_export(const field_type *__field, const char *file,
                  fortio_type *restart_fortio, field_file_format_type file_type,
                  bool output_transform, const char *init_file) {
    field_type *field =
        (field_type *)__field; /* Net effect is no change ... but */

    if (output_transform)
        field_output_transform(field);

    /*  Writes the field to in ecl_kw format to a new file.  */
    if ((file_type == ECL_KW_FILE_ALL_CELLS) ||
        (file_type == ECL_KW_FILE_ACTIVE_CELLS)) {
        fortio_type *fortio;
        bool fmt_file =
            false; /* For formats which support both formatted and unformatted output this is hardwired to unformatted. */

        fortio = fortio_open_writer(file, fmt_file, ECL_ENDIAN_FLIP);

        if (file_type == ECL_KW_FILE_ALL_CELLS)
            field_ecl_write3D_fortio(field, fortio, init_file);
        else
            field_ecl_write1D_fortio(field, fortio);

        fortio_fclose(fortio);
    } else if (file_type == ECL_GRDECL_FILE) {
        /* Writes the field to a new grdecl file. */
        auto stream = mkdir_fopen(fs::path(file), "w");
        field_ecl_grdecl_export(field, stream, init_file);
        fclose(stream);
    } else if (file_type == RMS_ROFF_FILE)
        /* Roff export */
        field_ROFF_export(field, file, init_file);
    else if (file_type == ECL_FILE)
        /* This entry point is used by the ecl_write() function to write to an ALREADY OPENED eclipse restart file. */
        field_ecl_write1D_fortio(field, restart_fortio);
    else
        util_abort("%s: internal error file_type = %d - aborting \n", __func__,
                   file_type);

    if (output_transform)
        field_revert_output_transform(field);
}

/**
   Observe that the output transform is hooked in here, that means
   that if you call e.g. the ROFF export function directly, the output
   transform will *NOT* be applied.

   Observe that the output transform is done one a copy of the data -
   not in place. When the export is complete the field->data will be
   unchanged.
*/
void field_ecl_write(const field_type *field, const char *run_path,
                     const char *file, void *filestream) {
    field_file_format_type export_format =
        field_config_get_export_format(field->config);

    if (export_format == ECL_FILE) {
        fortio_type *restart_fortio = fortio_safe_cast(filestream);
        field_export(field, NULL, restart_fortio, export_format, true, NULL);
        return;
    }

    char *full_path = util_alloc_filename(run_path, file, NULL);
    if (util_is_link(full_path))
        util_unlink_existing(full_path);

    field_export(field, full_path, NULL, export_format, true, NULL);
    free(full_path);
}

bool field_initialize(field_type *field, int iens, const char *init_file,
                      rng_type *rng) {
    bool ret = false;
    if (init_file) {
        if (field_fload(field, init_file)) {
            field_func_type *init_transform =
                field_config_get_init_transform(field->config);
            /*
         Doing the input transform - observe that this is done inplace on
         the data, not as the output transform which is done on a copy of
         prior to export.
      */
            if (init_transform) {
                field_apply(field, init_transform);
                if (!field_check_finite(field))
                    util_exit(
                        "Sorry: after applying the init transform field:%s "
                        "contains nan/inf or similar malformed values.\n",
                        field_config_get_key(field->config));
            }
            ret = true;
        }
    }

    return ret;
}

void field_free(field_type *field) {
    if (!field->shared_data) {
        free(field->data);
        field->data = NULL;
    }
    free(field);
}

void field_serialize(const field_type *field, node_id_type node_id,
                     const ActiveList *active_list, Eigen::MatrixXd &A,
                     int row_offset, int column) {
    const field_config_type *config = field->config;
    const int data_size = field_config_get_data_size(config);
    ecl_data_type data_type = field_config_get_ecl_data_type(config);

    enkf_matrix_serialize(field->data, data_size, data_type, active_list, A,
                          row_offset, column);
}

void field_deserialize(field_type *field, node_id_type node_id,
                       const ActiveList *active_list, const Eigen::MatrixXd &A,
                       int row_offset, int column) {
    const field_config_type *config = field->config;
    const int data_size = field_config_get_data_size(config);
    ecl_data_type data_type = field_config_get_ecl_data_type(config);

    enkf_matrix_deserialize(field->data, data_size, data_type, active_list, A,
                            row_offset, column);
}

static int __get_index(const field_type *field, int i, int j, int k) {
    return field_config_keep_inactive_cells(field->config)
               ? field_config_global_index(field->config, i, j, k)
               : field_config_active_index(field->config, i, j, k);
}

void field_ijk_get(const field_type *field, int i, int j, int k, void *value) {
    int index = __get_index(field, i, j, k);
    int sizeof_ctype = field_config_get_sizeof_ctype(field->config);
    memcpy(value, &field->data[index * sizeof_ctype], sizeof_ctype);
}

double field_ijk_get_double(const field_type *field, int i, int j, int k) {
    int index = __get_index(field, i, j, k);
    return field_iget_double(field, index);
}

/**
   Takes an active or global index as input, and returns a double.
*/
double field_iget_double(const field_type *field, int index) {
    ecl_data_type data_type = field_config_get_ecl_data_type(field->config);
    int sizeof_ctype = field_config_get_sizeof_ctype(field->config);
    char __buffer[8];
    char *buffer = &__buffer[0];
    memcpy(buffer, &field->data[index * sizeof_ctype], sizeof_ctype);
    if (ecl_type_is_double(data_type))
        return *((double *)buffer);
    else if (ecl_type_is_float(data_type)) {
        double double_value;
        float float_value;

        float_value = *((float *)buffer);
        double_value = float_value;

        return double_value;
    } else {
        util_abort("%s: failed - wrong internal type \n", __func__);
        return -1;
    }
}

/**
   Takes an active or global index as input, and returns a double.
*/
float field_iget_float(const field_type *field, int index) {
    ecl_data_type data_type = field_config_get_ecl_data_type(field->config);
    int sizeof_ctype = field_config_get_sizeof_ctype(field->config);
    char __buffer[8];
    char *buffer = &__buffer[0];
    memcpy(buffer, &field->data[index * sizeof_ctype], sizeof_ctype);
    if (ecl_type_is_float(data_type))
        return *((float *)buffer);
    else if (ecl_type_is_double(data_type)) {
        double double_value;
        float float_value;

        double_value = *((double *)buffer);
        float_value = double_value;

        return float_value;
    } else {
        util_abort("%s: failed - wrong internal type \n", __func__);
        return -1;
    }
}

#define INDEXED_UPDATE_MACRO(t, s, n, index, add)                              \
    {                                                                          \
        int i;                                                                 \
        if (add)                                                               \
            for (i = 0; i < (n); i++)                                          \
                (t)[index[i]] += (s)[i];                                       \
        else                                                                   \
            for (i = 0; i < (n); i++)                                          \
                (t)[index[i]] = (s)[i];                                        \
    }

/**
   Copying data from a (PACKED) ecl_kw instance down to a fields data.
*/
void field_copy_ecl_kw_data(field_type *field, const ecl_kw_type *ecl_kw) {
    const field_config_type *config = field->config;
    const int data_size = field_config_get_data_size(config);
    ecl_data_type field_type = field_config_get_ecl_data_type(field->config);
    ecl_data_type kw_type = ecl_kw_get_data_type(ecl_kw);

    if (data_size != ecl_kw_get_size(ecl_kw)) {
        fprintf(stderr, "\n");
        fprintf(stderr,
                " ** Fatal error - the number of active cells has changed \n");
        fprintf(stderr, " **   Grid:%s has %d active cells. \n",
                field_config_get_grid_name(config), data_size);
        fprintf(stderr, " **   %s loaded from file has %d active cells.\n",
                field_config_get_key(config), ecl_kw_get_size(ecl_kw));
        fprintf(stderr, " ** MINPV / MINPVV problem?? \n");
        util_abort("%s: Aborting \n", __func__);
    }

    ecl_util_memcpy_typed_data(field->data, ecl_kw_get_void_ptr(ecl_kw),
                               field_type, kw_type, ecl_kw_get_size(ecl_kw));
}

bool field_fload_rms(field_type *field, const char *filename,
                     bool keep_inactive) {
    {
        FILE *stream = util_fopen__(filename, "r");
        if (!stream)
            return false;

        fclose(stream);
    }

    {
        const char *key = field_config_get_ecl_kw_name(field->config);
        rms_file_type *rms_file = rms_file_alloc(filename, false);
        rms_tagkey_type *data_tag;
        if (field_config_enkf_mode(field->config))
            data_tag = rms_file_fread_alloc_data_tagkey(rms_file, "parameter",
                                                        "name", key);
        else {
            /*
          Setting the key - purely to support converting between
          different types of files, without knowing the key. A usable
          feature - but not really well defined.
      */

            rms_tag_type *rms_tag =
                rms_file_fread_alloc_tag(rms_file, "parameter", NULL, NULL);
            const char *parameter_name = rms_tag_get_namekey_name(rms_tag);
            field_config_set_key((field_config_type *)field->config,
                                 parameter_name);
            data_tag = rms_tagkey_copyc(rms_tag_get_key(rms_tag, "data"));
            rms_tag_free(rms_tag);
        }

        ecl_data_type data_type = rms_tagkey_get_ecl_data_type(data_tag);
        if (rms_tagkey_get_size(data_tag) !=
            field_config_get_volume(field->config))
            util_abort("%s: trying to import rms_data_tag from:%s with wrong "
                       "size - aborting \n",
                       __func__, filename);

        field_import3D(field, rms_tagkey_get_data_ref(data_tag), true,
                       keep_inactive, data_type);
        rms_tagkey_free(data_tag);
        rms_file_free(rms_file);
    }
    return true;
}

static bool field_fload_ecl_kw(field_type *field, const char *filename,
                               bool keep_inactive) {
    const char *key = field_config_get_ecl_kw_name(field->config);
    ecl_kw_type *ecl_kw = NULL;
    bool fmt_file = false;

    if (!ecl_util_fmt_file(filename, &fmt_file))
        util_abort("%s: could not determine formatted/unformatted status of "
                   "file:%s \n",
                   __func__, filename);

    fortio_type *fortio =
        fortio_open_reader(filename, fmt_file, ECL_ENDIAN_FLIP);
    if (!fortio)
        return false;

    ecl_kw_fseek_kw(key, true, true, fortio);
    ecl_kw = ecl_kw_fread_alloc(fortio);
    fortio_fclose(fortio);

    if (field_config_get_volume(field->config) == ecl_kw_get_size(ecl_kw))
        field_import3D(field, ecl_kw_get_void_ptr(ecl_kw), false, keep_inactive,
                       ecl_kw_get_data_type(ecl_kw));
    else
        /* Keyword is already packed - e.g. from a restart file. Size is
       verified in the _copy function.*/
        field_copy_ecl_kw_data(field, ecl_kw);

    ecl_kw_free(ecl_kw);
    return true;
}

/* No type translation possible */
static bool field_fload_ecl_grdecl(field_type *field, const char *filename,
                                   bool keep_inactive) {
    const char *key = field_config_get_ecl_kw_name(field->config);
    int size = field_config_get_volume(field->config);
    ecl_data_type data_type = field_config_get_ecl_data_type(field->config);
    ecl_kw_type *ecl_kw = NULL;
    {
        FILE *stream = util_fopen__(filename, "r");
        if (stream) {
            if (ecl_kw_grdecl_fseek_kw(key, false, stream))
                ecl_kw =
                    ecl_kw_fscanf_alloc_grdecl_data(stream, size, data_type);
            else
                util_exit("%s: Can not locate %s keyword in %s \n", __func__,
                          key, filename);
            fclose(stream);

            field_import3D(field, ecl_kw_get_void_ptr(ecl_kw), false,
                           keep_inactive, ecl_kw_get_data_type(ecl_kw));
            ecl_kw_free(ecl_kw);
            return true;
        }
    }
    return false;
}

bool field_fload_typed(field_type *field, const char *filename,
                       field_file_format_type file_type, bool keep_inactive) {
    bool loadOK = false;
    switch (file_type) {
    case (RMS_ROFF_FILE):
        loadOK = field_fload_rms(field, filename, keep_inactive);
        break;
    case (ECL_KW_FILE):
        loadOK = field_fload_ecl_kw(field, filename, keep_inactive);
        break;
    case (ECL_GRDECL_FILE):
        loadOK = field_fload_ecl_grdecl(field, filename, keep_inactive);
        break;
    default:
        util_abort("%s: file_type:%d not recognized - aborting \n", __func__,
                   file_type);
    }
    return loadOK;
}

static bool field_fload_custom__(field_type *field, const char *filename,
                                 bool keep_inactive) {
    if (util_file_readable(filename)) {
        field_file_format_type file_type =
            field_config_guess_file_type(filename);
        if (file_type == UNDEFINED_FORMAT)
            util_abort("%s - could not automagically infer type for file: %s\n",
                       __func__, filename);

        return field_fload_typed(field, filename, file_type, keep_inactive);
    } else
        return false;
}

bool field_fload(field_type *field, const char *filename) {
    bool keep_inactive = false;
    return field_fload_custom__(field, filename, keep_inactive);
}

bool field_fload_keep_inactive(field_type *field, const char *filename) {
    bool keep_inactive = true;
    return field_fload_custom__(field, filename, keep_inactive);
}

/**
  Here, index_key is i a tree digit string with the i, j and k indicies of
  the requested block separated by comma. E.g., 1,1,1.

  The string is supposed to contain indices in the range [1...nx] ,
  [1..ny] , [1...nz], they are immediately converted to C-based zero
  offset indices.
*/
C_USED bool field_user_get(const field_type *field, const char *index_key,
                           int report_step, double *value) {
    const bool internal_value = false;
    bool valid = false;
    int i = 0, j = 0, k = 0;
    int parse_user_key =
        field_config_parse_user_key(field->config, index_key, &i, &j, &k);

    if (parse_user_key == 0) {
        int active_index = field_config_active_index(field->config, i, j, k);
        *value = field_iget_double(field, active_index);
        valid = true;
    } else {
        if (parse_user_key == 1)
            fprintf(stderr, "Failed to parse \"%s\" as three integers \n",
                    index_key);
        else if (parse_user_key == 2)
            fprintf(stderr, " ijk: %d , %d, %d is invalid \n", i + 1, j + 1,
                    k + 1);
        else if (parse_user_key == 3)
            fprintf(stderr, " ijk: %d , %d, %d is an inactive cell. \n", i + 1,
                    j + 1, k + 1);
        else
            util_abort("%s: internal error -invalid value:%d \n", __func__,
                       parse_user_key);
        *value = 0.0;
        valid = false;
    }

    if (!internal_value && valid) {
        field_func_type *output_transform =
            field_config_get_output_transform(field->config);
        if (output_transform != NULL)
            *value = output_transform(*value);
        /* Truncation - ignored for now */
    }
    return valid;
}

/*
  These two functions assume float/double storage; will not work with
  field which is internally based on char *.

  MATH_OPS(field)
*/
UTIL_SAFE_CAST_FUNCTION(field, FIELD)
UTIL_SAFE_CAST_FUNCTION_CONST(field, FIELD)
UTIL_IS_INSTANCE_FUNCTION(field, FIELD)
VOID_ALLOC(field)
VOID_FREE(field)
VOID_ECL_WRITE(field)
VOID_COPY(field)
VOID_INITIALIZE(field);
VOID_USER_GET(field)
VOID_READ_FROM_BUFFER(field)
VOID_WRITE_TO_BUFFER(field)
VOID_CLEAR(field)
VOID_SERIALIZE(field)
VOID_DESERIALIZE(field)
VOID_FLOAD(field)
