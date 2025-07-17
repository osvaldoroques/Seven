
if(NOT "/workspaces/Seven/build/_deps/tl_lru_cache-subbuild/tl_lru_cache-populate-prefix/src/tl_lru_cache-populate-stamp/tl_lru_cache-populate-gitinfo.txt" IS_NEWER_THAN "/workspaces/Seven/build/_deps/tl_lru_cache-subbuild/tl_lru_cache-populate-prefix/src/tl_lru_cache-populate-stamp/tl_lru_cache-populate-gitclone-lastrun.txt")
  message(STATUS "Avoiding repeated git clone, stamp file is up to date: '/workspaces/Seven/build/_deps/tl_lru_cache-subbuild/tl_lru_cache-populate-prefix/src/tl_lru_cache-populate-stamp/tl_lru_cache-populate-gitclone-lastrun.txt'")
  return()
endif()

execute_process(
  COMMAND ${CMAKE_COMMAND} -E rm -rf "/workspaces/Seven/build/_deps/tl_lru_cache-src"
  RESULT_VARIABLE error_code
  )
if(error_code)
  message(FATAL_ERROR "Failed to remove directory: '/workspaces/Seven/build/_deps/tl_lru_cache-src'")
endif()

# try the clone 3 times in case there is an odd git clone issue
set(error_code 1)
set(number_of_tries 0)
while(error_code AND number_of_tries LESS 3)
  execute_process(
    COMMAND "/usr/bin/git"  clone --no-checkout --depth 1 --no-single-branch --config "advice.detachedHead=false" "https://github.com/TartanLlama/lru_cache.git" "tl_lru_cache-src"
    WORKING_DIRECTORY "/workspaces/Seven/build/_deps"
    RESULT_VARIABLE error_code
    )
  math(EXPR number_of_tries "${number_of_tries} + 1")
endwhile()
if(number_of_tries GREATER 1)
  message(STATUS "Had to git clone more than once:
          ${number_of_tries} times.")
endif()
if(error_code)
  message(FATAL_ERROR "Failed to clone repository: 'https://github.com/TartanLlama/lru_cache.git'")
endif()

execute_process(
  COMMAND "/usr/bin/git"  checkout main --
  WORKING_DIRECTORY "/workspaces/Seven/build/_deps/tl_lru_cache-src"
  RESULT_VARIABLE error_code
  )
if(error_code)
  message(FATAL_ERROR "Failed to checkout tag: 'main'")
endif()

set(init_submodules TRUE)
if(init_submodules)
  execute_process(
    COMMAND "/usr/bin/git"  submodule update --recursive --init 
    WORKING_DIRECTORY "/workspaces/Seven/build/_deps/tl_lru_cache-src"
    RESULT_VARIABLE error_code
    )
endif()
if(error_code)
  message(FATAL_ERROR "Failed to update submodules in: '/workspaces/Seven/build/_deps/tl_lru_cache-src'")
endif()

# Complete success, update the script-last-run stamp file:
#
execute_process(
  COMMAND ${CMAKE_COMMAND} -E copy
    "/workspaces/Seven/build/_deps/tl_lru_cache-subbuild/tl_lru_cache-populate-prefix/src/tl_lru_cache-populate-stamp/tl_lru_cache-populate-gitinfo.txt"
    "/workspaces/Seven/build/_deps/tl_lru_cache-subbuild/tl_lru_cache-populate-prefix/src/tl_lru_cache-populate-stamp/tl_lru_cache-populate-gitclone-lastrun.txt"
  RESULT_VARIABLE error_code
  )
if(error_code)
  message(FATAL_ERROR "Failed to copy script-last-run stamp file: '/workspaces/Seven/build/_deps/tl_lru_cache-subbuild/tl_lru_cache-populate-prefix/src/tl_lru_cache-populate-stamp/tl_lru_cache-populate-gitclone-lastrun.txt'")
endif()

