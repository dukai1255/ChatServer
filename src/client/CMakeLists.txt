#将当前路径下所有的源文件(即client下所有的源文件)，命名为SRC_LIST变量；
aux_source_directory(. SRC_LIST)
#生成可执行文件：
add_executable(ChatClient ${SRC_LIST})
#指定可执行文件链接时需要依赖的库文件：

target_link_libraries(ChatClient pthread)