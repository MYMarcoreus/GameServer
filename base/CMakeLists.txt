add_subdirectory(public)
add_subdirectory(config)
add_subdirectory(log)
add_subdirectory(utils)
add_subdirectory(net)

#! 使用PARENT_SCOPE关键字是的子目录可以更改父目录的变量：这里是添加子目录源文件到父目录的变量中
set(COMMON_SOURCES ${COMMON_SOURCES} ${FullLocalSrc} PARENT_SCOPE)
