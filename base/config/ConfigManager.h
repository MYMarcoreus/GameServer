#pragma warning(disable:4068)
#pragma clang diagnostic push
#pragma ide diagnostic ignored "OCUnusedGlobalDeclarationInspection"
#ifndef ____CONFIGMANAGER_H
#define ____CONFIGMANAGER_H

#include "Singleton.h"
#include "tinyxml/tinyxml2.h"
#include "RWLock.h"
#include "util_functions.h"

#include <tuple>
#include <string>
#include <unordered_map>
#include <algorithm>
#include <vector>
#include <iostream>
#include <memory>
#include <sstream>
#include <mutex>
#include <set>
#include <map>
#include <utility>
#include <filesystem>
#include <functional>
#include <cassert>

using namespace tinyxml2;

namespace yy::config {

///@brief 节点属性转换为C++的类型变量
template<typename T>
T XmlAttributeTo(const XMLAttribute* xml_attr)
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
        int ret = xml_attr->QueryBoolValue(&attr_val);
        if (ret == XML_WRONG_ATTRIBUTE_TYPE) {
            std::cerr << "属性[" << xml_attr->Name() << "]属性不为布尔类型！\n";
            throw std::bad_cast();
        }
        return attr_val;
    }
    //! 浮点数属性
    else
    if constexpr (std::is_floating_point_v<T>) {
        double attr_val;
        int ret = xml_attr->QueryDoubleValue(&attr_val);
        if (ret == XML_WRONG_ATTRIBUTE_TYPE) {
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
            int ret = xml_attr->QueryUnsignedValue(&attr_val);
            if (ret == XML_WRONG_ATTRIBUTE_TYPE) {
                std::cerr << "属性[" << xml_attr->Name() << "]属性不为无符号整型！\n";
                throw std::bad_cast();
            }
            return static_cast<T>(attr_val);
        }
        //! 带符号整数
        else
        if constexpr (std::is_signed_v<T>) {
            int attr_val;
            int ret = xml_attr->QueryIntValue(&attr_val);
            if (ret == XML_WRONG_ATTRIBUTE_TYPE) {
                std::cerr << "属性[" << xml_attr->Name() << "]属性不为整型！\n";
                throw std::bad_cast();
            }
            return static_cast<T>(attr_val);
        }
    } else {
        static_assert(sizeof(T) == 0, "XmlAttributeTo 不支持该类型");
    }
}





template<typename T>
class XmlElementTo
{
public:
    T operator()(const XMLElement *xml_remote_node);
};

///@brief 一个结点下面有多个子节点T，这些结点可以重复
template<typename T>
class XmlElementTo<std::vector<T>>
{
public:
    std::vector<T> operator()(const XMLElement *xml_parent)
    {
        std::vector<T> children;
        for (auto xml_child = xml_parent->FirstChildElement(); xml_child; xml_child = xml_child->NextSiblingElement()) {
            children.emplace_back(XmlElementTo<T>{}(xml_child));
        }
        return children;
    }
};

///@brief 一个结点下面有多个子节点T，这些结点不能重复
template<typename T>
class XmlElementTo<std::set<T>>
{
public:
    std::set<T> operator()(const XMLElement *xml_parent)
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
        std::transform(m_name.begin(), m_name.end(), m_name.begin(), ::tolower);
    }

    /// @brief 析构函数
    virtual ~ConfigVarBase() = default;

    /// @brief 返回配置参数名称
    [[nodiscard]] const std::string& GetName() const { return m_name;}

    /// @brief 返回配置参数的描述
    [[nodiscard]] const std::string& GetDescription() const { return m_description;}

    /// @brief 转成字符串
    // virtual std::string toXml() = 0;

    /// @brief 初始化值
    virtual bool FromXmlElement(const XMLElement *elem) = 0;

    virtual bool FromXmlAttribute(const XMLAttribute *attr) = 0;

    /// @brief 返回配置参数值的类型名称
    [[nodiscard]] virtual std::string GetTypeName() const = 0;

protected:
    std::string m_name;        // 配置参数的名称
    std::string m_description; // 配置参数的描述
};







template<class T>
class ConfigVar : public ConfigVarBase {
public:
    using ptr = std::shared_ptr<ConfigVar<T>>;
    using OnChangeCallback = std::function<void (const T & old_value, const T new_value)> ;

    /**
     * @brief 通过参数名,参数值,描述构造ConfigVar
     * @param[in] name 参数名称有效字符为[0-9a-z_.]
     * @param[in] default_value 参数的默认值
     * @param[in] description 参数的描述
     */
    ConfigVar(const std::string& name ,const T& default_value ,const std::string& description = "")
            :ConfigVarBase(name, description), m_val(default_value) { }

    ~ConfigVar() override = default;

    /**
     * @brief 将参数值转换成YAML String
     * @exception 当转换失败抛出异常
     */
    // XMLElement toXml() override {
    //     try {
    //         //return boost::lexical_cast<std::string>(m_val);
    //         util::ReadLockGuard lock(m_mutex);
    //         return ToStr()(m_val);
    //     } catch (std::exception& e) {
    //         std::cerr << "ConfigVar::toString exception "
    //                   << e.what() << " convert: " << TypeToName<T>() << " to string"
    //                   << " m_TypeName=" << m_name;
    //     }
    //     return "";
    // }

    /**
     * @brief 从 XML 转成参数的值
     * @exception 当转换失败抛出异常
     */
    bool
    FromXmlElement(const XMLElement *elem) override {
        if(!elem) return false;

        if constexpr (std::is_class_v<T> && !std::is_same_v<T, std::string>) {
            try {
                SetValue(XmlElementTo<T>{}(elem));
                if(elem->GetText())
                    m_description = elem->GetText();
                return true;
            } catch (std::exception &e) {
                std::cerr << "ConfigVar::FFromXmlElement exception "
                          << e.what() << " convert: XMLElement to " << GetTypeName() << std::endl;
            }
        }
        return false;
    }

    bool
    FromXmlAttribute(const XMLAttribute *attr) override
    {
        if(!attr) return false;
        if constexpr (!std::is_class_v<T> || std::is_same_v<T, std::string>)
        {
            try {
                SetValue(XmlAttributeTo<T>(attr));
                return true;
            } catch (std::exception& e) {
                std::cerr << "ConfigVar::FromXmlAttribute exception "
                          << e.what() << " convert: XMLAttribute to " << GetTypeName()
                          << ", m_TypeName=" << m_name << " - " << m_val << std::endl;
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
    [[nodiscard]] std::string GetTypeName() const override { return util::TypeToName<T>(); }

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
    mutable util::RWLock m_mutex;
    T m_val;
    //变更回调函数组, uint64_t key,要求唯一，一般可以用hash
    std::map<uint64_t, OnChangeCallback> m_cbs;
};








//! 必须为static类，这样才能在别的配置项的定义文件中调用AddConfigVar()添加配置项，以达到解耦的目的
class ConfigManager
{
public:
    ///@brief 查找配置项，返回配置项基类指针
    static ConfigVarBase::ptr
    LookUpBase(const std::string & name)
    {
        util::ReadLockGuard lock(getMutex());
        auto it = GetConfigVarMap().find(name);
        return it != GetConfigVarMap().end() ? it->second : nullptr ;
    }

    ///@brief 查找配置项，若不存在则新建配置项并赋予默认值default_value和描述信息description
    template<class T>
    static typename ConfigVar<T>::ptr
    LookUpOrAdd(const std::string & name, const T & default_value, const std::string& description = "")
    {
        util::WriteLockGuard lock{getMutex()};
        auto it = GetConfigVarMap().find(name);

        // lookup
        if(it != GetConfigVarMap().end())
        {
            auto var = std::dynamic_pointer_cast<ConfigVar<T>>(it->second);
            if(var) {
                return var;
            } else {
                std::cerr << "ConfigVar::LookUpOrAdd配置项转换失败，" << " m_TypeName=" << name << std::endl;
                return nullptr;
            }
        }
        // add
        else {
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
            typename ConfigVar<T>::ptr var{new ConfigVar<T>(name, default_value, description)};
            GetConfigVarMap()[name] = var;
            return var;
        }
    }

    ///@brief 按名称查找配置项，返回配置项指针，若查找不到则返回空指针
    template<class T>
    static typename ConfigVar<T>::ptr
    LookUp(const std::string & name)
    {
        util::ReadLockGuard lock(getMutex());
        auto it = GetConfigVarMap().find(name);
        return it == GetConfigVarMap().end() ? nullptr : std::dynamic_pointer_cast<ConfigVar<T>>(it->second);
    }

    ///@brief 静态bool成员变量，用于判断配置文件是否读取完毕
    static bool &
    GetIsLoaded() {
        static bool s_is_all_loaded = false;
        return s_is_all_loaded;
    }

    static void
    SetIsLoaded(bool val) { GetIsLoaded() = val; }

    ///@brief 获取配置文件的路径
    ///@return 返回局部静态变量的引用
    static std::filesystem::path &
    GetFilePath() {
        static std::filesystem::path s_config_file_path;
        return s_config_file_path;
    }

    static void
    SetFilePath(std::filesystem::path config_file_path) { GetFilePath() = config_file_path; }

    ///@brief 读取配置文件，若已读取，则再次读取
    static void
    LoadConfigs()
    {
        tinyxml2::XMLDocument  xml_doc;
        XMLElement * root_elem = read_root(xml_doc);

        std::vector<std::pair<std::string, void *>> all_nodes;

        // 将XML文件解析为 m_TypeName: TiXmlBase*
        traverse_nodes("", root_elem, all_nodes);

        print_all_nodes(all_nodes);

        // 将XML文件解析为 m_TypeName: 内部存储类
        parse_all_nodes(all_nodes);

        for(auto && it: GetConfigVarMap())
        {
            std::cout << it.first << ":: " << it.second->GetTypeName() << std::endl;
        }

        SetIsLoaded(true);
        std::cout << "所有配置读取完毕！\n";
    }



private:
    /**
     * @brief 保存所有配置项的map，保存root.log.logger[m_TypeName]的属性和root.log的结点
     *  结点使用`.`表示级别顺序，属性用`[属性名]`表示
     *       结点的名字为：root.log.logger
     *       属性的名字为：root.log.logger[m_TypeName]
     */
    static std::map<std::string, ConfigVarBase::ptr> &
    GetConfigVarMap()
    {
        static typename std::map<std::string, ConfigVarBase::ptr> s_configvar_map;
        return (s_configvar_map);
    }

    ///@brief 静态成员变量：读写锁，用于控制好配置项map的读写互斥访问
    ///@return 返回局部静态变量的引用
    static util::RWLock &
    getMutex() {
        static util::RWLock s_mutex;
        return s_mutex;
    }

    ///@brief 读取配置文件，返回配置文件的root结点
    static XMLElement *
    read_root(tinyxml2::XMLDocument & xml_doc)
    {
        XMLElement * root_elem;

        // 若已读取，则清空再读取
        if(!xml_doc.RootElement()) {
            xml_doc.Clear();
        }

        // 读取路径为GetConfigFilePath()的配置文件，若读取失败，则查找默认路径的xml文件
        XMLError ret = xml_doc.LoadFile(GetFilePath().string().c_str());
        if(ret != XML_SUCCESS ) {
            for (auto path: kConfigPaths) {
                if (xml_doc.LoadFile(path) == XML_SUCCESS) {
                    SetFilePath(std::filesystem::absolute(path));
                    break;
                }
            }
        }

        // 没有配置文件：结束程序
        if(GetFilePath().empty()) {
            std::cerr << "未找到Xml配置文件，请在程序所在目录创建configs.xml配置文件\n";
            std::terminate();
        }
        std::cout << "读取到xml文件：" << GetFilePath() << std::endl;

        // 读取root元素
        root_elem = xml_doc.RootElement();
        if (!root_elem) {
            std::cerr << "未找到Xml文件的root元素\n";
            std::terminate();
        }

        return root_elem;
    }

    /// @brief 将XML结点扁平化：将XML文件解析为 m_TypeName: TiXmlBase* 的形式
    static void
    traverse_nodes(const std::string & prefix, XMLElement * elem, // NOLINT(misc-no-recursion)
                   std::vector<std::pair<std::string, void *>> & all_nodes)
    {
        // 添加当前结点：root.log.logger（对于节点其Value等于Name）
        std::string elem_name = prefix.empty() ? elem->Name() : (prefix + "." + elem->Name());
        all_nodes.emplace_back(elem_name, elem);

        // 添加当前结点的属性：root.log.logger[m_TypeName]，是叶子节点（对于属性则分为Name: Value的形式）
        for(auto attr = elem->FirstAttribute(); attr ; attr = attr->Next())
        {
            std::string attr_name = elem_name + "[" + attr->Name() + "]";
            all_nodes.emplace_back(attr_name, (void*)attr);
        }

        // 遍历子节点
        for (auto child_elem = elem->FirstChildElement(); child_elem; child_elem = child_elem->NextSiblingElement()) {
            traverse_nodes(elem_name, child_elem, all_nodes);
        }
    }

    static void
    print_all_nodes(std::vector<std::pair<std::string, void *>> & all_nodes)
    {
        for(const auto& i: all_nodes)
        {
            auto name = i.first;
            auto nTab = std::count(name.begin(), name.end(), '.');

            if(name.back() == ']') {
                auto attr = (const XMLAttribute *)(i.second);
                assert(attr != nullptr);
                auto tabs = std::string(nTab+1, '\t');
                if(attr != nullptr) {
                    std::cout << tabs << name << ": " << attr->Value() << std::endl;
                }
            } else {
                auto elem = (const XMLElement *)(i.second);
                auto tabs = std::string(nTab, '\t');
                std::cout << tabs << name << std::endl;
            }
        }
    }

    /// @brief 将XML文件解析为 {m_TypeName: 内部存储类} 的形式
    static void
    parse_all_nodes(std::vector<std::pair<std::string, void *>> & all_nodes)
    {
        for(const auto& i: all_nodes)
        {
            auto name = i.first;
            const auto xml_base = i.second;

            std::transform(name.begin(), name.end(), name.begin(), ::tolower);
            auto var = LookUpBase(name);

            if(var) {
                // 属性
                if(name.back() == ']') {
                    var->FromXmlAttribute((const XMLAttribute *) xml_base);
                    std::cout << "属性已读取完毕：" << name << "，类型为：" << var->GetTypeName() << std::endl;
                } else {
                    var->FromXmlElement((const XMLElement *) xml_base);
                    std::cout << "结点已读取完毕：" << name << "，类型为：" << var->GetTypeName() << std::endl;
                }
            }
        }
    }

    // 配置文件默认路径：通过可执行文件的相对路径寻找
    static const char kConfigPaths[4][128];
};





}


#endif //____CONFIGMANAGER_H

#pragma clang diagnostic pop

