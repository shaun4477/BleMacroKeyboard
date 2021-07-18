# Import Arduino libraries from the Arduino library default
# directory by including all of the cpp/c/S files from the 
# 'src' subdirectory and adding the source directories to the 
# include path
function(__get_sources_from_subdirs subdir_names parent_dir sources_output_name includes_output_name)
  set(include_dirs "${${includes_output_name}}")
  set(srcs "${${sources_output_name}}")
  foreach(subdir_name ${subdir_names})
    set(abs_dir "${parent_dir}/${subdir_name}/src")
    if(NOT IS_DIRECTORY ${abs_dir})
      message(FATAL_ERROR "Dir '${abs_dir}' does not exist.")
    endif()

    file(GLOB_RECURSE dir_sources LIST_DIRECTORIES false "${abs_dir}/*.c" "${abs_dir}/*.cpp" "${abs_dir}/*.S")

    if(dir_sources)
      list(APPEND include_dirs "${abs_dir}")
      foreach(src ${dir_sources})
        get_filename_component(src "${src}" ABSOLUTE)
        list(APPEND srcs "${src}")
      endforeach()
    else()
      message(WARNING "No source files found for SRC_DIRS entry '${dir}'.")
    endif()
  endforeach()

  set(${sources_output_name} "${srcs}" PARENT_SCOPE)
  set(${includes_output_name} "${include_dirs}" PARENT_SCOPE)
endfunction()

