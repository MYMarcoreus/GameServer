## 文件组织结构

- `log.h`：实现了`LogLevel`、`LogMessage`、`Logger`和`LoggerManager`类，并向外部提供基于C++20的`std::format`式接口。

  - `LogLevel`：支持TRACE、DEBUG、INFO、WARN、ERROR、FATAL六种级别
  - `LogMessage`：一条日志分为日志级别、产生日志信息的时间、产生日志信息的文件名、产生日志信息的代码所在行号、线程号、具体内容。
  - `Logger`：支持同步写和异步写的日志器。
  - `LoggerManager`：管理所有日志器，缓冲区中的所有日志字符串通过一个单独的写日志线程写入到目的地。

- `LogFormatter.h`：利用多态实现日志格式的自定义。

- `LogBufferManager.h`：双缓冲区管理类。

  > - 假设只有单缓冲区：业务线程写日志时，如果缓冲区满了，就必须阻塞等待后台线程写完。
  > - 使用双缓冲后：当前缓冲区满时，业务线程无需等待，直接切换到备用缓冲区继续写。

- `ILogAppender.h`：提供了同步写日志和异步写日志的通用接口。

  - `FileLogAppender.h`：支持将日志写入文件，使用`::writev`。

  - `StdoutLogApeender.h`：支持将日志写入控制台，使用`std::cout`。


## 类图

```mermaid
classDiagram
	class LogMessage {
        日志级别
        产生日志信息的时间
        产生日志信息的文件名
        产生日志信息的代码所在行号
        线程号
        具体内容
	}

    class LoggerManager{
    	- unordered_map~string, Logger~  : Logger列表
    	- std::thread : 异步写日志线程
    }
        
    LoggerManager "1" o-- "n" Logger
    
    class Logger{
        + Log(LogMessage msg)
        - LogSynch(LogMessage) 同步写日志
        - LogAsync(LogMessage) 异步写日志
        - vector~LogAppender~ m_appenders
    }
    Logger "1" o-- "n" ILogAppender
    
    class ILogAppender {
    	<<Interface>>
    	+ WriteLog(LogMessage) （同步写）日志格式化后直接写一条日志
        + AppendBuffer(LogMessage) （异步写）将日志格式化后写入缓冲区
        + WriteAndFlush() （异步写日志线程）将缓冲区的数据写入到目的地
    	# LogFormatter 日志格式化器
    	# LogBufferManager 双缓冲区管理器
    	- Write(std::vector~LinearBuffer~)
    	- Flush()
    }
    
    class LogBufferManager {
        - LinearBuffer              前端缓冲区
        - LinearBuffer              备用缓冲区
        - std::vector~LinearBuffer~ 待写缓冲区
        + Append(格式化后的日志字符串) 
        + SwapAndWriteFlush()
    }
    
    class FileLogAppender {
        将日志字符串写到文件
    }
    class StdoutLogApeender {
    	将日志字符串写到控制台
    }   
    
    LoggerManager <-- ILogAppender : 异步写日志线程不断调用WriteAndFlush
    FileLogAppender   --|> ILogAppender
    StdoutLogApeender --|> ILogAppender
    LogFormatter "1" o-- "n" IFormatItem
    
    class LogFormatter{
        std::vector~IFormatItem~ 所支持的格式化项
        string format(LogMessage) 格式化消息为字符串
    }
    
    class IFormatItem{
        <<Interface>>
    }
    
    ILogAppender "1" *-- "1" LogFormatter
    ILogAppender "1" *-- "1" LogBufferManager
```



```mermaid
classDiagram
    - IFormatItem --|> PlainFormatItem
    - IFormatItem --|> LevelFormatItem
    - IFormatItem --|> ThreadIDFormatItem
    - IFormatItem --|> ContentFormatItem
    - IFormatItem --|> TimeFormatItem
    - IFormatItem --|> FilepathFormatItem
    - IFormatItem --|> FilelineFormatItem
    - IFormatItem --|> NewlineFormatItem
    - IFormatItem --|> TabFormatItem
    - IFormatItem --|> PercentSignFormatItem
```





