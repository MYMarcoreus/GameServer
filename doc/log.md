# 日志系统

## 文件组织结构

- `log.h`：实现了`LogLevel`、`LogMessage`、`Logger`和`LoggerManager`类，并向外部提供基于C++20的`std::format`式接口。

  - `LogLevel`：支持TRACE、DEBUG、INFO、WARN、ERROR、FATAL六种级别
  - `LogMessage`：一条日志分为日志级别、产生日志信息的时间、产生日志信息的文件名、产生日志信息的代码所在行号、线程号、具体内容。
  - `Logger`：支持同步写和异步写的日志器。
  - `LoggerManager`：管理所有日志器，缓冲区中的所有日志字符串通过一个单独的写日志线程写入到目的地。

- `LogFormatter.h`：利用多态实现日志格式的自定义。

- `LogBufferManager.h`：双缓冲区管理类。

  > ###### 为什么要用双缓冲区，而不用单缓冲区
  >
  > - ==**假设只有单缓冲区：业务线程写日志时，如果缓冲区满了，就必须阻塞等待后台线程写完。**== 
  > - **使用双缓冲后：当前缓冲区满时，业务线程无需等待，直接切换到备用缓冲区继续写。写满的缓冲区就交给后台线程写文件。** 

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

## 业务线程执行流

```mermaid
flowchart TD
    %% 业务接口模块
    subgraph BIZ["业务接口阶段"]
        A[业务代码调用日志接口] -->  E{日志级别过低}
        E -- 是 --> X[直接返回]
        E -- 否 --> G{异步或同步模式?}
    end

    %% 同步日志模块
    subgraph SYNC["同步写日志模块"]
        G -- 同步 --> H[开始同步写日志]
        H --> K[直接写文件]
        K --> L{日志是否为 FATAL 级别?}
        L -- 否 --> X
        L -- 是 --> Z[terminate]
    end

    %% 异步日志模块
    subgraph ASYNC["异步写日志模块"]
        G -- 异步 --> I[开始异步写日志]
        I --> M{日志是否为 FATAL 级别?}
        M -- 是 --> H
        M -- 否 --> N[将日志结构体构造为日志字符串]
        N --> Step1
    end

	subgraph Step1["将日志字符串写入缓冲区"]
        Q{主缓冲区是否有空间?}
        Lock --> Q
        Q -- 是 --> R[将日志写入主缓冲区]
        Q -- 否 --> S[将已满的主缓冲区移动到文件待写区]
        S --> T[将备用缓冲区作为新的主缓冲区]
        T --> U[将日志写入主缓冲区]
        U --> D[notify条件变量]
        R --> Unlock
        D --> Unlock
    end
```

## 后台写日志线程执行流

```mermaid
flowchart TD
    A[后台线程：对于每一个logger的每一种appender] --> Step1

    %% Step1：交换缓冲区
    subgraph Step1["Step1：加锁交换缓冲区"]
        B1[Lock 且 等待条件变量直到文件待写队列不为空]
        B2[将主缓冲区加入文件待写队列]
        B3[将文件待写队列的缓冲区swap到临时变量tempQueue中]
        B4[分配新的主/备用缓冲区：复用tempBuffer1/2]
        B5[Unlock]
        B1 --> B2 --> B3 --> B4 --> B5
    end

    Step1 --> Step2

    %% Step2：无锁写文件
    subgraph Step2["Step2: 无锁写文件"]
        C1[将tempQueue中的数据流写入文件：调用自定义回调]
    end

    Step2 --> Step3

    %% Step3：回收 buffer
    subgraph Step3["Step3: 回收 Buffer"]
        D1[将tempQueue中的两个缓冲区回收至tempBuffer1/2中]
        D2[初始化 tempBuffer1/2，供下一轮的Step1使用]
        D1 --> D2
    end

    Step3 --> Step4

    %% Step4：Flush 文件
    subgraph Step4["Step4: Flush 文件"]
        E1[Flush缓冲区：调用自定义回调]
    end

    Step4 --> F[等待下一次唤醒或定时触发]

```



