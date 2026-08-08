cmake_minimum_required(VERSION 3.20)
project(mosaic VERSION 0.16.0 LANGUAGES C CXX)

include(GNUInstallDirs)
include(CTest)

set(MOSAIC_SRC ${CMAKE_CURRENT_SOURCE_DIR}/src/mosaic.c)
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

add_library(mosaic_shared SHARED ${MOSAIC_SRC})
target_compile_definitions(mosaic_shared PRIVATE MOSAIC_LIBRARY_ONLY)
target_include_directories(mosaic_shared PUBLIC $<BUILD_INTERFACE:${MOSAIC_INCLUDE}> $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
set_target_properties(mosaic_shared PROPERTIES OUTPUT_NAME mosaic WINDOWS_EXPORT_ALL_SYMBOLS ON)
mosaic_warnings(mosaic_shared)

add_executable(mosaic-tokenizer ${MOSAIC_SRC})
target_include_directories(mosaic-tokenizer PRIVATE ${MOSAIC_INCLUDE})
mosaic_warnings(mosaic-tokenizer)

if(BUILD_TESTING)
  set(MOSAIC_TEST_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../conformance/c)
  set(MOSAIC_MODEL_PACK ${CMAKE_CURRENT_SOURCE_DIR}/../fixtures/packs/model-v2.mpack)
  set(MOSAIC_UNICODE_PACK ${CMAKE_CURRENT_SOURCE_DIR}/../fixtures/packs/unicode17-v1.mpack)
  set(MOSAIC_NORMALIZATION_PACK ${CMAKE_CURRENT_SOURCE_DIR}/../fixtures/packs/normalization16-v1.mpack)
  find_package(ICU COMPONENTS uc REQUIRED)

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

  add_executable(mosaic-normalization-smoke ${MOSAIC_TEST_DIR}/normalization_smoke.c)
  target_include_directories(mosaic-normalization-smoke PRIVATE ${MOSAIC_INCLUDE} ${ICU_INCLUDE_DIRS})
  target_link_libraries(mosaic-normalization-smoke PRIVATE mosaic_static ICU::uc)
  mosaic_warnings(mosaic-normalization-smoke)
  add_test(NAME normalization COMMAND mosaic-normalization-smoke ${MOSAIC_NORMALIZATION_PACK} ${MOSAIC_MODEL_PACK} ${MOSAIC_UNICODE_PACK})

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

  add_executable(mosaic-malformed-smoke ${MOSAIC_TEST_DIR}/malformed_smoke.c)
  target_include_directories(mosaic-malformed-smoke PRIVATE ${MOSAIC_INCLUDE})
  target_link_libraries(mosaic-malformed-smoke PRIVATE mosaic_static)
  mosaic_warnings(mosaic-malformed-smoke)
  add_test(NAME malformed COMMAND mosaic-malformed-smoke ${CMAKE_CURRENT_SOURCE_DIR}/..)
endif()

install(TARGETS mosaic_static mosaic_shared mosaic-tokenizer
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(FILES ${MOSAIC_INCLUDE}/mosaic.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
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
