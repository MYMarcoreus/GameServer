#include "ConfigManager.h"

using namespace tinyxml2;

namespace yy::config {

std::filesystem::path ConfigManager::GetProjectRoot()
{
    // 优先使用可执行文件自身路径（Linux 下 /proc/self/exe），避免依赖工作目录
    std::filesystem::path exe_path;
#if defined(__linux__)
    try {
        exe_path = std::filesystem::canonical("/proc/self/exe");
    } catch (...) {
        exe_path = std::filesystem::current_path();
    }
#else
    exe_path = std::filesystem::current_path();
#endif

    // 从可执行文件所在目录向上查找项目根（包含 vcpkg.json 或 CMakeLists.txt 的目录）
    for (auto dir = exe_path.parent_path(); !dir.empty() && dir != dir.root_path(); dir = dir.parent_path()) {
        if (std::filesystem::exists(dir / "vcpkg.json") ||
            std::filesystem::exists(dir / "CMakeLists.txt")) {
            return dir;
        }
    }

    // 回退：可执行文件所在目录的上一级
    return exe_path.parent_path().parent_path();
}

void ConfigManager::LoadXmlConfigs()
{
    XMLDocument  xml_doc;
    XMLElement * root_elem = read_root(xml_doc);

    NodeContainer all_xml_nodes;

    // 将XML文件解析为 typeName: TiXmlBase*
    traverse_xml_nodes("", root_elem, all_xml_nodes);

    print_all_xml_nodes(all_xml_nodes);

    // 将XML文件解析为 m_TypeName: 内部存储类
    parse_all_xml_nodes(all_xml_nodes);

    SetIsLoaded(true);
    std::cout << "所有配置读取完毕！\n";
}


XMLElement* ConfigManager::read_root(XMLDocument& xml_doc)
{
    // 若已读取，则清空再读取
    if(!xml_doc.RootElement()) {
        xml_doc.Clear();
    }

    // 读取路径为GetConfigFilePath()的配置文件，若读取失败，则查找默认路径的xml文件
    std::filesystem::path xml_file_path{};
    for (const auto& file_path : GetAllFilePath())
    {
        const XMLError ret = xml_doc.LoadFile(file_path.string().c_str());
        if (ret == XML_SUCCESS) {
            xml_file_path = file_path;
            break;
        }
    }

    // 没有配置文件：结束程序
    if(xml_file_path.empty()) {
        std::cerr << "未找到Xml配置文件，请在程序所在目录创建configs.xml配置文件\n";
        std::terminate();
    }
    std::cout << "读取到xml文件：" << xml_file_path << std::endl;

    // 读取root元素
    XMLElement* root_elem = xml_doc.RootElement();
    if (!root_elem) {
        std::cerr << "未找到Xml文件的root元素\n";
        std::terminate();
    }

    return root_elem;
}

void ConfigManager::traverse_xml_nodes(const std::string& prefix, XMLElement* elem, NodeContainer& all_nodes)
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
        traverse_xml_nodes(elem_name, child_elem, all_nodes);
    }
}

void ConfigManager::print_all_xml_nodes(const NodeContainer& all_nodes)
{
    for(const auto& [name, attr_or_elem]: all_nodes)
    {
        const auto nTab = std::ranges::count(name, '.');

        if(name.back() == ']') {
            const auto attr = static_cast<const XMLAttribute*>(attr_or_elem);
            assert(attr != nullptr);
            auto tabs = std::string(nTab+1, '\t');
            if(attr != nullptr) {
                std::cout << tabs << name << ": " << attr->Value() << std::endl;
            }
        } else {
            auto elem = static_cast<const XMLElement*>(attr_or_elem);
            auto tabs = std::string(nTab, '\t');
            std::cout << tabs << name << std::endl;
        }
    }
}

void ConfigManager::parse_all_xml_nodes(const NodeContainer& all_nodes)
{
    for(auto [name, xml_base]: all_nodes)
    {
        std::ranges::transform(name, name.begin(), tolower);
        auto var = LookUpBase(name);

        if(var) {
            // 属性
            if(name.back() == ']') {
                var->FromXmlAttribute(static_cast<const XMLAttribute*>(xml_base));
                std::cout << "属性已读取完毕：" << name << "，类型为：" << var->GetTypeName() << std::endl;
            } else {
                var->FromXmlElement(static_cast<const XMLElement*>(xml_base));
                std::cout << "结点已读取完毕：" << name << "，类型为：" << var->GetTypeName() << std::endl;
            }
        }
    }
}
}
