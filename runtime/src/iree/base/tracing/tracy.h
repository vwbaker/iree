// Copyright 2019 The IREE Authors
//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include "iree/base/attributes.h"
#include "iree/base/config.h"

#ifndef IREE_BASE_TRACING_TRACY_H_
#define IREE_BASE_TRACING_TRACY_H_

//===----------------------------------------------------------------------===//
// Tracy configuration
//===----------------------------------------------------------------------===//
// NOTE: order matters here as we are including files that require/define.

// Enable Tracy only when we are using tracing features.
#if IREE_TRACING_FEATURES != 0
#define TRACY_ENABLE 1
#endif  // IREE_TRACING_FEATURES

// Disable zone nesting verification in release builds.
// The verification makes it easy to find unbalanced zones but doubles the cost
// (at least) of each zone recorded. Run in debug builds to verify new
// instrumentation is correct before capturing traces in release builds.
#if defined(NDEBUG)
#define TRACY_NO_VERIFY 1
#endif  // NDEBUG

// Force callstack capture on all zones (even those without the C suffix).
#if (IREE_TRACING_FEATURES &                             \
     IREE_TRACING_FEATURE_INSTRUMENTATION_CALLSTACKS) || \
    (IREE_TRACING_FEATURES & IREE_TRACING_FEATURE_ALLOCATION_CALLSTACKS)
#define TRACY_CALLSTACK 1
#endif  // IREE_TRACING_FEATURE_INSTRUMENTATION_CALLSTACKS

// Guard tracy use of DbgHelp on Windows via IREEDbgHelp* functions.
// All our own usage of DbgHelp must be guarded with the same lock.
#define TRACY_DBGHELP_LOCK IREEDbgHelp

// Disable frame image capture to avoid the DXT compression code and the frame
// capture worker thread.
#define TRACY_NO_FRAME_IMAGE 1

// We don't care about vsync events as they can pollute traces and don't have
// much meaning in our workloads. If integrators still want them we can expose
// this as a tracing feature flag.
#define TRACY_NO_VSYNC_CAPTURE 1

// Enable fibers support.
// The manual warns that this adds overheads but it's the only way we can
// support fiber migration across OS threads.
#if IREE_TRACING_FEATURES & IREE_TRACING_FEATURE_FIBERS
#define TRACY_FIBERS 1
#endif  // IREE_TRACING_FEATURE_FIBERS

// Flush the settings we have so far; settings after this point will be
// overriding values set by Tracy itself.
#if defined(TRACY_ENABLE)
#include "tracy/TracyC.h"  // IWYU pragma: export
#endif

// Disable callstack capture if our depth is 0; this allows us to avoid any
// expensive capture (and all the associated dependencies) if we aren't going to
// use it. Note that this means that unless code is instrumented we won't be
// able to tell what's happening in the Tracy UI.
#if IREE_TRACING_MAX_CALLSTACK_DEPTH == 0
#undef TRACY_CALLSTACK
#endif  // IREE_TRACING_MAX_CALLSTACK_DEPTH

//===----------------------------------------------------------------------===//
// C API used for Tracy control
//===----------------------------------------------------------------------===//
// These functions are implementation details and should not be called directly.
// Always use the macros (or C++ RAII types).

// Local zone ID used for the C IREE_TRACE_ZONE_* macros.
typedef uint32_t iree_zone_id_t;

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

#if IREE_TRACING_FEATURES

typedef struct ___tracy_source_location_data iree_tracing_location_t;

#ifdef __cplusplus
#define iree_tracing_make_zone_ctx(zone_id) \
  TracyCZoneCtx { zone_id, 1 }
#else
#define iree_tracing_make_zone_ctx(zone_id) \
  (TracyCZoneCtx) { zone_id, 1 }
#endif  // __cplusplus

int64_t iree_tracing_time(void);
int64_t iree_tracing_frequency(void);

IREE_MUST_USE_RESULT iree_zone_id_t
iree_tracing_zone_begin_impl(const iree_tracing_location_t* src_loc,
                             const char* name, size_t name_length);
IREE_MUST_USE_RESULT iree_zone_id_t iree_tracing_zone_begin_external_impl(
    const char* file_name, size_t file_name_length, uint32_t line,
    const char* function_name, size_t function_name_length, const char* name,
    size_t name_length);
void iree_tracing_zone_end(iree_zone_id_t zone_id);

// Matches GpuContextType.
// TODO(benvanik): upstream a few more enum values for CUDA/Metal/etc.
// The only real behavior that changes in tracy is around whether multi-threaded
// recording is assumed and IREE_TRACING_GPU_CONTEXT_TYPE_VULKAN is a safe
// default choice - the context name provided during creation should be
// descriptive enough for the user.
typedef enum iree_tracing_gpu_context_type_e {
  IREE_TRACING_GPU_CONTEXT_TYPE_INVALID = 0,
  IREE_TRACING_GPU_CONTEXT_TYPE_OPENGL,
  IREE_TRACING_GPU_CONTEXT_TYPE_VULKAN,
  IREE_TRACING_GPU_CONTEXT_TYPE_OPENCL,
  IREE_TRACING_GPU_CONTEXT_TYPE_DIRECT3D12,
  IREE_TRACING_GPU_CONTEXT_TYPE_DIRECT3D11,
} iree_tracing_gpu_context_type_t;

uint8_t iree_tracing_gpu_context_allocate(iree_tracing_gpu_context_type_t type,
                                          const char* name, size_t name_length,
                                          bool is_calibrated,
                                          uint64_t cpu_timestamp,
                                          uint64_t gpu_timestamp,
                                          float timestamp_period);
void iree_tracing_gpu_context_calibrate(uint8_t context_id, int64_t cpu_delta,
                                        int64_t cpu_timestamp,
                                        int64_t gpu_timestamp);
void iree_tracing_gpu_zone_begin(uint8_t context_id, uint16_t query_id,
                                 const iree_tracing_location_t* src_loc);
void iree_tracing_gpu_zone_begin_external(
    uint8_t context_id, uint16_t query_id, const char* file_name,
    size_t file_name_length, uint32_t line, const char* function_name,
    size_t function_name_length, const char* name, size_t name_length);
void iree_tracing_gpu_zone_end(uint8_t context_id, uint16_t query_id);
void iree_tracing_gpu_zone_notify(uint8_t context_id, uint16_t query_id,
                                  int64_t gpu_timestamp);

void iree_tracing_set_plot_type_impl(const char* name_literal,
                                     uint8_t plot_type, bool step, bool fill,
                                     uint32_t color);
void iree_tracing_plot_value_i64_impl(const char* name_literal, int64_t value);
void iree_tracing_plot_value_f32_impl(const char* name_literal, float value);
void iree_tracing_plot_value_f64_impl(const char* name_literal, double value);

void iree_tracing_mutex_announce(const iree_tracing_location_t* src_loc,
                                 uint32_t* out_lock_id);
void iree_tracing_mutex_terminate(uint32_t lock_id);
void iree_tracing_mutex_before_lock(uint32_t lock_id);
void iree_tracing_mutex_after_lock(uint32_t lock_id);
void iree_tracing_mutex_after_try_lock(uint32_t lock_id, bool was_acquired);
void iree_tracing_mutex_after_unlock(uint32_t lock_id);

#endif  // IREE_TRACING_FEATURES

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

//===----------------------------------------------------------------------===//
// Instrumentation macros (C)
//===----------------------------------------------------------------------===//

#if IREE_TRACING_FEATURES & IREE_TRACING_FEATURE_INSTRUMENTATION

// Sets an application-specific payload that will be stored in the trace.
// This can be used to fingerprint traces to particular versions and denote
// compilation options or configuration. The given string value will be copied.
#define IREE_TRACE_SET_APP_INFO(value, value_length) \
  ___tracy_emit_message_appinfo(value, value_length)

// Sets the current thread name to the given string value.
// This will only set the thread name as it appears in the tracing backend and
// not set the OS thread name as it would appear in a debugger.
// The C-string |name| will be copied and does not need to be a literal.
#define IREE_TRACE_SET_THREAD_NAME(name) ___tracy_set_thread_name(name)

// Evalutes the expression code only if tracing is enabled.
//
// Example:
//  struct {
//    IREE_TRACE(uint32_t trace_only_value);
//  } my_object;
//  IREE_TRACE(my_object.trace_only_value = 5);
#define IREE_TRACE(expr) expr

#if IREE_TRACING_FEATURES & IREE_TRACING_FEATURE_FIBERS
// Enters a fiber context.
// |fiber| must be unique and remain live for the process lifetime.
#define IREE_TRACE_FIBER_ENTER(fiber) ___tracy_fiber_enter((const char*)fiber)
// Exits a fiber context.
#define IREE_TRACE_FIBER_LEAVE() ___tracy_fiber_leave()
#else
#define IREE_TRACE_FIBER_ENTER(fiber)
#define IREE_TRACE_FIBER_LEAVE()
#endif  // IREE_TRACING_FEATURE_FIBERS

// Begins a new zone with the parent function name.
#define IREE_TRACE_ZONE_BEGIN(zone_id) \
  IREE_TRACE_ZONE_BEGIN_NAMED(zone_id, NULL)

// Begins a new zone with the given compile-time literal name.
#define IREE_TRACE_ZONE_BEGIN_NAMED(zone_id, name_literal)                    \
  static const iree_tracing_location_t TracyConcat(                           \
      __tracy_source_location, __LINE__) = {name_literal, __FUNCTION__,       \
                                            __FILE__, (uint32_t)__LINE__, 0}; \
  iree_zone_id_t zone_id = iree_tracing_zone_begin_impl(                      \
      &TracyConcat(__tracy_source_location, __LINE__), NULL, 0);

// Begins a new zone with the given runtime dynamic string name.
// The |value| string will be copied into the trace buffer.
#define IREE_TRACE_ZONE_BEGIN_NAMED_DYNAMIC(zone_id, name, name_length) \
  static const iree_tracing_location_t TracyConcat(                     \
      __tracy_source_location, __LINE__) = {0, __FUNCTION__, __FILE__,  \
                                            (uint32_t)__LINE__, 0};     \
  iree_zone_id_t zone_id = iree_tracing_zone_begin_impl(                \
      &TracyConcat(__tracy_source_location, __LINE__), (name), (name_length));

// Begins an externally defined zone with a dynamic source location.
// The |file_name|, |function_name|, and optional |name| strings will be copied
// into the trace buffer and do not need to persist.
#define IREE_TRACE_ZONE_BEGIN_EXTERNAL(                                       \
    zone_id, file_name, file_name_length, line, function_name,                \
    function_name_length, name, name_length)                                  \
  iree_zone_id_t zone_id = iree_tracing_zone_begin_external_impl(             \
      file_name, file_name_length, line, function_name, function_name_length, \
      name, name_length)

// Sets the dynamic color of the zone to an XXBBGGRR value.
#define IREE_TRACE_ZONE_SET_COLOR(zone_id, color_xbgr) \
  ___tracy_emit_zone_color(iree_tracing_make_zone_ctx(zone_id), color_xbgr);

// Appends an integer value to the parent zone. May be called multiple times.
#define IREE_TRACE_ZONE_APPEND_VALUE(zone_id, value) \
  ___tracy_emit_zone_value(iree_tracing_make_zone_ctx(zone_id), value);

// Appends a string value to the parent zone. May be called multiple times.
// The |value| string will be copied into the trace buffer.
#define IREE_TRACE_ZONE_APPEND_TEXT(...)                                  \
  IREE_TRACE_IMPL_GET_VARIADIC_((__VA_ARGS__,                             \
                                 IREE_TRACE_ZONE_APPEND_TEXT_STRING_VIEW, \
                                 IREE_TRACE_ZONE_APPEND_TEXT_CSTRING))    \
  (__VA_ARGS__)
#define IREE_TRACE_ZONE_APPEND_TEXT_CSTRING(zone_id, value) \
  IREE_TRACE_ZONE_APPEND_TEXT_STRING_VIEW(zone_id, value, strlen(value))
#define IREE_TRACE_ZONE_APPEND_TEXT_STRING_VIEW(zone_id, value, value_length) \
  ___tracy_emit_zone_text(iree_tracing_make_zone_ctx(zone_id), value,         \
                          value_length)

// Ends the current zone. Must be passed the |zone_id| from the _BEGIN.
#define IREE_TRACE_ZONE_END(zone_id) iree_tracing_zone_end(zone_id)

// Ends the current zone before returning on a failure.
// Sugar for IREE_TRACE_ZONE_END+IREE_RETURN_IF_ERROR.
#define IREE_RETURN_AND_END_ZONE_IF_ERROR(zone_id, ...) \
  IREE_RETURN_AND_EVAL_IF_ERROR(IREE_TRACE_ZONE_END(zone_id), __VA_ARGS__)

// Configures the named plot with an IREE_TRACING_PLOT_TYPE_* representation.
#define IREE_TRACE_SET_PLOT_TYPE(name_literal, plot_type, step, fill, color) \
  iree_tracing_set_plot_type_impl(name_literal, plot_type, step, fill, color)
// Plots a value in the named plot group as an integer.
#define IREE_TRACE_PLOT_VALUE_I64(name_literal, value) \
  iree_tracing_plot_value_i64_impl(name_literal, value)
// Plots a value in the named plot group as a single-precision float.
#define IREE_TRACE_PLOT_VALUE_F32(name_literal, value) \
  iree_tracing_plot_value_f32_impl(name_literal, value)
// Plots a value in the named plot group as a double-precision float.
#define IREE_TRACE_PLOT_VALUE_F64(name_literal, value) \
  iree_tracing_plot_value_f64_impl(name_literal, value)

// Demarcates an advancement of the top-level unnamed frame group.
#define IREE_TRACE_FRAME_MARK() ___tracy_emit_frame_mark(NULL)
// Demarcates an advancement of a named frame group.
#define IREE_TRACE_FRAME_MARK_NAMED(name_literal) \
  ___tracy_emit_frame_mark(name_literal)
// Begins a discontinuous frame in a named frame group.
// Must be properly matched with a IREE_TRACE_FRAME_MARK_NAMED_END.
#define IREE_TRACE_FRAME_MARK_BEGIN_NAMED(name_literal) \
  ___tracy_emit_frame_mark_start(name_literal)
// Ends a discontinuous frame in a named frame group.
#define IREE_TRACE_FRAME_MARK_END_NAMED(name_literal) \
  ___tracy_emit_frame_mark_end(name_literal)

// Logs a message at the given logging level to the trace.
// The message text must be a compile-time string literal.
#define IREE_TRACE_MESSAGE(level, value_literal) \
  ___tracy_emit_messageLC(value_literal, IREE_TRACING_MESSAGE_LEVEL_##level, 0)
// Logs a message with the given color to the trace.
// Standard colors are defined as IREE_TRACING_MESSAGE_LEVEL_* values.
// The message text must be a compile-time string literal.
#define IREE_TRACE_MESSAGE_COLORED(color, value_literal) \
  ___tracy_emit_messageLC(value_literal, color, 0)
// Logs a dynamically-allocated message at the given logging level to the trace.
// The string |value| will be copied into the trace buffer.
#define IREE_TRACE_MESSAGE_DYNAMIC(level, value, value_length) \
  ___tracy_emit_messageC(value, value_length,                  \
                         IREE_TRACING_MESSAGE_LEVEL_##level, 0)
// Logs a dynamically-allocated message with the given color to the trace.
// Standard colors are defined as IREE_TRACING_MESSAGE_LEVEL_* values.
// The string |value| will be copied into the trace buffer.
#define IREE_TRACE_MESSAGE_DYNAMIC_COLORED(color, value, value_length) \
  ___tracy_emit_messageC(value, value_length, color, 0)

// Utilities:
#define IREE_TRACE_IMPL_GET_VARIADIC_HELPER_(_1, _2, _3, NAME, ...) NAME
#define IREE_TRACE_IMPL_GET_VARIADIC_(args) \
  IREE_TRACE_IMPL_GET_VARIADIC_HELPER_ args

#endif  // IREE_TRACING_FEATURE_INSTRUMENTATION

//===----------------------------------------------------------------------===//
// Allocation tracking macros (C/C++)
//===----------------------------------------------------------------------===//

#if IREE_TRACING_FEATURES & IREE_TRACING_FEATURE_ALLOCATION_TRACKING

#if IREE_TRACING_FEATURES & IREE_TRACING_FEATURE_ALLOCATION_CALLSTACKS

#define IREE_TRACE_ALLOC(ptr, size)               \
  ___tracy_emit_memory_alloc_callstack(ptr, size, \
                                       IREE_TRACING_MAX_CALLSTACK_DEPTH, 0)
#define IREE_TRACE_FREE(ptr) \
  ___tracy_emit_memory_free_callstack(ptr, IREE_TRACING_MAX_CALLSTACK_DEPTH, 0)
#define IREE_TRACE_ALLOC_NAMED(name, ptr, size) \
  ___tracy_emit_memory_alloc_callstack_named(   \
      ptr, size, IREE_TRACING_MAX_CALLSTACK_DEPTH, 0, name)
#define IREE_TRACE_FREE_NAMED(name, ptr)     \
  ___tracy_emit_memory_free_callstack_named( \
      ptr, IREE_TRACING_MAX_CALLSTACK_DEPTH, 0, name)

#else

#define IREE_TRACE_ALLOC(ptr, size) ___tracy_emit_memory_alloc(ptr, size, 0)
#define IREE_TRACE_FREE(ptr) ___tracy_emit_memory_free(ptr, 0)
#define IREE_TRACE_ALLOC_NAMED(name, ptr, size) \
  ___tracy_emit_memory_alloc_named(ptr, size, 0, name)
#define IREE_TRACE_FREE_NAMED(name, ptr) \
  ___tracy_emit_memory_free_named(ptr, 0, name)

#endif  // IREE_TRACING_FEATURE_ALLOCATION_CALLSTACKS

#endif  // IREE_TRACING_FEATURE_ALLOCATION_TRACKING

//===----------------------------------------------------------------------===//
// Instrumentation C++ RAII types, wrappers, and macros
//===----------------------------------------------------------------------===//

#ifdef __cplusplus

#if defined(TRACY_ENABLE)
#include "tracy/Tracy.hpp"  // IWYU pragma: export
#endif

#if IREE_TRACING_FEATURES & IREE_TRACING_FEATURE_INSTRUMENTATION
#define IREE_TRACE_SCOPE() ZoneScoped
#define IREE_TRACE_SCOPE_NAMED(name_literal) ZoneScopedN(name_literal)
#else
#define IREE_TRACE_SCOPE()
#define IREE_TRACE_SCOPE_NAMED(name_literal)
#endif  // IREE_TRACING_FEATURE_INSTRUMENTATION

#endif  // __cplusplus

#endif  // IREE_BASE_TRACING_TRACY_H_
