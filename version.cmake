macro(__version_file FILE_SRC FILE_HEADER)
add_custom_target(${PROJECT_NAME}-version
  COMMAND bash -c "echo -e \"extern const char * ${PROJECT_NAME}_version_string;\" > ${FILE_HEADER}.tmp"
  COMMAND bash -c "echo -e \"extern const unsigned int ${PROJECT_NAME}_version_major;\" >> ${FILE_HEADER}.tmp"
  COMMAND bash -c "echo -e \"extern const unsigned int ${PROJECT_NAME}_version_minor;\" >> ${FILE_HEADER}.tmp"
  COMMAND bash -c "echo -e \"extern const unsigned int ${PROJECT_NAME}_version_revision;\" >> ${FILE_HEADER}.tmp"
  COMMAND bash -c "echo -e \"extern const unsigned int ${PROJECT_NAME}_commit_count;\" >> ${FILE_HEADER}.tmp"
  COMMAND bash -c "echo -e \"extern const unsigned int ${PROJECT_NAME}_commit_sha1;\" >> ${FILE_HEADER}.tmp"
  COMMAND bash -c "( cd ${PROJECT_SOURCE_DIR} ; git describe --tags --dirty || echo unknown ) | sed 's/\\\(.*\\\)/const char * ${PROJECT_NAME}_version_string=\"\\1\"\;/'  > ${FILE_SRC}.tmp"
  COMMAND bash -c "( cd ${PROJECT_SOURCE_DIR} ; git describe --tags --dirty || echo \"0.0\" ) | sed 's/\\\([0-9]*\\\)[.]\\\([0-9]*\\\).*/const unsigned int ${PROJECT_NAME}_version_major=\\1;\\nconst unsigned int ${PROJECT_NAME}_version_minor=\\2\;/'  >> ${FILE_SRC}.tmp"
  COMMAND bash -c "echo -e \"const unsigned int ${PROJECT_NAME}_version_revision=0;\" >> ${FILE_SRC}.tmp"
  COMMAND bash -c "( cd ${PROJECT_SOURCE_DIR} ; git describe --tags --dirty --long || echo \"-0-\" ) | sed 's/.*-\\\([0-9]*\\\)-.*/const unsigned int ${PROJECT_NAME}_commit_count=\\1;/'  >> ${FILE_SRC}.tmp"
  COMMAND bash -c "( cd ${PROJECT_SOURCE_DIR} ; git describe --tags --dirty --long || echo \"-g0\" ) | sed 's/.*-g\\\([0-9a-fA-Z]*\\\).*/const unsigned int ${PROJECT_NAME}_commit_sha1=0x\\1;/'  >> ${FILE_SRC}.tmp"
  COMMAND bash -c "diff ${FILE_SRC}.tmp ${FILE_SRC} > /dev/null ; if [ $? -ne 0 ] ; then cp ${FILE_SRC}.tmp ${FILE_SRC} ; fi"
  COMMAND bash -c "diff ${FILE_HEADER}.tmp ${FILE_HEADER} > /dev/null ; if [ $? -ne 0 ] ; then cp ${FILE_HEADER}.tmp ${FILE_HEADER} ; fi"
  VERBATIM
  )
set_source_files_properties(${FILE_SRC} ${FILE_HEADER}
			PROPERTIES GENERATED yes)
endmacro(__version_file)

macro(version_file_c SRCS)
__version_file(${PROJECT_BINARY_DIR}/version.c ${PROJECT_BINARY_DIR}/${PROJECT_NAME}-version.h)
list(APPEND ${SRCS} ${PROJECT_BINARY_DIR}/version.c)
endmacro(version_file_c)

macro(version_file_cpp SRCS)
__version_file(${PROJECT_BINARY_DIR}/version.cpp ${PROJECT_BINARY_DIR}/${PROJECT_NAME}-version.h)
list(APPEND ${SRCS} ${PROJECT_BINARY_DIR}/version.cpp)
endmacro(version_file_cpp)

macro(version_add_dependencies TARGET)
add_dependencies(${TARGET} ${PROJECT_NAME}-version)
endmacro(version_add_dependencies)
