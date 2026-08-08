cmake_minimum_required(VERSION 3.20)
project(mosaic VERSION 0.28.0 LANGUAGES C CXX)

include(GNUInstallDirs)
include(CTest)
find_package(Threads REQUIRED)
option(MOSAIC_BUILD_TRUST "Build optional Ed25519 trust library" ON)
option(MOSAIC_REQUIRE_ICU_TESTS "Fail configuration when ICU normalization test dependency is unavailable" OFF)
if(MOSAIC_BUILD_TRUST)
  find_package(OpenSSL QUIET COMPONENTS Crypto)
endif()

set(MOSAIC_SRC ${CMAKE_CURRENT_SOURCE_DIR}/src/mosaic.c ${CMAKE_CURRENT_SOURCE_DIR}/src/mosaic_cache.c ${CMAKE_CURRENT_SOURCE_DIR}/src/mosaic_executor.c)
set(MOSAIC_INCLUDE ${CMAKE_CURRENT_SOURCE_DIR}/include)

function(mosaic_warnings target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /WX)
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror)
  endif()
endfunction()

add_library(mosaic_static STATIC ${MOSAIC_SRC})
target_compile_definitions(mosaic_static PRIVATE MOSAIC_LIBRARY_ONLY)
target_include_directories(mosaic_static PUBLIC $<BUILD_INTERFACE:${MOSAIC_INCLUDE}> $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
set_target_properties(mosaic_static PROPERTIES OUTPUT_NAME mosaic)
mosaic_warnings(mosaic_static)
target_link_libraries(mosaic_static PUBLIC Threads::Threads)

add_library(mosaic_shared SHARED ${MOSAIC_SRC})
target_compile_definitions(mosaic_shared PRIVATE MOSAIC_LIBRARY_ONLY)
target_include_directories(mosaic_shared PUBLIC $<BUILD_INTERFACE:${MOSAIC_INCLUDE}> $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
set_target_properties(mosaic_shared PROPERTIES OUTPUT_NAME mosaic WINDOWS_EXPORT_ALL_SYMBOLS ON)
mosaic_warnings(mosaic_shared)
target_link_libraries(mosaic_shared PUBLIC Threads::Threads)

add_executable(mosaic-tokenizer ${MOSAIC_SRC})
target_include_directories(mosaic-tokenizer PRIVATE ${MOSAIC_INCLUDE})
mosaic_warnings(mosaic-tokenizer)
target_link_libraries(mosaic-tokenizer PRIVATE Threads::Threads)

if(MOSAIC_BUILD_TRUST AND OpenSSL_FOUND)
  add_library(mosaic_trust_static STATIC ${CMAKE_CURRENT_SOURCE_DIR}/src/mosaic_trust.c)
  target_include_directories(mosaic_trust_static PUBLIC $<BUILD_INTERFACE:${MOSAIC_INCLUDE}> $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
  set_target_properties(mosaic_trust_static PROPERTIES OUTPUT_NAME mosaic_trust)
  mosaic_warnings(mosaic_trust_static)
  target_link_libraries(mosaic_trust_static PUBLIC OpenSSL::Crypto)

  add_library(mosaic_trust_shared SHARED ${CMAKE_CURRENT_SOURCE_DIR}/src/mosaic_trust.c)
  target_include_directories(mosaic_trust_shared PUBLIC $<BUILD_INTERFACE:${MOSAIC_INCLUDE}> $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
  set_target_properties(mosaic_trust_shared PROPERTIES OUTPUT_NAME mosaic_trust WINDOWS_EXPORT_ALL_SYMBOLS ON)
  mosaic_warnings(mosaic_trust_shared)
  target_link_libraries(mosaic_trust_shared PUBLIC OpenSSL::Crypto)
elseif(MOSAIC_BUILD_TRUST)
  message(STATUS "OpenSSL Crypto not found: optional Mosaic trust library disabled")
endif()

if(BUILD_TESTING)
  set(MOSAIC_TEST_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../conformance/c)
  set(MOSAIC_MODEL_PACK ${CMAKE_CURRENT_SOURCE_DIR}/../fixtures/packs/model-v2.mpack)
  set(MOSAIC_UNICODE_PACK ${CMAKE_CURRENT_SOURCE_DIR}/../fixtures/packs/unicode17-v1.mpack)
  set(MOSAIC_NORMALIZATION_PACK ${CMAKE_CURRENT_SOURCE_DIR}/../fixtures/packs/normalization16-v1.mpack)
  find_package(ICU COMPONENTS uc QUIET)
  if(MOSAIC_REQUIRE_ICU_TESTS AND NOT ICU_FOUND)
    message(FATAL_ERROR "ICU is required by MOSAIC_REQUIRE_ICU_TESTS")
  endif()

  add_executable(mosaic-api-smoke-static ${MOSAIC_TEST_DIR}/api_smoke.c)
  target_include_directories(mosaic-api-smoke-static PRIVATE ${MOSAIC_INCLUDE})
  target_link_libraries(mosaic-api-smoke-static PRIVATE mosaic_static)
  mosaic_warnings(mosaic-api-smoke-static)
  add_test(NAME api-static COMMAND mosaic-api-smoke-static ${MOSAIC_MODEL_PACK} ${MOSAIC_UNICODE_PACK})

  add_executable(mosaic-api-smoke-shared ${MOSAIC_TEST_DIR}/api_smoke.c)
  target_include_directories(mosaic-api-smoke-shared PRIVATE ${MOSAIC_INCLUDE})
  target_link_libraries(mosaic-api-smoke-shared PRIVATE mosaic_shared)
  mosaic_warnings(mosaic-api-smoke-shared)
  add_test(NAME api-shared COMMAND mosaic-api-smoke-shared ${MOSAIC_MODEL_PACK} ${MOSAIC_UNICODE_PACK})

  add_executable(mosaic-api-smoke-cpp ${MOSAIC_TEST_DIR}/api_smoke.cpp)
  target_include_directories(mosaic-api-smoke-cpp PRIVATE ${MOSAIC_INCLUDE})
  target_link_libraries(mosaic-api-smoke-cpp PRIVATE mosaic_shared)
  if(MSVC)
    target_compile_options(mosaic-api-smoke-cpp PRIVATE /W4 /WX)
  else()
    target_compile_options(mosaic-api-smoke-cpp PRIVATE -Wall -Wextra -Wpedantic -Werror)
  endif()
  add_test(NAME api-cpp COMMAND mosaic-api-smoke-cpp)

  add_executable(mosaic-language-smoke ${MOSAIC_TEST_DIR}/language_smoke.c)
  target_include_directories(mosaic-language-smoke PRIVATE ${MOSAIC_INCLUDE})
  target_link_libraries(mosaic-language-smoke PRIVATE mosaic_static)
  mosaic_warnings(mosaic-language-smoke)
  add_test(NAME language COMMAND mosaic-language-smoke ${MOSAIC_MODEL_PACK} ${MOSAIC_UNICODE_PACK}
    ${CMAKE_CURRENT_SOURCE_DIR}/../fixtures/packs/language/en-v1.mpack
    ${CMAKE_CURRENT_SOURCE_DIR}/../fixtures/packs/language/hi-v1.mpack
    ${CMAKE_CURRENT_SOURCE_DIR}/../fixtures/packs/language/ja-v1.mpack)

  add_executable(mosaic-detector-smoke ${MOSAIC_TEST_DIR}/detector_smoke.c)
  target_include_directories(mosaic-detector-smoke PRIVATE ${MOSAIC_INCLUDE})
  target_link_libraries(mosaic-detector-smoke PRIVATE mosaic_static)
  mosaic_warnings(mosaic-detector-smoke)
  add_test(NAME detector COMMAND mosaic-detector-smoke ${MOSAIC_MODEL_PACK} ${MOSAIC_UNICODE_PACK}
    ${CMAKE_CURRENT_SOURCE_DIR}/../fixtures/packs/detector/reference-v1.mpack
    ${CMAKE_CURRENT_SOURCE_DIR}/../fixtures/packs/language/en-v1.mpack
    ${CMAKE_CURRENT_SOURCE_DIR}/../fixtures/packs/language/hi-v1.mpack
    ${CMAKE_CURRENT_SOURCE_DIR}/../fixtures/packs/language/ja-v1.mpack)

  add_executable(mosaic-security-smoke ${MOSAIC_TEST_DIR}/security_smoke.c)
  target_include_directories(mosaic-security-smoke PRIVATE ${MOSAIC_INCLUDE})
  target_link_libraries(mosaic-security-smoke PRIVATE mosaic_static)
  mosaic_warnings(mosaic-security-smoke)
  add_test(NAME security COMMAND mosaic-security-smoke ${MOSAIC_MODEL_PACK} ${MOSAIC_UNICODE_PACK}
    ${CMAKE_CURRENT_SOURCE_DIR}/../fixtures/packs/security17-v1.mpack)

  if(ICU_FOUND)
    add_executable(mosaic-normalization-smoke ${MOSAIC_TEST_DIR}/normalization_smoke.c)
    target_include_directories(mosaic-normalization-smoke PRIVATE ${MOSAIC_INCLUDE} ${ICU_INCLUDE_DIRS})
    target_link_libraries(mosaic-normalization-smoke PRIVATE mosaic_static ICU::uc)
    mosaic_warnings(mosaic-normalization-smoke)
    add_test(NAME normalization COMMAND mosaic-normalization-smoke ${MOSAIC_NORMALIZATION_PACK} ${MOSAIC_MODEL_PACK} ${MOSAIC_UNICODE_PACK})
  else()
    message(STATUS "ICU not found: normalization differential smoke test disabled")
  endif()

  add_executable(mosaic-online-stream-smoke ${MOSAIC_TEST_DIR}/online_stream_smoke.c)
  target_include_directories(mosaic-online-stream-smoke PRIVATE ${MOSAIC_INCLUDE})
  target_link_libraries(mosaic-online-stream-smoke PRIVATE mosaic_static)
  mosaic_warnings(mosaic-online-stream-smoke)
  add_test(NAME online-stream COMMAND mosaic-online-stream-smoke ${MOSAIC_MODEL_PACK} ${MOSAIC_UNICODE_PACK}
    ${CMAKE_CURRENT_SOURCE_DIR}/../fixtures/packs/language/en-v1.mpack
    ${CMAKE_CURRENT_SOURCE_DIR}/../fixtures/packs/language/hi-v1.mpack
    ${CMAKE_CURRENT_SOURCE_DIR}/../fixtures/packs/language/ja-v1.mpack
    ${CMAKE_CURRENT_SOURCE_DIR}/../fixtures/packs/raw-bpe-v1.mpack
    ${CMAKE_CURRENT_SOURCE_DIR}/../fixtures/packs/online-adversarial-v1.mpack)

  add_executable(mosaic-incremental-smoke ${MOSAIC_TEST_DIR}/incremental_smoke.c)
  target_include_directories(mosaic-incremental-smoke PRIVATE ${MOSAIC_INCLUDE})
  target_link_libraries(mosaic-incremental-smoke PRIVATE mosaic_static)
  mosaic_warnings(mosaic-incremental-smoke)
  add_test(NAME incremental COMMAND mosaic-incremental-smoke ${MOSAIC_MODEL_PACK} ${MOSAIC_UNICODE_PACK}
    ${CMAKE_CURRENT_SOURCE_DIR}/../fixtures/packs/language/en-v1.mpack
    ${CMAKE_CURRENT_SOURCE_DIR}/../fixtures/packs/language/hi-v1.mpack
    ${CMAKE_CURRENT_SOURCE_DIR}/../fixtures/packs/language/ja-v1.mpack
    ${CMAKE_CURRENT_SOURCE_DIR}/../fixtures/packs/raw-bpe-v1.mpack)


  add_executable(mosaic-resync-smoke ${MOSAIC_TEST_DIR}/resync_smoke.c)
  target_include_directories(mosaic-resync-smoke PRIVATE ${MOSAIC_INCLUDE})
  target_link_libraries(mosaic-resync-smoke PRIVATE mosaic_static)
  mosaic_warnings(mosaic-resync-smoke)
  add_test(NAME resync COMMAND mosaic-resync-smoke ${MOSAIC_MODEL_PACK} ${MOSAIC_UNICODE_PACK}
    ${CMAKE_CURRENT_SOURCE_DIR}/../fixtures/packs/language/en-v1.mpack
    ${CMAKE_CURRENT_SOURCE_DIR}/../fixtures/packs/language/hi-v1.mpack
    ${CMAKE_CURRENT_SOURCE_DIR}/../fixtures/packs/language/ja-v1.mpack
    ${CMAKE_CURRENT_SOURCE_DIR}/../fixtures/packs/raw-bpe-v1.mpack)

  add_executable(mosaic-token-document-smoke ${MOSAIC_TEST_DIR}/token_document_smoke.c)
  target_include_directories(mosaic-token-document-smoke PRIVATE ${MOSAIC_INCLUDE})
  target_link_libraries(mosaic-token-document-smoke PRIVATE mosaic_static)
  mosaic_warnings(mosaic-token-document-smoke)
  add_test(NAME token-document COMMAND mosaic-token-document-smoke ${MOSAIC_MODEL_PACK} ${MOSAIC_UNICODE_PACK}
    ${CMAKE_CURRENT_SOURCE_DIR}/../fixtures/packs/language/en-v1.mpack
    ${CMAKE_CURRENT_SOURCE_DIR}/../fixtures/packs/language/hi-v1.mpack
    ${CMAKE_CURRENT_SOURCE_DIR}/../fixtures/packs/language/ja-v1.mpack
    ${CMAKE_CURRENT_SOURCE_DIR}/../fixtures/packs/detector/reference-v1.mpack)

  add_executable(mosaic-token-document-rich-smoke ${MOSAIC_TEST_DIR}/token_document_rich_smoke.c)
  target_include_directories(mosaic-token-document-rich-smoke PRIVATE ${MOSAIC_INCLUDE})
  target_link_libraries(mosaic-token-document-rich-smoke PRIVATE mosaic_static)
  mosaic_warnings(mosaic-token-document-rich-smoke)
  add_test(NAME token-document-rich COMMAND mosaic-token-document-rich-smoke ${MOSAIC_MODEL_PACK} ${MOSAIC_UNICODE_PACK}
    ${CMAKE_CURRENT_SOURCE_DIR}/../fixtures/packs/security17-v1.mpack
    ${MOSAIC_NORMALIZATION_PACK})

  add_executable(mosaic-token-document-serialization-smoke ${MOSAIC_TEST_DIR}/token_document_serialization_smoke.c)
  target_include_directories(mosaic-token-document-serialization-smoke PRIVATE ${MOSAIC_INCLUDE})
  target_link_libraries(mosaic-token-document-serialization-smoke PRIVATE mosaic_static)
  mosaic_warnings(mosaic-token-document-serialization-smoke)
  add_test(NAME token-document-serialization COMMAND mosaic-token-document-serialization-smoke ${MOSAIC_MODEL_PACK} ${MOSAIC_UNICODE_PACK}
    ${CMAKE_CURRENT_SOURCE_DIR}/../fixtures/packs/security17-v1.mpack
    ${MOSAIC_NORMALIZATION_PACK}
    ${CMAKE_CURRENT_SOURCE_DIR}/../fixtures/packs/lexer/c-v1.mpack)

  add_executable(mosaic-lexer-smoke ${MOSAIC_TEST_DIR}/lexer_smoke.c)
  target_include_directories(mosaic-lexer-smoke PRIVATE ${MOSAIC_INCLUDE})
  target_link_libraries(mosaic-lexer-smoke PRIVATE mosaic_static)
  mosaic_warnings(mosaic-lexer-smoke)
  add_test(NAME lexer COMMAND mosaic-lexer-smoke ${MOSAIC_MODEL_PACK} ${MOSAIC_UNICODE_PACK}
    ${CMAKE_CURRENT_SOURCE_DIR}/../fixtures/packs/lexer/c-v1.mpack
    ${CMAKE_CURRENT_SOURCE_DIR}/../fixtures/packs/lexer/python-v1.mpack
    ${CMAKE_CURRENT_SOURCE_DIR}/../fixtures/packs/lexer/rust-v1.mpack
    ${CMAKE_CURRENT_SOURCE_DIR}/../fixtures/packs/lexer/json-v1.mpack)

  add_executable(mosaic-semantic-subbyte-smoke ${MOSAIC_TEST_DIR}/semantic_subbyte_smoke.c)
  target_include_directories(mosaic-semantic-subbyte-smoke PRIVATE ${MOSAIC_INCLUDE})
  target_link_libraries(mosaic-semantic-subbyte-smoke PRIVATE mosaic_static)
  mosaic_warnings(mosaic-semantic-subbyte-smoke)
  add_test(NAME semantic-subbyte COMMAND mosaic-semantic-subbyte-smoke ${MOSAIC_MODEL_PACK} ${MOSAIC_UNICODE_PACK}
    ${CMAKE_CURRENT_SOURCE_DIR}/../fixtures/packs/lexer/c-v1.mpack)

  add_executable(mosaic-block-storage-smoke ${MOSAIC_TEST_DIR}/block_storage_smoke.c)
  target_include_directories(mosaic-block-storage-smoke PRIVATE ${MOSAIC_INCLUDE})
  target_link_libraries(mosaic-block-storage-smoke PRIVATE mosaic_static)
  mosaic_warnings(mosaic-block-storage-smoke)
  add_test(NAME block-storage COMMAND mosaic-block-storage-smoke ${MOSAIC_MODEL_PACK} ${MOSAIC_UNICODE_PACK})

  add_executable(mosaic-cache-smoke ${MOSAIC_TEST_DIR}/cache_smoke.c)
  target_include_directories(mosaic-cache-smoke PRIVATE ${MOSAIC_INCLUDE})
  target_link_libraries(mosaic-cache-smoke PRIVATE mosaic_static Threads::Threads)
  mosaic_warnings(mosaic-cache-smoke)
  add_test(NAME cache COMMAND mosaic-cache-smoke)

  add_executable(mosaic-cache-backend-smoke ${MOSAIC_TEST_DIR}/cache_backend_smoke.c)
  target_include_directories(mosaic-cache-backend-smoke PRIVATE ${MOSAIC_INCLUDE})
  target_link_libraries(mosaic-cache-backend-smoke PRIVATE mosaic_static Threads::Threads)
  mosaic_warnings(mosaic-cache-backend-smoke)
  add_test(NAME cache-backend COMMAND mosaic-cache-backend-smoke)

  add_executable(mosaic-runtime-policy-smoke ${MOSAIC_TEST_DIR}/runtime_policy_smoke.c)
  target_include_directories(mosaic-runtime-policy-smoke PRIVATE ${MOSAIC_INCLUDE})
  target_link_libraries(mosaic-runtime-policy-smoke PRIVATE mosaic_static Threads::Threads)
  mosaic_warnings(mosaic-runtime-policy-smoke)
  add_test(NAME runtime-policy COMMAND mosaic-runtime-policy-smoke ${MOSAIC_MODEL_PACK} ${MOSAIC_UNICODE_PACK}
    ${CMAKE_CURRENT_SOURCE_DIR}/../fixtures/packs/language/en-v1.mpack)

  add_executable(mosaic-executor-smoke ${MOSAIC_TEST_DIR}/executor_smoke.c)
  target_include_directories(mosaic-executor-smoke PRIVATE ${MOSAIC_INCLUDE})
  target_link_libraries(mosaic-executor-smoke PRIVATE mosaic_static Threads::Threads)
  mosaic_warnings(mosaic-executor-smoke)
  add_test(NAME executor COMMAND mosaic-executor-smoke ${MOSAIC_MODEL_PACK} ${MOSAIC_UNICODE_PACK})

  add_executable(mosaic-observability-smoke ${MOSAIC_TEST_DIR}/observability_smoke.c)
  target_include_directories(mosaic-observability-smoke PRIVATE ${MOSAIC_INCLUDE})
  target_link_libraries(mosaic-observability-smoke PRIVATE mosaic_static Threads::Threads)
  mosaic_warnings(mosaic-observability-smoke)
  add_test(NAME observability COMMAND mosaic-observability-smoke ${MOSAIC_MODEL_PACK} ${MOSAIC_UNICODE_PACK}
    ${CMAKE_CURRENT_SOURCE_DIR}/../fixtures/packs/detector/reference-v1.mpack
    ${CMAKE_CURRENT_SOURCE_DIR}/../fixtures/packs/language/en-v1.mpack
    ${CMAKE_CURRENT_SOURCE_DIR}/../fixtures/packs/security17-v1.mpack
    ${MOSAIC_NORMALIZATION_PACK}
    ${CMAKE_CURRENT_SOURCE_DIR}/../fixtures/packs/lexer/c-v1.mpack)

  if(TARGET mosaic_trust_static)
    add_executable(mosaic-trust-smoke ${MOSAIC_TEST_DIR}/trust_smoke.c)
    target_include_directories(mosaic-trust-smoke PRIVATE ${MOSAIC_INCLUDE})
    target_link_libraries(mosaic-trust-smoke PRIVATE mosaic_static mosaic_trust_static)
    mosaic_warnings(mosaic-trust-smoke)
    add_test(NAME trust COMMAND mosaic-trust-smoke ${MOSAIC_MODEL_PACK}
      ${CMAKE_CURRENT_SOURCE_DIR}/../fixtures/trust/conformance-ed25519.pub
      ${CMAKE_CURRENT_SOURCE_DIR}/../fixtures/packs/model-v2.mpack.sig)
  endif()

  add_executable(mosaic-reliability-smoke ${MOSAIC_TEST_DIR}/reliability_smoke.c)
  target_include_directories(mosaic-reliability-smoke PRIVATE ${MOSAIC_INCLUDE})
  target_link_libraries(mosaic-reliability-smoke PRIVATE mosaic_static Threads::Threads)
  mosaic_warnings(mosaic-reliability-smoke)
  add_test(NAME reliability COMMAND mosaic-reliability-smoke ${MOSAIC_MODEL_PACK} ${MOSAIC_UNICODE_PACK})
  set_tests_properties(reliability PROPERTIES ENVIRONMENT "MOSAIC_RELIABILITY_ITERS=1000")

  add_executable(mosaic-malformed-smoke ${MOSAIC_TEST_DIR}/malformed_smoke.c)
  target_include_directories(mosaic-malformed-smoke PRIVATE ${MOSAIC_INCLUDE})
  target_link_libraries(mosaic-malformed-smoke PRIVATE mosaic_static)
  mosaic_warnings(mosaic-malformed-smoke)
  add_test(NAME malformed COMMAND mosaic-malformed-smoke ${CMAKE_CURRENT_SOURCE_DIR}/..)
endif()

set(MOSAIC_INSTALL_TARGETS mosaic_static mosaic_shared mosaic-tokenizer)
if(TARGET mosaic_trust_static)
  list(APPEND MOSAIC_INSTALL_TARGETS mosaic_trust_static mosaic_trust_shared)
endif()
install(TARGETS ${MOSAIC_INSTALL_TARGETS}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(FILES ${MOSAIC_INCLUDE}/mosaic.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
if(TARGET mosaic_trust_static)
  install(FILES ${MOSAIC_INCLUDE}/mosaic_trust.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
endif()
install(FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/../fixtures/packs/model-v2.mpack
        ${CMAKE_CURRENT_SOURCE_DIR}/../fixtures/packs/unicode17-v1.mpack
        ${CMAKE_CURRENT_SOURCE_DIR}/../fixtures/packs/security17-v1.mpack
        ${CMAKE_CURRENT_SOURCE_DIR}/../fixtures/packs/normalization16-v1.mpack
        DESTINATION ${CMAKE_INSTALL_DATADIR}/mosaic/packs)
install(FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/../fixtures/packs/lexer/c-v1.mpack
        ${CMAKE_CURRENT_SOURCE_DIR}/../fixtures/packs/lexer/python-v1.mpack
        ${CMAKE_CURRENT_SOURCE_DIR}/../fixtures/packs/lexer/rust-v1.mpack
        ${CMAKE_CURRENT_SOURCE_DIR}/../fixtures/packs/lexer/json-v1.mpack
        DESTINATION ${CMAKE_INSTALL_DATADIR}/mosaic/packs/lexer)
install(FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/../fixtures/packs/detector/reference-v1.mpack
        DESTINATION ${CMAKE_INSTALL_DATADIR}/mosaic/packs/detector)
install(FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/../fixtures/packs/language/en-v1.mpack
        ${CMAKE_CURRENT_SOURCE_DIR}/../fixtures/packs/language/hi-v1.mpack
        ${CMAKE_CURRENT_SOURCE_DIR}/../fixtures/packs/language/ja-v1.mpack
        DESTINATION ${CMAKE_INSTALL_DATADIR}/mosaic/packs/language)
