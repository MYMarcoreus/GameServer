#ifndef GAMESERVER_FILELOGAPPENDER_H
#define GAMESERVER_FILELOGAPPENDER_H

#include "log.h"
#include "ILogAppender.h"

namespace yy::Ylog {

/// @brief 日志输出至文件
class FileLogAppender final : public ILogAppender
{
public:
    explicit FileLogAppender(const std::string& logfilepath, const std::string& format_pattern);

    ~FileLogAppender() override;

    /// @brief 将日志信息msg写到文件
    void WriteLog(const LogMessage::ptr& msg) override;

    void SetTimeFormat(const std::string &timeFmtPattern, bool need_us) override {
        m_formatter->SetTimeFormat(timeFmtPattern, need_us);
    }

private:
    LogFormatter::ptr m_formatter;

    std::string m_logfilepath; // 完整的文件路径
#if USE_CPP_STREAM
    std::ofstream m_ofs;  // 文件流
#else
    int m_filefd{};
    // FILE * m_filep;
#endif
};

} // namespace yy::Ylog


#endif //GAMESERVER_FILELOGAPPENDER_H
