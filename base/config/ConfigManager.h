#pragma once

#include <unordered_map>
#include <algorithm>
#include <vector>
#include <iostream>
#include <memory>
#include <sstream>
#include <mutex>
#include <set>
#include <utility>
#include <filesystem>
#include <cassert>
#include <string>
#include <functional>

#include "tinyxml/tinyxml2.h"
#include "RWLock.h"
#include "util_functions.h"


namespace yy::config {

/*
! 类型驱动的 XML 配置反序列化框架
! ①：类型特化：XmlElementTo<T> 模板类，为不同的目标类型提供不同的 XML 解析逻辑，解耦解析逻辑与数据结构。
! ②：统一转换接口：operator()(const XMLElement)*
*/


///@brief 节点属性转换为C++的类型变量
template<typename T>
T XmlAttributeTo(const tinyxml2::XMLAttribute* xml_attr)
{
    if (!xml_attr) {
        std::stringstream ss;
        ss << "xml_attr为空，该属性可能不存在！";
        throw std::invalid_argument(ss.str());
    }

    //! 字符串属性
    if constexpr (std::is_same_v<std::decay_t<T>, char*> || std::is_same_v<T, std::string>) {
        const char* attr_val = xml_attr->Value();
        return attr_val ? attr_val : "";
    }
    //! 布尔属性
    else
    if constexpr (std::is_same_v<T, bool>) {
        bool attr_val;
        const int ret = xml_attr->QueryBoolValue(&attr_val);
        if (ret == tinyxml2::XML_WRONG_ATTRIBUTE_TYPE) {
            std::cerr << "属性[" << xml_attr->Name() << "]属性不为布尔类型！\n";
            throw std::bad_cast();
        }
        return attr_val;
    }
    //! 浮点数属性
    else
    if constexpr (std::is_floating_point_v<T>) {
        double attr_val;
        const int ret = xml_attr->QueryDoubleValue(&attr_val);
        if (ret == tinyxml2::XML_WRONG_ATTRIBUTE_TYPE) {
            std::cerr << "属性[" << xml_attr->Name() << "]属性不为浮点类型！\n";
            throw std::bad_cast();
        }
        return static_cast<T>(attr_val);
    }
    //! 整数属性
    else
    if constexpr (std::is_integral_v<T>){
        //! 无符号整数
        if constexpr (std::is_unsigned_v<T>) {
            unsigned int attr_val;
            const int ret = xml_attr->QueryUnsignedValue(&attr_val);
            if (ret == tinyxml2::XML_WRONG_ATTRIBUTE_TYPE) {
                std::cerr << "属性[" << xml_attr->Name() << "]属性不为无符号整型！\n";
                throw std::bad_cast();
            }
            return static_cast<T>(attr_val);
        }
        //! 带符号整数
        else
        if constexpr (std::is_signed_v<T>) {
            int attr_val;
            const int ret = xml_attr->QueryIntValue(&attr_val);
            if (ret == tinyxml2::XML_WRONG_ATTRIBUTE_TYPE) {
                std::cerr << "属性[" << xml_attr->Name() << "]属性不为整型！\n";
                throw std::bad_cast();
            }
            return static_cast<T>(attr_val);
        }
    } else {
        static_assert(sizeof(T) == 0, "XmlAttributeTo 不支持该类型");
    }

    throw std::bad_cast();
}




//! 主模板声明（用于特化）：将XML序列化保存到数据结构T中
template<class T>
class XmlElementTo
{
public:
    T operator()(const tinyxml2::XMLElement *xml_remote_node);
};

///@brief 一个结点下面有多个子节点T，这些结点可以重复
template<class T>
class XmlElementTo<std::vector<T>>
{
public:
    //! 可以再重载一个
    std::vector<T> operator()(const tinyxml2::XMLElement *xml_parent)
    {
        std::vector<T> children;
        for (auto xml_child = xml_parent->FirstChildElement(); xml_child; xml_child = xml_child->NextSiblingElement()) {
            children.emplace_back(XmlElementTo<T>{}(xml_child));
        }
        return children;
    }
};

///@brief 一个结点下面有多个子节点T，这些结点不能重复
template<class T>
class XmlElementTo<std::set<T>>
{
public:
    std::set<T> operator()(const tinyxml2::XMLElement *xml_parent)
    {
        std::set<T> children;
        for (auto xml_child = xml_parent->FirstChildElement(); xml_child; xml_child = xml_child->NextSiblingElement()) {
            children.insert(XmlElementTo<T>{}(xml_child));
        }
        return children;
    }
};







/**
 * @brief 配置变量的基类
 */
class ConfigVarBase {
public:
    typedef std::shared_ptr<ConfigVarBase> ptr;
    /**
     * @brief 构造函数
     * @param[in] name 配置参数名称[0-9a-z_.]
     * @param[in] description 配置参数描述
     */
    explicit ConfigVarBase(std::string  name, std::string  description = "")
            :m_name(std::move(name)) ,m_description(std::move(description)) {
        std::ranges::transform(m_name, m_name.begin(), ::tolower);
    }

    /// @brief 析构函数
    virtual ~ConfigVarBase() = default;

    /// @brief 返回配置参数名称
    [[nodiscard]] const std::string& GetName() const { return m_name;}

    /// @brief 返回配置参数的描述
    [[nodiscard]] const std::string& GetDescription() const { return m_description;}

    /// @brief 转成字符串
    // virtual std::string toXml() = 0;

    virtual bool FromXmlElement(const tinyxml2::XMLElement * elem) = 0;

    virtual bool FromXmlAttribute(const tinyxml2::XMLAttribute * attr) = 0;

    /// @brief 返回配置参数值的类型名称
    [[nodiscard]] virtual std::string GetTypeName() const = 0;

protected:
    std::string m_name;        // 配置参数的名称
    std::string m_description; // 配置参数的描述
};





/// 特化：
///     ConfigVar<LogXmlConfig>
///     ConfigVar<RemoteXmlConfig>
///     ConfigVar<AppXmlConfig>
template<class T>
class ConfigVar final : public ConfigVarBase {
public:
    using ptr = std::shared_ptr<ConfigVar>;
    using OnChangeCallback = std::function<void (const T & old_value, const T new_value)> ;

    /**
     * @brief 通过参数名,参数值,描述构造ConfigVar
     * @param[in] name 参数名称有效字符为[0-9a-z_.]
     * @param[in] default_value 参数的默认值
     * @param[in] description 参数的描述
     */
    ConfigVar(const std::string& name ,const T& default_value ,const std::string& description = "") :
        ConfigVarBase(name, description), m_val(default_value) { }

    ~ConfigVar() override = default;

    /**
     * @brief 从 XML 转成参数的值
     * @exception 当转换失败抛出异常
     */
    bool
    FromXmlElement(const tinyxml2::XMLElement *elem) override
    {
        if(!elem) return false;
        if constexpr (std::is_class_v<T> && !std::is_same_v<T, std::string>)
        {
            try {
                SetValue(XmlElementTo<T>{}(elem));
                if(elem->GetText())
                    m_description = elem->GetText();
                return true;
            } catch (std::exception &e) {
                std::cerr << std::format("ConfigVar::FFromXmlElement exception {} convert: XMLElement to {}\n",
                                         e.what(), GetTypeName());
                std::terminate();
            }
        }
        return false;
    }

    bool
    FromXmlAttribute(const tinyxml2::XMLAttribute *attr) override
    {
        if(!attr) return false;
        if constexpr (!std::is_class_v<T> || std::is_same_v<T, std::string>)
        {
            try {
                SetValue(XmlAttributeTo<T>(attr));
                return true;
            } catch (std::exception& e) {
                std::cerr << std::format("ConfigVar::FromXmlAttribute exception {} convert: XMLAttribute to {}, m_TypeName={} - {}\n",
                                         e.what(), GetTypeName(), m_name, m_val);
                std::terminate();
            }
        }
        return false;
    }

    /**
     * @brief 获取当前参数的值
     */
    const T &
    GetValue() {
        util::ReadLockGuard lock(m_mutex);
        return m_val;
    }

    /**
     * @brief 设置当前参数的值
     * @details 如果参数的值有发生变化,则通知对应的注册回调函数
     */
    void
    SetValue(const T &v) {
        {
            util::ReadLockGuard lock(m_mutex);
            // if(v == m_val) {
            //     return;
            // }
            for (auto &i: m_cbs) {
                i.second(m_val, v);
            }
        }
        util::WriteLockGuard lock(m_mutex);
        m_val = v;
    }

    /**
     * @brief 返回参数值的类型名称(typeinfo)
     */
    [[nodiscard]] std::string GetTypeName() const override
    {
        return util::TypeToName<T>();
    }

    /**
     * @brief 添加变化回调函数
     * @return 返回该回调函数对应的唯一id,用于删除回调
     */
    uint64_t
    AddListener(OnChangeCallback cb) {
        static uint64_t s_fun_id = 0;
        util::WriteLockGuard lock(m_mutex);
        ++s_fun_id;
        m_cbs[s_fun_id] = cb;
        return s_fun_id;
    }

    /**
     * @brief 删除回调函数
     * @param[in] key 回调函数的唯一id
     */
    void
    DelListener(uint64_t key) {
        util::WriteLockGuard lock(m_mutex);
        m_cbs.erase(key);
    }

    /**
     * @brief 获取回调函数
     * @param[in] key 回调函数的唯一id
     * @return 如果存在返回对应的回调函数,否则返回nullptr
     */
    OnChangeCallback
    GetListener(uint64_t key) {
        util::ReadLockGuard lock(m_mutex);
        auto it = m_cbs.find(key);
        return it == m_cbs.end() ? nullptr : it->second;
    }

    /**
     * @brief 清理所有的回调函数
     */
    void
    ClearListener() {
        util::WriteLockGuard lock(m_mutex);
        m_cbs.clear();
    }

private:
    mutable util::RWMutex m_mutex;
    T m_val;
    //变更回调函数组, uint64_t key,要求唯一，一般可以用hash
    std::unordered_map <uint64_t, OnChangeCallback> m_cbs;
};



//! 必须为static类，这样才能在别的配置项的定义文件中调用LookUpOrAdd()添加配置项，以达到解耦的目的
class ConfigManagerBase
{
public:
    ///@brief 查找配置项，返回配置项基类指针
    static ConfigVarBase::ptr
    LookUpBase(const std::string & name)
    {
        util::ReadLockGuard lock(getMutex());
        const auto it = GetConfigVarMap().find(name);
        return it != GetConfigVarMap().end() ? it->second : nullptr ;
    }

    ///@brief 查找配置项，若不存在则新建配置项并赋予默认值default_value和描述信息description
    template<class T>
    static ConfigVar<T>::ptr
    LookUpOrAdd(const std::string & name, const T & default_value, const std::string& description = "")
    {
        util::WriteLockGuard lock{getMutex()};
        const auto it = GetConfigVarMap().find(name);

        // lookup
        if(it != GetConfigVarMap().end())
        {
            auto var = std::dynamic_pointer_cast<ConfigVar<T>>(it->second);
            if(var == nullptr) {
                std::cerr << "ConfigVar::LookUpOrAdd配置项转换失败，" << " m_TypeName=" << name << std::endl;
                return nullptr;
            }
            return var;
        }

        // add
        // 名称需要合法：只准字母和数字以及级别符.
        if(name.find_first_not_of("abcdefghikjlmnopqrstuvwxyz._012345678") != std::string::npos) {
            std::cerr << "Lookup m_TypeName invalid：" << name;
            throw std::invalid_argument(name);
        }

        //! 当不在T定义的.cpp文件中使用LookUpOrAdd<T>时，因为T被指定了，
        //! 所以fromXmlElement<T>中的XmlElementTo<T>也需要其对应声明
        //! 然而XmlElementTo<T>只在T定义的.cpp文件中定义，
        //! 因此LookUpOrAdd<T>只能在T定义的文件中使用，否则会在链接时报undefined reference错误
        //! 但是这样似乎也挺好，使得外部不能新增配置项，只能在对应的配置定义文件中新增配置项
        auto var = std::make_shared<ConfigVar<T>>(name, default_value, description);
        GetConfigVarMap()[name] = var;
        return var;
    }

    ///@brief 按名称查找配置项，返回配置项指针，若查找不到则返回空指针
    template<class T>
    static ConfigVar<T>::ptr
    LookUp(const std::string & name)
    {
        util::ReadLockGuard lock(getMutex());
        const auto it = GetConfigVarMap().find(name);
        return it == GetConfigVarMap().end() ? nullptr : std::dynamic_pointer_cast<ConfigVar<T>>(it->second);
    }

    ///@brief 静态bool成员变量，用于判断配置文件是否读取完毕
    static bool &
    GetIsLoaded()
    {
        static bool s_is_all_loaded = false;
        return s_is_all_loaded;
    }

    static void
    SetIsLoaded(const bool val)
    {
        GetIsLoaded() = val;
    }

    ///@brief 获取配置文件的路径
    ///@return 返回局部静态变量的引用
    static std::vector<std::filesystem::path> &
    GetAllFilePath()
    {
        static std::vector<std::filesystem::path>  s_config_file_paths;
        return s_config_file_paths;
    }

    static void
    AddFilePath(const std::filesystem::path& config_file_path)
    {
        GetAllFilePath().emplace(GetAllFilePath().begin(), config_file_path);
    }

private:
    /**
     * @brief 保存所有配置项的map，保存root.log.logger[m_TypeName]的属性和root.log的结点
     *  结点使用`.`表示级别顺序，属性用`[属性名]`表示
     *       结点的名字为：root.log.logger
     *       属性的名字为：root.log.logger[m_TypeName]
     */
    static std::unordered_map <std::string, ConfigVarBase::ptr> &
    GetConfigVarMap()
    {
        static std::unordered_map <std::string, ConfigVarBase::ptr> s_configvar_map;
        return s_configvar_map;
    }

    ///@brief 静态成员变量：读写锁，用于控制好配置项map的读写互斥访问
    ///@return 返回局部静态变量的引用
    static util::RWMutex &
    getMutex()
    {
        static util::RWMutex s_mutex;
        return s_mutex;
    }
};




class ConfigManager: public ConfigManagerBase
{
    using NodeContainer = std::vector<std::pair<std::string, void *>>;
public:
    ///@brief 读取配置文件，若已读取，则再次读取
    static void LoadXmlConfigs();

private:
    ///@brief 读取配置文件，返回配置文件的root结点
    static auto read_root(tinyxml2::XMLDocument&) -> tinyxml2::XMLElement*;

    /// @brief 将XML结点扁平化：将XML文件解析为 m_TypeName: TiXmlBase* 的形式
    static void traverse_xml_nodes(const std::string & prefix, tinyxml2::XMLElement * elem, NodeContainer & all_nodes);

    /// @brief 打印读取到的xml结点到控制台
    static void print_all_xml_nodes(const NodeContainer & all_nodes);

    /// @brief 将XML文件解析为 {m_TypeName: 内部存储类} 的形式
    static void parse_all_xml_nodes(const NodeContainer & all_nodes);
};


}

