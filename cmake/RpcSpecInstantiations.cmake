# Generates the explicit-instantiation translation units for the spec-driven handler
# entry points, so consumers do not have to hand-write them per handler.
#
# Why this exists
# ---------------
# `rpc::spec::HandlerFor<Input>` declares parseInput()/spec() but defines them out of line
# in HandlerForDefs.hpp. Exactly one translation unit per Input must combine those
# definitions, that handler's Spec.hpp (so `specFor` is visible to ADL) and an explicit
# instantiation. That keeps the heavy consteval specs out of the shared translation units
# that only dispatch.
#
# Usage (from a target that already has the consumer's xrpl headers on its include path):
#
#     rpcspec_generate_instantiations(OUT_VAR rpcspec_srcs)
#     target_sources(my_rpc_lib PRIVATE ${rpcspec_srcs})
#
# Add them to a STATIC (not OBJECT) library so the linker can drop the instantiations for
# handlers the consumer has not adopted yet.
#
#     rpcspec_generate_instantiations(
#         OUT_VAR   <var>          # out: list of generated .cpp paths
#         [INCLUDE_DIR <dir>]      # override the rpcspec include root (autodetected)
#         [HANDLERS <name>...]     # restrict to these handlers (default: all of them)
#     )

# Captured at include time: CMAKE_CURRENT_LIST_DIR is rebound to the caller inside a function.
set(RPCSPEC_CMAKE_DIR
    "${CMAKE_CURRENT_LIST_DIR}"
    CACHE INTERNAL "Directory holding the rpcspec CMake helpers and templates"
)

# Resolve the directory that contains rpcspec/handlers/*/Spec.hpp.
function(_rpcspec_include_root out_var)
  if(DEFINED xrpl-rpc-spec_INCLUDE_DIRS)
    list(GET xrpl-rpc-spec_INCLUDE_DIRS 0 root)
  elseif(TARGET rpcspec::rpcspec)
    get_target_property(dirs rpcspec::rpcspec INTERFACE_INCLUDE_DIRECTORIES)
    foreach(dir IN LISTS dirs)
      if(dir MATCHES "^\\$<BUILD_INTERFACE:(.+)>$")
        set(root "${CMAKE_MATCH_1}")
        break()
      elseif(NOT dir MATCHES "^\\$<")
        set(root "${dir}")
        break()
      endif()
    endforeach()
  endif()

  if(NOT root OR NOT IS_DIRECTORY "${root}/rpcspec/handlers")
    message(
            FATAL_ERROR
            "rpcspec: cannot locate the rpcspec include root (looked for "
            "'<root>/rpcspec/handlers'). Pass INCLUDE_DIR explicitly."
        )
  endif()

  set(${out_var} "${root}" PARENT_SCOPE)
endfunction()

function(rpcspec_generate_instantiations)
  cmake_parse_arguments(arg "" "OUT_VAR;INCLUDE_DIR" "HANDLERS" ${ARGN})

  if(NOT arg_OUT_VAR)
    message(FATAL_ERROR "rpcspec_generate_instantiations: OUT_VAR is required")
  endif()
  if(arg_UNPARSED_ARGUMENTS)
    message(
            FATAL_ERROR
            "rpcspec_generate_instantiations: unexpected arguments '${arg_UNPARSED_ARGUMENTS}'"
        )
  endif()

  if(arg_INCLUDE_DIR)
    set(root "${arg_INCLUDE_DIR}")
  else()
    _rpcspec_include_root(root)
  endif()

  set(handlers ${arg_HANDLERS})
  if(NOT handlers)
    file(GLOB specs CONFIGURE_DEPENDS "${root}/rpcspec/handlers/*/Spec.hpp")
    if(NOT specs)
      message(FATAL_ERROR "rpcspec: no handler specs found under '${root}/rpcspec/handlers'")
    endif()
    foreach(spec IN LISTS specs)
      get_filename_component(dir "${spec}" DIRECTORY)
      get_filename_component(name "${dir}" NAME)
      list(APPEND handlers "${name}")
    endforeach()
  endif()

  set(outdir "${CMAKE_CURRENT_BINARY_DIR}/rpcspec-instantiations")

  # Generated code is not reviewed, so it is not linted either.
  file(WRITE "${outdir}/.clang-tidy" "# Generated code - not linted.\nChecks: '-*'\n")

  set(generated "")
  foreach(handler IN LISTS handlers)
    if(NOT EXISTS "${root}/rpcspec/handlers/${handler}/Spec.hpp")
      message(FATAL_ERROR "rpcspec: no spec for handler '${handler}'")
    endif()

    set(out "${outdir}/${handler}.cpp")
    set(RPCSPEC_HANDLER "${handler}")
    configure_file("${RPCSPEC_CMAKE_DIR}/Instantiate.cpp.in" "${out}" @ONLY)
    list(APPEND generated "${out}")
  endforeach()

  list(SORT generated)
  set(${arg_OUT_VAR} "${generated}" PARENT_SCOPE)
endfunction()
