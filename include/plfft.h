/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

/**
 * @file
 * @brief PerfLibs Fast Fourier Transform interface.
 */

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup plfft PLFFT API
 * @brief Plan and execute one-dimensional Fourier and real-to-real transforms.
 *
 * A transform is used in the following stages:
 *
 * 1. Create a configuration for one transform kind.
 * 2. Set its data type, batching, and alias options.
 * 3. Create a plan from the configuration.
 * 4. Destroy the configuration.
 * 5. Execute the plan one or more times.
 * 6. Destroy the plan.
 *
 * See the @ref md_docs_plfft "documentation main page" for information on
 * buffer layouts, transform conventions, and batching capabilities.
 *
 * @{
 */

/**
 * @brief Opaque, mutable transform configuration.
 *
 * Create with a `plfft_config_create*` function, configure with
 * `plfft_config_set*` functions, query with `plfft_config_get*` functions, and
 * release with plfft_config_destroy(). A configuration is independent of every
 * plan already created from it.
 */
typedef struct plfft_config_t plfft_config_t;

/**
 * @brief Opaque executable transform plan.
 *
 * Create with plfft_plan_create_from_config(), execute with
 * plfft_plan_execute(), and release with plfft_plan_destroy().
 */
typedef struct plfft_plan_t plfft_plan_t;

/** @brief Status values returned by fallible PLFFT operations. */
typedef enum plfft_status_t {
  /** The operation completed successfully. */
  PLFFT_OK = 0,
  /** A required pointer argument was null. */
  PLFFT_NULL_ARGUMENT = 1,
  /** The configured transform length was zero or negative. */
  PLFFT_INVALID_SIZE = 2,
  /** The configured batch count, stride, or distance was invalid. */
  PLFFT_UNSUPPORTED_BATCH = 3,
  /** A direction or real-to-real transform kind was invalid. */
  PLFFT_INVALID_TRANSFORM_KIND = 4,
  /** The configured data type was invalid or unavailable in this build. */
  PLFFT_INVALID_DATA_TYPE = 5,
  /** The configured input/output alias mode was invalid. */
  PLFFT_INVALID_IO_ALIAS = 6,
  /** No executable plan could be produced for the validated configuration. */
  PLFFT_PLAN_CREATION_FAILED = 7,
  /** The configured SME mode was invalid. */
  PLFFT_INVALID_SME_MODE = 8,
  /** SME was requested, but SME support was not enabled in this build. */
  PLFFT_SME_NOT_ENABLED = 9,
  /** SME was requested for a real-to-real transform. */
  PLFFT_SME_UNSUPPORTED_R2R = 10,
  /** FP16 was requested, but runtime support is not available. */
  PLFFT_FP16_NOT_AVAILABLE = 11,
  /** SME was requested, but runtime support is not available. */
  PLFFT_SME_NOT_AVAILABLE = 12,
  /** SME was requested for a fixed-point transform. */
  PLFFT_SME_UNSUPPORTED_FIXED_POINT = 13,
  /** The configured planner rigor was invalid. */
  PLFFT_INVALID_PLANNER_RIGOR = 14
} plfft_status_t;

/** @brief Direction of a complex-to-complex Fourier transform. */
typedef enum plfft_direction_t {
  /** Forward transform, using a negative sign in the complex exponential. */
  PLFFT_FORWARD = -1,
  /** Backward transform, using a positive sign in the complex exponential. */
  PLFFT_BACKWARD = 1
} plfft_direction_t;

/**
 * @brief Kind of unnormalized real-to-real transform.
 *
 * The DCT and DST definitions and scaling match FFTW's real-to-real transform
 * kinds. PLFFT_R2R_R2HC and PLFFT_R2R_HC2R use FFTW half-complex ordering.
 */
typedef enum plfft_r2r_kind_t {
  /** Type-I discrete cosine transform (FFTW `REDFT00`); requires `n > 1`. */
  PLFFT_R2R_DCT_1 = 0,
  /** Type-II discrete cosine transform (FFTW `REDFT10`). */
  PLFFT_R2R_DCT_2 = 1,
  /** Type-III discrete cosine transform (FFTW `REDFT01`). */
  PLFFT_R2R_DCT_3 = 2,
  /** Type-IV discrete cosine transform (FFTW `REDFT11`). */
  PLFFT_R2R_DCT_4 = 3,
  /** Type-I discrete sine transform (FFTW `RODFT00`). */
  PLFFT_R2R_DST_1 = 4,
  /** Type-II discrete sine transform (FFTW `RODFT10`). */
  PLFFT_R2R_DST_2 = 5,
  /** Type-III discrete sine transform (FFTW `RODFT01`). */
  PLFFT_R2R_DST_3 = 6,
  /** Type-IV discrete sine transform (FFTW `RODFT11`). */
  PLFFT_R2R_DST_4 = 7,
  /** Discrete Hartley transform. */
  PLFFT_R2R_DHT = 8,
  /**
   * Real data to a length-`n` half-complex Fourier representation.
   *
   * For the complex spectrum `X`, the output stores `Re(X[k])` at index `k`
   * for `0 <= k <= floor(n / 2)`, and `Im(X[k])` at index `n - k` for
   * `1 <= k <= floor((n - 1) / 2)`.
   */
  PLFFT_R2R_R2HC = 9,
  /** Inverse transform from the half-complex layout produced by R2HC. */
  PLFFT_R2R_HC2R = 10
} plfft_r2r_kind_t;

/**
 * @brief Scalar storage and arithmetic type used by a plan.
 *
 * The type applies to both real components of a complex element. Q7 and Q15
 * plans use fixed-point scaling and are available only in builds with
 * fixed-point support. Real-to-real fixed-point plans are not supported.
 */
typedef enum plfft_data_type_t {
  /** IEEE single precision. */
  PLFFT_DATA_TYPE_FP32 = 0,
  /** IEEE double precision. */
  PLFFT_DATA_TYPE_FP64 = 1,
  /** IEEE half precision. */
  PLFFT_DATA_TYPE_FP16 = 2,
  /** Signed Q0.7 fixed point stored in `int8_t`. */
  PLFFT_DATA_TYPE_Q7 = 3,
  /** Signed Q0.15 fixed point stored in `int16_t`. */
  PLFFT_DATA_TYPE_Q15 = 4
} plfft_data_type_t;

/** @brief Permitted relationship between input and output storage. */
typedef enum plfft_io_alias_t {
  /** Input and output storage do not overlap. */
  PLFFT_IO_NO_ALIAS = 0,
  /**
   * Plan for execution where input and output storage may overlap.
   *
   * Disjoint input and output storage is also supported. When the buffers
   * overlap, the underlying allocation must satisfy both the input and output
   * size requirements. In a batch, overlap is only supported within a single
   * member. A member's output must not overlap another member's input or
   * output.
   */
  PLFFT_IO_MAY_ALIAS = 1
} plfft_io_alias_t;

/**
 * @brief Whether plan creation requests SME, Scalable Matrix Extension.
 *
 * SME is an architecture extension for ARMv9-A that provides enhanced support
 * for matrix operations.
 */
typedef enum plfft_sme_mode_t {
  /** Do not allow SME implementations. This is the default. */
  PLFFT_SME_DISABLED = 0,
  /**
   * Allow the planner to use SME implementations. A successfully created plan
   * is not guaranteed to use them.
   *
   * Planning fails when SME was not enabled in the build or is unavailable on
   * the current CPU. Real-to-real transforms do not support this mode.
   */
  PLFFT_SME_ENABLED = 1
} plfft_sme_mode_t;

/** @brief Rigor used while searching for an execution plan. */
typedef enum plfft_rigor_t {
  /** Create a plan without timing candidate implementations. */
  PLFFT_ESTIMATE = 0,
  /** Measure a standard set of candidate implementations. */
  PLFFT_MEASURE = 1,
  /** Spend more time measuring candidate implementations. */
  PLFFT_PATIENT = 2,
  /** Perform the most extensive candidate search. */
  PLFFT_EXHAUSTIVE = 3,
} plfft_rigor_t;

/**
 * @brief Return a human-readable description of a status value.
 *
 * The returned, library-owned string has static storage duration and must not
 * be modified or freed. An unrecognized value produces `"unknown status"`.
 *
 * @param[in] status Status value to describe.
 * @return A non-null pointer to a null-terminated string.
 */
const char *plfft_status_to_string(plfft_status_t status);

/**
 * @brief Create a complex-to-complex transform configuration.
 *
 * The new configuration defaults to FP32, non-aliased I/O, SME disabled, and
 * one transform with unit input/output strides and input/output distances
 * equal to `n`. The transform size is validated later, during plan creation.
 *
 * If @p config is non-null, `*config` is set to null before validation and
 * remains null when an error is returned.
 *
 * @param[out] config Receives the newly allocated configuration.
 * @param[in] n Fourier transform length.
 * @param[in] direction Fourier transform direction.
 * @retval PLFFT_OK The configuration was created.
 * @retval PLFFT_NULL_ARGUMENT @p config is null.
 * @retval PLFFT_INVALID_TRANSFORM_KIND @p direction is not a valid enumerator.
 *
 * @note Configuration allocation failure terminates the process rather than
 * returning a status.
 */
plfft_status_t plfft_config_create(plfft_config_t **config, int64_t n,
                                   plfft_direction_t direction);

/**
 * @brief Create a forward real-to-complex transform configuration.
 *
 * A transform consumes `n` real elements and produces `floor(n / 2) + 1`
 * interleaved complex elements. Defaults and deferred size validation are the
 * same as for plfft_config_create().
 *
 * If @p config is non-null, `*config` is set to null before allocation and
 * remains null when an error is returned.
 *
 * @param[out] config Receives the newly allocated configuration.
 * @param[in] n Fourier transform length.
 * @retval PLFFT_OK The configuration was created.
 * @retval PLFFT_NULL_ARGUMENT @p config is null.
 *
 * @note Configuration allocation failure terminates the process rather than
 * returning a status.
 */
plfft_status_t plfft_config_create_r2c(plfft_config_t **config, int64_t n);

/**
 * @brief Create a backward complex-to-real transform configuration.
 *
 * A transform consumes `floor(n / 2) + 1` interleaved complex frequency bins
 * representing a Hermitian spectrum and produces `n` real elements. Defaults
 * and deferred size validation are the same as for plfft_config_create().
 *
 * If @p config is non-null, `*config` is set to null before allocation and
 * remains null when an error is returned.
 *
 * @param[out] config Receives the newly allocated configuration.
 * @param[in] n Fourier transform length.
 * @retval PLFFT_OK The configuration was created.
 * @retval PLFFT_NULL_ARGUMENT @p config is null.
 *
 * @note Configuration allocation failure terminates the process rather than
 * returning a status.
 */
plfft_status_t plfft_config_create_c2r(plfft_config_t **config, int64_t n);

/**
 * @brief Create a real-to-real transform configuration.
 *
 * A transform consumes and produces `n` real elements. Defaults and deferred
 * size validation are the same as for plfft_config_create().
 *
 * If @p config is non-null, `*config` is set to null before validation and
 * remains null when an error is returned.
 *
 * @param[out] config Receives the newly allocated configuration.
 * @param[in] n Fourier transform length.
 * @param[in] r2r_kind Real-to-real transform kind.
 * @retval PLFFT_OK The configuration was created.
 * @retval PLFFT_NULL_ARGUMENT @p config is null.
 * @retval PLFFT_INVALID_TRANSFORM_KIND @p r2r_kind is not a valid enumerator.
 *
 * @note Configuration allocation failure terminates the process rather than
 * returning a status.
 */
plfft_status_t plfft_config_create_r2r(plfft_config_t **config, int64_t n,
                                       plfft_r2r_kind_t r2r_kind);

/**
 * @brief Destroy a configuration object.
 *
 * Passing null has no effect. Otherwise, @p config must identify a live
 * configuration returned by a creation function and becomes invalid on
 * return. Plans previously created from it remain valid.
 *
 * @param[in] config Configuration to destroy, or null.
 */
void plfft_config_destroy(plfft_config_t *config);

/**
 * @brief Set the scalar data type for this configuration.
 *
 * The value is stored without validation. plfft_plan_create_from_config()
 * reports an invalid or unavailable data type.
 *
 * @param[in,out] config Configuration to modify.
 * @param[in] precision Scalar data type to store.
 * @pre @p config identifies a live, non-null configuration.
 */
void plfft_config_set_data_type(plfft_config_t *config,
                                plfft_data_type_t precision);

/**
 * @brief Get the configured scalar data type.
 * @param[in] config Configuration to query.
 * @return The stored data type.
 * @pre @p config identifies a live, non-null configuration.
 */
plfft_data_type_t plfft_config_get_data_type(const plfft_config_t *config);

/**
 * @brief Set the input/output alias mode for this configuration.
 *
 * The value is stored without validation. plfft_plan_create_from_config()
 * reports an invalid mode.
 *
 * @param[in,out] config Configuration to modify.
 * @param[in] alias Alias mode to store.
 * @pre @p config identifies a live, non-null configuration.
 */
void plfft_config_set_io_alias(plfft_config_t *config, plfft_io_alias_t alias);

/**
 * @brief Get the configured input/output alias mode.
 * @param[in] config Configuration to query.
 * @return The stored mode.
 * @pre @p config identifies a live, non-null configuration.
 */
plfft_io_alias_t plfft_config_get_io_alias(const plfft_config_t *config);

/**
 * @brief Set the SME mode for this configuration.
 *
 * The value is stored without validation. plfft_plan_create_from_config()
 * reports an invalid or unsupported mode.
 *
 * @param[in,out] config Configuration to modify.
 * @param[in] sme_mode SME mode to store.
 * @pre @p config identifies a live, non-null configuration.
 */
void plfft_config_set_sme_mode(plfft_config_t *config,
                               plfft_sme_mode_t sme_mode);

/**
 * @brief Get the configured SME mode.
 * @param[in] config Configuration to query.
 * @return The stored mode.
 * @pre @p config identifies a live, non-null configuration.
 */
plfft_sme_mode_t plfft_config_get_sme_mode(const plfft_config_t *config);

/**
 * @brief Set the planning rigor for this configuration.
 *
 * The value is stored without validation. plfft_plan_create_from_config()
 * reports an invalid rigor level.
 *
 * @param[in,out] config Configuration to modify.
 * @param[in] rigor Planning rigor to store.
 * @pre @p config identifies a live, non-null configuration.
 */
void plfft_config_set_rigor(plfft_config_t *config, plfft_rigor_t rigor);

/**
 * @brief Get the configured planning rigor.
 * @param[in] config Configuration to query.
 * @return The stored planning rigor.
 * @pre @p config identifies a live, non-null configuration.
 */
plfft_rigor_t plfft_config_get_rigor(const plfft_config_t *config);

/**
 * @brief Set the batch configuration for this configuration.
 *
 * All values are stored and validated later during plan creation. Plan
 * creation requires positive input and output strides and a positive @p
 * howmany. When @p howmany is greater than one, it also requires positive
 * input and output distances.
 *
 * Strides and distances are in logical elements of their corresponding
 * buffers, never bytes or scalar components of a complex element. Logical
 * element `k` of transform `b` is addressed as
 * `input[b * idist + k * istride]` or `output[b * odist + k * ostride]`.
 * The output of each batch member must be disjoint from every other member's
 * input and output. With PLFFT_IO_MAY_ALIAS, a member's own input and output
 * may overlap.
 *
 * @param[in,out] config Configuration to modify.
 * @param[in] howmany Number of transforms in the batch.
 * @param[in] istride Distance between consecutive logical input elements.
 * @param[in] idist Distance between the starts of consecutive inputs.
 * @param[in] ostride Distance between consecutive logical output elements.
 * @param[in] odist Distance between the starts of consecutive outputs.
 * @pre @p config identifies a live, non-null configuration.
 */
void plfft_config_set_batch(plfft_config_t *config, int64_t howmany,
                            int64_t istride, int64_t idist, int64_t ostride,
                            int64_t odist);

/**
 * @brief Get the configured number of transforms in a batch.
 * @param[in] config Configuration to query.
 * @return The stored batch count.
 * @pre @p config identifies a live, non-null configuration.
 */
int64_t plfft_config_get_howmany(const plfft_config_t *config);

/**
 * @brief Get the configured input-element stride.
 * @param[in] config Configuration to query.
 * @return The stored input stride in logical input elements.
 * @pre @p config identifies a live, non-null configuration.
 */
int64_t plfft_config_get_istride(const plfft_config_t *config);

/**
 * @brief Get the configured distance between consecutive inputs.
 * @param[in] config Configuration to query.
 * @return The stored input distance in logical input elements.
 * @pre @p config identifies a live, non-null configuration.
 */
int64_t plfft_config_get_idist(const plfft_config_t *config);

/**
 * @brief Get the configured output-element stride.
 * @param[in] config Configuration to query.
 * @return The stored output stride in logical output elements.
 * @pre @p config identifies a live, non-null configuration.
 */
int64_t plfft_config_get_ostride(const plfft_config_t *config);

/**
 * @brief Get the configured distance between consecutive outputs.
 * @param[in] config Configuration to query.
 * @return The stored output distance in logical output elements.
 * @pre @p config identifies a live, non-null configuration.
 */
int64_t plfft_config_get_odist(const plfft_config_t *config);

/**
 * @brief Create an executable plan from a configuration.
 *
 * If @p plan is non-null, `*plan` is set to null before the configuration is
 * validated and remains null whenever an error is returned. The plan, once
 * created, is independent of its configuration. The caller may immediately
 * change or destroy @p config without affecting the plan.
 *
 * @param[out] plan Receives the newly allocated plan.
 * @param[in] config Configuration to validate and plan.
 * @retval PLFFT_OK The plan was created.
 * @retval PLFFT_NULL_ARGUMENT @p plan or @p config is null.
 * @retval PLFFT_INVALID_SIZE The configured transform length is not positive.
 * @retval PLFFT_UNSUPPORTED_BATCH The batch count, a stride, or a required
 * distance is not positive.
 * @retval PLFFT_INVALID_TRANSFORM_KIND The stored transform selector is
 * invalid.
 * @retval PLFFT_INVALID_DATA_TYPE The stored data type is invalid or is not
 * enabled in this build.
 * @retval PLFFT_INVALID_IO_ALIAS The stored alias mode is invalid.
 * @retval PLFFT_INVALID_SME_MODE The stored SME mode is invalid.
 * @retval PLFFT_SME_NOT_ENABLED SME was requested in a build without SME.
 * @retval PLFFT_SME_NOT_AVAILABLE SME was requested on a CPU without SME.
 * @retval PLFFT_SME_UNSUPPORTED_R2R SME was requested for a real-to-real
 * transform.
 * @retval PLFFT_SME_UNSUPPORTED_FIXED_POINT SME was requested for a Q7 or Q15
 * transform.
 * @retval PLFFT_FP16_NOT_AVAILABLE The requested FP16 implementation is not
 * available on this CPU and backend.
 * @retval PLFFT_PLAN_CREATION_FAILED The validated transform has no supported
 * implementation (including fixed-point real-to-real transforms).
 */
plfft_status_t plfft_plan_create_from_config(plfft_plan_t **plan,
                                             const plfft_config_t *config);

/**
 * @brief Destroy a plan.
 *
 * Passing null has no effect. Otherwise, @p plan must identify a live plan and
 * becomes invalid on return.
 *
 * @param[in] plan Plan to destroy, or null.
 */
void plfft_plan_destroy(plfft_plan_t *plan);

/**
 * @brief Execute a previously created plan.
 *
 * The pointed-to arrays must use the data type, shape, strides, distances, and
 * alias relationship captured when the plan was created. Both pointers must be
 * non-null and suitably sized for the selected scalar type. With
 * PLFFT_IO_NO_ALIAS, each member's input and output storage must be disjoint.
 * PLFFT_IO_MAY_ALIAS permits a member's input and output storage to overlap. In
 * either mode, a member's output must not overlap another member's input or
 * output.
 *
 * For overlapping R2C or C2R execution, the underlying allocation must cover
 * both the real and complex views. The caller owns all input and output
 * storage, which must remain valid for the duration of the call.
 *
 * A plan may be executed concurrently by multiple threads without
 * synchronizing access to the plan itself. The caller must avoid conflicting
 * access to input and output storage and must not destroy the plan until all
 * executions have completed.
 *
 * @param[in] plan Plan to execute.
 * @param[in] input Start of the input storage.
 * @param[out] output Start of the output storage.
 * @pre @p plan identifies a live, non-null plan.
 * @pre @p input and @p output satisfy the plan's buffer requirements.
 */
void plfft_plan_execute(const plfft_plan_t *plan, const void *input,
                        void *output);

/** @} */

#ifdef __cplusplus
}
#endif
