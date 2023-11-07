#ifndef ____WRAP_FILE_H_
#define ____WRAP_FILE_H_

#include"wrap_fd.h"
#include"noncopyable.h"
#include<sys/types.h>
#include<sys/stat.h>
#include<string>
#include<sys/fcntl.h>


namespace yy::util {

// File不负责关闭文件，只是提供各种方便的文件方法
class File : public FileDescriper, noncopyable
{
/* 静态函数 */
public:
    /* 查询当前进程对某个文件的访问权限，若pathname为符号链接，access()将对其解引用
        F_OK：文件存在？
        R_OK：有读权限？
        W_OK：有写权限？
        X_OK：有执行权限？ */
    static bool Access(const std::string & pathname, int type);

    ///@brief 查询一个文件是否存在
    static bool isExist(const std::string & pathname) { return File::Access(pathname, F_OK); }
    static bool canRead(const std::string & pathname) { return File::Access(pathname, R_OK); }
    static bool canWrite(const std::string & pathname) { return File::Access(pathname, W_OK); }
    static bool canExecute(const std::string & pathname) { return File::Access(pathname, X_OK); }

    static struct stat Stat(const std::string & pathname);

    /// @brief Realpath = Dirname + Basename
    static std::string Realpath(const std::string & pathname);

    /// @brief 如： /home/yy/hello.cpp 中的 /home/yy
    static std::string Dirname(const std::string & fullname);

    /// @brief 如： /home/yy/hello.cpp 中的 hello.cpp
    static std::string Basename(const std::string & fullname);

public:
    explicit File(const FileDescriper & fd);

    // 直接打开已存在文件，若不存在则报错
    File(const std::string & pathname, int O_XXXX);
    File(const std::string & pathname, int O_XXXX, mode_t mode);
    ~File() = default;

    /// @brief 打开一个文件 
    /// @param pathname 可以是绝对路径名，也可以是相对路径；
    /// 如果以`/`开头，则是绝对路径，否则是相对路径
    ///     Open函数会根据pathname获得文件的绝对路径名并保存至fullname_成员中
    /// @param O_XXXX  指定打开文件的方式
    /// @verbatim
    ///     1、O_RDONLY   ：以只读方式打开指定文件，目录可以以读方式打开
    ///     2、O_WRONLY   ：以只写方式打开指定文件，目录不能以写方式打开  
    ///     3、O_RDWR     ：以可读写方式打开指定文件  
    ///     4、O_APPEND   ：以追加的方式打开指定文件  
    ///     5、O_CREAT    ：如果指定文件不存在，则创建这个文件。需要指定mode参数， 
    ///                  　否则该值为栈中的随机值。即使文件以只读方式打开，该标志仍有效。 
    ///     6、O_EXCL     ：如果指定文件已存在，则出错，同时返回-1，并且修改 errno 的值。一般配合O_CREAT使用。 
    ///     7、O_TRUNC    ：如果指定文件已存在且为普通文件，则清空原文件，长度被截为0，属性不变。
    ///                   　无论以读/写的方式打开文件，都可以将文件清空。 
    ///     8、O_NONBLOCK ：以非阻塞方式打开  
    /// @endverbatim
    /// @param mode 使用8进制，如0644代表110(rw-) 100(r--) 100(r--)
    /// @return 打开文件的文件描述符
    int Open(const std::string & pathname, int O_XXXX, mode_t mode);


    /// @brief 获取文件的绝对路径名
    [[nodiscard]] std::string getFullname() const;

    /// @brief 获取文件所在的目录路径
    [[nodiscard]] std::string getDirname() const;

    /// @brief 获取文件名
    [[nodiscard]] std::string getBasename() const;

    /// @brief 获取文件大小
    [[nodiscard]] long get_size() const;

    /// @brief 获取当前文件指针的偏移量
    [[nodiscard]] off_t get_offset() const;


    /* 查询当前进程对File对象所含文件的访问权限
        R_OK：有读权限？
        W_OK：有写权限？
        X_OK：有执行权限？ */
    [[nodiscard]] bool Access(int type) const;

    [[nodiscard]] bool canRead() const { return Access(R_OK); }

    [[nodiscard]] bool canWrite() const { return Access(W_OK); }

    [[nodiscard]] bool canExecute() const { return Access(X_OK); }

    /// @brief 用来控制该文件的读写位置，whence传入SEEK_XXX的宏
    off_t Lseek(int SEEK_XXX, off_t offset) const; // NOLINT(modernize-use-nodiscard)

    /// @brief 将文件截断为指定的大小。可以大于文件原大小，此时功能为拓展文件(用0填充)
    void Truncate(off_t length) const;

    /// @brief 穿透，Stat会输出其符号链接指向的文件
    [[nodiscard]] struct stat Stat() const;

    /// @brief 非穿透，Lstat直接输出符号链接文件本身
    [[nodiscard]] struct stat Lstat() const;

    /// @brief 设置文件的权限(穿透，会改变符号链接所指代的文件的权限)
    void Chmod(mode_t mode) const;

    /// @brief 重命名文件，不能改变文件所在的文件夹
    void set_filename(const char *newfilename);

protected:
    std::string fullname_;  // 例：home/yy/hello.cpp
};


}


#endif