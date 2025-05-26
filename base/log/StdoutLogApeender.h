#ifndef GAMESERVER_STDOUTLOGAPEENDER_H
#define GAMESERVER_STDOUTLOGAPEENDER_H

#include "log.h"
#include "ILogAppender.h"

namespace yy::Ylog {


/// @brief 日志输出至标准输出
class StdoutLogApeender final : public ILogAppender {
public:
    StdoutLogApeender() = delete;

    explicit StdoutLogApeender(const std::string &format_pattern);

    ~StdoutLogApeender() override = default;

    void SetTimeFormat(const std::string &timeFmtPattern, bool need_us) override {
        m_formatter->SetTimeFormat(timeFmtPattern, need_us);
    }

    /// @brief 将日志信息msg写到标准输出
    void WriteLog(const LogMessage::ptr &msg) override;
private:
    LogFormatter::ptr m_formatter;
};

}

#endif //GAMESERVER_STDOUTLOGAPEENDER_H
