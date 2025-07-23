#include "UserConnection.h"
#include "UserBuffer.h"
#include "player.pb.h"

using yy::core::UserBuffer;
using yy::core::UserConnection;

// void Event_ReceiveOne(UserBuffer buffer)
// {
//     char temp_recvBuf[16] = "abcdefghijklmno";
//
//     buffer.ReadFromCBuffer(temp_recvBuf, sizeof temp_recvBuf);
//     buffer.set_isCompleted(true); // Receiver线程标记数据接收完成，Handler可处理
// }
//
// void Update_ReadPackage(UserConnection userdata)
// {
//     auto & recvBuf = userdata.recv_buf;
//     auto & sendBuf = userdata.send_buf;
//
//     // 数据还没准备好，跳过本次Update
//     if(!userdata.recv_buf.get_isCompleted()) {
//         return;
//     }
//
//     yy::app::protocol::PlayerID playerID;
//     playerID.set_uid(127);
//     recvBuf.WriteToProtobuf(playerID, playerID.ByteSizeLong());
//
//     auto i = int{10};
//     sendBuf.ReadFromStruct(i);
//     sendBuf.ReadFromProtobuf(playerID);
//
//     recvBuf.set_isCompleted(false);
// }




void Tester1(UserBuffer & buffer)
{
    char recvBuf[6]{};

    buffer.ReadFromCBuffer("abc", 3); // head = 0, tail = 3
    buffer.print();
    buffer.WriteToCBuffer(recvBuf, 3); // head = 3, tail = 3
    buffer.print();
    printf("\n");

    buffer.ReadFromCBuffer("def", 3); // tail = 6
    buffer.print();
    buffer.WriteToCBuffer(recvBuf, 3);
    buffer.print();
    printf("\n");


    buffer.ReadFromCBuffer("ghi", 3); // tail = 1
    buffer.print();
    buffer.WriteToCBuffer(recvBuf, 3);
    buffer.print();
    printf("\n");
    std::cout << buffer.Maxsize() << " " << buffer.RemainedSize() << " " << buffer.DataSize() << std::endl;

    buffer.ReadFromCBuffer("jkl", 3); // tail = 4
    buffer.print();
    buffer.WriteToCBuffer(recvBuf, 3);
    buffer.print();
    printf("\n");

    buffer.ReadFromCBuffer("mno", 3); // tail = 4
    buffer.print();
    buffer.WriteToCBuffer(recvBuf, 3);
    buffer.print();
    printf("\n");

    buffer.ReadFromCBuffer("pqr", 3); // tail = 4
    buffer.print();
    buffer.WriteToCBuffer(recvBuf, 3);
    buffer.print();
    printf("\n");
}

void Tester2(UserBuffer & buffer)
{
    char recvBuf[6]{};

    yy::app::protocol::PlayerID playerIDread, playerIDwrite;

    playerIDread.set_uid(1);
    buffer.ReadFromProtobuf(playerIDread);
    // printf("%u, %zu, %zu\n", playerIDwrite.uid(), buffer.get_head(), buffer.get_tail());
    buffer.WriteToProtobuf(playerIDwrite, playerIDread.ByteSizeLong());
    printf("%u\n", playerIDwrite.uid());

    playerIDread.set_uid(2);
    buffer.ReadFromProtobuf(playerIDread);
    buffer.WriteToProtobuf(playerIDwrite, playerIDread.ByteSizeLong());
    printf("%u\n", playerIDwrite.uid());

    playerIDread.set_uid(3);
    buffer.ReadFromProtobuf(playerIDread);
    // printf("%u, %zu, %zu\n", playerIDwrite.uid(), buffer.get_head(), buffer.get_tail());
    buffer.WriteToProtobuf(playerIDwrite, playerIDread.ByteSizeLong());
    printf("%u\n", playerIDwrite.uid());

    playerIDread.set_uid(4);
    buffer.ReadFromProtobuf(playerIDread);
    buffer.WriteToProtobuf(playerIDwrite, playerIDread.ByteSizeLong());
    printf("%u\n", playerIDwrite.uid());
}



int main()
{
    UserBuffer buffer{7};
    Tester2(buffer);


    return 0;
}