# 配置系统

## 配置系统介绍

实现了强类型XML配置反序列化框架，利用SFINAE与模板特化支持任意类型的序列化/反序列化。

- 类型安全：通过读取XML并自动映射到C++的强类型变量，使得在编译期/运行期（服务器启动时）都能发现类型不匹配问题。
- 可扩展：通过模板特化实现不同配置类型的专属解析逻辑，结合全局注册机制，实现了配置项的灵活扩展与统一管理，确保配置解析与管理职责清晰且可扩展性强。
- **职责分离**：`XmlElementTo<AppXmlConfig>`负责解析，`AppXmlConfig`负责存储。每个配置项（如 `AppXmlConfig`、`LogXmlConfig`）独立封装，职责单一，结构清晰。
- 集中式配置管理：通过 `ConfigManager` 统一管理所有配置项，支持全局访问（如 `g_app_config`），简化配置项查找与更新。

## 配置系统的类图

```mermaid
classDiagram
    direction TB

    class LogXmlConfig {
        + vector~Logger~ m_loggers
        + void load(const XMLElement * xml_log)
    }

    class Logger {
        +vector~Appender~ m_appenders
    }

    class Appender {
        +Type m_type
    }

    class Type {
        <<enum>>
        STDOUT
        FILE
    }
    
    class AppXmlConfig {
        + void load(const XMLElement * xml_app)    
    }
    
    class RemoteXmlConfig {
        + void load(const XMLElement * xml_remote)
    }

    class ConfigManagerBase {
        - map~std::string, ConfigVarBase::ptr~
        + LookUpBase()
        + LookUpOrAdd()
    }

    class ConfigManager {
        + LoadXmlConfigs()
    }

    class ConfigVarBase {
    	+ FromXmlElement(const XMLElement * elem) bool
    	+ FromXmlAttribute(const XMLAttribute * attr) bool
    	
        # std::string m_connName
        # std::string m_description
    }

    class ConfigVar~T~ {
        + T value
        
        + FromXmlElement(const XMLElement * elem) bool
    	+ FromXmlAttribute(const XMLAttribute * attr) bool
    }

    
	ConfigManagerBase <|-- ConfigManager
    ConfigVarBase <|-- ConfigVar~T~

    class XmlElementTo~T~ {
        +T operator()(const XMLElement*)
    }


    ConfigManager --> ConfigVarBase : manages
    ConfigVar --> LogXmlConfig : specializes
    ConfigVar --> AppXmlConfig : specializes
    ConfigVar --> RemoteXmlConfig : specializes
    
    LogXmlConfig "1" --> "n" Logger : contains
    Logger "1" --> "n" Appender : contains
    Appender --> Type : uses
    
    LogXmlConfig <-- XmlElementTo : specializes
    Logger       <-- XmlElementTo : specializes
    Appender     <-- XmlElementTo : specializes
    AppXmlConfig     <-- XmlElementTo : specializes
    RemoteXmlConfig     <-- XmlElementTo : specializes
```

## 加载日志配置的流程图


```mermaid
flowchart TD
    A["LoadXmlConfigs"] --> B[ConfigManager::parse_all_xml_nodes]
    B --> D[ConfigVar&lt;LogXmlConfig&gt;::FromXmlAttribute]
    B --> C[ConfigVar&lt;LogXmlConfig&gt;::FromXmlElement]
    C --> G["XmlElementTo&lt;LogXmlConfig&gt;::operator()"]
    D --> F[XmlAttributeTo&lt;LogXmlConfig&gt;]
    G --> H["XmlElementTo&lt;vector&lt;Logger&gt;&gt;::operator()"]
    H --> I["XmlElementTo&lt;Logger&gt;::operator()"]
    I --> J["XmlElementTo&lt;vector&lt;Appender&gt;&gt;::operator()"]
    J --> K["XmlElementTo&lt;Appender&gt;::operator()"]

```



