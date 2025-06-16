cmake_minimum_required(VERSION 3.22)
project(GameServer)
set(CMAKE_CXX_STANDARD 20)

#set(LocalSrc
#
#)
#
##! 给本地相对路径添加前缀，变为绝对路径
#foreach(LocalFile ${LocalSrc})
#    list(PREPEND FullLocalSrc "${CMAKE_CURRENT_SOURCE_DIR}/${LocalFile}")
#endforeach()
#
#
#set(FullLocalSrc)

add_subdirectory(public)
add_subdirectory(config)
add_subdirectory(log)
add_subdirectory(utils)
add_subdirectory(net)

#! 使用PARENT_SCOPE关键字是的子目录可以更改父目录的变量：这里是添加子目录源文件到父目录的变量中
set(COMMON_SOURCES ${COMMON_SOURCES} ${FullLocalSrc} PARENT_SCOPE)
