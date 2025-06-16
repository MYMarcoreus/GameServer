function(ProtoGenCpp ProtocDirname)
    # 不带引号时，ProtocArgs后面的参数是数组，如果带引号则会变成带;的字符串
    set(ProtocCmd ${Protobuf_PROTOC_EXECUTABLE})
    set(ProtocArgs # 不能带引号！
            --proto_path=${ProtocDirname}
            --cpp_out=${ProtocDirname}
            --experimental_allow_proto3_optional
    )
    file(GLOB ProtocFiles ${ProtocDirname}/*.proto )

    message(execute_process: ${ProtocCmd} ${ProtocArgs} ${ProtocFiles})

    execute_process(
            COMMAND ${ProtocCmd} ${ProtocArgs} ${ProtocFiles}
            WORKING_DIRECTORY ${ProtocDirname}
    )
endfunction()