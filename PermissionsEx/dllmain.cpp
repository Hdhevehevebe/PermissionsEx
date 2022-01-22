// dllmain.cpp : Определяет точку входа для приложения DLL.
#include <Windows.h>
#include <codecvt>
#include <locale>
#define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING
#include <EventAPI.h>
#include <LLAPI.h>
#include <MC/Actor.hpp>
#include <MC/Block.hpp>
#include <MC/CommandOrigin.hpp>
#include <MC/CommandOutput.hpp>
#include <MC/CommandPosition.hpp>
#include <MC/CommandRegistry.hpp>
#include <MC/CommandParameterData.hpp>
#include <MC/ComponentItem.hpp>
#include <MC/CompoundTag.hpp>
#include <MC/HashedString.hpp>
#include <MC/Item.hpp>
#include <MC/ItemStack.hpp>
#include <MC/Level.hpp>
#include <MC/Mob.hpp>
#include <MC/MobEffect.hpp>
#include <MC/MobEffectInstance.hpp>
#include <MC/Player.hpp>
#include <MC/ServerPlayer.hpp>
#include <MC/Tag.hpp>
#include <MC/Types.hpp>
#include <MC/Dimension.hpp>
#include <MC/Container.hpp>
#include <MC/Block.hpp>
#include <MC/BlockSource.hpp>
#include <RegCommandAPI.h>
#include <MC/SerializedSkin.hpp>
#include <MC/BinaryStream.hpp>
#include <ServerAPI.h>
#include <MC/GameTypeConv.hpp>
#include <iostream>
#pragma warning(push)
#pragma warning(disable : 4996)
#pragma warning(disable : 4251)
#pragma warning(disable : 4275)
#include<third-party/yaml-cpp/yaml.h>
#pragma warning(pop)
#include <algorithm>
#include <fstream>
#include <iostream>
#include <MC/PropertiesSettings.hpp>
#include <regex>
using namespace std::filesystem;

#pragma comment(lib, "bedrock_server_api.lib")
#pragma comment(lib, "bedrock_server_var.lib")
#pragma comment(lib, "LiteLoader.lib")
#pragma comment(lib, "yaml-cpp.lib")
#pragma comment(lib, "SymDBHelper.lib")

using namespace std;

std::wstring to_wstring(const std::string& str,
    const std::locale& loc = std::locale())
{
    std::vector<wchar_t> buf(str.size());
    std::use_facet<std::ctype<wchar_t>>(loc).widen(str.data(),
        str.data() + str.size(),
        buf.data());
    return std::wstring(buf.data(), buf.size());
}

std::string utf8_encode(const std::wstring& wstr)
{
    if (wstr.empty()) return std::string();
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
    return strTo;
}

string utf8_to_string(const char* utf8str, const locale& loc)
{
    // UTF-8 to wstring
    wstring_convert<codecvt_utf8<wchar_t>> wconv;
    wstring wstr = wconv.from_bytes(utf8str);
    // wstring to string
    vector<char> buf(wstr.size());
    use_facet<ctype<wchar_t>>(loc).narrow(wstr.data(), wstr.data() + wstr.size(), '?', buf.data());
    return string(buf.data(), buf.size());
}

BOOL APIENTRY DllMain(HMODULE hModule,
    DWORD  ul_reason_for_call,
    LPVOID lpReserved
)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        LL::registerPlugin("PermissionsEx 1.0", "Порт PermissionsEx с Bukkit под LiteLoader 2.0", LL::Version(0, 0, 0, LL::Version::Dev));
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

struct ChatConfig
{
    bool enabled;
    bool range_mode;
    int chat_range;
    string display_name_format;
    string global_message_format;
    string message_format;
};

struct ModifyworldConfig
{
    bool informPlayers;
    bool whitelist = false;
    vector<string> messages;
};

class World
{
public:
    string name;
    vector<string> permissions;
    string group;
    string prefix;
    string suffix;
   vector<string> inheritances;
};

class _Group
{
public:
    string name; //имя группы
    string prefix;
    string suffix;
    vector<string> perms;       //права
    bool is_default;  //установлен ли по умолчанию
    bool build; //строительство вкл/выкл
    vector<string> inheritances; //группу,которую будем наследовать
    vector<World> worlds; 
};

class _Groups {
public:
    vector<_Group> groups; //группы
};

class _User {
public:
    string nickname; //ник
    string prefix;
    string suffix;
    vector<string> groups;       //группа игрока
    vector<string> permissions; //права игрока
    vector<World> worlds;
};

class Users {
public:
    vector<_User> users; //игроки
};

namespace YAML
{
    template <>
    struct convert<ChatConfig>
    {
        static Node encode(const ChatConfig& rhs)
        {
            Node node;
            node["chat-range"] = rhs.chat_range;
            node["display-name-format"] = rhs.display_name_format;
            node["global-message-format"] = rhs.global_message_format;
            node["enabled"] = rhs.enabled;
            node["message-format"] = rhs.message_format;
            node["ranged-mode"] = rhs.range_mode;
            return node;
        }
        static bool decode(const Node& node,ChatConfig& rhs)
        {
            using namespace std;
            rhs.chat_range = node["chat-range"].as<int>();
            rhs.display_name_format = node["display-name-format"].as<string>();
            rhs.global_message_format= node["global-message-format"].as<string>();
            rhs.enabled = node["enabled"].as<bool>();
            rhs.message_format = node["message-format"].as<string>();
            rhs.range_mode = node["ranged-mode"].as<bool>();
            return true;
        }
    };
} // namespace YAML

namespace YAML
{
    template <>
    struct convert<ModifyworldConfig>
    {
        static Node encode(const ModifyworldConfig& rhs)
        {
            Node node;
            node["informPlayers"] = rhs.informPlayers;
            if (rhs.messages.size() == 0)
            {
                node["messages"] = {};
            }
            node["messages"] = rhs.messages;
            node["whitelist"] = rhs.whitelist;
            return node;
        }
        static bool decode(const Node& node, ModifyworldConfig& rhs)
        {
            using namespace std;
            rhs.informPlayers = node["informPlayers"].as<bool>();
            rhs.messages = node["messages"].as<vector<string>>();
            rhs.whitelist = node["whitelist"].as<bool>();
            return true;
        }
    };
} // namespace YAML

namespace YAML
{
    template <>
    struct convert<World>
    {
        static Node encode(const World& rhs)
        {
            Node node;
            node[rhs.name] = YAML::Node(YAML::NodeType::Map);;
            if (rhs.permissions.size() == 0)
            {
                node[rhs.name]["permissions"] = {};
            }
            node[rhs.name]["permissions"] = rhs.permissions;
            node[rhs.name]["group"] = rhs.group;
            node[rhs.name]["prefix"] = rhs.prefix;
            node[rhs.name]["suffix"] = rhs.suffix;
            if (rhs.inheritances.size() == 0)
            {
                node[rhs.name]["inheritance"] = {};
            }
            node[rhs.name]["inheritance"] = rhs.inheritances;
            return node;
        }
        static bool decode(const Node& node, World& rhs)
        {
            using namespace std;
            string name;
            for (const auto& kv : node) {
                if (kv.first.as<std::string>() == "")
                {
                    continue;
                }
                name = kv.first.as<std::string>();
                break;
            }
            rhs.name = name;
            rhs.permissions = node[name]["permissions"].as<vector<string>>();
            rhs.group = node[name]["group"].as<string>();
            rhs.prefix = node[name]["prefix"].as<string>();
            rhs.suffix = node[name]["suffix"].as<string>();
            rhs.inheritances = node[name]["inheritance"].as<vector<string>>();
            return true;
        }
    };
} // namespace YAML

namespace YAML
{
    template <>
    struct convert<_Group>
    {
        static Node encode(const _Group& rhs)
        {
            Node node;
            node[rhs.name] = YAML::Node(YAML::NodeType::Map);
            node[rhs.name]["prefix"] = rhs.prefix;
            node[rhs.name]["suffix"] = rhs.suffix;
            if (rhs.inheritances.size() == 0)
            {
                node[rhs.name]["inheritance"] = {};
            }
            node[rhs.name]["inheritance"] = rhs.inheritances;
            node[rhs.name]["default"] = rhs.is_default;
            node[rhs.name]["build"] = rhs.build;
            if (rhs.perms.size() == 0)
            {
                node[rhs.name]["permissions"] = vector<string>(0);
            }
            node[rhs.name]["permissions"] = rhs.perms;
            node[rhs.name]["worlds"] = YAML::Node(YAML::NodeType::Map);
            if (rhs.worlds.size() == 0)
            {
                node[rhs.name]["worlds"] = {};
            }
            for (auto v : rhs.worlds)
            {
                node[rhs.name]["worlds"][v.name]["prefix"] = v.prefix;
                node[rhs.name]["worlds"][v.name]["suffix"] = v.suffix;
                if (v.permissions.size() == 0)
                {
                    node[rhs.name]["worlds"][v.name]["permissions"] = vector<string>(0);
                }
                node[rhs.name]["worlds"][v.name]["permissions"] = v.permissions;
                node[rhs.name]["worlds"][v.name]["group"] = v.group;
                if (v.inheritances.size() == 0)
                {
                    node[rhs.name]["worlds"][v.name]["inheritance"] = vector<string>(0);
                }
                node[rhs.name]["worlds"][v.name]["inheritance"] = v.inheritances;
            }
            return node;
        }
        static bool decode(const Node& node, _Group& rhs)
        {
            using namespace std;
            string name;
            for (const auto& kv : node)
            {
                if (kv.first.as<std::string>() == "")
                {
                    continue;
                }
                name = kv.first.as<std::string>();
                break;
            }
            rhs.name = name;
            rhs.prefix = node[name]["prefix"].as<std::string>();
            rhs.suffix = node[name]["suffix"].as<std::string>();
            rhs.inheritances = node[name]["inheritance"].as<std::vector<string>>();
            rhs.is_default = node[name]["default"].as<bool>();
            rhs.build = node[name]["build"].as<bool>();
            rhs.perms = node[name]["permissions"].as<std::vector<string>>();
            string name1;
            for (const auto& kv : node[name]["worlds"])
            {
                if (kv.first.as<std::string>() == "")
                {
                    continue;
                }
                name1 = kv.first.as<std::string>();
                World world;
                world.name = name1;
                world.prefix = node[name]["worlds"][name1]["prefix"].as<string>();
                world.suffix = node[name]["worlds"][name1]["suffix"].as<string>();
                world.permissions = node[name]["worlds"][name1]["permissions"].as<vector<string>>();
                world.group = node[name]["worlds"][name1]["group"].as<string>();
                world.inheritances = node[name]["worlds"][name1]["inheritance"].as<vector<string>>();
                rhs.worlds.push_back(world);
            }
            return true;
        }
    };
} // namespace YAML

namespace YAML
{
    template <>
    struct convert<_User>
    {
        static Node encode(const _User& rhs)
        {
            Node node;
            node[rhs.nickname] = YAML::Node(YAML::NodeType::Map);
            node[rhs.nickname]["group"] = (rhs.groups);
            node[rhs.nickname]["prefix"] = (rhs.prefix);
            node[rhs.nickname]["suffix"] = (rhs.suffix);
            if (rhs.permissions.size() == 0)
            {
                node[rhs.nickname]["permissions"] = {};
            }
            node[rhs.nickname]["permissions"] = rhs.permissions;
            node[rhs.nickname]["worlds"] = YAML::Node(YAML::NodeType::Map);
            if (rhs.worlds.size() == 0)
            {
                node[rhs.nickname]["worlds"] = {};
            }
            for (auto v : rhs.worlds)
            {
                node[rhs.nickname]["worlds"][v.name]["prefix"] = v.prefix;
                node[rhs.nickname]["worlds"][v.name]["suffix"] = v.suffix;
                if (v.permissions.size() == 0)
                {
                    node[rhs.nickname]["worlds"][v.name]["permissions"] = vector<string>(0);
                }
                node[rhs.nickname]["worlds"][v.name]["permissions"] = v.permissions;
                node[rhs.nickname]["worlds"][v.name]["group"] = v.group;
                node[rhs.nickname]["worlds"][v.name]["inheritance"] = v.inheritances;
            }
            return node;
        }

        static bool decode(const Node& node, _User& rhs) {
            using namespace std;
            string name;
            for (const auto& kv : node) {
                if (kv.first.as<std::string>() == "")
                {
                    continue;
                }
                name = kv.first.as<std::string>();
                break;
            }
            rhs.nickname = name;
            rhs.groups = node[name]["group"].as<vector<string>>();
            rhs.prefix = node[name]["prefix"].as<string>();
            rhs.suffix = node[name]["suffix"].as<string>();
            rhs.permissions = node[name]["permissions"].as<vector<string>>();
            string name1;
            for (const auto& kv : node[name]["worlds"])
            {
                if (kv.first.as<std::string>() == "")
                {
                    continue;
                }
                name1 = kv.first.as<std::string>();
                World world;
                world.name = name1;
                world.prefix = node[name]["worlds"][name1]["prefix"].as<string>();
                world.suffix = node[name]["worlds"][name1]["suffix"].as<string>();
                world.permissions = node[name]["worlds"][name1]["permissions"].as<vector<string>>();
                world.group = node[name]["worlds"][name1]["group"].as<string>();
                world.inheritances = node[name]["worlds"][name1]["inheritance"].as<vector<string>>();
                rhs.worlds.push_back(world);
            }
            return true;
        }
    };
} // namespace YAML

_Group load_group(string name) {
    YAML::Node config = YAML::LoadFile("plugins/Permissions Ex/groups.yml");
    _Group group;
    using namespace std;
    for (const auto& p : config["groups"]) {
        _Group g = p.as<_Group>();
        if (g.name == name)
        {
            group = g;
            break;
        }
    }
    return group;
}

_User load_user(string nick) {
    YAML::Node config = YAML::LoadFile("plugins/Permissions Ex/users.yml");
    using namespace std;
    _User user;
    for (const auto& p : config["users"]) {
        _User us = p.as<_User>();
        if (nick == us.nickname)
        {
            user = us;
            break;
        }
    }
    return user;
}

vector<string> split(string s, string delimiter) {
    size_t         pos_start = 0, pos_end, delim_len = delimiter.length();
    string         token;
    vector<string> res;

    while ((pos_end = s.find(delimiter, pos_start)) != string::npos) {
        token = s.substr(pos_start, pos_end - pos_start);
        pos_start = pos_end + delim_len;
        res.push_back(token);
    }
    res.push_back(s.substr(pos_start));
    return res;
}

YAML::Node  config;
_Groups    groups;
YAML::Node config1;
Users      users;
YAML::Node chatconf;
ChatConfig chatconfig;
YAML::Node modworldconf;
ModifyworldConfig modwc;

bool debug_mode;

int chat_range = 0; //из конфига чата,радиус локального чата
bool chat_ranged,enabled; //on/off локальный чат и вкл/выкл ли вшитая модификация на чат
string display_name_format,global_message_format, message_format; //общий формат отображения ника,затем идёт фулл формат сообщения(формат ника + формат сообщения),и последнее это формат не в виде ник + соообщение,а просто сообщение

bool informPlayers; //из конфига Modifyworld: вкл/выкл оповещания игрока об изменении его группы,прав и т.д
bool whitelist; //белый список

bool checkPerm(string pl, string perm)
{
    _Groups    groups;
    Users      users;
    try
    {
        auto nodes = YAML::LoadFile("plugins/Permissions Ex/users.yml");
        for (const auto& p : nodes["users"])
        {
            users.users.push_back(p.as<_User>());
        }
        auto nodes1 = YAML::LoadFile("plugins/Permissions Ex/groups.yml");
        for (const auto& p : nodes1["groups"])
        {
            groups.groups.push_back(p.as<_Group>());
        }
        auto plain = pl;
        using namespace std;
        auto nick = split(plain, " ");
        string res_nick;
        for (auto n : nick)
        {
            for (auto v : users.users)
            {
                if (n == v.nickname)
                {
                    res_nick = n;
                    break;
                }
            }
        }
        auto pl1 = load_user(res_nick);
        _Group gr;
        for (auto xh : pl1.groups)
        {
            auto l = split(xh, ":");
            if (l.size() > 0)
                gr = load_group(l[0]);
            else
                gr = load_group(xh);
            for (auto v : pl1.permissions)
            {
                if (v == perm)
                {
                    return 1;
                }
            }
            for (auto v : gr.perms)
            {
                if (v == perm)
                {
                    return 1;
                }
            }
        }
    }
    catch (exception& e)
    {
        cerr << e.what() << endl;
    }
    return 0;
}

bool checkPermWorlds(string pl, string perm,string world)
{
    _Groups    groups;
    Users      users;
    auto nodes = YAML::LoadFile("plugins/Permissions Ex/users.yml");
    for (const auto& p : nodes["users"])
    {
        users.users.push_back(p.as<_User>());
    }
    auto nodes1 = YAML::LoadFile("plugins/Permissions Ex/groups.yml");
    for (const auto& p : nodes1["groups"])
    {
        groups.groups.push_back(p.as<_Group>());
    }
    auto plain = pl;
    using namespace std;
    auto nick = split(plain, " ");
    string res_nick;
    for (auto n : nick)
    {
        for (auto v : users.users)
        {
            if (n == v.nickname)
            {
                res_nick = n;
                break;
            }
        }
    }
    auto pl1 = load_user(res_nick);
    _Group gr;
    for (auto xh : pl1.groups)
    {
        auto l = split(xh, ":");
        if (l.size() > 0)
            gr = load_group(l[0]);
        else
            gr = load_group(xh);
        if (world == "OverWorld")
        {
            for (auto dim : pl1.worlds)
            {
                if (dim.name == "OverWorld")
                {
                    for (auto v : dim.permissions)
                    {
                        if (v == perm)
                        {
                            return 1;
                        }
                    }
                }
            }
        }
        else if (world == "Nether")
        {
            for (auto dim : pl1.worlds)
            {
                if (dim.name == "Nether")
                {
                    for (auto v : dim.permissions)
                    {
                        if (v == perm)
                        {
                            return 1;
                        }
                    }
                }
            }
        }
        else if (world == "End")
        {
            for (auto dim : pl1.worlds)
            {
                if (dim.name == "End")
                {
                    for (auto v : dim.permissions)
                    {
                        if (v == perm)
                        {
                            return 1;
                        }
                    }
                }
            }
        }
        if (world == "OverWorld")
        {
            for (auto dim : gr.worlds)
            {
                if (dim.name == "OverWorld")
                {
                    for (auto v : dim.permissions)
                    {
                        if (v == perm)
                        {
                            return 1;
                        }
                    }
                }
            }
        }
        else if (world == "Nether")
        {
            for (auto dim : gr.worlds)
            {
                if (dim.name == "Nether")
                {
                    for (auto v : dim.permissions)
                    {
                        if (v == perm)
                        {
                            return 1;
                        }
                    }
                }
            }
        }
        else if (world == "End")
        {
            for (auto dim : gr.worlds)
            {
                if (dim.name == "End")
                {
                    for (auto v : dim.permissions)
                    {
                        if (v == perm)
                        {
                            return 1;
                        }
                    }
                }
            }
        }
    }
    return 0;
}

string get_msg(string type)
{
    YAML::Node modworld_configuration = YAML::LoadFile("plugins/Permissions Ex/modifyworld.yml");
    ModifyworldConfig modworld = modworld_configuration.as<ModifyworldConfig>();
    for (auto v : modworld.messages)
    {
        auto vec = split(v, ":");
        if (type == vec[0])
            return vec[1];
    }
}

enum CommandParameterOption;

const std::string currentDateTime() {
    time_t     now = time(0);
    struct tm  tstruct;
    char       buf[80];
    tstruct = *localtime(&now);
    strftime(buf, sizeof(buf), "%Y.%m.%d %X", &tstruct);
    return buf;
}

class Pex : public Command
{
    enum class Operation
    {
        Toggle,
        User,
        Reload,
        Hierarcy,
        Users,
        Groups,
        Group,
        Worlds,
        World,
        Default,
        Set,
        Help
    } op;
    enum class Set_Operation
    {
        Default
    } set_op;
    enum class Default_Set_Operation
    {
        Group
    } def_set_op;
    enum class Default_Operation
    {
        Group
    } def_op;
    enum class Toggle_Operation
    {
        Debug
    } tg_op;
    enum class User_Operation
    {
        Prefix,
        Suffix,
        Delete,
        Add,
        Remove,
        Group,
        Check
    } us_op;
    enum class User_Group_Operation
    {
        Add,
        Remove,
        List,
        Set
    } us_gr_op;
    enum class Group_Operation
    {
        Prefix,
        Suffix,
        Create,
        Delete,
        Parents,
        Add,
        Remove,
        Timed,
        User,
        Users
    } gr_op;
    enum class Group_Parents_Operation
    {
        Set
    } gr_pr_op;
    enum class Group_Timed_Operation
    {
        Add,
        Remove
    } gr_tm_op;
    enum class Group_User_Operation
    {
        Add,
        Remove
    } gr_us_op;
    enum class World_Operation
    {
        Inherit
    } world_op;
    CommandSelector<Actor> player;
    string user_permission, group_permission, world, user_prefix, user_suffix, group_prefix, group_suffix,group,parent_world;
    int lifetime;
    string parent;
    int is_default = -1;
    bool is_list,is_add,is_remove,is_delete;
    string opval,opval1,opval2,opval3;
    bool is_parentset;
    bool is_setdefgroup;
    bool is_timeadd, is_timeremove,is_users,is_usradd,is_usrremove,is_inherit,is_defgr;
public:
    void execute(CommandOrigin const& ori, CommandOutput& output) const override
    {
       switch (op)
       {
       case Operation::Toggle:
       {
          switch (tg_op)
          {
           case Toggle_Operation::Debug:
           {
               string dim;
               if (ori.getDimension()->getDimensionId() == 0)
                   dim = "OverWorld";
               else if (ori.getDimension()->getDimensionId() == 1)
                   dim = "Nether";
               else if (ori.getDimension()->getDimensionId() == 2)
                   dim = "End";
               string error_msg = get_msg("permissionDenied");
               string perm = "permissions.manage";
               if (ori.getPermissionsLevel() == CommandPermissionLevel::Console)
               {
                   if (debug_mode == false)
                   {
                       debug_mode = true;
                       output.success(utf8_encode(L"[Permissions Ex]: Режим отладки активирован!"));
                       return;
                   }
                   else
                   {
                       debug_mode = false;
                       output.success(utf8_encode(L"[Permissions Ex]: Режим отладки дизактивирован!"));
                       return;
                   }
               }
               else if (checkPerm(ori.getPlayer()->getName(), perm) || checkPerm(ori.getPlayer()->getName(), "plugins.*") || checkPerm(ori.getPlayer()->getName(), "permissions.*") || checkPermWorlds(ori.getPlayer()->getName(), perm,dim) || checkPermWorlds(ori.getPlayer()->getName(), "plugins.*",dim) || checkPermWorlds(ori.getPlayer()->getName(), "permissions.*",dim))
               {
                   if (debug_mode == false)
                   {
                       debug_mode = true;
                       output.success(utf8_encode(L"[Permissions Ex]: Режим отладки активирован!"));
                       return;
                   }
                   else
                   {
                       debug_mode = false;
                       output.success(utf8_encode(L"[Permissions Ex]: Режим отладки дизактивирован!"));
                       return;
                   }
               }
               output.error(error_msg);
               return;
           }
         }
       }
       case Operation::Reload:
       {
           string dim;
           if (ori.getDimension()->getDimensionId() == 0)
               dim = "OverWorld";
           else if (ori.getDimension()->getDimensionId() == 1)
               dim = "Nether";
           else if (ori.getDimension()->getDimensionId() == 2)
               dim = "End";
           string error_msg = get_msg("permissionDenied");
           string perm = "permissions.manage.reload";
           if (ori.getPermissionsLevel() == CommandPermissionLevel::Console)
           {
               _Groups    groups;
               Users      users;
               auto nodes = YAML::LoadFile("plugins/Permissions Ex/users.yml");
               for (const auto& p : nodes["users"])
               {
                   users.users.push_back(p.as<_User>());
               }
               auto nodes1 = YAML::LoadFile("plugins/Permissions Ex/groups.yml");
               for (const auto& p : nodes1["groups"])
               {
                   groups.groups.push_back(p.as<_Group>());
               }
               config.reset();
               config1.reset();
               for (auto v : groups.groups)
                   config["groups"].push_back(v);
               for (auto v : users.users)
                   config1["users"].push_back(v);
               output.success(utf8_encode(L"[Permissions Ex]: Плагин успешно перезагружен!"));
               return;
           }
           else if (checkPerm(ori.getPlayer()->getName(), perm) || checkPerm(ori.getPlayer()->getName(), "plugins.*") || checkPerm(ori.getPlayer()->getName(), "permissions.*") || checkPermWorlds(ori.getPlayer()->getName(), perm, dim) || checkPermWorlds(ori.getPlayer()->getName(), "plugins.*", dim) || checkPermWorlds(ori.getPlayer()->getName(), "permissions.*", dim))
           {
               _Groups    groups;
               Users      users;
               auto nodes = YAML::LoadFile("plugins/Permissions Ex/users.yml");
               for (const auto& p : nodes["users"])
               {
                   users.users.push_back(p.as<_User>());
               }
               auto nodes1 = YAML::LoadFile("plugins/Permissions Ex/groups.yml");
               for (const auto& p : nodes1["groups"])
               {
                   groups.groups.push_back(p.as<_Group>());
               }
               config.reset();
               config1.reset();
               for (auto v : groups.groups)
                   config["groups"].push_back(v);
               for (auto v : users.users)
                   config1["users"].push_back(v);
               output.success(utf8_encode(L"[Permissions Ex]: Плагин успешно перезагружен!"));
               return;
           }
           output.error(error_msg);
           return;
       }
       case Operation::Hierarcy:
       {
           string dim;
           if (ori.getDimension()->getDimensionId() == 0)
               dim = "OverWorld";
           else if (ori.getDimension()->getDimensionId() == 1)
               dim = "Nether";
           else if (ori.getDimension()->getDimensionId() == 2)
               dim = "End";
           string error_msg = get_msg("permissionDenied");
           string perm = "permissions.manage.users";
           if (ori.getPermissionsLevel() == CommandPermissionLevel::Console)
           {
               _Groups    groups;
               auto nodes1 = YAML::LoadFile("plugins/Permissions Ex/groups.yml");
               for (const auto& p : nodes1["groups"])
               {
                   groups.groups.push_back(p.as<_Group>());
               }
               string outp = "[Permissions Ex]: Иерархия групп: \n" ;
               for (auto v : groups.groups)
               {
                   outp += "- " + v.name + "\n";
               }
               output.success(outp);
               return;
           }
           else if (checkPerm(ori.getPlayer()->getName(), perm) || checkPerm(ori.getPlayer()->getName(), "plugins.*") || checkPerm(ori.getPlayer()->getName(), "permissions.*") || checkPermWorlds(ori.getPlayer()->getName(), perm, dim) || checkPermWorlds(ori.getPlayer()->getName(), "plugins.*", dim) || checkPermWorlds(ori.getPlayer()->getName(), "permissions.*", dim))
           {
               _Groups    groups;
               auto nodes1 = YAML::LoadFile("plugins/Permissions Ex/groups.yml");
               for (const auto& p : nodes1["groups"])
               {
                   groups.groups.push_back(p.as<_Group>());
               }
               string outp = "[Permissions Ex]: Иерархия групп: \n";
               for (auto v : groups.groups)
               {
                   outp += "- " + v.name + "\n";
               }
               output.success(outp);
               return;
           }
           output.error(error_msg);
           return;
       }
       case Operation::User:
       {
           switch (us_op)
           {
             case User_Operation::Prefix:
             {
                 string dim;
                 if (ori.getDimension()->getDimensionId() == 0)
                     dim = "OverWorld";
                 else if (ori.getDimension()->getDimensionId() == 1)
                     dim = "Nether";
                 else if (ori.getDimension()->getDimensionId() == 2)
                     dim = "End";
                 string error_msg = get_msg("permissionDenied"),error_msg1 = get_msg("invalidArgument");
                 string perm = "permissions.manage.users.prefix." + player.getName();
                 if (player.getName() == "" || user_prefix == "")
                 {
                     output.error(error_msg1);
                     return;
                 }
                 if (ori.getPermissionsLevel() == CommandPermissionLevel::Console && world == "")
                 {
                     Users users;
                     auto node = YAML::LoadFile("plugins/Permissions Ex/users.yml");
                     for (const auto& p : node["users"])
                     {
                         users.users.push_back(p.as<_User>());
                     }
                     string prefix = user_prefix;
                     for (int i = 0; i < prefix.size(); ++i)
                     {
                         if (prefix[i] == '&')
                         {
                             prefix[i] = '§';
                             break;
                         }
                     }
                     prefix = utf8_encode(to_wstring(prefix));
                     for (int i = 0; i < users.users.size(); ++i)
                     {
                         if (player.getName() == users.users[i].nickname)
                         {
                             users.users[i].prefix = prefix;
                             break;
                         }
                     }
                     node.reset();
                     for (auto us : users.users)
                         node["users"].push_back(us);
                     remove("plugins/Permissions Ex/users.yml");
                     ofstream fout("plugins/Permissions Ex/users.yml");
                     fout << node;
                     fout.close();
                     output.success(utf8_encode(L"[Permissions Ex]: Префикс успешно изменен!"));
                     return;
                 }
                 else if (checkPerm(ori.getPlayer()->getName(), perm) || checkPerm(ori.getPlayer()->getName(), "plugins.*") || checkPerm(ori.getPlayer()->getName(), "permissions.*") || checkPermWorlds(ori.getPlayer()->getName(), perm, dim) || checkPermWorlds(ori.getPlayer()->getName(), "plugins.*", dim) || checkPermWorlds(ori.getPlayer()->getName(), "permissions.*", dim)  && world == "")
                 {
                     auto node = YAML::LoadFile("plugins/Permissions Ex/users.yml");
                     Users users;
                     for (const auto& p : node["users"])
                     {
                         users.users.push_back(p.as<_User>());
                     }
                     string prefix = user_prefix;
                     for (int i = 0; i < prefix.size(); ++i)
                     {
                         if (prefix[i] == '&')
                         {
                             prefix[i] = '§';
                             break;
                         }
                     }
                     for (int i = 0; i < users.users.size(); ++i)
                     {
                         if (player.getName() == users.users[i].nickname)
                         {
                             users.users[i].prefix = prefix;
                             break;
                         }
                     }
                     prefix = utf8_encode(to_wstring(prefix));
                     remove("plugins/Permissions Ex/users.yml");
                     node.reset();
                     for (auto us : users.users)
                         node["users"].push_back(us);
                     ofstream fout("plugins/Permissions Ex/users.yml");
                     fout << node;
                     fout.close();
                     output.success(utf8_encode(L"[Permissions Ex]: Префикс успешно изменен!"));
                     return;
                 }
                 if (ori.getPermissionsLevel() == CommandPermissionLevel::Console && world != "")
                 {
                     Users users;
                     auto node = YAML::LoadFile("plugins/Permissions Ex/users.yml");
                     for (const auto& p : node["users"])
                     {
                         users.users.push_back(p.as<_User>());
                     }
                     string prefix = user_prefix;
                     for (int i = 0; i < prefix.size(); ++i)
                     {
                         if (prefix[i] == '&')
                         {
                             prefix[i] = '§';
                             break;
                         }
                     }
                     prefix = utf8_encode(to_wstring(prefix));
                     for (int i = 0; i < users.users.size(); ++i)
                     {
                         if (users.users[i].nickname == player.getName())
                         {
                             for (int j = 0; j < users.users[i].worlds.size(); ++j)
                             {
                                 if (world == users.users[i].worlds[j].name)
                                 {
                                     users.users[i].worlds[j].prefix = prefix;
                                     break;
                                 }
                             }
                             break;
                         }
                     }
                     remove("plugins/Permissions Ex/users.yml");
                     node.reset();
                     for (auto us : users.users)
                         node["users"].push_back(us);
                     ofstream fout("plugins/Permissions Ex/users.yml");
                     fout << node;
                     fout.close();
                     output.success(utf8_encode(L"[Permissions Ex]: Префикс успешно изменен!"));
                     return;
                 }
                 else if (checkPerm(ori.getPlayer()->getName(), perm) || checkPerm(ori.getPlayer()->getName(), "plugins.*") || checkPerm(ori.getPlayer()->getName(), "permissions.*") || checkPermWorlds(ori.getPlayer()->getName(), perm, dim) || checkPermWorlds(ori.getPlayer()->getName(), "plugins.*", dim) || checkPermWorlds(ori.getPlayer()->getName(), "permissions.*", dim) && world != "")
                 {
                     auto node = YAML::LoadFile("plugins/Permissions Ex/users.yml");
                     for (const auto& p : node["users"])
                     {
                         users.users.push_back(p.as<_User>());
                     }
                     string prefix = user_prefix;
                     for (int i = 0; i < prefix.size(); ++i)
                     {
                         if (prefix[i] == '&')
                         {
                             prefix[i] = '§';
                             break;
                         }
                     }
                     prefix = utf8_encode(to_wstring(prefix));
                     for (int i = 0; i < users.users.size(); ++i)
                     {
                         if (users.users[i].nickname == player.getName())
                         {
                             for (int j = 0; j < users.users[i].worlds.size(); ++j)
                             {
                                 if (world == users.users[i].worlds[j].name)
                                 {
                                     users.users[i].worlds[j].prefix = prefix;
                                     break;
                                 }
                             }
                             break;
                         }
                     }
                     remove("plugins/Permissions Ex/users.yml");
                     node.reset();
                     for (auto us : users.users)
                         node["users"].push_back(us);
                     ofstream fout("plugins/Permissions Ex/users.yml");
                     fout << node;
                     fout.close();
                     output.success(utf8_encode(L"[Permissions Ex]: Префикс успешно изменен!"));
                     return;
                 }
                 output.error(error_msg);
                 return;
             }
             case User_Operation::Suffix:
             {
                 string dim;
                 if (ori.getDimension()->getDimensionId() == 0)
                     dim = "OverWorld";
                 else if (ori.getDimension()->getDimensionId() == 1)
                     dim = "Nether";
                 else if (ori.getDimension()->getDimensionId() == 2)
                     dim = "End";
                 string error_msg = get_msg("permissionDenied"), error_msg1 = get_msg("invalidArgument");
                 string perm = "permissions.manage.users.suffix." + player.getName();
                 if (player.getName() == "" || user_suffix == "")
                 {
                     output.error(error_msg1);
                     return;
                 }
                 if (ori.getPermissionsLevel() == CommandPermissionLevel::Console && world == "")
                 {
                     Users users;
                     auto node = YAML::LoadFile("plugins/Permissions Ex/users.yml");
                     for (const auto& p : node["users"])
                     {
                         users.users.push_back(p.as<_User>());
                     }
                     string suffix = user_suffix;
                     for (int i = 0; i < suffix.size(); ++i)
                     {
                         if (suffix[i] == '&')
                         {
                             suffix[i] = '§';
                         }
                     }
                     suffix = utf8_encode(to_wstring(suffix));
                     for (int i = 0; i < users.users.size(); ++i)
                     {
                         if (users.users[i].nickname == player.getName())
                         {
                             users.users[i].suffix = suffix;
                             break;
                         }
                     }
                     remove("plugins/Permissions Ex/users.yml");
                     node.reset();
                     for (auto us : users.users)
                         node["users"].push_back(us);
                     ofstream fout("plugins/Permissions Ex/users.yml");
                     fout << node;
                     fout.close();
                     output.success(utf8_encode(L"[Permissions Ex]: Суфикс успешно изменен!"));
                     return;
                 }
                 else if (checkPerm(ori.getPlayer()->getName(), perm) || checkPerm(ori.getPlayer()->getName(), "plugins.*") || checkPerm(ori.getPlayer()->getName(), "permissions.*") || checkPermWorlds(ori.getPlayer()->getName(), perm, dim) || checkPermWorlds(ori.getPlayer()->getName(), "plugins.*", dim) || checkPermWorlds(ori.getPlayer()->getName(), "permissions.*", dim) && world == "")
                 {
                     try
                     {
                         Users users;
                         auto node = YAML::LoadFile("plugins/Permissions Ex/users.yml");
                         for (const auto& p : node["users"])
                         {
                             users.users.push_back(p.as<_User>());
                         }
                         string suffix = user_suffix;
                         for (int i = 0; i < suffix.size(); ++i)
                         {
                             if (suffix[i] == '&')
                             {
                                 suffix[i] = '§';
                             }
                         }
                         suffix = utf8_encode(to_wstring(suffix));
                         for (int i = 0; i < users.users.size(); ++i)
                         {
                             if (users.users[i].nickname == player.getName())
                             {
                                 users.users[i].suffix = suffix;
                                 break;
                             }
                         }
                         remove("plugins/Permissions Ex/users.yml");
                         node.reset();
                         for (auto us : users.users)
                             node["users"].push_back(us);
                         ofstream fout("plugins/Permissions Ex/users.yml");
                         fout << node;
                         fout.close();
                         auto out_res = utf8_encode(L"[Permissions Ex]: Суфикс успешно изменен!");
                         output.success(out_res);
                         return;
                     }
                     catch (exception& e)
                     {
                         cerr << e.what() << endl;
                     }
                 }
                 if (ori.getPermissionsLevel() == CommandPermissionLevel::Console && world != "")
                 {
                     Users users;
                     auto node = YAML::LoadFile("plugins/Permissions Ex/users.yml");
                     for (const auto& p : node["users"])
                     {
                         users.users.push_back(p.as<_User>());
                     }
                     string suffix = user_suffix;
                     for (int i = 0; i < suffix.size(); ++i)
                     {
                         if (suffix[i] == '&')
                         {
                             suffix[i] = '§';
                         }
                     }
                     suffix = utf8_encode(to_wstring(suffix));
                     for (int i = 0; i < users.users.size(); ++i)
                     {
                         if (users.users[i].nickname == player.getName())
                         {
                             for (int j = 0; j < users.users[i].worlds.size(); ++j)
                             {
                                 if (world == users.users[i].worlds[j].name)
                                 {
                                     users.users[i].worlds[j].suffix = suffix;
                                     break;
                                 }
                             }
                             break;
                         }
                     }
                     remove("plugins/Permissions Ex/users.yml");
                     node.reset();
                     for (auto us : users.users)
                         node["users"].push_back(us);
                     ofstream fout("plugins/Permissions Ex/users.yml");
                     fout << node;
                     fout.close();
                     output.success(utf8_encode(L"[Permissions Ex]: Суфикс успешно изменен!"));
                     return;
                 }
                 else if (checkPerm(ori.getPlayer()->getName(), perm) || checkPerm(ori.getPlayer()->getName(), "plugins.*") || checkPerm(ori.getPlayer()->getName(), "permissions.*") || checkPermWorlds(ori.getPlayer()->getName(), perm, dim) || checkPermWorlds(ori.getPlayer()->getName(), "plugins.*", dim) || checkPermWorlds(ori.getPlayer()->getName(), "permissions.*", dim) && world != "")
                 {
                     Users users;
                     auto node = YAML::LoadFile("plugins/Permissions Ex/users.yml");
                     for (const auto& p : node["users"])
                     {
                         users.users.push_back(p.as<_User>());
                     }
                     string suffix = user_suffix;
                     for (int i = 0; i < suffix.size(); ++i)
                     {
                         if (suffix[i] == '&')
                         {
                             suffix[i] = '§';
                         }
                     }
                     suffix = utf8_encode(to_wstring(suffix));
                     for (int i = 0; i < users.users.size(); ++i)
                     {
                         if (users.users[i].nickname == player.getName())
                         {
                             for (int j = 0; j < users.users[i].worlds.size(); ++j)
                             {
                                 if (world == users.users[i].worlds[j].name)
                                 {
                                     users.users[i].worlds[j].suffix = suffix;
                                     break;
                                 }
                             }
                             break;
                         }
                     }
                     remove("plugins/Permissions Ex/users.yml");
                     node.reset();
                     for (auto us : users.users)
                         node["users"].push_back(us);
                     ofstream fout("plugins/Permissions Ex/users.yml");
                     fout << node;
                     fout.close();
                     output.success(utf8_encode(L"[Permissions Ex]: Суфикс успешно изменен!"));
                     return;
                 }
                 output.error(error_msg);
                 return;          
             }
             case User_Operation::Check: //будет проверять право и у игрока лично,и у его группы,+ по всем мирам Примечание: здесь user_permission != только право у пользователя
             {
                 if (opval != "")
                 {
                     goto jxds;
                 }
                 if (opval1 != "")
                 {
                     goto jdc1;
                 }
                 if (opval2 != "")
                 {
                     goto jdc2;
                 }
                 if (is_list)
                 {
                     goto jdc3;
                 }
                 string dim;
                 if (ori.getDimension()->getDimensionId() == 0)
                     dim = "OverWorld";
                 else if (ori.getDimension()->getDimensionId() == 1)
                     dim = "Nether";
                 else if (ori.getDimension()->getDimensionId() == 2)
                     dim = "End";
                 string error_msg = get_msg("permissionDenied"), error_msg1 = get_msg("invalidArgument");
                 string perm = "permissions.manage." + player.getName();
                 if (player.getName() == "" || user_permission == "") 
                 {
                     output.error(error_msg1);
                     return;
                 }
                 if (ori.getPermissionsLevel() == CommandPermissionLevel::Console)
                 {
                     if (debug_mode)
                     {
                         int hasPerm = checkPerm(player.getName(), user_permission) + checkPermWorlds(player.getName(), user_permission, "OverWorld") + checkPermWorlds(player.getName(), user_permission, "Nether") + checkPermWorlds(player.getName(), user_permission, "End");
                         if (hasPerm != 0)
                         {
                             wstring outp = L"[Permissions Ex]: Игрок " + to_wstring(player.getName()) + L" владеет правом " + to_wstring(user_permission) + L"!\n" + L"[" + to_wstring(currentDateTime()) + L"] Игрок Console проверял право " + to_wstring(user_permission) + L" у игрока " + to_wstring(player.getName());
                             ifstream ch("server.log");
                             ofstream fout;
                             if (ch.is_open())
                             {
                                 ch.close();
                                 fout.open("server.log", ios_base::app);
                             }
                             else
                             {
                                 ch.close();
                                 fout.open("server.log");
                             }
                             fout << "[" << currentDateTime() << "] Игрок " + ori.getPlayer()->getName() + " проверял право " << user_permission << " у игрока " << player.getName() << "\n";
                             fout.close();
                             output.success(utf8_encode(outp));
                             return;
                         }
                         else
                         {
                             wstring outp = L"[Permissions Ex]: Игрок " + to_wstring(player.getName()) + L" не владеет правом " + to_wstring(user_permission) + L"!\n" + L"[" + to_wstring(currentDateTime()) + L"] Игрок Console проверял право " + to_wstring(user_permission) + L" у игрока " + to_wstring(player.getName());
                             ifstream ch("server.log");
                             ofstream fout;
                             if (ch.is_open())
                             {
                                 ch.close();
                                 fout.open("server.log", ios_base::app);
                             }
                             else
                             {
                                 ch.close();
                                 fout.open("server.log");
                             }
                             fout << "[" << currentDateTime() << "] Игрок " + ori.getPlayer()->getName() + " проверял право " << user_permission << " у игрока " << player.getName() << "\n";
                             fout.close();
                             output.success(utf8_encode(outp));
                             return;
                         }
                     }
                     else if (!debug_mode)
                     {
                         int hasPerm = checkPerm(player.getName(), user_permission) + checkPermWorlds(player.getName(), user_permission, "OverWorld") + checkPermWorlds(player.getName(), user_permission, "Nether") + checkPermWorlds(player.getName(), user_permission, "End");
                         if (hasPerm != 0)
                         {
                             output.success(utf8_encode(L"[Permissions Ex]: Игрок " + to_wstring(player.getName()) + L" владеет правом " + to_wstring(user_permission) + L"!\n"));
                             return;
                         }
                         else
                         {
                             output.success(utf8_encode(L"[Permissions Ex]: Игрок " + to_wstring(player.getName()) + L" не владеет правом " + to_wstring(user_permission) + L"!\n"));
                             return;
                         }
                     }
                 }
                 else if (checkPerm(ori.getPlayer()->getName(), perm) || checkPerm(ori.getPlayer()->getName(), ".**") || checkPerm(ori.getPlayer()->getName(), "permissions.*") || checkPermWorlds(ori.getPlayer()->getName(), perm, dim) || checkPermWorlds(ori.getPlayer()->getName(), "plugins.*", dim) || checkPermWorlds(ori.getPlayer()->getName(), "permissions.*", dim))
                 {
                     if (debug_mode)
                     {
                         int hasPerm = checkPerm(player.getName(), user_permission) + checkPermWorlds(player.getName(), user_permission, "OverWorld") + checkPermWorlds(player.getName(), user_permission, "Nether") + checkPermWorlds(player.getName(), user_permission, "End");
                         if (hasPerm != 0)
                         {
                             wstring outp = L"[Permissions Ex]: Игрок " + to_wstring(player.getName()) + L" владеет правом " + to_wstring(user_permission) + L"!\n" + L"[" + to_wstring(currentDateTime()) + L"] Игрок " +to_wstring(player.getName()) + L" проверял право " + to_wstring(user_permission) + L" у игрока " + to_wstring(player.getName());
                             ifstream ch("server.log");
                             ofstream fout;
                             if (ch.is_open())
                             {
                                 ch.close();
                                 fout.open("server.log", ios_base::app);
                             }
                             else
                             {
                                 ch.close();
                                 fout.open("server.log");
                             }
                             fout << "[" << currentDateTime() << "] Игрок " + ori.getPlayer()->getName() + " проверял право " << user_permission << " у игрока " << player.getName() << "\n";
                             fout.close();
                             output.success(utf8_encode(outp));
                             return;
                         }
                         else
                         {
                             wstring outp = L"[Permissions Ex]: Игрок " + to_wstring(player.getName()) + L" не владеет правом " + to_wstring(user_permission) + L"!\n" + L"[" + to_wstring(currentDateTime()) + L"] Игрок " + to_wstring(player.getName()) + L" проверял право " + to_wstring(user_permission) + L" у игрока " + to_wstring(player.getName());
                             ifstream ch("server.log");
                             ofstream fout;
                             if (ch.is_open())
                             {
                                 ch.close();
                                 fout.open("server.log", ios_base::app);
                             }
                             else
                             {
                                 ch.close();
                                 fout.open("server.log");
                             }
                             fout << "[" << currentDateTime() << "] Игрок " + ori.getPlayer()->getName() + " проверял право " << user_permission << " у игрока " << player.getName() << "\n";
                             fout.close();
                             output.success(utf8_encode(outp));
                             return;
                         }
                     }
                     else if (!debug_mode)
                     {
                         int hasPerm = checkPerm(player.getName(), user_permission) + checkPermWorlds(player.getName(), user_permission, "OverWorld") + checkPermWorlds(player.getName(), user_permission, "Nether") + checkPermWorlds(player.getName(), user_permission, "End");
                         if (hasPerm != 0)
                         {
                             output.success(utf8_encode(L"[Permissions Ex]: Игрок " + to_wstring(player.getName()) + L" владеет правом " + to_wstring(user_permission) + L"!\n"));
                             return;
                         }
                         else
                         {
                             output.success(utf8_encode(L"[Permissions Ex]: Игрок " + to_wstring(player.getName()) + L" не владеет правом " + to_wstring(user_permission) + L"!\n"));
                             return;
                         }
                     }
                 }
                 output.error(error_msg);
                 return;
             }
             case User_Operation::Add:
             {
                 string dim;
                 if (ori.getDimension()->getDimensionId() == 0)
                     dim = "OverWorld";
                 else if (ori.getDimension()->getDimensionId() == 1)
                     dim = "Nether";
                 else if (ori.getDimension()->getDimensionId() == 2)
                     dim = "End";
                 string error_msg = get_msg("permissionDenied"), error_msg1 = get_msg("invalidArgument");
                 string perm = "permissions.manage.users.permissions." + player.getName();
                 if (player.getName() == "" || user_permission == "")
                 {
                     output.error(error_msg1);
                     return;
                 }
                 if (ori.getPermissionsLevel() == CommandPermissionLevel::Console && world == "")
                 {
                     Users users;
                     auto node = YAML::LoadFile("plugins/Permissions Ex/users.yml");
                     for (const auto& p : node["users"])
                     {
                         users.users.push_back(p.as<_User>());
                     }
                     for (int i = 0; i < users.users.size(); ++i)
                     {
                         if (users.users[i].nickname == player.getName())
                         {
                             users.users[i].permissions.push_back(user_permission);
                             break;
                         }
                     }
                     remove("plugins/Permissions Ex/users.yml");
                     node.reset();
                     for (auto us : users.users)
                         node["users"].push_back(us);
                     ofstream fout("plugins/Permissions Ex/users.yml");
                     fout << node;
                     fout.close();
                     output.success(utf8_encode(L"[Permissions Ex]: Право выдано успешно!"));
                     return;
                 }
                 else if (checkPerm(ori.getPlayer()->getName(), perm) || checkPerm(ori.getPlayer()->getName(), "plugins.*") || checkPerm(ori.getPlayer()->getName(), "permissions.*") || checkPermWorlds(ori.getPlayer()->getName(), perm, dim) || checkPermWorlds(ori.getPlayer()->getName(), "plugins.*", dim) || checkPermWorlds(ori.getPlayer()->getName(), "permissions.*", dim) && world == "")
                 {
                     Users users;
                     auto node = YAML::LoadFile("plugins/Permissions Ex/users.yml");
                     for (const auto& p : node["users"])
                     {
                         users.users.push_back(p.as<_User>());
                     }
                     for (int i = 0; i < users.users.size(); ++i)
                     {
                         if (users.users[i].nickname == player.getName())
                         {
                             users.users[i].permissions.push_back(user_permission);
                             break;
                         }
                     }
                     remove("plugins/Permissions Ex/users.yml");
                     node.reset();
                     for (auto us : users.users)
                         node["users"].push_back(us);
                     ofstream fout("plugins/Permissions Ex/users.yml");
                     fout << node;
                     fout.close();
                     output.success(utf8_encode(L"[Permissions Ex]: Право выдано успешно!"));
                     return;
                 }
                 if (ori.getPermissionsLevel() == CommandPermissionLevel::Console && world != "")
                 {
                     Users users;
                     auto node = YAML::LoadFile("plugins/Permissions Ex/users.yml");
                     for (const auto& p : node["users"])
                     {
                         users.users.push_back(p.as<_User>());
                     }
                     for (int i = 0; i < users.users.size(); ++i)
                     {
                         if (users.users[i].nickname == player.getName())
                         {
                             for (int j = 0; j < users.users[i].worlds.size(); ++j)
                             {
                                 if (world == users.users[i].worlds[j].name)
                                 {
                                     users.users[i].worlds[j].permissions.push_back(user_permission);
                                     break;
                                 }
                             }
                         }
                     }
                     remove("plugins/Permissions Ex/users.yml");
                     node.reset();
                     for (auto us : users.users)
                         node["users"].push_back(us);
                     ofstream fout("plugins/Permissions Ex/users.yml");
                     fout << node;
                     fout.close();
                     output.success(utf8_encode(L"[Permissions Ex]: Право выдано успешно!"));
                     return;
                 }
                 else if (checkPerm(ori.getPlayer()->getName(), perm) || checkPerm(ori.getPlayer()->getName(), "plugins.*") || checkPerm(ori.getPlayer()->getName(), "permissions.*") || checkPermWorlds(ori.getPlayer()->getName(), perm, dim) || checkPermWorlds(ori.getPlayer()->getName(), "plugins.*", dim) || checkPermWorlds(ori.getPlayer()->getName(), "permissions.*", dim) && world != "")
                 {
                     Users users;
                     auto node = YAML::LoadFile("plugins/Permissions Ex/users.yml");
                     for (const auto& p : node["users"])
                     {
                         users.users.push_back(p.as<_User>());
                     }
                     for (int i = 0; i < users.users.size(); ++i)
                     {
                         if (users.users[i].nickname == player.getName())
                         {
                             for (int j = 0; j < users.users[i].worlds.size(); ++j)
                             {
                                 if (world == users.users[i].worlds[j].name)
                                 {
                                     users.users[i].worlds[j].permissions.push_back(user_permission);
                                     break;
                                 }
                             }
                         }
                     }
                     remove("plugins/Permissions Ex/users.yml");
                     node.reset();
                     for (auto us : users.users)
                         node["users"].push_back(us);
                     ofstream fout("plugins/Permissions Ex/users.yml");
                     fout << node;
                     fout.close();
                     output.success(utf8_encode(L"[Permissions Ex]: Право выдано успешно!"));
                     return;
                 }
                 output.error(error_msg);
                 return;
             }
             case User_Operation::Remove:
             {
                 string dim;
                 if (ori.getDimension()->getDimensionId() == 0)
                     dim = "OverWorld";
                 else if (ori.getDimension()->getDimensionId() == 1)
                     dim = "Nether";
                 else if (ori.getDimension()->getDimensionId() == 2)
                     dim = "End";
                 string error_msg = get_msg("permissionDenied"), error_msg1 = get_msg("invalidArgument");
                 string perm = "permissions.manage.users.permissions." + player.getName();
                 if (player.getName() == "" || user_permission == "")
                 {
                     output.error(error_msg1);
                     return;
                 }
                 if (ori.getPermissionsLevel() == CommandPermissionLevel::Console && world == "")
                 {
                     Users users;
                     auto node = YAML::LoadFile("plugins/Permissions Ex/users.yml");
                     for (const auto& p : node["users"])
                     {
                         users.users.push_back(p.as<_User>());
                     }
                     for (int i = 0; i < users.users.size(); ++i)
                     {
                         if (player.getName() == users.users[i].nickname)
                         {
                             int cnt = 0;
                             for (auto fgh : users.users[i].permissions)
                             {
                                 if (user_permission == fgh)
                                     break;
                                 cnt++;
                             }
                             auto sz = users.users[i].permissions.size();
                             users.users[i].permissions.erase(users.users[i].permissions.begin() + cnt, users.users[i].permissions.begin() + cnt);
                             users.users[i].permissions.resize(sz - 1);
                             break;
                         }
                     }
                     remove("plugins/Permissions Ex/users.yml");
                     node.reset();
                     for (auto us : users.users)
                         node["users"].push_back(us);
                     ofstream fout("plugins/Permissions Ex/users.yml");
                     fout << node;
                     fout.close();
                     output.success(utf8_encode(L"[Permissions Ex]: Право забрано успешно!"));
                     return;
                 }
                 else if (checkPerm(ori.getPlayer()->getName(), perm) || checkPerm(ori.getPlayer()->getName(), "plugins.*") || checkPerm(ori.getPlayer()->getName(), "permissions.*") || checkPermWorlds(ori.getPlayer()->getName(), perm, dim) || checkPermWorlds(ori.getPlayer()->getName(), "plugins.*", dim) || checkPermWorlds(ori.getPlayer()->getName(), "permissions.*", dim) && world == "")
                 {
                     Users users;
                     auto node = YAML::LoadFile("plugins/Permissions Ex/users.yml");
                     for (const auto& p : node["users"])
                     {
                         users.users.push_back(p.as<_User>());
                     }
                     for (int i = 0; i < users.users.size(); ++i)
                     {
                         if (player.getName() == users.users[i].nickname)
                         {
                             int cnt = 0;
                             for (auto fgh : users.users[i].permissions)
                             {
                                 if (user_permission == fgh)
                                     break;
                                 cnt++;
                             }
                             auto sz = users.users[i].permissions.size();
                             users.users[i].permissions.erase(users.users[i].permissions.begin() + cnt, users.users[i].permissions.begin() + cnt);
                             users.users[i].permissions.resize(sz - 1);
                             break;
                         }
                     }
                     remove("plugins/Permissions Ex/users.yml");
                     node.reset();
                     for (auto us : users.users)
                         node["users"].push_back(us);
                     ofstream fout("plugins/Permissions Ex/users.yml");
                     fout << node;
                     fout.close();
                     output.success(utf8_encode(L"[Permissions Ex]: Право забрано успешно!"));
                     return;
                 }
                 if (ori.getPermissionsLevel() == CommandPermissionLevel::Console && world != "")
                 {
                     Users users;
                     auto node = YAML::LoadFile("plugins/Permissions Ex/users.yml");
                     for (const auto& p : node["users"])
                     {
                         users.users.push_back(p.as<_User>());
                     }
                     for (int i = 0; i < users.users.size(); ++i)
                     {
                         if (player.getName() == users.users[i].nickname)
                         {
                             for (int j = 0; j < users.users[i].worlds.size(); ++i)
                             {
                                 if (world == users.users[i].worlds[j].name)
                                 {
                                     int cnt = 0;
                                     for (auto fgh : users.users[i].worlds[j].permissions)
                                     {
                                         if (user_permission == fgh)
                                             break;
                                         cnt++;
                                     }
                                     auto sz = users.users[i].worlds[j].permissions.size();
                                     users.users[i].worlds[j].permissions.erase(users.users[i].worlds[j].permissions.begin() + cnt, users.users[i].worlds[j].permissions.begin() + cnt);
                                     users.users[i].worlds[j].permissions.resize(sz - 1);
                                     break;
                                 }
                             }
                         }
                     }
                     remove("plugins/Permissions Ex/users.yml");
                     node.reset();
                     for (auto us : users.users)
                         node["users"].push_back(us);
                     ofstream fout("plugins/Permissions Ex/users.yml");
                     fout << node;
                     fout.close();
                     output.success(utf8_encode(L"[Permissions Ex]: Право забрано успешно!"));
                     return;
                 }
                 else if (checkPerm(ori.getPlayer()->getName(), perm) || checkPerm(ori.getPlayer()->getName(), "plugins.*") || checkPerm(ori.getPlayer()->getName(), "permissions.*") || checkPermWorlds(ori.getPlayer()->getName(), perm, dim) || checkPermWorlds(ori.getPlayer()->getName(), "plugins.*", dim) || checkPermWorlds(ori.getPlayer()->getName(), "permissions.*", dim) && world != "")
                 {
                     Users users;
                     auto node = YAML::LoadFile("plugins/Permissions Ex/users.yml");
                     for (const auto& p : node["users"])
                     {
                         users.users.push_back(p.as<_User>());
                     }
                     for (int i = 0; i < users.users.size(); ++i)
                     {
                         if (player.getName() == users.users[i].nickname)
                         {
                             for (int j = 0; j < users.users[i].worlds.size(); ++i)
                             {
                                 if (world == users.users[i].worlds[j].name)
                                 {
                                     int cnt = 0;
                                     for (auto fgh : users.users[i].worlds[j].permissions)
                                     {
                                         if (user_permission == fgh)
                                             break;
                                         cnt++;
                                     }
                                     auto sz = users.users[i].worlds[j].permissions.size();
                                     users.users[i].worlds[j].permissions.erase(users.users[i].worlds[j].permissions.begin() + cnt, users.users[i].worlds[j].permissions.begin() + cnt);
                                     users.users[i].worlds[j].permissions.resize(sz - 1);
                                     break;
                                 }
                             }
                         }
                     }
                     remove("plugins/Permissions Ex/users.yml");
                     node.reset();
                     for (auto us : users.users)
                         node["users"].push_back(us);
                     ofstream fout("plugins/Permissions Ex/users.yml");
                     fout << node;
                     fout.close();
                     output.success(utf8_encode(L"[Permissions Ex]: Право забрано успешно!"));
                     return;
                 }
                 output.error(error_msg);
                 return;
             }
             case User_Operation::Delete:
             {
                 string dim;
                 if (ori.getDimension()->getDimensionId() == 0)
                     dim = "OverWorld";
                 else if (ori.getDimension()->getDimensionId() == 1)
                     dim = "Nether";
                 else if (ori.getDimension()->getDimensionId() == 2)
                     dim = "End";
                 string error_msg = get_msg("permissionDenied"), error_msg1 = get_msg("invalidArgument");
                 string perm = "permissions.manage.users." + player.getName();
                 if (player.getName() == "")
                 {
                     output.error(error_msg1);
                     return;
                 }
                 int cnt = 0;
                 if (ori.getPermissionsLevel() == CommandPermissionLevel::Console)
                 {
                     Users users;
                     auto node = YAML::LoadFile("plugins/Permissions Ex/users.yml");
                     for (const auto& p : node["users"])
                     {
                         users.users.push_back(p.as<_User>());
                     }
                     for (int i = 0; i < users.users.size(); ++i)
                     {
                         if (player.getName() == users.users[i].nickname)
                         {
                             auto sz = users.users.size();
                             users.users.erase(users.users.begin() + cnt, users.users.begin() + cnt);
                             users.users.resize(sz - 1);
                             break;
                         }
                         cnt++;
                     }
                     remove("plugins/Permissions Ex/users.yml");
                     node.reset();
                     for (auto us : users.users)
                         node["users"].push_back(us);
                     ofstream fout("plugins/Permissions Ex/users.yml");
                     fout << node;
                     fout.close();
                     output.success(utf8_encode(L"[Permissions Ex]: Игрок " + to_wstring(player.getName()) + L" удален из бэкенда успешно!"));
                     return;
                 }
                 else if (checkPerm(ori.getPlayer()->getName(), perm) || checkPerm(ori.getPlayer()->getName(), "plugins.*") || checkPerm(ori.getPlayer()->getName(), "permissions.*") || checkPermWorlds(ori.getPlayer()->getName(), perm, dim) || checkPermWorlds(ori.getPlayer()->getName(), "plugins.*", dim) || checkPermWorlds(ori.getPlayer()->getName(), "permissions.*", dim))
                 {
                     Users users;
                     auto node = YAML::LoadFile("plugins/Permissions Ex/users.yml");
                     for (const auto& p : node["users"])
                     {
                         users.users.push_back(p.as<_User>());
                     }
                     for (int i = 0; i < users.users.size(); ++i)
                     {
                         if (player.getName() == users.users[i].nickname)
                         {
                             auto sz = users.users.size();
                             users.users.erase(users.users.begin() + cnt, users.users.begin() + cnt);
                             users.users.resize(sz - 1);
                             break;
                         }
                         cnt++;
                     }
                     remove("plugins/Permissions Ex/users.yml");
                     node.reset();
                     for (auto us : users.users)
                         node["users"].push_back(us);
                     ofstream fout("plugins/Permissions Ex/users.yml");
                     fout << node;
                     fout.close();
                     output.success(utf8_encode(L"[Permissions Ex]: Игрок " + to_wstring(player.getName()) + L" удален из бэкенда успешно!"));
                     return;
                 }
                 output.error(error_msg);
                 return;
             }
             case User_Operation::Group:
             {
                 switch (us_gr_op)
                 {
                     case User_Group_Operation::Add:
                     {
                       jxds:
                         string dim;
                         if (ori.getDimension()->getDimensionId() == 0)
                             dim = "OverWorld";
                         else if (ori.getDimension()->getDimensionId() == 1)
                             dim = "Nether";
                         else if (ori.getDimension()->getDimensionId() == 2)
                             dim = "End";
                         string error_msg = get_msg("permissionDenied"), error_msg1 = get_msg("invalidArgument");
                         if (player.getName() == "" || opval == "")
                         {
                             output.error(error_msg1);
                             return;
                         }
                         _Groups groups;
                         YAML::Node node = YAML::LoadFile("plugins/Permissions Ex/groups.yml");
                         for (const auto& k : node["groups"])
                             groups.groups.push_back(k.as<_Group>());
                         bool is_group = false;
                         for (auto g : groups.groups)
                         {
                             if (opval == g.name)
                             {
                                 is_group = true;
                                 break;
                             }
                         }
                         if (is_group)
                         {
                             string perm = "permissions.manage.membership." + player.getName();
                             if (ori.getPermissionsLevel() == CommandPermissionLevel::Console && world == "")
                             {
                                 Users users;
                                 auto node = YAML::LoadFile("plugins/Permissions Ex/users.yml");
                                 for (const auto& p : node["users"])
                                 {
                                     users.users.push_back(p.as<_User>());
                                 }
                                 for (int i = 0; i < users.users.size(); ++i)
                                 {
                                     if (player.getName() == users.users[i].nickname)
                                     {
                                         if (lifetime == 0)
                                         {
                                             users.users[i].groups.push_back(opval + ":0");
                                             auto gr = load_group(opval);
                                             users.users[i].prefix = gr.prefix;
                                             users.users[i].suffix = gr.suffix;
                                             for (int j = 0; j < gr.worlds.size(); ++j)
                                             {
                                                 users.users[i].worlds[j].prefix = gr.worlds[j].prefix;
                                                 users.users[i].worlds[j].suffix = gr.worlds[j].suffix;
                                             }
                                         }
                                         else
                                         {
                                             users.users[i].groups.push_back(opval + ":" + to_string(lifetime));
                                             auto gr = load_group(opval);
                                             users.users[i].prefix = gr.prefix;
                                             users.users[i].suffix = gr.suffix;
                                             for (int j = 0; j < gr.worlds.size(); ++j)
                                             {
                                                 users.users[i].worlds[j].prefix = gr.worlds[j].prefix;
                                                 users.users[i].worlds[j].suffix = gr.worlds[j].suffix;
                                             }
                                         }
                                         break;
                                     }
                                 }
                                 remove("plugins/Permissions Ex/users.yml");
                                 node.reset();
                                 for (auto us : users.users)
                                     node["users"].push_back(us);
                                 ofstream fout("plugins/Permissions Ex/users.yml");
                                 fout << node;
                                 fout.close();
                                 output.success(utf8_encode(L"[Permissions Ex]: Временная група выдана успешно!"));
                                 return;
                             }
                             if (ori.getPermissionsLevel() == CommandPermissionLevel::Console && world != "")
                             {
                                 Users users;
                                 auto node = YAML::LoadFile("plugins/Permissions Ex/users.yml");
                                 for (const auto& p : node["users"])
                                 {
                                     users.users.push_back(p.as<_User>());
                                 }
                                 bool is_group = false;

                                 for (int i = 0; i < users.users.size(); ++i)
                                 {
                                     if (player.getName() == users.users[i].nickname)
                                     {
                                         for (int j = 0; j < users.users[i].worlds.size(); ++j)
                                         {
                                             if (world == users.users[i].worlds[j].name)
                                             {
                                                 if (lifetime == 0)
                                                 {
                                                     users.users[i].worlds[j].group = opval + ":0";
                                                     auto gr = load_group(opval);
                                                     users.users[i].worlds[j].prefix = gr.prefix;
                                                     users.users[i].worlds[j].suffix = gr.suffix;
                                                 }
                                                 else
                                                 {
                                                     users.users[i].worlds[j].group = opval + ":" + to_string(lifetime);
                                                     auto gr = load_group(opval);
                                                     users.users[i].worlds[j].prefix = gr.prefix;
                                                     users.users[i].worlds[j].suffix = gr.suffix;
                                                 }
                                             }
                                         }
                                         break;
                                     }
                                 }
                                 remove("plugins/Permissions Ex/users.yml");
                                 node.reset();
                                 for (auto us : users.users)
                                     node["users"].push_back(us);
                                 ofstream fout("plugins/Permissions Ex/users.yml");
                                 fout << node;
                                 fout.close();
                                 output.success(utf8_encode(L"[Permissions Ex]: Временная група выдана успешно!"));
                                 return;
                             }
                             if ((checkPerm(ori.getPlayer()->getName(), perm) || checkPerm(ori.getPlayer()->getName(), "plugins.*") || checkPerm(ori.getPlayer()->getName(), "permissions.*") || checkPermWorlds(ori.getPlayer()->getName(), perm, dim) || checkPermWorlds(ori.getPlayer()->getName(), "plugins.*", dim) || checkPermWorlds(ori.getPlayer()->getName(), "permissions.*", dim)) && world == "")
                             {
                                 Users users;
                                 auto node = YAML::LoadFile("plugins/Permissions Ex/users.yml");
                                 for (const auto& p : node["users"])
                                 {
                                     users.users.push_back(p.as<_User>());
                                 }
                                 for (int i = 0; i < users.users.size(); ++i)
                                 {
                                     if (player.getName() == users.users[i].nickname)
                                     {
                                         if (lifetime == 0)
                                         {
                                             users.users[i].groups.push_back(opval + ":0");
                                             auto gr = load_group(opval);
                                             users.users[i].prefix = gr.prefix;
                                             users.users[i].suffix = gr.suffix;
                                             for (int j = 0; j < gr.worlds.size(); ++j)
                                             {
                                                 users.users[i].worlds[j].prefix = gr.worlds[j].prefix;
                                                 users.users[i].worlds[j].suffix = gr.worlds[j].suffix;
                                             }
                                         }
                                         else
                                         {
                                             users.users[i].groups.push_back(opval + ":" + to_string(lifetime));
                                             auto gr = load_group(opval);
                                             users.users[i].prefix = gr.prefix;
                                             users.users[i].suffix = gr.suffix;
                                             for (int j = 0; j < gr.worlds.size(); ++j)
                                             {
                                                 users.users[i].worlds[j].prefix = gr.worlds[j].prefix;
                                                 users.users[i].worlds[j].suffix = gr.worlds[j].suffix;
                                             }
                                         }
                                         break;
                                     }
                                 }
                                 remove("plugins/Permissions Ex/users.yml");
                                 node.reset();
                                 for (auto us : users.users)
                                     node["users"].push_back(us);
                                 ofstream fout("plugins/Permissions Ex/users.yml");
                                 fout << node;
                                 fout.close();
                                 output.success(utf8_encode(L"[Permissions Ex]: Временная група выдана успешно!"));
                                 return;
                             }
                             else if (checkPerm(ori.getPlayer()->getName(), perm) || checkPerm(ori.getPlayer()->getName(), "plugins.*") || checkPerm(ori.getPlayer()->getName(), "permissions.*") || checkPermWorlds(ori.getPlayer()->getName(), perm, dim) || checkPermWorlds(ori.getPlayer()->getName(), "plugins.*", dim) || checkPermWorlds(ori.getPlayer()->getName(), "permissions.*", dim) && world == "")
                             {
                                 Users users;
                                 auto node = YAML::LoadFile("plugins/Permissions Ex/users.yml");
                                 for (const auto& p : node["users"])
                                 {
                                     users.users.push_back(p.as<_User>());
                                 }
                                 for (int i = 0; i < users.users.size(); ++i)
                                 {
                                     if (player.getName() == users.users[i].nickname)
                                     {
                                         for (int j = 0; j < users.users[i].worlds.size(); ++j)
                                         {
                                             if (world == users.users[i].worlds[j].name)
                                             {
                                                 if (lifetime == 0)
                                                 {
                                                     users.users[i].worlds[j].group = opval + ":0";
                                                     auto gr = load_group(opval);
                                                     users.users[i].worlds[j].prefix = gr.prefix;
                                                     users.users[i].worlds[j].suffix = gr.suffix;
                                                 }
                                                 else
                                                 {
                                                     users.users[i].worlds[j].group = opval + ":" + to_string(lifetime);
                                                     auto gr = load_group(opval);
                                                     users.users[i].worlds[j].prefix = gr.prefix;
                                                     users.users[i].worlds[j].suffix = gr.suffix;
                                                 }
                                             }
                                         }
                                         break;
                                     }
                                 }
                                 remove("plugins/Permissions Ex/users.yml");
                                 node.reset();
                                 for (auto us : users.users)
                                     node["users"].push_back(us);
                                 ofstream fout("plugins/Permissions Ex/users.yml");
                                 fout << node;
                                 fout.close();
                                 output.success(utf8_encode(L"[Permissions Ex]: Временная група выдана успешно!"));
                                 return;
                             }
                         }
                        else if (!is_group)
                        {
                        string perm = "permissions.manage.users.permissions.timed." + player.getName();
                         if (ori.getPermissionsLevel() == CommandPermissionLevel::Console && world == "")
                         {
                             Users users;
                             auto node = YAML::LoadFile("plugins/Permissions Ex/users.yml");
                             for (const auto& p : node["users"])
                             {
                                 users.users.push_back(p.as<_User>());
                             }
                             for (int i = 0; i < users.users.size(); ++i)
                             {
                                 if (player.getName() == users.users[i].nickname)
                                 {
                                     if (lifetime == 0)
                                     {
                                         users.users[i].permissions.push_back(opval + ":0");
                                     }
                                     else
                                     {
                                         users.users[i].permissions.push_back(opval + ":" + to_string(lifetime));
                                     }
                                     break;
                                 }
                             }
                             remove("plugins/Permissions Ex/users.yml");
                             node.reset();
                             for (auto us : users.users)
                                 node["users"].push_back(us);
                             ofstream fout("plugins/Permissions Ex/users.yml");
                             fout << node;
                             fout.close();
                             output.success(utf8_encode(L"[Permissions Ex]: Временое право выдано успешно!"));
                             return;
                         }
                         if (ori.getPermissionsLevel() == CommandPermissionLevel::Console && world != "")
                         {
                             Users users;
                             auto node = YAML::LoadFile("plugins/Permissions Ex/users.yml");
                             for (const auto& p : node["users"])
                             {
                                 users.users.push_back(p.as<_User>());
                             }
                             for (int i = 0; i < users.users.size(); ++i)
                             {
                                 if (player.getName() == users.users[i].nickname)
                                 {
                                     for (int j = 0; j < users.users[i].worlds.size(); ++j)
                                     {
                                         if (world == users.users[i].worlds[j].name)
                                         {
                                             if (lifetime == 0)
                                             {
                                                 users.users[i].worlds[j].permissions.push_back(opval + ":0");
                                             }
                                             else
                                             {
                                                 users.users[i].worlds[j].permissions.push_back(opval + ":" + to_string(lifetime));
                                             }
                                         }
                                     }
                                     break;
                                 }
                             }
                             remove("plugins/Permissions Ex/users.yml");
                             node.reset();
                             for (auto us : users.users)
                                 node["users"].push_back(us);
                             ofstream fout("plugins/Permissions Ex/users.yml");
                             fout << node;
                             fout.close();
                             output.success(utf8_encode(L"[Permissions Ex]: Временое право выдано успешно!"));
                             return;
                         }
                         if (checkPerm(ori.getPlayer()->getName(), perm) || checkPerm(ori.getPlayer()->getName(), "plugins.*") || checkPerm(ori.getPlayer()->getName(), "permissions.*") || checkPermWorlds(ori.getPlayer()->getName(), perm, dim) || checkPermWorlds(ori.getPlayer()->getName(), "plugins.*", dim) || checkPermWorlds(ori.getPlayer()->getName(), "permissions.*", dim) && world == "")
                         {
                             Users users;
                             auto node = YAML::LoadFile("plugins/Permissions Ex/users.yml");
                             for (const auto& p : node["users"])
                             {
                                 users.users.push_back(p.as<_User>());
                             }
                             for (int i = 0; i < users.users.size(); ++i)
                             {
                                 if (player.getName() == users.users[i].nickname)
                                 {
                                     if (lifetime == 0)
                                     {
                                         users.users[i].permissions.push_back(opval + ":0");
                                     }
                                     else
                                     {
                                         users.users[i].permissions.push_back(opval + ":" + to_string(lifetime));
                                     }
                                     break;
                                 }
                             }
                             remove("plugins/Permissions Ex/users.yml");
                             node.reset();
                             for (auto us : users.users)
                                 node["users"].push_back(us);
                             ofstream fout("plugins/Permissions Ex/users.yml");
                             fout << node;
                             fout.close();
                             output.success(utf8_encode(L"[Permissions Ex]: Временое право выдано успешно!"));
                             return;
                         }
                         else if (checkPerm(ori.getPlayer()->getName(), perm) || checkPerm(ori.getPlayer()->getName(), "plugins.*") || checkPerm(ori.getPlayer()->getName(), "permissions.*") || checkPermWorlds(ori.getPlayer()->getName(), perm, dim) || checkPermWorlds(ori.getPlayer()->getName(), "plugins.*", dim) || checkPermWorlds(ori.getPlayer()->getName(), "permissions.*", dim) && world == "")
                         {
                             Users users;
                             auto node = YAML::LoadFile("plugins/Permissions Ex/users.yml");
                             for (const auto& p : node["users"])
                             {
                                 users.users.push_back(p.as<_User>());
                             }
                             for (int i = 0; i < users.users.size(); ++i)
                             {
                                 if (player.getName() == users.users[i].nickname)
                                 {
                                     for (int j = 0; j < users.users[i].worlds.size(); ++j)
                                     {
                                         if (world == users.users[i].worlds[j].name)
                                         {
                                             if (lifetime == 0)
                                             {
                                                 users.users[i].worlds[j].permissions.push_back(opval + ":0");

                                             }
                                             else
                                             {
                                                 users.users[i].worlds[j].permissions.push_back(opval + ":" + to_string(lifetime));
                                             }
                                         }
                                     }
                                     break;
                                 }
                             }
                             remove("plugins/Permissions Ex/users.yml");
                             node.reset();
                             for (auto us : users.users)
                                 node["users"].push_back(us);
                             ofstream fout("plugins/Permissions Ex/users.yml");
                             fout << node;
                             fout.close();
                             output.success(utf8_encode(L"[Permissions Ex]: Временое право выдано успешно!"));
                             return;
                         }
                        }
                        output.error(error_msg);
                        return;
                     }
                     case User_Group_Operation::Remove:
                     {
                         jdc1:
                         string dim;
                         if (ori.getDimension()->getDimensionId() == 0)
                             dim = "OverWorld";
                         else if (ori.getDimension()->getDimensionId() == 1)
                             dim = "Nether";
                         else if (ori.getDimension()->getDimensionId() == 2)
                             dim = "End";
                         string error_msg = get_msg("permissionDenied"), error_msg1 = get_msg("invalidArgument");
                         if (player.getName() == "" || opval1 == "")
                         {
                             output.error(error_msg1);
                             return;
                         }
                         _Groups groups;
                         YAML::Node node = YAML::LoadFile("plugins/Permissions Ex/groups.yml");
                         for (const auto& k : node["groups"])
                             groups.groups.push_back(k.as<_Group>());
                         bool is_group = false;
                         for (auto g : groups.groups)
                         {
                             if (opval1 == g.name)
                             {
                                 is_group = true;
                                 break;
                             }
                         }
                         if (is_group)
                         {
                             string perm = "permissions.manage.membership." + player.getName();
                             if (ori.getPermissionsLevel() == CommandPermissionLevel::Console && world == "")
                             {
                                 Users users;
                                 auto node = YAML::LoadFile("plugins/Permissions Ex/users.yml");
                                 for (const auto& p : node["users"])
                                 {
                                     users.users.push_back(p.as<_User>());
                                 }
                                 for (int i = 0; i < users.users.size(); ++i)
                                 {
                                     if (player.getName() == users.users[i].nickname)
                                     {
                                         for (int j = 0; j < users.users[i].groups.size(); ++j)
                                         {
                                             if (opval1 == users.users[i].groups[j])
                                             {
                                                 auto sz = users.users[i].groups.size();
                                                 users.users[i].groups.erase(users.users[i].groups.begin() + j, users.users[i].groups.begin() + j);
                                                 users.users[i].groups.resize(sz - 1);
                                                 if (users.users[i].groups.size() == 0)
                                                 {
                                                     for (auto vv : groups.groups)
                                                     {
                                                         if (vv.is_default == true)
                                                         {
                                                             users.users[i].groups.push_back(vv.name);
                                                             users.users[i].prefix = vv.prefix;
                                                             users.users[i].suffix = vv.suffix;
                                                             break;
                                                         }
                                                     }
                                                     break;
                                                 }
                                                 else
                                                 {
                                                     auto def = load_group(users.users[i].groups[users.users[i].groups.size() - 1]);
                                                     users.users[i].prefix = def.prefix;
                                                     users.users[i].suffix = def.suffix;
                                                     break;
                                                 }
                                             }
                                         }
                                     }
                                 }
                                 remove("plugins/Permissions Ex/users.yml");
                                 node.reset();
                                 for (auto us : users.users)
                                     node["users"].push_back(us);
                                 ofstream fout("plugins/Permissions Ex/users.yml");
                                 fout << node;
                                 fout.close();
                                 output.success(utf8_encode(L"[Permissions Ex]: Временная група забрана успешно!"));
                                 return;
                             }
                             if (ori.getPermissionsLevel() == CommandPermissionLevel::Console && world != "")
                             {
                                 Users users;
                                 auto node = YAML::LoadFile("plugins/Permissions Ex/users.yml");
                                 for (const auto& p : node["users"])
                                 {
                                     users.users.push_back(p.as<_User>());
                                 }
                                 for (int i = 0; i < users.users.size(); ++i)
                                 {
                                     if (player.getName() == users.users[i].nickname)
                                     {
                                         for (auto vv : groups.groups)
                                         {
                                             if (vv.is_default == true)
                                             {
                                                 for (int j = 0;j < users.users[i].worlds.size(); ++j)
                                                 {
                                                     if (world == users.users[i].worlds[j].name)
                                                     {
                                                         users.users[i].worlds[j].group = vv.name;
                                                         users.users[i].worlds[j].prefix = vv.prefix;
                                                         users.users[i].worlds[j].suffix = vv.suffix;
                                                         break;
                                                     }
                                                 }
                                             }
                                         }
                                     }
                                 }
                                 remove("plugins/Permissions Ex/users.yml");
                                 node.reset();
                                 for (auto us : users.users)
                                     node["users"].push_back(us);
                                 ofstream fout("plugins/Permissions Ex/users.yml");
                                 fout << node;
                                 fout.close();
                                 output.success(utf8_encode(L"[Permissions Ex]: Временная група забрана успешно!"));
                                 return;
                             }
                             if ((checkPerm(ori.getPlayer()->getName(), perm) || checkPerm(ori.getPlayer()->getName(), "plugins.*") || checkPerm(ori.getPlayer()->getName(), "permissions.*") || checkPermWorlds(ori.getPlayer()->getName(), perm, dim) || checkPermWorlds(ori.getPlayer()->getName(), "plugins.*", dim) || checkPermWorlds(ori.getPlayer()->getName(), "permissions.*", dim)) && world == "")
                             {
                                 Users users;
                                 auto node = YAML::LoadFile("plugins/Permissions Ex/users.yml");
                                 for (const auto& p : node["users"])
                                 {
                                     users.users.push_back(p.as<_User>());
                                 }
                                 for (int i = 0; i < users.users.size(); ++i)
                                 {
                                     if (player.getName() == users.users[i].nickname)
                                     {
                                         for (int j = 0; j < users.users[i].groups.size(); ++j)
                                         {
                                             if (opval1 == users.users[i].groups[j])
                                             {
                                                 auto sz = users.users[i].groups.size();
                                                 users.users[i].groups.erase(users.users[i].groups.begin() + j, users.users[i].groups.begin() + j);
                                                 users.users[i].groups.resize(sz - 1);
                                                 if (users.users[i].groups.size() == 0)
                                                 {
                                                     for (auto vv : groups.groups)
                                                     {
                                                         if (vv.is_default == true)
                                                         {
                                                             users.users[i].groups.push_back(vv.name);
                                                             users.users[i].prefix = vv.prefix;
                                                             users.users[i].suffix = vv.suffix;
                                                             break;
                                                         }
                                                     }
                                                     break;
                                                 }
                                                 else
                                                 {
                                                     auto def = load_group(users.users[i].groups[users.users[i].groups.size() - 1]);
                                                     users.users[i].prefix = def.prefix;
                                                     users.users[i].suffix = def.suffix;
                                                     break;
                                                 }
                                             }
                                         }
                                     }
                                 }
                                 remove("plugins/Permissions Ex/users.yml");
                                 node.reset();
                                 for (auto us : users.users)
                                     node["users"].push_back(us);
                                 ofstream fout("plugins/Permissions Ex/users.yml");
                                 fout << node;
                                 fout.close();
                                 output.success(utf8_encode(L"[Permissions Ex]: Временная група забрана успешно!"));
                                 return;
                             }
                             else if (checkPerm(ori.getPlayer()->getName(), perm) || checkPerm(ori.getPlayer()->getName(), "plugins.*") || checkPerm(ori.getPlayer()->getName(), "permissions.*") || checkPermWorlds(ori.getPlayer()->getName(), perm, dim) || checkPermWorlds(ori.getPlayer()->getName(), "plugins.*", dim) || checkPermWorlds(ori.getPlayer()->getName(), "permissions.*", dim) && world == "")
                             {
                                 Users users;
                                 auto node = YAML::LoadFile("plugins/Permissions Ex/users.yml");
                                 for (const auto& p : node["users"])
                                 {
                                     users.users.push_back(p.as<_User>());
                                 }
                                 for (int i = 0; i < users.users.size(); ++i)
                                 {
                                     if (player.getName() == users.users[i].nickname)
                                     {
                                         for (auto vv : groups.groups)
                                         {
                                             if (vv.is_default == true)
                                             {
                                                 for (int j = 0; j < users.users[i].worlds.size(); ++j)
                                                 {
                                                     if (world == users.users[i].worlds[j].name)
                                                     {
                                                         users.users[i].worlds[j].group = vv.name;
                                                         users.users[i].worlds[j].prefix = vv.prefix;
                                                         users.users[i].worlds[j].suffix = vv.suffix;
                                                         break;
                                                     }
                                                 }
                                             }
                                         }
                                     }
                                 }
                                 remove("plugins/Permissions Ex/users.yml");
                                 node.reset();
                                 for (auto us : users.users)
                                     node["users"].push_back(us);
                                 ofstream fout("plugins/Permissions Ex/users.yml");
                                 fout << node;
                                 fout.close();
                                 output.success(utf8_encode(L"[Permissions Ex]: Временная група забрана успешно!"));
                                 return;
                             }
                         }
                         else if (!is_group)
                         {
                             string perm = "permissions.manage.users.permissions.timed." + player.getName();
                             if (ori.getPermissionsLevel() == CommandPermissionLevel::Console && world == "")
                             {
                                 Users users;
                                 auto node = YAML::LoadFile("plugins/Permissions Ex/users.yml");
                                 for (const auto& p : node["users"])
                                 {
                                     users.users.push_back(p.as<_User>());
                                 }
                                 for (int i = 0; i < users.users.size(); ++i)
                                 {
                                     if (player.getName() == users.users[i].nickname)
                                     {
                                         for (int j = 0; j < users.users[i].permissions.size(); ++j)
                                         {
                                             regex r(user_permission);
                                             smatch sm;
                                             if (regex_search(users.users[i].permissions[j], sm, r))
                                             {
                                                 auto sz = users.users[i].permissions.size();
                                                 users.users[i].permissions.erase(users.users[i].permissions.begin() + i, users.users[i].permissions.begin() + i);
                                                 users.users[i].permissions.resize(sz - 1);
                                                 break;
                                             }
                                         }
                                     }
                                 }
                                 remove("plugins/Permissions Ex/users.yml");
                                 node.reset();
                                 for (auto us : users.users)
                                     node["users"].push_back(us);
                                 ofstream fout("plugins/Permissions Ex/users.yml");
                                 fout << node;
                                 fout.close();
                                 output.success(utf8_encode(L"[Permissions Ex]: Временое право забрано успешно!"));
                                 return;
                             }
                             if (ori.getPermissionsLevel() == CommandPermissionLevel::Console && world != "")
                             {
                                 Users users;
                                 auto node = YAML::LoadFile("plugins/Permissions Ex/users.yml");
                                 for (const auto& p : node["users"])
                                 {
                                     users.users.push_back(p.as<_User>());
                                 }
                                 for (int i = 0; i < users.users.size(); ++i)
                                 {
                                     if (player.getName() == users.users[i].nickname)
                                     {
                                         for (int j = 0; j < users.users[i].worlds.size(); ++j)
                                         {
                                             if (world == users.users[i].worlds[j].name)
                                             {
                                                 for (int j1 = 0; j1 < users.users[i].worlds[j].permissions.size(); ++j1)
                                                 {
                                                     regex r(user_permission);
                                                     smatch sm;
                                                     if (regex_search(users.users[i].worlds[j].permissions[j1], sm, r))
                                                     {
                                                         auto sz = users.users[i].worlds[j].permissions.size();
                                                         users.users[i].worlds[j].permissions.erase(users.users[i].permissions.begin() + i, users.users[i].permissions.begin() + i);
                                                         users.users[i].worlds[j].permissions.resize(sz - 1);
                                                         break;
                                                     }
                                                 }
                                             }
                                         }
                                     }
                                 }
                                 remove("plugins/Permissions Ex/users.yml");
                                 node.reset();
                                 for (auto us : users.users)
                                     node["users"].push_back(us);
                                 ofstream fout("plugins/Permissions Ex/users.yml");
                                 fout << node;
                                 fout.close();
                                 output.success(utf8_encode(L"[Permissions Ex]: Временое право забрано успешно!"));
                                 return;
                             }
                             if ((checkPerm(ori.getPlayer()->getName(), perm) || checkPerm(ori.getPlayer()->getName(), "plugins.*") || checkPerm(ori.getPlayer()->getName(), "permissions.*") || checkPermWorlds(ori.getPlayer()->getName(), perm, dim) || checkPermWorlds(ori.getPlayer()->getName(), "plugins.*", dim) || checkPermWorlds(ori.getPlayer()->getName(), "permissions.*", dim)) && world == "")
                             {
                                 Users users;
                                 auto node = YAML::LoadFile("plugins/Permissions Ex/users.yml");
                                 for (const auto& p : node["users"])
                                 {
                                     users.users.push_back(p.as<_User>());
                                 }
                                 for (int i = 0; i < users.users.size(); ++i)
                                 {
                                     if (player.getName() == users.users[i].nickname)
                                     {
                                         for (int j = 0; j < users.users[i].permissions.size(); ++j)
                                         {
                                             regex r(user_permission);
                                             smatch sm;
                                             if (regex_search(users.users[i].permissions[j], sm, r))
                                             {
                                                 auto sz = users.users[i].permissions.size();
                                                 users.users[i].permissions.erase(users.users[i].permissions.begin() + i, users.users[i].permissions.begin() + i);
                                                 users.users[i].permissions.resize(sz - 1);
                                                 break;
                                             }
                                         }
                                     }
                                 }
                                 remove("plugins/Permissions Ex/users.yml");
                                 node.reset();
                                 for (auto us : users.users)
                                     node["users"].push_back(us);
                                 ofstream fout("plugins/Permissions Ex/users.yml");
                                 fout << node;
                                 fout.close();
                                 output.success(utf8_encode(L"[Permissions Ex]: Временое право забрано успешно!"));
                                 return;
                             }
                             else if (checkPerm(ori.getPlayer()->getName(), perm) || checkPerm(ori.getPlayer()->getName(), "plugins.*") || checkPerm(ori.getPlayer()->getName(), "permissions.*") || checkPermWorlds(ori.getPlayer()->getName(), perm, dim) || checkPermWorlds(ori.getPlayer()->getName(), "plugins.*", dim) || checkPermWorlds(ori.getPlayer()->getName(), "permissions.*", dim) && world == "")
                             {
                                 Users users;
                                 auto node = YAML::LoadFile("plugins/Permissions Ex/users.yml");
                                 for (const auto& p : node["users"])
                                 {
                                     users.users.push_back(p.as<_User>());
                                 }
                                 for (int i = 0; i < users.users.size(); ++i)
                                 {
                                     if (player.getName() == users.users[i].nickname)
                                     {
                                         for (int j = 0; j < users.users[i].worlds.size(); ++j)
                                         {
                                             if (world == users.users[i].worlds[j].name)
                                             {
                                                 for (int j1 = 0; j1 < users.users[i].worlds[j].permissions.size(); ++j1)
                                                 {
                                                     regex r(user_permission);
                                                     smatch sm;
                                                     if (regex_search(users.users[i].worlds[j].permissions[j1], sm, r))
                                                     {
                                                         auto sz = users.users[i].worlds[j].permissions.size();
                                                         users.users[i].worlds[j].permissions.erase(users.users[i].permissions.begin() + i, users.users[i].permissions.begin() + i);
                                                         users.users[i].worlds[j].permissions.resize(sz - 1);
                                                         break;
                                                     }
                                                 }
                                             }
                                         }
                                     }
                                 }
                                 remove("plugins/Permissions Ex/users.yml");
                                 node.reset();
                                 for (auto us : users.users)
                                     node["users"].push_back(us);
                                 ofstream fout("plugins/Permissions Ex/users.yml");
                                 fout << node;
                                 fout.close();
                                 output.success(utf8_encode(L"[Permissions Ex]: Временое право забрано успешно!"));
                                 return;
                             }
                         
                         }
                         output.error(error_msg);
                         return;
                     }
                     case User_Group_Operation::Set:
                     {
                     jdc2:
                         string dim;
                         if (ori.getDimension()->getDimensionId() == 0)
                             dim = "OverWorld";
                         else if (ori.getDimension()->getDimensionId() == 1)
                             dim = "Nether";
                         else if (ori.getDimension()->getDimensionId() == 2)
                             dim = "End";
                         string error_msg = get_msg("permissionDenied"), error_msg1 = get_msg("invalidArgument");
                         string perm = "permissions.manage.membership." + player.getName();
                         if (player.getName() == "" || opval2 == "")
                         {
                             output.error(error_msg1);
                             return;
                         }
                         if (ori.getPermissionsLevel() == CommandPermissionLevel::Console && world == "")
                         {
                             Users users;
                             auto node = YAML::LoadFile("plugins/Permissions Ex/users.yml");
                             for (const auto& p : node["users"])
                             {
                                 users.users.push_back(p.as<_User>());
                             }
                             for (int i = 0; i < users.users.size(); ++i)
                             {
                                 if (player.getName() == users.users[i].nickname)
                                 {
                                     auto gr = load_group(opval2);
                                     users.users[i].groups.push_back(opval2);
                                     users.users[i].prefix = gr.prefix;
                                     users.users[i].suffix = gr.suffix;
                                     for (int j = 0; j < gr.worlds.size(); ++j)
                                     {
                                         users.users[i].worlds[j].prefix = gr.worlds[j].prefix;
                                         users.users[i].worlds[j].suffix = gr.worlds[j].suffix;
                                     }
                                     break;
                                 }
                             }
                             remove("plugins/Permissions Ex/users.yml");
                             node.reset();
                             for (auto us : users.users)
                                 node["users"].push_back(us);
                             ofstream fout("plugins/Permissions Ex/users.yml");
                             fout << node;
                             fout.close();
                             output.success(utf8_encode(L"[Permissions Ex]: Группа " + to_wstring(opval2) + L" была выдана игроку " + to_wstring(player.getName()) + L" успешно!"));
                             return;
                         }
                         else if ((checkPerm(ori.getPlayer()->getName(), perm) || checkPerm(ori.getPlayer()->getName(), "plugins.*") || checkPerm(ori.getPlayer()->getName(), "permissions.*") || checkPermWorlds(ori.getPlayer()->getName(), perm, dim) || checkPermWorlds(ori.getPlayer()->getName(), "plugins.*", dim) || checkPermWorlds(ori.getPlayer()->getName(), "permissions.*", dim)) && world == "")
                         {
                             Users users;
                             auto node = YAML::LoadFile("plugins/Permissions Ex/users.yml");
                             for (const auto& p : node["users"])
                             {
                                 users.users.push_back(p.as<_User>());
                             }
                             for (int i = 0; i < users.users.size(); ++i)
                             {
                                 if (player.getName() == users.users[i].nickname)
                                 {
                                     auto gr = load_group(opval2);
                                     users.users[i].groups.push_back(opval2);
                                     users.users[i].prefix = gr.prefix;
                                     users.users[i].suffix = gr.suffix;
                                     for (int j = 0; j < gr.worlds.size(); ++j)
                                     {
                                         users.users[i].worlds[j].prefix = gr.worlds[j].prefix;
                                         users.users[i].worlds[j].suffix = gr.worlds[j].suffix;
                                     }
                                     break;
                                 }
                             }
                             remove("plugins/Permissions Ex/users.yml");
                             node.reset();
                             for (auto us : users.users)
                                 node["users"].push_back(us);
                             ofstream fout("plugins/Permissions Ex/users.yml");
                             fout << node;
                             fout.close();
                             output.success(utf8_encode(L"[Permissions Ex]: Группа " + to_wstring(opval2) + L" была выдана игроку " + to_wstring(player.getName()) + L" успешно!"));
                             return;
                         }
                         if (ori.getPermissionsLevel() == CommandPermissionLevel::Console && world != "")
                         {
                             Users users;
                             auto node = YAML::LoadFile("plugins/Permissions Ex/users.yml");
                             for (const auto& p : node["users"])
                             {
                                 users.users.push_back(p.as<_User>());
                             }
                             for (int i = 0; i < users.users.size(); ++i)
                             {
                                 if (player.getName() == users.users[i].nickname)
                                 {
                                     for (int j = 0; j < users.users[i].worlds.size(); ++j)
                                     {
                                         if (world == users.users[i].worlds[j].name)
                                         {
                                             auto gr = load_group(opval2);
                                             users.users[i].worlds[j].group = opval2;
                                             users.users[i].worlds[j].prefix = gr.prefix;
                                             users.users[i].worlds[j].suffix = gr.suffix;
                                             break;
                                         }
                                     }
                                     break;
                                 }
                             }
                             remove("plugins/Permissions Ex/users.yml");
                             node.reset();
                             for (auto us : users.users)
                                 node["users"].push_back(us);
                             ofstream fout("plugins/Permissions Ex/users.yml");
                             fout << node;
                             fout.close();
                             output.success(utf8_encode(L"[Permissions Ex]: Группа " + to_wstring(opval2) + L" была выдана игроку " + to_wstring(player.getName()) + L" успешно!"));
                             return;
                         }
                         else if ((checkPerm(ori.getPlayer()->getName(), perm) || checkPerm(ori.getPlayer()->getName(), "plugins.*") || checkPerm(ori.getPlayer()->getName(), "permissions.*") || checkPermWorlds(ori.getPlayer()->getName(), perm, dim) || checkPermWorlds(ori.getPlayer()->getName(), "plugins.*", dim) || checkPermWorlds(ori.getPlayer()->getName(), "permissions.*", dim)) && world != "")
                         {
                             Users users;
                             auto node = YAML::LoadFile("plugins/Permissions Ex/users.yml");
                             for (const auto& p : node["users"])
                             {
                                 users.users.push_back(p.as<_User>());
                             }
                             for (int i = 0; i < users.users.size(); ++i)
                             {
                                 if (player.getName() == users.users[i].nickname)
                                 {
                                     for (int j = 0; j < users.users[i].worlds.size(); ++j)
                                     {
                                         if (world == users.users[i].worlds[j].name)
                                         {
                                             auto gr = load_group(opval2);
                                             users.users[i].worlds[j].group = opval2;
                                             users.users[i].worlds[j].prefix = gr.prefix;
                                             users.users[i].worlds[j].suffix = gr.suffix;
                                             break;
                                         }
                                     }
                                     break;
                                 }
                             }
                             remove("plugins/Permissions Ex/users.yml");
                             node.reset();
                             for (auto us : users.users)
                                 node["users"].push_back(us);
                             ofstream fout("plugins/Permissions Ex/users.yml");
                             fout << node;
                             fout.close();
                             output.success(utf8_encode(L"[Permissions Ex]: Группа " + to_wstring(opval2) + L" была выдана игроку " + to_wstring(player.getName()) + L" успешно!"));
                             return;
                         }
                         output.error(error_msg);
                         return;
                     }
                     case User_Group_Operation::List:
                     {
                     jdc3:
                         string dim;
                         if (ori.getDimension()->getDimensionId() == 0)
                             dim = "OverWorld";
                         else if (ori.getDimension()->getDimensionId() == 1)
                             dim = "Nether";
                         else if (ori.getDimension()->getDimensionId() == 2)
                             dim = "End";
                         string error_msg = get_msg("permissionDenied"), error_msg1 = get_msg("invalidArgument");
                         string perm = "permissions.manage.membership." + player.getName();
                         if (player.getName() == "")
                         {
                             output.error(error_msg1);
                             return;
                         }
                         if (ori.getPermissionsLevel() == CommandPermissionLevel::Console && world == "")
                         {
                             Users users;
                             auto node = YAML::LoadFile("plugins/Permissions Ex/users.yml");
                             for (const auto& p : node["users"])
                             {
                                 users.users.push_back(p.as<_User>());
                             }
                             for (auto us : users.users)
                             {
                                 if (player.getName() == us.nickname)
                                 {
                                     string outp;
                                     for (auto g : us.groups)
                                     {
                                         auto gr = load_group(g);
                                         outp += "- " + gr.name + "\n";
                                         for (auto inh : gr.inheritances)
                                         {
                                             if (inh != "")
                                             {
                                                 while (!gr.is_default)
                                                 {
                                                     gr = load_group(inh);
                                                     outp += "- " + gr.name + "\n";
                                                 }
                                             }
                                         }
                                     }
                                     wstring out_msg = L"[Permissions Ex]: " + to_wstring(outp);
                                     output.success(utf8_encode(out_msg));
                                     return;
                                 }
                             }
                         }
                         else if ((checkPerm(ori.getPlayer()->getName(), perm) || checkPerm(ori.getPlayer()->getName(), "plugins.*") || checkPerm(ori.getPlayer()->getName(), "permissions.*") || checkPermWorlds(ori.getPlayer()->getName(), perm, dim) || checkPermWorlds(ori.getPlayer()->getName(), "plugins.*", dim) || checkPermWorlds(ori.getPlayer()->getName(), "permissions.*", dim)) && world == "")
                         {
                             Users users;
                             auto node = YAML::LoadFile("plugins/Permissions Ex/users.yml");
                             for (const auto& p : node["users"])
                             {
                                 users.users.push_back(p.as<_User>());
                             }
                             for (auto us : users.users)
                             {
                                 if (player.getName() == us.nickname)
                                 {
                                     string outp;
                                     for (auto g : us.groups)
                                     {
                                         auto gr = load_group(g);
                                         outp += "- " + gr.name + "\n";
                                         for (auto inh : gr.inheritances)
                                         {
                                             if (inh != "")
                                             {
                                                 while (!gr.is_default)
                                                 {
                                                     gr = load_group(inh);
                                                     outp += "- " + gr.name + "\n";
                                                 }
                                             }
                                         }
                                     }
                                     wstring out_msg = L"[Permissions Ex]: " + to_wstring(outp);
                                     output.success(utf8_encode(out_msg));
                                     return;
                                 }
                             }
                         }
                         if (ori.getPermissionsLevel() == CommandPermissionLevel::Console && world != "")
                         {
                             Users users;
                             auto node = YAML::LoadFile("plugins/Permissions Ex/users.yml");
                             for (const auto& p : node["users"])
                             {
                                 users.users.push_back(p.as<_User>());
                             }
                             for (auto us : users.users)
                             {
                                 if (player.getName() == us.nickname)
                                 {
                                     for (auto j = 0; j < us.worlds.size(); ++j)
                                     {
                                         if (world == us.worlds[j].name)
                                         {
                                             string outp;
                                             auto gr = load_group(us.worlds[j].group);
                                             outp += "- " + gr.name + "\n";
                                             for (auto inh : gr.inheritances)
                                             {
                                                 if (inh != "")
                                                 {
                                                     while (!gr.is_default)
                                                     {
                                                         gr = load_group(inh);
                                                         outp += "- " + gr.name + "\n";
                                                     }
                                                 }
                                             }
                                             wstring out_msg = L"[Permissions Ex]: " + to_wstring(outp);
                                             output.success(utf8_encode(out_msg));
                                             return;
                                         }
                                     }
                                 }
                             }
                         }
                         else if ((checkPerm(ori.getPlayer()->getName(), perm) || checkPerm(ori.getPlayer()->getName(), "plugins.*") || checkPerm(ori.getPlayer()->getName(), "permissions.*") || checkPermWorlds(ori.getPlayer()->getName(), perm, dim) || checkPermWorlds(ori.getPlayer()->getName(), "plugins.*", dim) || checkPermWorlds(ori.getPlayer()->getName(), "permissions.*", dim)) && world != "")
                         {
                             Users users;
                             auto node = YAML::LoadFile("plugins/Permissions Ex/users.yml");
                             for (const auto& p : node["users"])
                             {
                                 users.users.push_back(p.as<_User>());
                             }
                             for (auto us : users.users)
                             {
                                 if (player.getName() == us.nickname)
                                 {
                                     for (auto j = 0; j < us.worlds.size(); ++j)
                                     {
                                         if (world == us.worlds[j].name)
                                         {
                                             string outp;
                                             auto gr = load_group(us.worlds[j].group);
                                             outp += "- " + gr.name + "\n";
                                             for (auto inh : gr.inheritances)
                                             {
                                                 if (inh != "")
                                                 {
                                                     while (!gr.is_default)
                                                     {
                                                         gr = load_group(inh);
                                                         outp += "- " + gr.name + "\n";
                                                     }
                                                 }
                                             }
                                             wstring out_msg = L"[Permissions Ex]: " + to_wstring(outp);
                                             output.success(utf8_encode(out_msg));
                                             return;
                                         }
                                     }
                                 }
                             }
                         }
                         output.error(error_msg);
                         return;
                     }
                 }
             }
           }
       }
        case Operation::Group:
        {
            switch (gr_op)
            {
              case Group_Operation::Prefix:
              {
                  string dim;
                  if (ori.getDimension()->getDimensionId() == 0)
                      dim = "OverWorld";
                  else if (ori.getDimension()->getDimensionId() == 1)
                      dim = "Nether";
                  else if (ori.getDimension()->getDimensionId() == 2)
                      dim = "End";
                  string error_msg = get_msg("permissionDenied"), error_msg1 = get_msg("invalidArgument");
                  string perm = "permissions.manage.groups.prefix." + group;
                  if (group == "")
                  {
                      output.error(error_msg1);
                      return;
                  }
                  if (ori.getPermissionsLevel() == CommandPermissionLevel::Console && world == "")
                  {
                      _Groups groups;
                      YAML::Node node = YAML::LoadFile("plugins/Permissions Ex/groups.yml");
                      for (const auto& p : node["groups"])
                          groups.groups.push_back(p.as<_Group>());
                      string prefix = group_prefix;
                      for (int i = 0; i < prefix.size(); ++i)
                      {
                          if (prefix[i] == '&')
                          {
                              prefix[i] = '§';
                          }
                      }
                      prefix = utf8_encode(to_wstring(prefix));
                      for (int i = 0; i < groups.groups.size(); ++i)
                      {
                          if (group == groups.groups[i].name)
                          {
                              groups.groups[i].prefix = prefix;
                              break;
                          }
                      }
                      remove("plugins/Permissions Ex/groups.yml");
                      node.reset();
                      for (auto gr : groups.groups)
                          node["groups"].push_back(gr);
                      ofstream fout("plugins/Permissions Ex/groups.yml");
                      fout << node;
                      fout.close();
                      output.success(utf8_encode(L"[Permissions Ex]: Префикс группы изменен успешно!"));
                      return;
                  }
                  else if ((checkPerm(ori.getPlayer()->getName(), perm) || checkPerm(ori.getPlayer()->getName(), "plugins.*") || checkPerm(ori.getPlayer()->getName(), "permissions.*") || checkPermWorlds(ori.getPlayer()->getName(), perm, dim) || checkPermWorlds(ori.getPlayer()->getName(), "plugins.*", dim) || checkPermWorlds(ori.getPlayer()->getName(), "permissions.*", dim)) && world == "")
                  {
                      _Groups groups;
                      YAML::Node node = YAML::LoadFile("plugins/Permissions Ex/groups.yml");
                      for (const auto& p : node["groups"])
                          groups.groups.push_back(p.as<_Group>());
                      string prefix = group_prefix;
                      for (int i = 0; i < prefix.size(); ++i)
                      {
                          if (prefix[i] == '&')
                          {
                              prefix[i] = '§';
                          }
                      }
                      prefix = utf8_encode(to_wstring(prefix));
                      for (int i = 0; i < groups.groups.size(); ++i)
                      {
                          if (group == groups.groups[i].name)
                          {
                              groups.groups[i].prefix = prefix;
                              break;
                          }
                      }
                      remove("plugins/Permissions Ex/groups.yml");
                      node.reset();
                      for (auto gr : groups.groups)
                          node["groups"].push_back(gr);
                      ofstream fout("plugins/Permissions Ex/groups.yml");
                      fout << node;
                      fout.close();
                      output.success(utf8_encode(L"[Permissions Ex]: Префикс группы изменен успешно!"));
                      return;
                  }
                  if (ori.getPermissionsLevel() == CommandPermissionLevel::Console && world != "")
                  {
                      _Groups groups;
                      YAML::Node node = YAML::LoadFile("plugins/Permissions Ex/groups.yml");
                      for (const auto& p : node["groups"])
                          groups.groups.push_back(p.as<_Group>());
                      string prefix = group_prefix;
                      for (int i = 0; i < prefix.size(); ++i)
                      {
                          if (prefix[i] == '&')
                          {
                              prefix[i] = '§';
                          }
                      }
                      prefix = utf8_encode(to_wstring(prefix));
                      for (int i = 0; i < groups.groups.size(); ++i)
                      {
                          if (group == groups.groups[i].name)
                          {
                              for (auto j = 0; j < groups.groups[i].worlds.size(); ++j)
                              {
                                  if (world == groups.groups[i].worlds[j].name)
                                  {
                                      groups.groups[i].worlds[j].prefix = prefix;
                                      break;
                                  }
                              }
                          }
                      }
                      remove("plugins/Permissions Ex/groups.yml");
                      node.reset();
                      for (auto gr : groups.groups)
                          node["groups"].push_back(gr);
                      ofstream fout("plugins/Permissions Ex/groups.yml");
                      fout << node;
                      fout.close();
                      output.success(utf8_encode(L"[Permissions Ex]: Префикс группы изменен успешно!"));
                      return;
                  }
                  else if ((checkPerm(ori.getPlayer()->getName(), perm) || checkPerm(ori.getPlayer()->getName(), "plugins.*") || checkPerm(ori.getPlayer()->getName(), "permissions.*") || checkPermWorlds(ori.getPlayer()->getName(), perm, dim) || checkPermWorlds(ori.getPlayer()->getName(), "plugins.*", dim) || checkPermWorlds(ori.getPlayer()->getName(), "permissions.*", dim)) && world != "")
                  {
                      _Groups groups;
                      YAML::Node node = YAML::LoadFile("plugins/Permissions Ex/groups.yml");
                      for (const auto& p : node["groups"])
                          groups.groups.push_back(p.as<_Group>());
                      string prefix = group_prefix;
                      for (int i = 0; i < prefix.size(); ++i)
                      {
                          if (prefix[i] == '&')
                          {
                              prefix[i] = '§';
                          }
                      }
                      prefix = utf8_encode(to_wstring(prefix));
                      for (int i = 0; i < groups.groups.size(); ++i)
                      {
                          if (group == groups.groups[i].name)
                          {
                              for (auto j = 0; j < groups.groups[i].worlds.size(); ++j)
                              {
                                  if (world == groups.groups[i].worlds[j].name)
                                  {
                                      groups.groups[i].worlds[j].prefix = prefix;
                                      break;
                                  }
                              }
                          }
                      }
                      remove("plugins/Permissions Ex/groups.yml");
                      node.reset();
                      for (auto gr : groups.groups)
                          node["groups"].push_back(gr);
                      ofstream fout("plugins/Permissions Ex/groups.yml");
                      fout << node;
                      fout.close();
                      output.success(utf8_encode(L"[Permissions Ex]: Префикс группы изменен успешно!"));
                      return;
                  }
                  output.error(error_msg);
                  return;
              }
              case Group_Operation::Suffix:
              {
                  string dim;
                  if (ori.getDimension()->getDimensionId() == 0)
                      dim = "OverWorld";
                  else if (ori.getDimension()->getDimensionId() == 1)
                      dim = "Nether";
                  else if (ori.getDimension()->getDimensionId() == 2)
                      dim = "End";
                  string error_msg = get_msg("permissionDenied"), error_msg1 = get_msg("invalidArgument");
                  string perm = "permissions.manage.groups.suffix." + group;
                  if (group == "")
                  {
                      output.error(error_msg1);
                      return;
                  }
                  if (ori.getPermissionsLevel() == CommandPermissionLevel::Console && world == "")
                  {
                      _Groups groups;
                      YAML::Node node = YAML::LoadFile("plugins/Permissions Ex/groups.yml");
                      for (const auto& p : node["groups"])
                          groups.groups.push_back(p.as<_Group>());
                      string suffix = group_suffix;
                      for (int i = 0; i < suffix.size(); ++i)
                      {
                          if (suffix[i] == '&')
                          {
                              suffix[i] = '§';
                          }
                      }
                      suffix = utf8_encode(to_wstring(suffix));
                      for (int i = 0; i < groups.groups.size(); ++i)
                      {
                          if (group == groups.groups[i].name)
                          {
                              groups.groups[i].suffix = suffix;
                              break;
                          }
                      }
                      remove("plugins/Permissions Ex/groups.yml");
                      node.reset();
                      for (auto gr : groups.groups)
                          node["groups"].push_back(gr);
                      ofstream fout("plugins/Permissions Ex/groups.yml");
                      fout << node;
                      fout.close();
                      output.success(utf8_encode(L"[Permissions Ex]: Суфикс группы изменен успешно!"));
                      return;
                  }
                  else if ((checkPerm(ori.getPlayer()->getName(), perm) || checkPerm(ori.getPlayer()->getName(), "plugins.*") || checkPerm(ori.getPlayer()->getName(), "permissions.*") || checkPermWorlds(ori.getPlayer()->getName(), perm, dim) || checkPermWorlds(ori.getPlayer()->getName(), "plugins.*", dim) || checkPermWorlds(ori.getPlayer()->getName(), "permissions.*", dim)) && world == "")
                  {
                      _Groups groups;
                      YAML::Node node = YAML::LoadFile("plugins/Permissions Ex/groups.yml");
                      for (const auto& p : node["groups"])
                          groups.groups.push_back(p.as<_Group>());
                      string suffix = group_suffix;
                      for (int i = 0; i < suffix.size(); ++i)
                      {
                          if (suffix[i] == '&')
                          {
                              suffix[i] = '§';
                          }
                      }
                      suffix = utf8_encode(to_wstring(suffix));
                      for (int i = 0; i < groups.groups.size(); ++i)
                      {
                          if (group == groups.groups[i].name)
                          {
                              groups.groups[i].suffix = suffix;
                              break;
                          }
                      }
                      remove("plugins/Permissions Ex/groups.yml");
                      node.reset();
                      for (auto gr : groups.groups)
                          node["groups"].push_back(gr);
                      ofstream fout("plugins/Permissions Ex/groups.yml");
                      fout << node;
                      fout.close();
                      output.success(utf8_encode(L"[Permissions Ex]: Суфикс группы изменен успешно!"));
                      return;
                  }
                  if (ori.getPermissionsLevel() == CommandPermissionLevel::Console && world != "")
                  {
                      _Groups groups;
                      YAML::Node node = YAML::LoadFile("plugins/Permissions Ex/groups.yml");
                      for (const auto& p : node["groups"])
                          groups.groups.push_back(p.as<_Group>());
                      string suffix = group_suffix;
                      for (int i = 0; i < suffix.size(); ++i)
                      {
                          if (suffix[i] == '&')
                          {
                              suffix[i] = '§';
                          }
                      }
                      suffix = utf8_encode(to_wstring(suffix));
                      for (int i = 0; i < groups.groups.size(); ++i)
                      {
                          if (group == groups.groups[i].name)
                          {
                              for (auto j = 0; j < groups.groups[i].worlds.size(); ++j)
                              {
                                  if (world == groups.groups[i].worlds[j].name)
                                  {
                                      groups.groups[i].worlds[j].suffix = suffix;
                                      break;
                                  }
                              }
                          }
                      }
                      remove("plugins/Permissions Ex/groups.yml");
                      node.reset();
                      for (auto gr : groups.groups)
                          node["groups"].push_back(gr);
                      ofstream fout("plugins/Permissions Ex/groups.yml");
                      fout << node;
                      fout.close();
                      output.success(utf8_encode(L"[Permissions Ex]: Суфикс группы изменен успешно!"));
                      return;
                  }
                  else if ((checkPerm(ori.getPlayer()->getName(), perm) || checkPerm(ori.getPlayer()->getName(), "plugins.*") || checkPerm(ori.getPlayer()->getName(), "permissions.*") || checkPermWorlds(ori.getPlayer()->getName(), perm, dim) || checkPermWorlds(ori.getPlayer()->getName(), "plugins.*", dim) || checkPermWorlds(ori.getPlayer()->getName(), "permissions.*", dim)) && world != "")
                  {
                      _Groups groups;
                      YAML::Node node = YAML::LoadFile("plugins/Permissions Ex/groups.yml");
                      for (const auto& p : node["groups"])
                          groups.groups.push_back(p.as<_Group>());
                      string suffix = group_suffix;
                      for (int i = 0; i < suffix.size(); ++i)
                      {
                          if (suffix[i] == '&')
                          {
                              suffix[i] = '§';
                          }
                      }
                      suffix = utf8_encode(to_wstring(suffix));
                      for (int i = 0; i < groups.groups.size(); ++i)
                      {
                          if (group == groups.groups[i].name)
                          {
                              for (auto j = 0; j < groups.groups[i].worlds.size(); ++j)
                              {
                                  if (world == groups.groups[i].worlds[j].name)
                                  {
                                      groups.groups[i].worlds[j].suffix = suffix;
                                      break;
                                  }
                              }
                          }
                      }
                      remove("plugins/Permissions Ex/groups.yml");
                      node.reset();
                      for (auto gr : groups.groups)
                          node["groups"].push_back(gr);
                      ofstream fout("plugins/Permissions Ex/groups.yml");
                      fout << node;
                      fout.close();
                      output.success(utf8_encode(L"[Permissions Ex]: Суфикс группы изменен успешно!"));
                      return;
                  }
                  output.error(error_msg);
                  return;
              }
              case Group_Operation::Create:
              {
                  if (is_delete)
                      goto jcc2;
                  string dim;
                  if (ori.getDimension()->getDimensionId() == 0)
                      dim = "OverWorld";
                  else if (ori.getDimension()->getDimensionId() == 1)
                      dim = "Nether";
                  else if (ori.getDimension()->getDimensionId() == 2)
                      dim = "End";
                  string error_msg = get_msg("permissionDenied"), error_msg1 = get_msg("invalidArgument");
                  string perm = "permissions.manageups.create." + group;
                  if (group == "" || group_prefix == "" || group_suffix == "" || is_default == -1)
                  {
                      output.error(error_msg1);
                      return;
                  }
                  if (ori.getPermissionsLevel() == CommandPermissionLevel::Console)
                  {
                      _Groups groups;
                      YAML::Node node = YAML::LoadFile("plugins/Permissions Ex/groups.yml");
                      for (const auto& p : node["groups"])
                          groups.groups.push_back(p.as<_Group>());
                      _Group gr;
                      gr.name = group;
                      string prefix = group_prefix,suffix = group_suffix;
                      for (int i = 0; i < group_prefix.size(); ++i)
                      {
                          if (group_prefix[i] == '&')
                          {
                              prefix[i] = '§';
                          }
                      }
                      prefix = utf8_encode(to_wstring(prefix));
                      for (int i = 0; i < group_suffix.size(); ++i)
                      {
                          if (group_suffix[i] == '&')
                          {
                              suffix[i] = '§';
                          }
                      }
                      suffix = utf8_encode(to_wstring(suffix));
                      gr.prefix = prefix;
                      gr.suffix = suffix;
                      if (is_default)
                      {
                          for (int i = 0; i < groups.groups.size(); ++i)
                          {
                              if (groups.groups[i].is_default)
                                  groups.groups[i].is_default = false;
                          }
                          gr.is_default = is_default;
                      }
                      else
                      {
                          gr.is_default = is_default;
                      }
                      gr.perms = {};
                      auto spp = split(parent, ":");
                      if (spp.size() == 0)
                      {
                          gr.inheritances.push_back(parent);
                      }
                      else
                      {
                          for (auto xxl : spp)
                          {
                              gr.inheritances.push_back(xxl);
                          }
                      }
                      World overworld;
                      overworld.name = "OverWorld";
                      World nether;
                      nether.name = "Nether";
                      World end;
                      end.name = "End";
                      gr.worlds.push_back(overworld);
                      gr.worlds.push_back(nether);
                      gr.worlds.push_back(end);
                      groups.groups.push_back(gr);
                      remove("plugins/Permissions Ex/groups.yml");
                      node.reset();
                      for (auto gr : groups.groups)
                          node["groups"].push_back(gr);
                      ofstream fout("plugins/Permissions Ex/groups.yml");
                      fout << node;
                      fout.close();
                      output.success(utf8_encode(L"[Permissions Ex]: Группа успешно создана!"));
                      return;
                  }
                  else if ((checkPerm(ori.getPlayer()->getName(), perm) || checkPerm(ori.getPlayer()->getName(), "plugins.*") || checkPerm(ori.getPlayer()->getName(), "permissions.*") || checkPermWorlds(ori.getPlayer()->getName(), perm, dim) || checkPermWorlds(ori.getPlayer()->getName(), "plugins.*", dim) || checkPermWorlds(ori.getPlayer()->getName(), "permissions.*", dim)))
                  {
                      _Groups groups;
                      YAML::Node node = YAML::LoadFile("plugins/Permissions Ex/groups.yml");
                      for (const auto& p : node["groups"])
                          groups.groups.push_back(p.as<_Group>());
                      _Group gr;
                      gr.name = group;
                      gr.prefix = group_prefix;
                      gr.suffix = group_suffix;
                      if (is_default)
                      {
                          for (int i = 0; i < groups.groups.size(); ++i)
                          {
                              if (groups.groups[i].is_default)
                                  groups.groups[i].is_default = false;
                          }
                          gr.is_default = is_default;
                      }
                      else
                      {
                          gr.is_default = is_default;
                      }
                      gr.perms = {};
                      auto spp = split(parent, ":");
                      if (spp.size() == 0)
                      {
                          gr.inheritances.push_back(parent);
                      }
                      else
                      {
                          for (auto xxl : spp)
                          {
                              gr.inheritances.push_back(xxl);
                          }
                      }
                      World overworld;
                      overworld.name = "OverWorld";
                      World nether;
                      nether.name = "Nether";
                      World end;
                      end.name = "End";
                      gr.worlds.push_back(overworld);
                      gr.worlds.push_back(nether);
                      gr.worlds.push_back(end);
                      groups.groups.push_back(gr);
                      remove("plugins/Permissions Ex/groups.yml");
                      node.reset();
                      for (auto gr : groups.groups)
                          node["groups"].push_back(gr);
                      ofstream fout("plugins/Permissions Ex/groups.yml");
                      fout << node;
                      fout.close();
                      output.success(utf8_encode(L"[Permissions Ex]: Группа успешно создана!"));
                      return;
                  }
                  output.error(error_msg);
                  return;
              }
              case Group_Operation::Add:
              {
                  jxd:
                  string dim;
                  if (ori.getDimension()->getDimensionId() == 0)
                      dim = "OverWorld";
                  else if (ori.getDimension()->getDimensionId() == 1)
                      dim = "Nether";
                  else if (ori.getDimension()->getDimensionId() == 2)
                      dim = "End";
                  string error_msg = get_msg("permissionDenied"), error_msg1 = get_msg("invalidArgument");
                  string perm = "permissions.manage.groups.permissions." + group;
                  if (group == "" || group_permission == "")
                  {
                      output.error(error_msg1);
                      return;
                  }
                  if (ori.getPermissionsLevel() == CommandPermissionLevel::Console && world == "")
                  {
                      _Groups groups;
                      YAML::Node node = YAML::LoadFile("plugins/Permissions Ex/groups.yml");
                      for (const auto& p : node["groups"])
                          groups.groups.push_back(p.as<_Group>());
                      for (int i = 0; i < groups.groups.size(); ++i)
                      {
                          if (group == groups.groups[i].name)
                          {
                              groups.groups[i].perms.push_back(group_permission);
                              break;
                          }
                      }
                      remove("plugins/Permissions Ex/groups.yml");
                      node.reset();
                      for (auto gr : groups.groups)
                          node["groups"].push_back(gr);
                      ofstream fout("plugins/Permissions Ex/groups.yml");
                      fout << node;
                      fout.close();
                      output.success(utf8_encode(L"[Permissions Ex]: Право для группы выдано успешно!"));
                      return;
                  }
                  else if ((checkPerm(ori.getPlayer()->getName(), perm) || checkPerm(ori.getPlayer()->getName(), "plugins.*") || checkPerm(ori.getPlayer()->getName(), "permissions.*") || checkPermWorlds(ori.getPlayer()->getName(), perm, dim) || checkPermWorlds(ori.getPlayer()->getName(), "plugins.*", dim) || checkPermWorlds(ori.getPlayer()->getName(), "permissions.*", dim)) && world == "")
                  {
                      _Groups groups;
                      YAML::Node node = YAML::LoadFile("plugins/Permissions Ex/groups.yml");
                      for (const auto& p : node["groups"])
                          groups.groups.push_back(p.as<_Group>());
                      for (int i = 0; i < groups.groups.size(); ++i)
                      {
                          if (group == groups.groups[i].name)
                          {
                              groups.groups[i].perms.push_back(group_permission);
                              break;
                          }
                      }
                      remove("plugins/Permissions Ex/groups.yml");
                      node.reset();
                      for (auto gr : groups.groups)
                          node["groups"].push_back(gr);
                      ofstream fout("plugins/Permissions Ex/groups.yml");
                      fout << node;
                      fout.close();
                      output.success(utf8_encode(L"[Permissions Ex]: Право для группы выдано успешно!"));
                      return;
                  }
                  if (ori.getPermissionsLevel() == CommandPermissionLevel::Console && world != "")
                  {
                      _Groups groups;
                      YAML::Node node = YAML::LoadFile("plugins/Permissions Ex/groups.yml");
                      for (const auto& p : node["groups"])
                          groups.groups.push_back(p.as<_Group>());
                      for (int i = 0; i < groups.groups.size(); ++i)
                      {
                          if (group == groups.groups[i].name)
                          {
                              for (int j = 0; j < groups.groups[i].worlds.size(); ++j)
                              {
                                  if (world == groups.groups[i].worlds[j].name)
                                  {
                                      groups.groups[i].worlds[j].permissions.push_back(group_permission);
                                      break;
                                  }
                              }
                          }
                      }
                      remove("plugins/Permissions Ex/groups.yml");
                      node.reset();
                      for (auto gr : groups.groups)
                          node["groups"].push_back(gr);
                      ofstream fout("plugins/Permissions Ex/groups.yml");
                      fout << node;
                      fout.close();
                      output.success(utf8_encode(L"[Permissions Ex]: Право для группы выдано успешно!"));
                      return;
                  }
                  else if ((checkPerm(ori.getPlayer()->getName(), perm) || checkPerm(ori.getPlayer()->getName(), "plugins.*") || checkPerm(ori.getPlayer()->getName(), "permissions.*") || checkPermWorlds(ori.getPlayer()->getName(), perm, dim) || checkPermWorlds(ori.getPlayer()->getName(), "plugins.*", dim) || checkPermWorlds(ori.getPlayer()->getName(), "permissions.*", dim)) && world != "")
                  {
                      _Groups groups;
                      YAML::Node node = YAML::LoadFile("plugins/Permissions Ex/groups.yml");
                      for (const auto& p : node["groups"])
                          groups.groups.push_back(p.as<_Group>());
                      for (int i = 0; i < groups.groups.size(); ++i)
                      {
                          if (group == groups.groups[i].name)
                          {
                              for (int j = 0; j < groups.groups[i].worlds.size(); ++j)
                              {
                                  if (world == groups.groups[i].worlds[j].name)
                                  {
                                      groups.groups[i].worlds[j].permissions.push_back(group_permission);
                                      break;
                                  }
                              }
                          }
                      }
                      remove("plugins/Permissions Ex/groups.yml");
                      node.reset();
                      for (auto gr : groups.groups)
                          node["groups"].push_back(gr);
                      ofstream fout("plugins/Permissions Ex/groups.yml");
                      fout << node;
                      fout.close();
                      output.success(utf8_encode(L"[Permissions Ex]: Право для группы выдано успешно!"));
                      return;
                  }
                  output.error(error_msg);
                  return;
              }
              case Group_Operation::Remove:
              {
              jcc:
                  string dim;
                  if (ori.getDimension()->getDimensionId() == 0)
                      dim = "OverWorld";
                  else if (ori.getDimension()->getDimensionId() == 1)
                      dim = "Nether";
                  else if (ori.getDimension()->getDimensionId() == 2)
                      dim = "End";
                  string error_msg = get_msg("permissionDenied"), error_msg1 = get_msg("invalidArgument");
                  string perm = "permissions.manage.groups.permissions." + group;
                  if (group == "" || group_permission == "")
                  {
                      output.error(error_msg1);
                      return;
                  }
                  if (ori.getPermissionsLevel() == CommandPermissionLevel::Console && world == "")
                  {
                      _Groups groups;
                      YAML::Node node = YAML::LoadFile("plugins/Permissions Ex/groups.yml");
                      for (const auto& p : node["groups"])
                          groups.groups.push_back(p.as<_Group>());
                      for (int i = 0; i < groups.groups.size(); ++i)
                      {
                          if (group == groups.groups[i].name)
                          {
                              for (int j = 0; j < groups.groups[i].perms.size(); ++j)
                              {
                                  if (group_permission == groups.groups[i].perms[j])
                                  {
                                      auto sz = groups.groups[i].perms.size();
                                      groups.groups[i].perms.erase(groups.groups[i].perms.begin() + j, groups.groups[i].perms.begin() + j);
                                      groups.groups[i].perms.resize(sz - 1);
                                      break;
                                  }
                              }

                              break;
                          }
                      }
                      remove("plugins/Permissions Ex/groups.yml");
                      node.reset();
                      for (auto gr : groups.groups)
                          node["groups"].push_back(gr);
                      ofstream fout("plugins/Permissions Ex/groups.yml");
                      fout << node;
                      fout.close();
                      output.success(utf8_encode(L"[Permissions Ex]: Право у группы забрано успешно!"));
                      return;
                  }
                  if (ori.getPermissionsLevel() == CommandPermissionLevel::Console && world != "")
                  {
                      _Groups groups;
                      YAML::Node node = YAML::LoadFile("plugins/Permissions Ex/groups.yml");
                      for (const auto& p : node["groups"])
                          groups.groups.push_back(p.as<_Group>());
                      for (int i = 0; i < groups.groups.size(); ++i)
                      {
                          if (group == groups.groups[i].name)
                          {
                              for (int j = 0; j < groups.groups[i].worlds.size(); ++j)
                              {
                                  if (world == groups.groups[i].worlds[j].name)
                                  {
                                      for (int j1 = 0; j1 < groups.groups[i].worlds[j].permissions.size(); ++j1)
                                      {
                                          if (group_permission == groups.groups[i].worlds[j].permissions[j1])
                                          {
                                              auto sz = groups.groups[i].worlds[j].permissions.size();
                                              groups.groups[i].worlds[j].permissions.erase(groups.groups[i].worlds[j].permissions.begin() + j1, groups.groups[i].worlds[j].permissions.begin() + j1);
                                              groups.groups[i].worlds[j].permissions.resize(sz - 1);
                                              break;
                                          }
                                      }
                                      break;
                                  }
                              }
                          }
                      }
                      remove("plugins/Permissions Ex/groups.yml");
                      node.reset();
                      for (auto gr : groups.groups)
                          node["groups"].push_back(gr);
                      ofstream fout("plugins/Permissions Ex/groups.yml");
                      fout << node;
                      fout.close();
                      output.success(utf8_encode(L"[Permissions Ex]: Право у группы забрано успешно!"));
                      return;
                  }
                  if ((checkPerm(ori.getPlayer()->getName(), perm) || checkPerm(ori.getPlayer()->getName(), "plugins.*") || checkPerm(ori.getPlayer()->getName(), "permissions.*") || checkPermWorlds(ori.getPlayer()->getName(), perm, dim) || checkPermWorlds(ori.getPlayer()->getName(), "plugins.*", dim) || checkPermWorlds(ori.getPlayer()->getName(), "permissions.*", dim)) && world == "")
                  {
                      _Groups groups;
                      YAML::Node node = YAML::LoadFile("plugins/Permissions Ex/groups.yml");
                      for (const auto& p : node["groups"])
                          groups.groups.push_back(p.as<_Group>());
                      for (int i = 0; i < groups.groups.size(); ++i)
                      {
                          if (group == groups.groups[i].name)
                          {
                              for (int j = 0; j < groups.groups[i].perms.size(); ++j)
                              {
                                  if (group_permission == groups.groups[i].perms[j])
                                  {
                                      auto sz = groups.groups[i].perms.size();
                                      groups.groups[i].perms.erase(groups.groups[i].perms.begin() + j, groups.groups[i].perms.begin() + j);
                                      groups.groups[i].perms.resize(sz - 1);
                                      break;
                                  }
                              }

                              break;
                          }
                      }
                      remove("plugins/Permissions Ex/groups.yml");
                      node.reset();
                      for (auto gr : groups.groups)
                          node["groups"].push_back(gr);
                      ofstream fout("plugins/Permissions Ex/groups.yml");
                      fout << node;
                      fout.close();
                      output.success(utf8_encode(L"[Permissions Ex]: Право у группы забрано успешно!"));
                      return;
                  }
                  else if ((checkPerm(ori.getPlayer()->getName(), perm) || checkPerm(ori.getPlayer()->getName(), "plugins.*") || checkPerm(ori.getPlayer()->getName(), "permissions.*") || checkPermWorlds(ori.getPlayer()->getName(), perm, dim) || checkPermWorlds(ori.getPlayer()->getName(), "plugins.*", dim) || checkPermWorlds(ori.getPlayer()->getName(), "permissions.*", dim)) && world != "")
                  {
                      _Groups groups;
                      YAML::Node node = YAML::LoadFile("plugins/Permissions Ex/groups.yml");
                      for (const auto& p : node["groups"])
                          groups.groups.push_back(p.as<_Group>());
                      for (int i = 0; i < groups.groups.size(); ++i)
                      {
                          if (group == groups.groups[i].name)
                          {
                              for (int j = 0; j < groups.groups[i].worlds.size(); ++j)
                              {
                                  if (world == groups.groups[i].worlds[j].name)
                                  {
                                      for (int j1 = 0; j1 < groups.groups[i].worlds[j].permissions.size(); ++j1)
                                      {
                                          if (group_permission == groups.groups[i].worlds[j].permissions[j1])
                                          {
                                              auto sz = groups.groups[i].worlds[j].permissions.size();
                                              groups.groups[i].worlds[j].permissions.erase(groups.groups[i].worlds[j].permissions.begin() + j1, groups.groups[i].worlds[j].permissions.begin() + j1);
                                              groups.groups[i].worlds[j].permissions.resize(sz - 1);
                                              break;
                                          }
                                      }
                                      break;
                                  }
                              }
                          }
                      }
                      remove("plugins/Permissions Ex/groups.yml");
                      node.reset();
                      for (auto gr : groups.groups)
                          node["groups"].push_back(gr);
                      ofstream fout("plugins/Permissions Ex/groups.yml");
                      fout << node;
                      fout.close();
                      output.success(utf8_encode(L"[Permissions Ex]: Право у группы забрано успешно!"));
                      return;
                  }
                  output.error(error_msg);
                  return;
              }
              case Group_Operation::Delete:
              {
              jcc2:
                  if (is_usradd)
                      goto jex;
                  string dim;
                  if (ori.getDimension()->getDimensionId() == 0)
                      dim = "OverWorld";
                  else if (ori.getDimension()->getDimensionId() == 1)
                      dim = "Nether";
                  else if (ori.getDimension()->getDimensionId() == 2)
                      dim = "End";
                  string error_msg = get_msg("permissionDenied"), error_msg1 = get_msg("invalidArgument");
                  string perm = "permissions.manage.groups.remove." + group;
                  if (is_add)
                  {
                      goto jxd;
                  }
                  if (group == "")
                  {
                      output.error(error_msg1);
                      return;
                  }
                  if (ori.getPermissionsLevel() == CommandPermissionLevel::Console)
                  {
                      _Groups groups;
                      YAML::Node node = YAML::LoadFile("plugins/Permissions Ex/groups.yml");
                      for (const auto& p : node["groups"])
                          groups.groups.push_back(p.as<_Group>());
                      Users users;
                      YAML::Node node2 = YAML::LoadFile("plugins/Permissions Ex/users.yml");
                      for (const auto& p : node2["users"])
                          users.users.push_back(p.as<_User>());
                      for (int i = 0; i < groups.groups.size(); ++i)
                      {
                          if (group == groups.groups[i].name)
                          {
                              auto sz = groups.groups.size();
                              groups.groups.erase(groups.groups.begin() + i, groups.groups.begin() + i);
                              groups.groups.resize(sz - 1);
                              for (int j = 0; j < users.users.size(); ++j)
                              {
                                  for (int jj = 0; jj < users.users[j].groups.size(); ++jj)
                                  {
                                      if (users.users[j].groups[jj] == group)
                                      {
                                          auto sz = users.users[j].groups.size();
                                          users.users[j].groups.erase(users.users[j].groups.begin() + jj, users.users[j].groups.begin() + jj);
                                          users.users[j].groups.resize(sz - 1);
                                          auto gro = load_group(users.users[j].groups[(users.users[j].groups.size() - 1)]);
                                          users.users[j].prefix = gro.prefix;
                                          users.users[j].suffix = gro.suffix;
                                          break;
                                      }
                                  }
                                  for (int j11 = 0; j11 < users.users[j].worlds.size(); ++j11)
                                  {
                                      if (users.users[j].worlds[j11].group == group)
                                      {
                                          for (int i1 = 0; i1 < groups.groups.size(); ++i1)
                                          {
                                              if (groups.groups[i1].is_default)
                                              {
                                                  users.users[j].worlds[j11].group = groups.groups[i1].name;
                                                  users.users[j].worlds[j11].prefix = groups.groups[i1].prefix;
                                                  users.users[j].worlds[j11].suffix = groups.groups[i1].suffix;
                                                  break;
                                              }
                                          }
                                      }
                                  }
                              }
                              break;
                          }
                      }
                      remove("plugins/Permissions Ex/groups.yml");
                      remove("plugins/Permissions Ex/users.yml");
                      node.reset();
                      for (auto gr : groups.groups)
                          node["groups"].push_back(gr);
                      node2.reset();
                      for (auto us :users.users)
                          node2["users"].push_back(us);
                      ofstream fout("plugins/Permissions Ex/groups.yml");
                      fout << node;
                      fout.close();
                      fout.open("plugins/Permissions Ex/users.yml");
                      fout << node2;
                      fout.close();
                      output.success(utf8_encode(L"[Permissions Ex]: Группа удалена успешно!"));
                      return;
                  }
                  else if ((checkPerm(ori.getPlayer()->getName(), perm) || checkPerm(ori.getPlayer()->getName(), "plugins.*") || checkPerm(ori.getPlayer()->getName(), "permissions.*") || checkPermWorlds(ori.getPlayer()->getName(), perm, dim) || checkPermWorlds(ori.getPlayer()->getName(), "plugins.*", dim) || checkPermWorlds(ori.getPlayer()->getName(), "permissions.*", dim)))
                  {
                      _Groups groups;
                      YAML::Node node = YAML::LoadFile("plugins/Permissions Ex/groups.yml");
                      for (const auto& p : node["groups"])
                          groups.groups.push_back(p.as<_Group>());
                      Users users;
                      YAML::Node node2 = YAML::LoadFile("plugins/Permissions Ex/users.yml");
                      for (const auto& p : node2["users"])
                          users.users.push_back(p.as<_User>());
                      for (int i = 0; i < groups.groups.size(); ++i)
                      {
                          if (group == groups.groups[i].name)
                          {
                              auto sz = groups.groups.size();
                              groups.groups.erase(groups.groups.begin() + i, groups.groups.begin() + i);
                              groups.groups.resize(sz - 1);
                              for (int j = 0; j < users.users.size(); ++j)
                              {
                                  for (int jj = 0; jj < users.users[j].groups.size(); ++jj)
                                  {
                                      if (users.users[j].groups[jj] == group)
                                      {
                                          auto sz = users.users[j].groups.size();
                                          users.users[j].groups.erase(users.users[j].groups.begin() + jj, users.users[j].groups.begin() + jj);
                                          users.users[j].groups.resize(sz - 1);
                                          auto gro = load_group(users.users[j].groups[(users.users[j].groups.size() - 1)]);
                                          users.users[j].prefix = gro.prefix;
                                          users.users[j].suffix = gro.suffix;
                                          break;
                                      }
                                  }
                                  for (int j11 = 0; j11 < users.users[j].worlds.size(); ++j11)
                                  {
                                      if (users.users[j].worlds[j11].group == group)
                                      {
                                          for (int i1 = 0; i1 < groups.groups.size(); ++i1)
                                          {
                                              if (groups.groups[i1].is_default)
                                              {
                                                  users.users[j].worlds[j11].group = groups.groups[i1].name;
                                                  users.users[j].worlds[j11].prefix = groups.groups[i1].prefix;
                                                  users.users[j].worlds[j11].suffix = groups.groups[i1].suffix;
                                                  break;
                                              }
                                          }
                                      }
                                  }
                              }
                              break;
                          }
                      }
                      remove("plugins/Permissions Ex/groups.yml");
                      remove("plugins/Permissions Ex/users.yml");
                      node.reset();
                      for (auto gr : groups.groups)
                          node["groups"].push_back(gr);
                      node2.reset();
                      for (auto us : users.users)
                          node2["users"].push_back(us);
                      ofstream fout("plugins/Permissions Ex/groups.yml");
                      fout << node;
                      fout.close();
                      fout.open("plugins/Permissions Ex/users.yml");
                      fout << node2;
                      fout.close();
                      output.success(utf8_encode(L"[Permissions Ex]: Группа удалена успешно!"));
                      return;
                  }
                  output.error(error_msg);
                  return;
              }
              case Group_Operation::Parents:
              {
              jar:
                  if (is_users)
                      goto jet;
                  if (is_usrremove)
                      goto jex1;
                  if (is_remove)
                      goto jcc;
                  string dim;
                  if (ori.getDimension()->getDimensionId() == 0)
                      dim = "OverWorld";
                  else if (ori.getDimension()->getDimensionId() == 1)
                      dim = "Nether";
                  else if (ori.getDimension()->getDimensionId() == 2)
                      dim = "End";
                  string error_msg = get_msg("permissionDenied"), error_msg1 = get_msg("invalidArgument");
                  if (!is_parentset)
                  {
                      string perm = "permissions.manage.groups.inheritance." + group;
                      if (group == "")
                      {
                          output.error(error_msg1);
                          return;
                      }
                      if (ori.getPermissionsLevel() == CommandPermissionLevel::Console && world == "")
                      {
                          wstring outp = L"[Permissions Ex]: Список родительских групп\n";
                          auto gr = load_group(group);
                          _Group gr1;
                          if (gr.inheritances.size() != 0)
                          {
                              for (auto xh : gr.inheritances)
                              {
                                  if (xh != "")
                                  {
                                      gr1 = load_group(xh);
                                  }
                                  else
                                  {
                                      output.success(utf8_encode(outp));
                                      return;
                                  }
                                  outp += to_wstring(gr1.name);
                                  while (!gr1.is_default)
                                  {
                                      outp += L"- " + to_wstring(gr.name) + L"\n";
                                  }
                              }
                          }
                          output.success(utf8_encode(outp));
                          return;
                      }
                      if (ori.getPermissionsLevel() == CommandPermissionLevel::Console && world != "")
                      {
                          wstring outp = L"[Permissions Ex]: Список родительских групп\n";
                          auto gr = load_group(group);
                          _Group gr1;
                          for (auto worl : gr.worlds)
                          {
                              if (worl.inheritances.size() != 0)
                              {
                                  for (auto xh : worl.inheritances)
                                  {
                                      if (xh != "")
                                      {
                                          gr1 = load_group(xh);
                                      }
                                      else
                                      {
                                          output.success(utf8_encode(outp));
                                          return;
                                      }
                                      outp += to_wstring(gr1.name);
                                      while (!gr1.is_default)
                                      {
                                          outp += L"- " + to_wstring(gr.name) + L"\n";
                                      }
                                  }
                              }
                              output.success(utf8_encode(outp));
                              return;
                          }
                          output.success(utf8_encode(outp));
                          return;
                      }
                      if ((checkPerm(ori.getPlayer()->getName(), perm) || checkPerm(ori.getPlayer()->getName(), "plugins.*") || checkPerm(ori.getPlayer()->getName(), "permissions.*") || checkPermWorlds(ori.getPlayer()->getName(), perm, dim) || checkPermWorlds(ori.getPlayer()->getName(), "plugins.*", dim) || checkPermWorlds(ori.getPlayer()->getName(), "permissions.*", dim)) && world == "")
                      {
                          wstring outp = L"[Permissions Ex]: Список родительских групп\n";
                          auto gr = load_group(group);
                          _Group gr1;
                          if (gr.inheritances.size() != 0)
                          {
                              for (auto xh : gr.inheritances)
                              {
                                  if (xh != "")
                                  {
                                      gr1 = load_group(xh);
                                  }
                                  else
                                  {
                                      output.success(utf8_encode(outp));
                                      return;
                                  }
                                  outp += to_wstring(gr1.name);
                                  while (!gr1.is_default)
                                  {
                                      outp += L"- " + to_wstring(gr.name) + L"\n";
                                  }
                              }
                          }
                          output.success(utf8_encode(outp));
                          return;
                      }
                      else if ((checkPerm(ori.getPlayer()->getName(), perm) || checkPerm(ori.getPlayer()->getName(), "plugins.*") || checkPerm(ori.getPlayer()->getName(), "permissions.*") || checkPermWorlds(ori.getPlayer()->getName(), perm, dim) || checkPermWorlds(ori.getPlayer()->getName(), "plugins.*", dim) || checkPermWorlds(ori.getPlayer()->getName(), "permissions.*", dim)) && world != "")
                      {
                          wstring outp = L"[Permissions Ex]: Список родительских групп\n";
                          auto gr = load_group(group);
                          _Group gr1;
                          for (auto worl : gr.worlds)
                          {
                              if (worl.inheritances.size() != 0)
                              {
                                  for (auto xh : worl.inheritances)
                                  {
                                      if (xh != "")
                                      {
                                          gr1 = load_group(xh);
                                      }
                                      else
                                      {
                                          output.success(utf8_encode(outp));
                                          return;
                                      }
                                      outp += to_wstring(gr1.name);
                                      while (!gr1.is_default)
                                      {
                                          outp += L"- " + to_wstring(gr.name) + L"\n";
                                      }
                                  }
                              }
                              output.success(utf8_encode(outp));
                              return;
                          }
                          output.success(utf8_encode(outp));
                          return;
                      }
                 }
                 else if (is_parentset)
                 {
                   string perm = "permissions.manage.groups.inheritance." + group;
                   if (group == "")
                   {
                     output.error(error_msg1);
                     return;
                   }
                   if (ori.getPermissionsLevel() == CommandPermissionLevel::Console && world == "")
                   {
                       YAML::Node node = YAML::LoadFile("plugins/Permissions Ex/groups.yml");
                       _Groups groups;
                       for (const auto& p : node["groups"])
                           groups.groups.push_back(p.as<_Group>());
                       for (int i = 0; i < groups.groups.size(); ++i)
                       {
                           if (group == groups.groups[i].name)
                           {
                               if (groups.groups[i].inheritances.size() == 1 && groups.groups[i].inheritances[0] == "")
                                   groups.groups[i].inheritances.clear();
                               groups.groups[i].inheritances.push_back(parent);
                               break;
                           }
                       }
                       remove("plugins/Permissions Ex/groups.yml");
                       node.reset();
                       for (auto gr : groups.groups)
                           node["groups"].push_back(gr);
                       ofstream fout("plugins/Permissions Ex/groups.yml");
                       fout << node;
                       fout.close();
                       output.success(utf8_encode(L"[Permissions Ex]: Наследник для группы установлен успешно"));
                       return;
                   }
                   if (ori.getPermissionsLevel() == CommandPermissionLevel::Console && world != "")
                   {
                       YAML::Node node = YAML::LoadFile("plugins/Permissions Ex/groups.yml");
                       _Groups groups;
                       for (const auto& p : node["groups"])
                           groups.groups.push_back(p.as<_Group>());
                       for (int i = 0; i < groups.groups.size(); ++i)
                       {
                           if (group == groups.groups[i].name)
                           {
                               for (int j = 0; j < groups.groups[i].worlds.size(); ++j)
                               {
                                   if (world == groups.groups[i].worlds[j].name)
                                   {
                                       groups.groups[i].worlds[j].inheritances.push_back(parent);
                                       break;
                                   }
                               }
                           }
                       }
                       remove("plugins/Permissions Ex/groups.yml");
                       node.reset();
                       for (auto gr : groups.groups)
                           node["groups"].push_back(gr);
                       ofstream fout("plugins/Permissions Ex/groups.yml");
                       fout << node;
                       fout.close();
                       output.success(utf8_encode(L"[Permissions Ex]: Наследник для группы установлен успешно"));
                       return;
                   }
                   if ((checkPerm(ori.getPlayer()->getName(), perm) || checkPerm(ori.getPlayer()->getName(), "plugins.*") || checkPerm(ori.getPlayer()->getName(), "permissions.*") || checkPermWorlds(ori.getPlayer()->getName(), perm, dim) || checkPermWorlds(ori.getPlayer()->getName(), "plugins.*", dim) || checkPermWorlds(ori.getPlayer()->getName(), "permissions.*", dim)) && world == "")
                   {
                       YAML::Node node = YAML::LoadFile("plugins/Permissions Ex/groups.yml");
                       _Groups groups;
                       for (const auto& p : node["groups"])
                           groups.groups.push_back(p.as<_Group>());
                       for (int i = 0; i < groups.groups.size(); ++i)
                       {
                           if (group == groups.groups[i].name)
                           {
                               if (groups.groups[i].inheritances.size() == 1 && groups.groups[i].inheritances[0] == "")
                                   groups.groups[i].inheritances.clear();
                               groups.groups[i].inheritances.push_back(parent);
                               break;
                           }
                       }
                       remove("plugins/Permissions Ex/groups.yml");
                       node.reset();
                       for (auto gr : groups.groups)
                           node["groups"].push_back(gr);
                       ofstream fout("plugins/Permissions Ex/groups.yml");
                       fout << node;
                       fout.close();
                       output.success(utf8_encode(L"[Permissions Ex]: Наследник для группы установлен успешно"));
                       return;
                   }
                   else if ((checkPerm(ori.getPlayer()->getName(), perm) || checkPerm(ori.getPlayer()->getName(), "plugins.*") || checkPerm(ori.getPlayer()->getName(), "permissions.*") || checkPermWorlds(ori.getPlayer()->getName(), perm, dim) || checkPermWorlds(ori.getPlayer()->getName(), "plugins.*", dim) || checkPermWorlds(ori.getPlayer()->getName(), "permissions.*", dim)) && world != "")
                   {
                       YAML::Node node = YAML::LoadFile("plugins/Permissions Ex/groups.yml");
                       _Groups groups;
                       for (const auto& p : node["groups"])
                           groups.groups.push_back(p.as<_Group>());
                       for (int i = 0; i < groups.groups.size(); ++i)
                       {
                           if (group == groups.groups[i].name)
                           {
                               for (int j = 0; j < groups.groups[i].worlds.size(); ++j)
                               {
                                   if (world == groups.groups[i].worlds[j].name)
                                   {
                                       groups.groups[i].worlds[j].inheritances.push_back(parent);
                                       break;
                                   }
                               }
                           }
                       }
                       remove("plugins/Permissions Ex/groups.yml");
                       node.reset();
                       for (auto gr : groups.groups)
                           node["groups"].push_back(gr);
                       ofstream fout("plugins/Permissions Ex/groups.yml");
                       fout << node;
                       fout.close();
                       output.success(utf8_encode(L"[Permissions Ex]: Наследник для группы установлен успешно"));
                       return;
                   }
                 }
                 output.error(error_msg);
                 return;
              }
              case Group_Operation::Timed:
              {
                  switch (gr_tm_op)
                  {
                    case Group_Timed_Operation::Add:
                    {
                        java:
                        string dim;
                        if (ori.getDimension()->getDimensionId() == 0)
                            dim = "OverWorld";
                        else if (ori.getDimension()->getDimensionId() == 1)
                            dim = "Nether";
                        else if (ori.getDimension()->getDimensionId() == 2)
                            dim = "End";
                        string error_msg = get_msg("permissionDenied"), error_msg1 = get_msg("invalidArgument");
                        string perm = "permissions.manage.groups.permissions.timed." + group;
                        if (group == "" || group_permission == "")
                        {
                            output.error(error_msg1);
                            return;
                        }
                        if (ori.getPermissionsLevel() == CommandPermissionLevel::Console && world == "")
                        {
                            YAML::Node node = YAML::LoadFile("plugins/Permissions Ex/groups.yml");
                            _Groups groups;
                            for (const auto& p : node["groups"])
                                groups.groups.push_back(p.as<_Group>());
                            for (int i = 0; i < groups.groups.size(); ++i)
                            {
                                if (group == groups.groups[i].name)
                                {
                                    groups.groups[i].perms.push_back(group_permission + ":" + to_string(lifetime));
                                    break;
                                }
                            }
                            remove("plugins/Permissions Ex/groups.yml");
                            node.reset();
                            for (auto gr : groups.groups)
                                node["groups"].push_back(gr);
                            ofstream fout("plugins/Permissions Ex/groups.yml");
                            fout << node;
                            fout.close();
                            output.success(utf8_encode(L"[Permissions Ex]: Временное право для группы выдано успешно"));
                            return;
                        }
                        if (ori.getPermissionsLevel() == CommandPermissionLevel::Console && world != "")
                        {
                            YAML::Node node = YAML::LoadFile("plugins/Permissions Ex/groups.yml");
                            _Groups groups;
                            for (const auto& p : node["groups"])
                                groups.groups.push_back(p.as<_Group>());
                            for (int i = 0; i < groups.groups.size(); ++i)
                            {
                                if (group == groups.groups[i].name)
                                {
                                    for (int j = 0; j < groups.groups[i].worlds.size(); ++j)
                                    {
                                        if (world == groups.groups[i].worlds[j].name)
                                        {
                                            groups.groups[i].worlds[j].permissions.push_back(group_permission + ":" + to_string(lifetime));
                                            break;
                                        }
                                    }
                                }
                            }
                            remove("plugins/Permissions Ex/groups.yml");
                            node.reset();
                            for (auto gr : groups.groups)
                                node["groups"].push_back(gr);
                            ofstream fout("plugins/Permissions Ex/groups.yml");
                            fout << node;
                            fout.close();
                            output.success(utf8_encode(L"[Permissions Ex]: Временное право для группы выдано успешно"));
                            return;
                        }
                        if ((checkPerm(ori.getPlayer()->getName(), perm) || checkPerm(ori.getPlayer()->getName(), "plugins.*") || checkPerm(ori.getPlayer()->getName(), "permissions.*") || checkPermWorlds(ori.getPlayer()->getName(), perm, dim) || checkPermWorlds(ori.getPlayer()->getName(), "plugins.*", dim) || checkPermWorlds(ori.getPlayer()->getName(), "permissions.*", dim)) && world == "")
                        {
                            YAML::Node node = YAML::LoadFile("plugins/Permissions Ex/groups.yml");
                            _Groups groups;
                            for (const auto& p : node["groups"])
                                groups.groups.push_back(p.as<_Group>());
                            for (int i = 0; i < groups.groups.size(); ++i)
                            {
                                if (group == groups.groups[i].name)
                                {
                                    groups.groups[i].perms.push_back(group_permission + ":" + to_string(lifetime));
                                    break;
                                }
                            }
                            remove("plugins/Permissions Ex/groups.yml");
                            node.reset();
                            for (auto gr : groups.groups)
                                node["groups"].push_back(gr);
                            ofstream fout("plugins/Permissions Ex/groups.yml");
                            fout << node;
                            fout.close();
                            output.success(utf8_encode(L"[Permissions Ex]: Временное право для группы выдано успешно"));
                            return;
                        }
                        else if ((checkPerm(ori.getPlayer()->getName(), perm) || checkPerm(ori.getPlayer()->getName(), "plugins.*") || checkPerm(ori.getPlayer()->getName(), "permissions.*") || checkPermWorlds(ori.getPlayer()->getName(), perm, dim) || checkPermWorlds(ori.getPlayer()->getName(), "plugins.*", dim) || checkPermWorlds(ori.getPlayer()->getName(), "permissions.*", dim)) && world != "")
                        {
                            YAML::Node node = YAML::LoadFile("plugins/Permissions Ex/groups.yml");
                            _Groups groups;
                            for (const auto& p : node["groups"])
                                groups.groups.push_back(p.as<_Group>());
                            for (int i = 0; i < groups.groups.size(); ++i)
                            {
                                if (group == groups.groups[i].name)
                                {
                                    for (int j = 0; j < groups.groups[i].worlds.size(); ++j)
                                    {
                                        if (world == groups.groups[i].worlds[j].name)
                                        {
                                            groups.groups[i].worlds[j].permissions.push_back(group_permission + ":" + to_string(lifetime));
                                            break;
                                        }
                                    }
                                }
                            }
                            remove("plugins/Permissions Ex/groups.yml");
                            node.reset();
                            for (auto gr : groups.groups)
                                node["groups"].push_back(gr);
                            ofstream fout("plugins/Permissions Ex/groups.yml");
                            fout << node;
                            fout.close();
                            output.success(utf8_encode(L"[Permissions Ex]: Временное право для группы выдано успешно"));
                            return;
                        }
                        output.error(error_msg);
                        return;
                    }
                    case Group_Timed_Operation::Remove:
                    {
                        jar1:
                        string dim;
                        if (ori.getDimension()->getDimensionId() == 0)
                            dim = "OverWorld";
                        else if (ori.getDimension()->getDimensionId() == 1)
                            dim = "Nether";
                        else if (ori.getDimension()->getDimensionId() == 2)
                            dim = "End";
                        string error_msg = get_msg("permissionDenied"), error_msg1 = get_msg("invalidArgument");
                        string perm = "permissions.manage.groups.permissions.timed." + group;
                        if (group == "" || group_permission == "")
                        {
                            output.error(error_msg1);
                            return;
                        }
                        if (ori.getPermissionsLevel() == CommandPermissionLevel::Console && world == "")
                        {
                            YAML::Node node = YAML::LoadFile("plugins/Permissions Ex/groups.yml");
                            _Groups groups;
                            for (const auto& p : node["groups"])
                                groups.groups.push_back(p.as<_Group>());
                            for (int i = 0; i < groups.groups.size(); ++i)
                            {
                                if (group == groups.groups[i].name)
                                {
                                    for (int j = 0; j < groups.groups[i].perms.size(); ++j)
                                    {
                                        auto vec = split(groups.groups[i].perms[j], ":");
                                        if (vec.size() > 0)
                                        {
                                            auto sz = groups.groups[i].perms.size();
                                            groups.groups[i].perms.erase(groups.groups[i].perms.begin() + j, groups.groups[i].perms.begin() + j);
                                            groups.groups[i].perms.resize(sz - 1);
                                            break;
                                        }
                                    }
                                }
                            }
                            remove("plugins/Permissions Ex/groups.yml");
                            node.reset();
                            for (auto gr : groups.groups)
                                node["groups"].push_back(gr);
                            ofstream fout("plugins/Permissions Ex/groups.yml");
                            fout << node;
                            fout.close();
                            output.success(utf8_encode(L"[Permissions Ex]: Временное право для группы забрано успешно"));
                            return;
                        }
                        if (ori.getPermissionsLevel() == CommandPermissionLevel::Console && world != "")
                        {
                            YAML::Node node = YAML::LoadFile("plugins/Permissions Ex/groups.yml");
                            _Groups groups;
                            for (const auto& p : node["groups"])
                                groups.groups.push_back(p.as<_Group>());
                            for (int i = 0; i < groups.groups.size(); ++i)
                            {
                                if (group == groups.groups[i].name)
                                {
                                    for (int h = 0; h < groups.groups[i].worlds.size(); ++h)
                                    {
                                        if (world == groups.groups[i].worlds[h].name)
                                        {
                                            for (int j = 0; j < groups.groups[i].perms.size(); ++j)
                                            {
                                                auto vec = split(groups.groups[i].perms[j], ":");
                                                if (vec.size() > 0)
                                                {
                                                    auto sz = groups.groups[i].worlds[h].permissions.size();
                                                    groups.groups[i].worlds[h].permissions.erase(groups.groups[i].worlds[h].permissions.begin() + j, groups.groups[i].perms.begin() + j);
                                                    groups.groups[i].worlds[h].permissions.resize(sz - 1);
                                                    break;
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                            remove("plugins/Permissions Ex/groups.yml");
                            node.reset();
                            for (auto gr : groups.groups)
                                node["groups"].push_back(gr);
                            ofstream fout("plugins/Permissions Ex/groups.yml");
                            fout << node;
                            fout.close();
                            output.success(utf8_encode(L"[Permissions Ex]: Временное право для группы забрано успешно"));
                            return;
                        }
                        if ((checkPerm(ori.getPlayer()->getName(), perm) || checkPerm(ori.getPlayer()->getName(), "plugins.*") || checkPerm(ori.getPlayer()->getName(), "permissions.*") || checkPermWorlds(ori.getPlayer()->getName(), perm, dim) || checkPermWorlds(ori.getPlayer()->getName(), "plugins.*", dim) || checkPermWorlds(ori.getPlayer()->getName(), "permissions.*", dim)) && world == "")
                        {
                            YAML::Node node = YAML::LoadFile("plugins/Permissions Ex/groups.yml");
                            _Groups groups;
                            for (const auto& p : node["groups"])
                                groups.groups.push_back(p.as<_Group>());
                            for (int i = 0; i < groups.groups.size(); ++i)
                            {
                                if (group == groups.groups[i].name)
                                {
                                    for (int j = 0; j < groups.groups[i].perms.size(); ++j)
                                    {
                                        auto vec = split(groups.groups[i].perms[j], ":");
                                        if (vec.size() > 0)
                                        {
                                            auto sz = groups.groups[i].perms.size();
                                            groups.groups[i].perms.erase(groups.groups[i].perms.begin() + j, groups.groups[i].perms.begin() + j);
                                            groups.groups[i].perms.resize(sz - 1);
                                            break;
                                        }
                                    }
                                }
                            }
                            remove("plugins/Permissions Ex/groups.yml");
                            node.reset();
                            for (auto gr : groups.groups)
                                node["groups"].push_back(gr);
                            ofstream fout("plugins/Permissions Ex/groups.yml");
                            fout << node;
                            fout.close();
                            output.success(utf8_encode(L"[Permissions Ex]: Временное право для группы забрано успешно"));
                            return;
                        }
                        else if ((checkPerm(ori.getPlayer()->getName(), perm) || checkPerm(ori.getPlayer()->getName(), "plugins.*") || checkPerm(ori.getPlayer()->getName(), "permissions.*") || checkPermWorlds(ori.getPlayer()->getName(), perm, dim) || checkPermWorlds(ori.getPlayer()->getName(), "plugins.*", dim) || checkPermWorlds(ori.getPlayer()->getName(), "permissions.*", dim)) && world != "")
                        {
                            YAML::Node node = YAML::LoadFile("plugins/Permissions Ex/groups.yml");
                            _Groups groups;
                            for (const auto& p : node["groups"])
                                groups.groups.push_back(p.as<_Group>());
                            for (int i = 0; i < groups.groups.size(); ++i)
                            {
                                if (group == groups.groups[i].name)
                                {
                                    for (int h = 0; h < groups.groups[i].worlds.size(); ++h)
                                    {
                                        if (world == groups.groups[i].worlds[h].name)
                                        {
                                            for (int j = 0; j < groups.groups[i].perms.size(); ++j)
                                            {
                                                auto vec = split(groups.groups[i].perms[j], ":");
                                                if (vec.size() > 0)
                                                {
                                                    auto sz = groups.groups[i].worlds[h].permissions.size();
                                                    groups.groups[i].worlds[h].permissions.erase(groups.groups[i].worlds[h].permissions.begin() + j, groups.groups[i].perms.begin() + j);
                                                    groups.groups[i].worlds[h].permissions.resize(sz - 1);
                                                    break;
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                            remove("plugins/Permissions Ex/groups.yml");
                            node.reset();
                            for (auto gr : groups.groups)
                                node["groups"].push_back(gr);
                            ofstream fout("plugins/Permissions Ex/groups.yml");
                            fout << node;
                            fout.close();
                            output.success(utf8_encode(L"[Permissions Ex]: Временное право для группы забрано успешно"));
                            return;
                        }
                        output.error(error_msg);
                        return;
                    }
                  }
              }
              case Group_Operation::Users:
              {
              jet:
                  if (is_timeadd)
                      goto java;
                  if (is_timeremove)
                      goto jar1;
                  string dim;
                  if (ori.getDimension()->getDimensionId() == 0)
                      dim = "OverWorld";
                  else if (ori.getDimension()->getDimensionId() == 1)
                      dim = "Nether";
                  else if (ori.getDimension()->getDimensionId() == 2)
                      dim = "End";
                  string error_msg = get_msg("permissionDenied"), error_msg1 = get_msg("invalidArgument");
                  string perm = "permissions.manage.membership." + group;
                  if (group == "")
                  {
                      output.error(error_msg1);
                      return;
                  }
                  if (ori.getPermissionsLevel() == CommandPermissionLevel::Console)
                  {
                      Users users;
                      YAML::Node node = YAML::LoadFile("plugins/Permissions Ex/users.yml");
                      for (const auto& p : node["users"])
                          users.users.push_back(p.as<_User>());
                      wstring outp = L"[Permissions Ex]: Список игроков,состоящих в гурппе:\n";
                      for (auto v : users.users)
                      {
                          for (auto xh : v.groups)
                          {
                              if (xh == group)
                              {
                                  outp += L"- " + to_wstring(v.nickname) + L"\n";
                              }
                          }
                      }
                      output.success(utf8_encode(outp));
                      return;
                  }
                  else if ((checkPerm(ori.getPlayer()->getName(), perm) || checkPerm(ori.getPlayer()->getName(), "plugins.*") || checkPerm(ori.getPlayer()->getName(), "permissions.*") || checkPermWorlds(ori.getPlayer()->getName(), perm, dim) || checkPermWorlds(ori.getPlayer()->getName(), "plugins.*", dim) || checkPermWorlds(ori.getPlayer()->getName(), "permissions.*", dim)))
                  {
                      Users users;
                      YAML::Node node = YAML::LoadFile("plugins/Permissions Ex/users.yml");
                      for (const auto& p : node["users"])
                          users.users.push_back(p.as<_User>());
                      wstring outp = L"[Permissions Ex]: Список игроков,состоящих в гурппе:\n";
                      for (auto v : users.users)
                      {
                          for (auto xh : v.groups)
                          {
                              if (xh == group)
                              {
                                  outp += L"- " + to_wstring(v.nickname) + L"\n";
                              }
                          }
                      }
                      output.success(utf8_encode(outp));
                      return;
                  }
                  output.error(error_msg);
                  return;
              }
              case Group_Operation::User:
              {
                  switch (gr_us_op)
                  {
                    case Group_User_Operation::Add:
                    {
                        jex:
                        string dim;
                        if (ori.getDimension()->getDimensionId() == 0)
                            dim = "OverWorld";
                        else if (ori.getDimension()->getDimensionId() == 1)
                            dim = "Nether";
                        else if (ori.getDimension()->getDimensionId() == 2)
                            dim = "End";
                        string error_msg = get_msg("permissionDenied"), error_msg1 = get_msg("invalidArgument");
                        string perm = "permissions.manage.membership." + group;
                        if (group == "" || player.getName() == "")
                        {
                            output.error(error_msg1);
                            return;
                        }
                        if (ori.getPermissionsLevel() == CommandPermissionLevel::Console && world == "")
                        {
                            Users users;
                            YAML::Node node = YAML::LoadFile("plugins/Permissions Ex/users.yml");
                            for (const auto& p : node["users"])
                                users.users.push_back(p.as<_User>());
                            for (int i = 0; i < users.users.size(); ++i)
                            {
                                if (player.getName() == users.users[i].nickname)
                                {
                                    auto gr = load_group(group);
                                    users.users[i].groups.push_back(group + ":" + to_string(lifetime));
                                    users.users[i].prefix = gr.prefix;
                                    users.users[i].suffix = gr.suffix;
                                    for (int j = 0; j < gr.worlds.size(); ++j)
                                    {
                                        users.users[i].worlds[j].prefix = gr.worlds[j].prefix;
                                        users.users[i].worlds[j].suffix = gr.worlds[j].suffix;
                                    }
                                    break;
                                }
                            }
                            remove("plugins/Permissions Ex/users.yml");
                            node.reset();
                            for (auto gr : users.users)
                                node["users"].push_back(gr);
                            ofstream fout("plugins/Permissions Ex/users.yml");
                            fout << node;
                            fout.close();
                            output.success(utf8_encode(L"[Permissions Ex]: Игроку была выдана временная группа успешно!"));
                            return;
                        }
                        if (ori.getPermissionsLevel() == CommandPermissionLevel::Console && world != "")
                        {
                            Users users;
                            YAML::Node node = YAML::LoadFile("plugins/Permissions Ex/users.yml");
                            for (const auto& p : node["users"])
                                users.users.push_back(p.as<_User>());
                            for (int i = 0; i < users.users.size(); ++i)
                            {
                                if (player.getName() == users.users[i].nickname)
                                {
                                    for (int j = 0; j < users.users[i].worlds.size(); ++j)
                                    {
                                        if (world == users.users[i].worlds[j].name)
                                        {
                                            auto gr = load_group(group);
                                            users.users[i].worlds[j].group = group + ":" + to_string(lifetime);
                                            users.users[i].worlds[j].prefix = gr.worlds[j].prefix;
                                            users.users[i].worlds[j].suffix = gr.worlds[j].suffix;
                                            break;
                                        }
                                    }
                                }
                            }
                            remove("plugins/Permissions Ex/users.yml");
                            node.reset();
                            for (auto gr : users.users)
                                node["users"].push_back(gr);
                            ofstream fout("plugins/Permissions Ex/users.yml");
                            fout << node;
                            fout.close();
                            output.success(utf8_encode(L"[Permissions Ex]: Игроку была выдана временная группа успешно!"));
                            return;
                        }
                        if ((checkPerm(ori.getPlayer()->getName(), perm) || checkPerm(ori.getPlayer()->getName(), "plugins.*") || checkPerm(ori.getPlayer()->getName(), "permissions.*") || checkPermWorlds(ori.getPlayer()->getName(), perm, dim) || checkPermWorlds(ori.getPlayer()->getName(), "plugins.*", dim) || checkPermWorlds(ori.getPlayer()->getName(), "permissions.*", dim)) && world == "")
                        {
                            Users users;
                            YAML::Node node = YAML::LoadFile("plugins/Permissions Ex/users.yml");
                            for (const auto& p : node["users"])
                                users.users.push_back(p.as<_User>());
                            for (int i = 0; i < users.users.size(); ++i)
                            {
                                if (player.getName() == users.users[i].nickname)
                                {
                                    auto gr = load_group(group);
                                    users.users[i].groups.push_back(group + ":" + to_string(lifetime));
                                    users.users[i].prefix = gr.prefix;
                                    users.users[i].suffix = gr.suffix;
                                    for (int j = 0; j < gr.worlds.size(); ++j)
                                    {
                                        users.users[i].worlds[j].prefix = gr.worlds[j].prefix;
                                        users.users[i].worlds[j].suffix = gr.worlds[j].suffix;
                                    }
                                    break;
                                }
                            }
                            remove("plugins/Permissions Ex/users.yml");
                            node.reset();
                            for (auto gr : users.users)
                                node["users"].push_back(gr);
                            ofstream fout("plugins/Permissions Ex/users.yml");
                            fout << node;
                            fout.close();
                            output.success(utf8_encode(L"[Permissions Ex]: Игроку была выдана временная группа успешно!"));
                            return;
                        }
                        else if ((checkPerm(ori.getPlayer()->getName(), perm) || checkPerm(ori.getPlayer()->getName(), "plugins.*") || checkPerm(ori.getPlayer()->getName(), "permissions.*") || checkPermWorlds(ori.getPlayer()->getName(), perm, dim) || checkPermWorlds(ori.getPlayer()->getName(), "plugins.*", dim) || checkPermWorlds(ori.getPlayer()->getName(), "permissions.*", dim)) && world != "")
                        {
                            Users users;
                            YAML::Node node = YAML::LoadFile("plugins/Permissions Ex/users.yml");
                            for (const auto& p : node["users"])
                                users.users.push_back(p.as<_User>());
                            for (int i = 0; i < users.users.size(); ++i)
                            {
                                if (player.getName() == users.users[i].nickname)
                                {
                                    for (int j = 0; j < users.users[i].worlds.size(); ++j)
                                    {
                                        if (world == users.users[i].worlds[j].name)
                                        {
                                            auto gr = load_group(group);
                                            users.users[i].worlds[j].group = group + ":" + to_string(lifetime);
                                            users.users[i].worlds[j].prefix = gr.worlds[j].prefix;
                                            users.users[i].worlds[j].suffix = gr.worlds[j].suffix;
                                            break;
                                        }
                                    }
                                }
                            }
                            remove("plugins/Permissions Ex/users.yml");
                            node.reset();
                            for (auto gr : users.users)
                                node["users"].push_back(gr);
                            ofstream fout("plugins/Permissions Ex/users.yml");
                            fout << node;
                            fout.close();
                            output.success(utf8_encode(L"[Permissions Ex]: Игроку была выдана временная группа успешно!"));
                            return;
                        }
                        output.error(error_msg);
                        return;
                    }
                    case Group_User_Operation::Remove:
                    {
                    jex1:
                        string dim;
                        if (ori.getDimension()->getDimensionId() == 0)
                            dim = "OverWorld";
                        else if (ori.getDimension()->getDimensionId() == 1)
                            dim = "Nether";
                        else if (ori.getDimension()->getDimensionId() == 2)
                            dim = "End";
                        string error_msg = get_msg("permissionDenied"), error_msg1 = get_msg("invalidArgument");
                        string perm = "permissions.manage.membership." + group;
                        if (group == "" || player.getName() == "")
                        {
                            output.error(error_msg1);
                            return;
                        }
                        if (ori.getPermissionsLevel() == CommandPermissionLevel::Console && world == "")
                        {
                            Users users;
                            YAML::Node node = YAML::LoadFile("plugins/Permissions Ex/users.yml");
                            for (const auto& p : node["users"])
                                users.users.push_back(p.as<_User>());
                            for (int i = 0; i < users.users.size(); ++i)
                            {
                                if (player.getName() == users.users[i].nickname)
                                {
                                    for (int j = 0; j < users.users[i].groups.size(); ++j)
                                    {
                                        auto vec = split(users.users[i].groups[j], ":");
                                        if (vec.size() > 0)
                                        {
                                            auto sz = users.users[i].groups.size();
                                            users.users[i].groups.erase(users.users[i].groups.begin() + j, users.users[i].groups.begin() + j);
                                            users.users[i].groups.resize(sz - 1);
                                            auto gro = load_group(users.users[i].groups[users.users[i].groups.size() - 1]);
                                            users.users[i].prefix = gro.prefix;
                                            users.users[i].suffix = gro.suffix;
                                            break;
                                        }
                                        else
                                        {
                                            output.error(utf8_encode(L"[Permissions Ex]: У игрока постоянная группа"));
                                            return;
                                        }
                                    }
                                   
                                }
                            }
                            remove("plugins/Permissions Ex/users.yml");
                            node.reset();
                            for (auto gr : users.users)
                                node["users"].push_back(gr);
                            ofstream fout("plugins/Permissions Ex/users.yml");
                            fout << node;
                            fout.close();
                            output.success(utf8_encode(L"[Permissions Ex]: У игрока была забрана временная группа успешно!"));
                            return;
                        }
                        if (ori.getPermissionsLevel() == CommandPermissionLevel::Console && world != "")
                        {
                            Users users;
                            YAML::Node node = YAML::LoadFile("plugins/Permissions Ex/users.yml");
                            for (const auto& p : node["users"])
                                users.users.push_back(p.as<_User>());
                            for (int i = 0; i < users.users.size(); ++i)
                            {
                                if (player.getName() == users.users[i].nickname)
                                {
                                    for (int j = 0; j < users.users[i].worlds.size(); ++j)
                                    {
                                        if (world == users.users[i].worlds[j].name)
                                        {
                                            auto vec = split(users.users[i].worlds[j].group, ":");
                                            if (vec.size() > 0)
                                            {
                                                for (auto vv : groups.groups)
                                                {
                                                    if (vv.is_default == true)
                                                    {
                                                        users.users[i].worlds[j].group = vv.name;
                                                        users.users[i].worlds[j].prefix = vv.prefix;
                                                        users.users[i].worlds[j].suffix = vv.suffix;
                                                        break;
                                                    }
                                                }
                                            }
                                            else
                                            {
                                                output.error(utf8_encode(L"[Permissions Ex]: У игрока постоянная группа"));
                                                return;
                                            }
                                        }
                                    }
                                }
                            }
                            remove("plugins/Permissions Ex/users.yml");
                            node.reset();
                            for (auto gr : users.users)
                                node["users"].push_back(gr);
                            ofstream fout("plugins/Permissions Ex/users.yml");
                            fout << node;
                            fout.close();
                            output.success(utf8_encode(L"[Permissions Ex]: У игрока была забрана временная группа успешно!"));
                            return;
                        }
                        if ((checkPerm(ori.getPlayer()->getName(), perm) || checkPerm(ori.getPlayer()->getName(), "plugins.*") || checkPerm(ori.getPlayer()->getName(), "permissions.*") || checkPermWorlds(ori.getPlayer()->getName(), perm, dim) || checkPermWorlds(ori.getPlayer()->getName(), "plugins.*", dim) || checkPermWorlds(ori.getPlayer()->getName(), "permissions.*", dim)) && world == "")
                        {
                            Users users;
                            YAML::Node node = YAML::LoadFile("plugins/Permissions Ex/users.yml");
                            for (const auto& p : node["users"])
                                users.users.push_back(p.as<_User>());
                            for (int i = 0; i < users.users.size(); ++i)
                            {
                                if (player.getName() == users.users[i].nickname)
                                {
                                    for (int j = 0; j < users.users[i].groups.size(); ++j)
                                    {
                                        auto vec = split(users.users[i].groups[j], ":");
                                        if (vec.size() > 0)
                                        {
                                            auto sz = users.users[i].groups.size();
                                            users.users[i].groups.erase(users.users[i].groups.begin() + j, users.users[i].groups.begin() + j);
                                            users.users[i].groups.resize(sz - 1);
                                            auto gro = load_group(users.users[i].groups[users.users[i].groups.size() - 1]);
                                            users.users[i].prefix = gro.prefix;
                                            users.users[i].suffix = gro.suffix;
                                            break;
                                        }
                                        else
                                        {
                                            output.error(utf8_encode(L"[Permissions Ex]: У игрока постоянная группа"));
                                            return;
                                        }
                                    }

                                }
                            }
                            remove("plugins/Permissions Ex/users.yml");
                            node.reset();
                            for (auto gr : users.users)
                                node["users"].push_back(gr);
                            ofstream fout("plugins/Permissions Ex/users.yml");
                            fout << node;
                            fout.close();
                            output.success(utf8_encode(L"[Permissions Ex]: Игроку была выдана временная группа успешно!"));
                            return;
                        }
                        else if ((checkPerm(ori.getPlayer()->getName(), perm) || checkPerm(ori.getPlayer()->getName(), "plugins.*") || checkPerm(ori.getPlayer()->getName(), "permissions.*") || checkPermWorlds(ori.getPlayer()->getName(), perm, dim) || checkPermWorlds(ori.getPlayer()->getName(), "plugins.*", dim) || checkPermWorlds(ori.getPlayer()->getName(), "permissions.*", dim)) && world != "")
                        {
                            Users users;
                            YAML::Node node = YAML::LoadFile("plugins/Permissions Ex/users.yml");
                            for (const auto& p : node["users"])
                                users.users.push_back(p.as<_User>());
                            for (int i = 0; i < users.users.size(); ++i)
                            {
                                if (player.getName() == users.users[i].nickname)
                                {
                                    for (int j = 0; j < users.users[i].worlds.size(); ++j)
                                    {
                                        if (world == users.users[i].worlds[j].name)
                                        {
                                            auto vec = split(users.users[i].worlds[j].group, ":");
                                            if (vec.size() > 0)
                                            {
                                                for (auto vv : groups.groups)
                                                {
                                                    if (vv.is_default == true)
                                                    {
                                                        users.users[i].worlds[j].group = vv.name;
                                                        users.users[i].worlds[j].prefix = vv.prefix;
                                                        users.users[i].worlds[j].suffix = vv.suffix;
                                                        break;
                                                    }
                                                }
                                            }
                                            else
                                            {
                                                output.error(utf8_encode(L"[Permissions Ex]: У игрока постоянная группа"));
                                                return;
                                            }
                                        }
                                    }
                                }
                            }
                            remove("plugins/Permissions Ex/users.yml");
                            node.reset();
                            for (auto gr : users.users)
                                node["users"].push_back(gr);
                            ofstream fout("plugins/Permissions Ex/users.yml");
                            fout << node;
                            fout.close();
                            output.success(utf8_encode(L"[Permissions Ex]: У игрока была забрана временная группа успешно!"));
                            return;
                        }
                        output.error(error_msg);
                        return;
                    }
                  }
              }
            }
       }
       case Operation::Groups:
       {
           string dim;
           if (ori.getDimension()->getDimensionId() == 0)
               dim = "OverWorld";
           else if (ori.getDimension()->getDimensionId() == 1)
               dim = "Nether";
           else if (ori.getDimension()->getDimensionId() == 2)
               dim = "End";
           string error_msg = get_msg("permissionDenied");
           string perm = "permissions.manage.groups.list";
           if (ori.getPermissionsLevel() == CommandPermissionLevel::Console)
           {
               wstring outp = L"[Permissions Ex]: Список групп:\n";
               YAML::Node node = YAML::LoadFile("plugins/Permissions Ex/groups.yml");
               _Groups groups;
               for (const auto& p :node["groups"])
               {
                   groups.groups.push_back(p.as<_Group>());
               }
               for (auto g : groups.groups)
               {
                   outp += L"- " + to_wstring(g.name) + L"\n";
               }
               output.success(utf8_encode(outp));
               return;
           }
           else if ((checkPerm(ori.getPlayer()->getName(), perm) || checkPerm(ori.getPlayer()->getName(), "plugins.*") || checkPerm(ori.getPlayer()->getName(), "permissions.*") || checkPermWorlds(ori.getPlayer()->getName(), perm, dim) || checkPermWorlds(ori.getPlayer()->getName(), "plugins.*", dim) || checkPermWorlds(ori.getPlayer()->getName(), "permissions.*", dim)))
           {
               wstring outp = L"[Permissions Ex]: Список групп:\n";
               YAML::Node node = YAML::LoadFile("plugins/Permissions Ex/groups.yml");
               _Groups groups;
               for (const auto& p : node["groups"])
               {
                   groups.groups.push_back(p.as<_Group>());
               }
               for (auto g : groups.groups)
               {
                   outp += L"- " + to_wstring(g.name) + L"\n";
               }
               output.success(utf8_encode(outp));
               return;
           }
           output.error(error_msg);
           return;
       }
       case Operation::Users:
       {
           string dim;
           if (ori.getDimension()->getDimensionId() == 0)
               dim = "OverWorld";
           else if (ori.getDimension()->getDimensionId() == 1)
               dim = "Nether";
           else if (ori.getDimension()->getDimensionId() == 2)
               dim = "End";
           string error_msg = get_msg("permissionDenied");
           string perm = "permissions.manage.users";
           if (ori.getPermissionsLevel() == CommandPermissionLevel::Console)
           {
               wstring outp = L"[Permissions Ex]: Список игроков бекенда:\n";
               YAML::Node node = YAML::LoadFile("plugins/Permissions Ex/users.yml");
               Users users;
               for (const auto& p : node["users"])
               {
                   users.users.push_back(p.as<_User>());
               }
               for (auto g : users.users)
               {
                   outp += L"- " + to_wstring(g.nickname) + L"\n";
               }
               output.success(utf8_encode(outp));
               return;
           }
           else if ((checkPerm(ori.getPlayer()->getName(), perm) || checkPerm(ori.getPlayer()->getName(), "plugins.*") || checkPerm(ori.getPlayer()->getName(), "permissions.*") || checkPermWorlds(ori.getPlayer()->getName(), perm, dim) || checkPermWorlds(ori.getPlayer()->getName(), "plugins.*", dim) || checkPermWorlds(ori.getPlayer()->getName(), "permissions.*", dim)))
           {
               wstring outp = L"[Permissions Ex]: Список игроков бекенда:\n";
               YAML::Node node = YAML::LoadFile("plugins/Permissions Ex/users.yml");
               Users users;
               for (const auto& p : node["users"])
               {
                   users.users.push_back(p.as<_User>());
               }
               for (auto g : users.users)
               {
                   outp += L"- " + to_wstring(g.nickname) + L"\n";
               }
               output.success(utf8_encode(outp));
               return;
           }
           output.error(error_msg);
           return;
       }
       case Operation::Worlds:
       {
           string dim;
           if (ori.getDimension()->getDimensionId() == 0)
               dim = "OverWorld";
           else if (ori.getDimension()->getDimensionId() == 1)
               dim = "Nether";
           else if (ori.getDimension()->getDimensionId() == 2)
               dim = "End";
           string error_msg = get_msg("permissionDenied");
           string perm = "permissions.manage.worlds";
           if (ori.getPermissionsLevel() == CommandPermissionLevel::Console)
           {
               wstring outp = L"[Permissions Ex]: Список загруженных миров:\n- OverWorld\n- Nether\n- End";
               output.success(utf8_encode(outp));
               return;
           }
           else if ((checkPerm(ori.getPlayer()->getName(), perm) || checkPerm(ori.getPlayer()->getName(), "plugins.*") || checkPerm(ori.getPlayer()->getName(), "permissions.*") || checkPermWorlds(ori.getPlayer()->getName(), perm, dim) || checkPermWorlds(ori.getPlayer()->getName(), "plugins.*", dim) || checkPermWorlds(ori.getPlayer()->getName(), "permissions.*", dim)))
           {
               wstring outp = L"[Permissions Ex]: Список загруженных миров:\n- OverWorld\n- Nether\n- End";
               output.success(utf8_encode(outp));
               return;
           }
           output.error(error_msg);
           return;
       }
       case Operation::World:
       {
           switch (world_op)
           {
             case World_Operation::Inherit:
             {
                 string dim;
                 if (ori.getDimension()->getDimensionId() == 0)
                     dim = "OverWorld";
                 else if (ori.getDimension()->getDimensionId() == 1)
                     dim = "Nether";
                 else if (ori.getDimension()->getDimensionId() == 2)
                     dim = "End";
                 string error_msg = get_msg("permissionDenied");
                 if (!is_inherit)
                 {
                     string perm = "permissions.manage.worlds";
                     if (ori.getPermissionsLevel() == CommandPermissionLevel::Console)
                     {
                         wstring outp = L"[Permissions Ex]: Наследует миры:\n";
                         YAML::Node node = YAML::LoadFile("plugins/Permissions Ex/worlds.yml");
                         for (const auto& p : node["worlds"])
                         {
                             if (world == p.as<World>().name)
                             {
                                 for (auto inh : p.as<World>().inheritances)
                                 {
                                     outp += to_wstring(p.as<World>().name) + L" -> " + to_wstring(inh) + L"\n";
                                 }
                                 break;
                             }
                         }
                         output.success(utf8_encode(outp));
                         return;
                     }
                     else if ((checkPerm(ori.getPlayer()->getName(), perm) || checkPerm(ori.getPlayer()->getName(), "plugins.*") || checkPerm(ori.getPlayer()->getName(), "permissions.*") || checkPermWorlds(ori.getPlayer()->getName(), perm, dim) || checkPermWorlds(ori.getPlayer()->getName(), "plugins.*", dim) || checkPermWorlds(ori.getPlayer()->getName(), "permissions.*", dim)))
                     {
                         wstring outp = L"[Permissions Ex]: Наследует миры:\n";
                         YAML::Node node = YAML::LoadFile("plugins/Permissions Ex/worlds.yml");
                         for (const auto& p : node["worlds"])
                         {
                             if (world == p.as<World>().name)
                             {
                                 for (auto inh : p.as<World>().inheritances)
                                 {
                                     outp += to_wstring(p.as<World>().name) + L" -> " + to_wstring(inh) + L"\n";
                                 }
                                 break;
                             }
                         }
                         output.success(utf8_encode(outp));
                         return;
                     }
                }
                else if (is_inherit)
                {
                     string perm = "permissions.manage.worlds.inheritance";
                     string error_msg1 = get_msg("invalidArgument");
                     if (world == "" || parent_world == "")
                     {
                         output.error(error_msg1);
                         return;
                     }
                     if (ori.getPermissionsLevel() == CommandPermissionLevel::Console)
                     {
                         YAML::Node worlds = YAML::LoadFile("plugins/Permissions Ex/worlds.yml");
                         vector<World> worlds_v;
                         for (const auto& p : worlds["worlds"])
                             worlds_v.push_back(p.as<World>());
                         World overworld = worlds_v[0];
                         World nether = worlds_v[1];
                         World end = worlds_v[2];
                         if (world == "OverWorld")
                         {
                             overworld.inheritances.push_back(parent_world);
                         }
                         else if (world == "Nether")
                         {
                             nether.inheritances.push_back(parent_world);
                         }
                         else if (world == "END")
                         {
                             end.inheritances.push_back(parent_world);
                         }
                         worlds.reset();
                         worlds["worlds"].push_back(overworld);
                         worlds["worlds"].push_back(nether);
                         worlds["worlds"].push_back(end);
                         remove("plugins/Permissions Ex/worlds.yml");
                         ofstream fout("plugins/Permissions Ex/worlds.yml");
                         fout << worlds;
                         fout.close();
                         output.success(utf8_encode(L"[Permissions Ex]: Родительский мир для мира " + to_wstring(world) + L" обновлен успешно!"));
                         return;
                     }
                     else if ((checkPerm(ori.getPlayer()->getName(), perm) || checkPerm(ori.getPlayer()->getName(), "plugins.*") || checkPerm(ori.getPlayer()->getName(), "permissions.*") || checkPermWorlds(ori.getPlayer()->getName(), perm, dim) || checkPermWorlds(ori.getPlayer()->getName(), "plugins.*", dim) || checkPermWorlds(ori.getPlayer()->getName(), "permissions.*", dim)))
                     {
                         YAML::Node worlds = YAML::LoadFile("plugins/Permissions Ex/worlds.yml");
                         World overworld = worlds["worlds"]["OverWorld"].as<World>();
                         World nether = worlds["worlds"]["Nether"].as<World>();
                         World end = worlds["worlds"]["End"].as<World>();
                         if (world == "OverWorld")
                         {
                             overworld.inheritances.push_back(parent_world);
                         }
                         else if (world == "Nether")
                         {
                             nether.inheritances.push_back(parent_world);
                         }
                         else if (world == "END")
                         {
                             end.inheritances.push_back(parent_world);
                         }
                         worlds.reset();
                         worlds["worlds"].push_back(overworld);
                         worlds["worlds"].push_back(nether);
                         worlds["worlds"].push_back(end);
                         remove("plugins/Permissions Ex/worlds.yml");
                         ofstream fout("plugins/Permissions Ex/worlds.yml");
                         fout << worlds;
                         fout.close();
                         output.success(utf8_encode(L"[Permissions Ex]: Родительский мир для мира " + to_wstring(world) + L" обновлен успешно!"));
                         return;
                     }
                }
                 output.error(error_msg);
                 return;
             }
           }
       }
       case Operation::Default:
       {
           switch (def_op)
           {
             case Default_Operation::Group:
             {
                 defgr:
                 string dim;
                 if (ori.getDimension()->getDimensionId() == 0)
                     dim = "OverWorld";
                 else if (ori.getDimension()->getDimensionId() == 1)
                     dim = "Nether";
                 else if (ori.getDimension()->getDimensionId() == 2)
                     dim = "End";
                 string error_msg = get_msg("permissionDenied");
                 string perm = "permissions.man";
                 string error_msg1 = get_msg("invalidArgument");
                 if (ori.getPermissionsLevel() == CommandPermissionLevel::Console)
                 {
                     YAML::Node worlds = YAML::LoadFile("plugins/Permissions Ex/worlds.yml");
                     vector<World> worlds_v;
                     for (const auto& p : worlds["worlds"])
                         worlds_v.push_back(p.as<World>());
                     World overworld = worlds_v[0];
                     World nether = worlds_v[1];
                     World end = worlds_v[2];
                     if (world == "")
                     {
                         output.success(utf8_encode(L"[Permissions Ex]: Группа по умолчанию для мира OverWorld является " + to_wstring(overworld.group) + L"!"));
                         return;
                     }
                     else if (world == "Nether")
                     {
                         output.success(utf8_encode(L"[Permissions Ex]: Группа по умолчанию для мира " + to_wstring(world) + L" является " + to_wstring(nether.group) + L"!"));
                         return;
                     }
                     else if (world == "End")
                     {
                         output.success(utf8_encode(L"[Permissions Ex]: Группа по умолчанию для мира " + to_wstring(world) + L" является " + to_wstring(end.group) + L"!"));
                         return;
                     }
                 }
                 else if ((checkPerm(ori.getPlayer()->getName(), perm) || checkPerm(ori.getPlayer()->getName(), "plugins.*") || checkPerm(ori.getPlayer()->getName(), "permissions.*") || checkPermWorlds(ori.getPlayer()->getName(), perm, dim) || checkPermWorlds(ori.getPlayer()->getName(), "plugins.*", dim) || checkPermWorlds(ori.getPlayer()->getName(), "permissions.*", dim)))
                 {
                     YAML::Node worlds = YAML::LoadFile("plugins/Permissions Ex/worlds.yml");
                     vector<World> worlds_v;
                     for (const auto& p : worlds["worlds"])
                         worlds_v.push_back(p.as<World>());
                     World overworld = worlds_v[0];
                     World nether = worlds_v[1];
                     World end = worlds_v[2];
                     if (world == "")
                     {
                         output.success(utf8_encode(L"[Permissions Ex]: Группа по умолчанию для мира OverWorld является " + to_wstring(overworld.group) + L"!"));
                         return;
                     }
                     else if (world == "Nether")
                     {
                         output.success(utf8_encode(L"[Permissions Ex]: Группа по умолчанию для мира " + to_wstring(world) + L" является " + to_wstring(nether.group) + L"!"));
                         return;
                     }
                     else if (world == "End")
                     {
                         output.success(utf8_encode(L"[Permissions Ex]: Группа по умолчанию для мира " + to_wstring(world) + L" является " + to_wstring(end.group) + L"!"));
                         return;
                     }
                 }
                 output.error(error_msg);
                 return;
             }
           }
       }
       case Operation::Set:
       {
           switch (set_op)
           {
            case Set_Operation::Default:
            {
               switch (def_set_op)
               {
                case Default_Set_Operation::Group:
                {
                    jet23:
                    if (is_defgr)
                        goto defgr;
                    string dim;
                    if (ori.getDimension()->getDimensionId() == 0)
                        dim = "OverWorld";
                    else if (ori.getDimension()->getDimensionId() == 1)
                        dim = "Nether";
                    else if (ori.getDimension()->getDimensionId() == 2)
                        dim = "End";
                    string error_msg = get_msg("permissionDenied");
                    string perm = "permissions.manage.groups.inheritance";
                    string error_msg1 = get_msg("invalidArgument");
                    if (group == "")
                    {
                        output.error(error_msg1);
                        return;
                    }
                    if (ori.getPermissionsLevel() == CommandPermissionLevel::Console && world == "")
                    {
                        _Groups groups;
                        YAML::Node node = YAML::LoadFile("plugins/Permissions Ex/groups.yml");
                        for (const auto& p : node["groups"])
                            groups.groups.push_back(p.as<_Group>());
                        for (int i = 0; i < groups.groups.size(); ++i)
                        {
                            if (groups.groups[i].is_default)
                            {
                                groups.groups[i].is_default = false;
                            }
                            if (group == groups.groups[i].name)
                                groups.groups[i].is_default = true;
                        }
                        node.reset();
                        for (auto gr : groups.groups)
                            node["groups"].push_back(gr);
                        remove("plugins/Permissions Ex/groups.yml");
                        ofstream fout("plugins/Permissions Ex/groups.yml");
                        fout << node;
                        fout.close();
                        output.success(utf8_encode(L"[Permissions Ex]: Группа по умолчанию обновлена успешно!"));
                        return;
                    }
                    if (ori.getPermissionsLevel() == CommandPermissionLevel::Console && world != "")
                    {
                        YAML::Node worlds = YAML::LoadFile("plugins/Permissions Ex/worlds.yml");
                        vector<World> worlds_v;
                        for (const auto& p : worlds["worlds"])
                            worlds_v.push_back(p.as<World>());
                        World overworld = worlds_v[0];
                        World nether = worlds_v[1];
                        World end = worlds_v[2];
                        if (world == "OverWorld")
                        {
                            overworld.group = group;
                        }
                        else  if (world == "Nether")
                        {
                            nether.group = group;
                        }
                        else  if (world == "End")
                        {
                            end.group = group;
                        }
                        worlds.reset();
                        worlds["worlds"].push_back(overworld);
                        worlds["worlds"].push_back(nether);
                        worlds["worlds"].push_back(end);
                        remove("plugins/Permissions Ex/worlds.yml");
                        ofstream fout("plugins/Permissions Ex/worlds.yml");
                        fout << worlds;
                        fout.close();
                        output.success(utf8_encode(L"[Permissions Ex]: Группа по умолчанию для мира " + to_wstring(world) + L" обновлена успешно!"));
                        return;
                    }
                    output.error(error_msg);
                    return;
                }
               }
            }
           }
       }
       case Operation::Help:
        {
           if (is_setdefgroup)
               goto jet23;
           string dim;
           if (ori.getDimension()->getDimensionId() == 0)
               dim = "OverWorld";
           else if (ori.getDimension()->getDimensionId() == 1)
               dim = "Nether";
           else if (ori.getDimension()->getDimensionId() == 2)
               dim = "End";
           string error_msg = get_msg("permissionDenied");
            string perm = "permissions.manage";
            if (ori.getPermissionsLevel() == CommandPermissionLevel::Console)
            {
                string outp = utf8_encode(L"[Permissions Ex]: Информация:\n/pex toggle debug - Вкл./откл. режим отладки (создает много отладочной информации в server.log)\n/pex reload - Перезагружает плагин\n/pex hierarchy - Показывает полную иерархию игроков/групп\n/pex users - Показывает список всех записанных пользователей. И игроков сервера\n/pex groups - Показать все зарегистрированные группы\n/pex worlds - Показать загруженные миры\n/pex default group [world] - Выводит информацию об группе мира,установленной по умолчанию\n/pex user <user> <world> - Показывает права игрока <user>\n/pex user <user> prefix [prefix] [world] - Установить игроку <user> префикс [prefix]\n/pex user <user> suffix [suffix] [world] - Установить игроку <user> суффикс [suffix]\n/pex user <user> _delete - Удалить игрока <user> из бэкенда, используемого на данный момент.\n/pex user <user> _add <permission> [world] - Дать право <permission> игроку <user>\n/pex user <user> remove <permission> [world] - Забрать право <permission> у игрока <user>\n/pex user <user> group _add <permission> [lifetime] [world] - Дать временное право <permission> игроку <user> на время [lifetime] (в сек.) Выставите значение на "" (две двойные скобки) если вы хотите использовать право во всех мирах!\n/pex user <user> group remove <permission> - Дать временное право <permission> игроку <user> на время [lifetime] (в сек.) Выставите значение на "" (две двойные скобки) если вы хотите использовать право во всех мирах!\n/pex user <user> group list [world] - Показать список групп в которых состоит игрок <user>\n/pex user <user> group _add <group> [lifetime] [world] - Добавить игрока <user> в группу <group> на время [lifetime] в секундах\n/pex user <user> group set <group> [world] - Установить группу <group> для игрока <user> (удалит его из остальных групп)\n/pex user <user> group remove <group> [world] - Удалить игрока <user> из группы <group>\n/pex set default group <group> [world] - Установить группу <group>, как группу по-умолчанию\n/pex group <group> prefix [prefix] [world] - 	Установить группе <group> префикс [prefix]\n/pex group <group> suffix [suffix] [world] - Установить группе <group> суффикс [suffix]\n/pex group <group> create <prefix> <suffix> <default> [parent] - Создать группу <group> и если нужно установить для нее родительскую группу/группы [parents]\n/pex group <group> delete - 	Удалить группу <group>\n/pex group <group> parents [world] - Список родительских групп для группы <group>\n/pex group <group> parents set <parent> [world] - Установить группе <group> родительскую группу <parent>\n/pex group <group> - Показать все права группы <group>\n/pex group <group> add <permission> [world] - Дать право <permission> группе <group>\n/pex group <group> remove <permission> [world] - Забрать право <permission> у группы <group>\n/pex group <group> timed add <permission> [lifetime] [world] - Дать временное право <permission> группе <group> на время [lifetime] (в сек.)\n/pex group <group> timed remove <permission> [world] - Забрать временное право <permission> у группы <group>\n/pex group <group> users - Показать всех игроков в группе <group>\n/pex group <group> user add <user> [world] [time]\n/pex group <group> user add <user> [world] [time] - Добавить игрока/игроков <user> в группу <group> на время [time]\n/pex group <group> user remove <user> [world] - Удалить игрока/игроков из группы <group>\n/pex world <world> - Показать информацию о наследственности мира <world>\n/pex world <world> inherit <parentWorld> - Установить родительский мир <parentWorlds> для мира <world>");
                output.success(outp);
                return;
            }
            else if (checkPerm(ori.getPlayer()->getName(), perm) || checkPerm(ori.getPlayer()->getName(), "plugins.*") || checkPerm(ori.getPlayer()->getName(), "permissions.*") || checkPermWorlds(ori.getPlayer()->getName(), perm, dim) || checkPermWorlds(ori.getPlayer()->getName(), "plugins.*", dim) || checkPermWorlds(ori.getPlayer()->getName(), "permissions.*", dim))
            {
                string outp = utf8_encode(L"[Permissions Ex]: Информация:\n/pex toggle debug - Вкл./откл. режим отладки (создает много отладочной информации в server.log)\n/pex reload - Перезагружает плагин\n/pex hierarchy - Показывает полную иерархию игроков/групп\n/pex users - Показывает список всех записанных пользователей. И игроков сервера\n/pex groups - Показать все зарегистрированные группы\n/pex worlds - Показать загруженные миры\n/pex default group [world] - Выводит информацию об группе мира,установленной по умолчанию\n/pex user <user> <world> - Показывает права игрока <user>\n/pex user <user> prefix [prefix] [world] - Установить игроку <user> префикс [prefix]\n/pex user <user> suffix [suffix] [world] - Установить игроку <user> суффикс [suffix]\n/pex user <user> _delete - Удалить игрока <user> из бэкенда, используемого на данный момент.\n/pex user <user> _add <permission> [world] - Дать право <permission> игроку <user>\n/pex user <user> remove <permission> [world] - Забрать право <permission> у игрока <user>\n/pex user <user> group _add <permission> [lifetime] [world] - Дать временное право <permission> игроку <user> на время [lifetime] (в сек.) Выставите значение на "" (две двойные скобки) если вы хотите использовать право во всех мирах!\n/pex user <user> group remove <permission> - Дать временное право <permission> игроку <user> на время [lifetime] (в сек.) Выставите значение на "" (две двойные скобки) если вы хотите использовать право во всех мирах!\n/pex user <user> group list [world] - Показать список групп в которых состоит игрок <user>\n/pex user <user> group _add <group> [lifetime] [world] - Добавить игрока <user> в группу <group> на время [lifetime] в секундах\n/pex user <user> group set <group> [world] - Установить группу <group> для игрока <user> (удалит его из остальных групп)\n/pex user <user> group remove <group> [world] - Удалить игрока <user> из группы <group>\n/pex set default group <group> [world] - Установить группу <group>, как группу по-умолчанию\n/pex group <group> prefix [prefix] [world] - 	Установить группе <group> префикс [prefix]\n/pex group <group> suffix [suffix] [world] - Установить группе <group> суффикс [suffix]\n/pex group <group> create <prefix> <suffix> <default> [parent] - Создать группу <group> и если нужно установить для нее родительскую группу/группы [parents]\n/pex group <group> delete - 	Удалить группу <group>\n/pex group <group> parents [world] - Список родительских групп для группы <group>\n/pex group <group> parents set <parent> [world] - Установить группе <group> родительскую группу <parent>\n/pex group <group> - Показать все права группы <group>\n/pex group <group> add <permission> [world] - Дать право <permission> группе <group>\n/pex group <group> remove <permission> [world] - Забрать право <permission> у группы <group>\n/pex group <group> timed add <permission> [lifetime] [world] - Дать временное право <permission> группе <group> на время [lifetime] (в сек.)\n/pex group <group> timed remove <permission> [world] - Забрать временное право <permission> у группы <group>\n/pex group <group> users - Показать всех игроков в группе <group>\n/pex group <group> user add <user> [world] [time]\n/pex group <group> user add <user> [world] [time] - Добавить игрока/игроков <user> в группу <group> на время [time]\n/pex group <group> user remove <user> [world] - Удалить игрока/игроков из группы <group>\n/pex world <world> - Показать информацию о наследственности мира <world>\n/pex world <world> inherit <parentWorld> - Установить родительский мир <parentWorlds> для мира <world>");
                output.success(outp);
                return;
            }
            output.error(error_msg);
            return;
        }
       }
    }
    static void setup(CommandRegistry* r) 
    {
        r->registerCommand(
            "pex", "Permissions Ex commands.", CommandPermissionLevel::Any, { (CommandFlagValue)0 },
            { (CommandFlagValue)0x80 });
        r->addEnum<Operation>("help", { { "help",Operation::Help} });
        r->addEnum<Operation>("toggle", { { "toggle",Operation::Toggle } });
        r->addEnum<Operation>("reload", { { "reload",Operation::Reload } });
        r->addEnum<Operation>("hierarchy", { { "hierarchy",Operation::Hierarcy } });
        r->addEnum<Operation>("users", { { "users",Operation::Users } });
        r->addEnum<Operation>("groups", { { "groups",Operation::Groups } });
        r->addEnum<Operation>("user", { { "user",Operation::User} });
        r->addEnum<Operation>("group", { { "group",Operation::Group } });
        r->addEnum<Operation>("worlds", { { "worlds",Operation::Worlds } });
        r->addEnum<Operation>("world", { { "world",Operation::World } });
        r->addEnum<Operation>("default", { { "default",Operation::Default } });
        r->addEnum<Operation>("set", { { "set",Operation::Set } });
        r->addEnum<Toggle_Operation>("debug", { { "debug",Toggle_Operation::Debug } });
        r->addEnum<User_Operation>("prefix", { { "prefix",User_Operation::Prefix } });
        r->addEnum<User_Operation>("suffix", { { "suffix",User_Operation::Suffix } });
        r->addEnum<User_Operation>("_add", { { "_add",User_Operation::Add } });
        r->addEnum<User_Operation>("remove", { { "remove",User_Operation::Remove } });
        r->addEnum<User_Operation>("check", { { "check",User_Operation::Check } });
        r->addEnum<User_Operation>("_delete", { { "_delete",User_Operation::Delete } });
        r->addEnum<User_Operation>("group", { { "group",User_Operation::Group} });
        r->addEnum<User_Group_Operation>("_add", { { "_add",User_Group_Operation::Add } });
        r->addEnum<User_Group_Operation>("remove", { { "remove",User_Group_Operation::Remove} });
        r->addEnum<User_Group_Operation>("list", { { "list",User_Group_Operation::List } });
        r->addEnum<User_Group_Operation>("set", { { "set",User_Group_Operation::Set } });
        r->addEnum<Group_Operation>("prefix", { { "prefix",Group_Operation::Prefix } });
        r->addEnum<Group_Operation>("suffix", { { "suffix",Group_Operation::Suffix } });
        r->addEnum<Group_Operation>("create", { { "create",Group_Operation::Create } });
        r->addEnum<Group_Operation>("_delete", { { "_delete",Group_Operation::Delete } });
        r->addEnum<Group_Operation>("parents", { { "parents",Group_Operation::Parents } });
        r->addEnum<Group_Operation>("_add", { { "_add",Group_Operation::Add} });
        r->addEnum<Group_Operation>("remove", { { "remove",Group_Operation::Remove } });
        r->addEnum<Group_Operation>("timed", { { "timed",Group_Operation::Timed } });
        r->addEnum<Group_Operation>("users", { { "users",Group_Operation::Users } });
        r->addEnum<Group_Operation>("user", { { "user",Group_Operation::User } });
        r->addEnum<Group_Parents_Operation>("set", { { "set",Group_Parents_Operation::Set } });
        r->addEnum<Group_Timed_Operation>("_add", { { "_add",Group_Timed_Operation::Add} });
        r->addEnum<Group_Timed_Operation>("remove", { { "remove",Group_Timed_Operation::Remove } });
        r->addEnum<Group_User_Operation>("_add", { { "_add",Group_User_Operation::Add} });
        r->addEnum<Group_User_Operation>("remove", { { "remove",Group_User_Operation::Remove } });
        r->addEnum<World_Operation>("inherit", { { "inherit",World_Operation::Inherit} });
        r->addEnum<Set_Operation>("default", { {"default",Set_Operation::Default} });
        r->addEnum<Default_Set_Operation>("group", { {"group",Default_Set_Operation::Group} });
        r->addEnum<Default_Operation>("group", { {"group",Default_Operation::Group} });
        r->registerOverload<Pex>("pex", RegisterCommandHelper::makeMandatory<CommandParameterDataType::ENUM>(&Pex::op, "help", "help").addOptions((CommandParameterOption)1));
        r->registerOverload<Pex>("pex", RegisterCommandHelper::makeMandatory<CommandParameterDataType::ENUM>(&Pex::op, "reload", "reload").addOptions((CommandParameterOption)1));
        r->registerOverload<Pex>("pex", RegisterCommandHelper::makeMandatory<CommandParameterDataType::ENUM>(&Pex::op, "hierarchy", "hierarchy").addOptions((CommandParameterOption)1));
        r->registerOverload<Pex>("pex", RegisterCommandHelper::makeMandatory<CommandParameterDataType::ENUM>(&Pex::op, "users", "users").addOptions((CommandParameterOption)1));
        r->registerOverload<Pex>("pex", RegisterCommandHelper::makeMandatory<CommandParameterDataType::ENUM>(&Pex::op, "toggle", "toggle").addOptions((CommandParameterOption)1), RegisterCommandHelper::makeMandatory<CommandParameterDataType::ENUM>(&Pex::tg_op, "debug", "debug").addOptions((CommandParameterOption)1));
        r->registerOverload<Pex>("pex", RegisterCommandHelper::makeMandatory<CommandParameterDataType::ENUM>(&Pex::op, "user", "user").addOptions((CommandParameterOption)1), RegisterCommandHelper::makeOptional(&Pex::world, "world"), RegisterCommandHelper::makeMandatory(&Pex::player,"user"));
        r->registerOverload<Pex>("pex", RegisterCommandHelper::makeMandatory<CommandParameterDataType::ENUM>(&Pex::op, "user", "user").addOptions((CommandParameterOption)1), RegisterCommandHelper::makeMandatory(&Pex::player, "user"), RegisterCommandHelper::makeMandatory<CommandParameterDataType::ENUM>(&Pex::us_op, "prefix", "prefix").addOptions((CommandParameterOption)1), RegisterCommandHelper::makeOptional(&Pex::user_prefix, "prefix"), RegisterCommandHelper::makeOptional(&Pex::world, "world"));
        r->registerOverload<Pex>("pex", RegisterCommandHelper::makeMandatory<CommandParameterDataType::ENUM>(&Pex::op, "user", "user").addOptions((CommandParameterOption)1), RegisterCommandHelper::makeMandatory(&Pex::player, "user"), RegisterCommandHelper::makeMandatory<CommandParameterDataType::ENUM>(&Pex::us_op, "suffix", "suffix").addOptions((CommandParameterOption)1), RegisterCommandHelper::makeOptional(&Pex::user_suffix, "suffix"), RegisterCommandHelper::makeOptional(&Pex::world, "world"));
        r->registerOverload<Pex>("pex", RegisterCommandHelper::makeMandatory<CommandParameterDataType::ENUM>(&Pex::op, "user", "user").addOptions((CommandParameterOption)1), RegisterCommandHelper::makeMandatory(&Pex::player, "user"), RegisterCommandHelper::makeMandatory<CommandParameterDataType::ENUM>(&Pex::us_op, "_delete", "_delete").addOptions((CommandParameterOption)1));
        r->registerOverload<Pex>("pex", RegisterCommandHelper::makeMandatory<CommandParameterDataType::ENUM>(&Pex::op, "user", "user").addOptions((CommandParameterOption)1), RegisterCommandHelper::makeMandatory(&Pex::player, "user"), RegisterCommandHelper::makeMandatory<CommandParameterDataType::ENUM>(&Pex::us_op, "_add", "_add").addOptions((CommandParameterOption)1), RegisterCommandHelper::makeMandatory(&Pex::user_permission, "permission"), RegisterCommandHelper::makeOptional(&Pex::world, "world"));
        r->registerOverload<Pex>("pex", RegisterCommandHelper::makeMandatory<CommandParameterDataType::ENUM>(&Pex::op, "user", "user").addOptions((CommandParameterOption)1), RegisterCommandHelper::makeMandatory(&Pex::player, "user"), RegisterCommandHelper::makeMandatory<CommandParameterDataType::ENUM>(&Pex::us_op, "remove", "remove").addOptions((CommandParameterOption)1), RegisterCommandHelper::makeOptional(&Pex::user_permission, "permission"), RegisterCommandHelper::makeOptional(&Pex::world, "world"));
        r->registerOverload<Pex>("pex", RegisterCommandHelper::makeMandatory<CommandParameterDataType::ENUM>(&Pex::op, "user", "user").addOptions((CommandParameterOption)1), RegisterCommandHelper::makeMandatory(&Pex::player, "user"), RegisterCommandHelper::makeMandatory<CommandParameterDataType::ENUM>(&Pex::us_op, "group", "group").addOptions((CommandParameterOption)1), RegisterCommandHelper::makeOptional<CommandParameterDataType::ENUM>(&Pex::us_gr_op, "_add", "_add").addOptions((CommandParameterOption)1), RegisterCommandHelper::makeOptional(&Pex::opval, "permission|group"), RegisterCommandHelper::makeOptional(&Pex::world, "world"), RegisterCommandHelper::makeOptional(&Pex::lifetime, "lifetime"));
        r->registerOverload<Pex>("pex", RegisterCommandHelper::makeMandatory<CommandParameterDataType::ENUM>(&Pex::op, "user", "user").addOptions((CommandParameterOption)1), RegisterCommandHelper::makeMandatory(&Pex::player, "user"), RegisterCommandHelper::makeMandatory<CommandParameterDataType::ENUM>(&Pex::us_op, "group", "group").addOptions((CommandParameterOption)1), RegisterCommandHelper::makeMandatory<CommandParameterDataType::ENUM>(&Pex::us_gr_op, "remove", "remove").addOptions((CommandParameterOption)1), RegisterCommandHelper::makeMandatory(&Pex::opval1, "permission/group"), RegisterCommandHelper::makeOptional(&Pex::world, "world"));
        r->registerOverload<Pex>("pex", RegisterCommandHelper::makeMandatory<CommandParameterDataType::ENUM>(&Pex::op, "user", "user").addOptions((CommandParameterOption)1), RegisterCommandHelper::makeMandatory(&Pex::player, "user"), RegisterCommandHelper::makeMandatory<CommandParameterDataType::ENUM>(&Pex::us_op, "group", "group").addOptions((CommandParameterOption)1), RegisterCommandHelper::makeMandatory<CommandParameterDataType::ENUM>(&Pex::us_gr_op, "list", "list",&Pex::is_list).addOptions((CommandParameterOption)1), RegisterCommandHelper::makeOptional(&Pex::world, "world"));
        r->registerOverload<Pex>("pex", RegisterCommandHelper::makeMandatory<CommandParameterDataType::ENUM>(&Pex::op, "user", "user").addOptions((CommandParameterOption)1), RegisterCommandHelper::makeMandatory(&Pex::player, "user"), RegisterCommandHelper::makeMandatory<CommandParameterDataType::ENUM>(&Pex::us_op, "group", "group").addOptions((CommandParameterOption)1), RegisterCommandHelper::makeMandatory<CommandParameterDataType::ENUM>(&Pex::us_gr_op, "set", "set").addOptions((CommandParameterOption)1), RegisterCommandHelper::makeMandatory(&Pex::opval2, "group"), RegisterCommandHelper::makeOptional(&Pex::world, "world"));
        r->registerOverload<Pex>("pex", RegisterCommandHelper::makeMandatory<CommandParameterDataType::ENUM>(&Pex::op, "user", "user").addOptions((CommandParameterOption)1), RegisterCommandHelper::makeMandatory(&Pex::player, "user"),  RegisterCommandHelper::makeMandatory<CommandParameterDataType::ENUM>(&Pex::us_op, "check", "check").addOptions((CommandParameterOption)1), RegisterCommandHelper::makeOptional(&Pex::user_permission, "permission"));
        r->registerOverload<Pex>("pex", RegisterCommandHelper::makeMandatory<CommandParameterDataType::ENUM>(&Pex::op, "group", "group").addOptions((CommandParameterOption)1), RegisterCommandHelper::makeMandatory(&Pex::group, "group"), RegisterCommandHelper::makeMandatory<CommandParameterDataType::ENUM>(&Pex::gr_op, "prefix", "prefix").addOptions((CommandParameterOption)1), RegisterCommandHelper::makeMandatory(&Pex::group_prefix, "prefix"), RegisterCommandHelper::makeOptional(&Pex::world, "world"));
        r->registerOverload<Pex>("pex", RegisterCommandHelper::makeMandatory<CommandParameterDataType::ENUM>(&Pex::op, "group", "group").addOptions((CommandParameterOption)1), RegisterCommandHelper::makeMandatory(&Pex::group, "group"), RegisterCommandHelper::makeMandatory<CommandParameterDataType::ENUM>(&Pex::gr_op, "suffix", "suffix").addOptions((CommandParameterOption)1), RegisterCommandHelper::makeMandatory(&Pex::group_suffix, "suffix"), RegisterCommandHelper::makeOptional(&Pex::world, "world"));
        r->registerOverload<Pex>("pex", RegisterCommandHelper::makeMandatory<CommandParameterDataType::ENUM>(&Pex::op, "group", "group").addOptions((CommandParameterOption)1), RegisterCommandHelper::makeMandatory(&Pex::group, "group"), RegisterCommandHelper::makeMandatory<CommandParameterDataType::ENUM>(&Pex::gr_op, "create", "create").addOptions((CommandParameterOption)1), RegisterCommandHelper::makeMandatory(&Pex::group_prefix, "prefix"), RegisterCommandHelper::makeMandatory(&Pex::group_suffix, "suffix"), RegisterCommandHelper::makeMandatory(&Pex::is_default, "default"),RegisterCommandHelper::makeOptional(&Pex::parent, "parent"));
        r->registerOverload<Pex>("pex", RegisterCommandHelper::makeMandatory<CommandParameterDataType::ENUM>(&Pex::op, "group", "group").addOptions((CommandParameterOption)1), RegisterCommandHelper::makeMandatory(&Pex::group, "group"), RegisterCommandHelper::makeMandatory<CommandParameterDataType::ENUM>(&Pex::gr_op, "_delete", "_delete",&Pex::is_delete).addOptions((CommandParameterOption)1));
        r->registerOverload<Pex>("pex", RegisterCommandHelper::makeMandatory<CommandParameterDataType::ENUM>(&Pex::op, "group", "group").addOptions((CommandParameterOption)1), RegisterCommandHelper::makeMandatory(&Pex::group, "group"), RegisterCommandHelper::makeMandatory<CommandParameterDataType::ENUM>(&Pex::gr_op, "prents", "parents").addOptions((CommandParameterOption)1),RegisterCommandHelper::makeOptional<CommandParameterDataType::ENUM>(&Pex::gr_pr_op,"set","set", & Pex::is_parentset).addOptions((CommandParameterOption)1), RegisterCommandHelper::makeOptional(&Pex::parent, "parent"),RegisterCommandHelper::makeOptional(&Pex::world, "world"));
        r->registerOverload<Pex>("pex", RegisterCommandHelper::makeMandatory<CommandParameterDataType::ENUM>(&Pex::op, "group", "group").addOptions((CommandParameterOption)1), RegisterCommandHelper::makeMandatory(&Pex::group, "group"), RegisterCommandHelper::makeMandatory<CommandParameterDataType::ENUM>(&Pex::gr_op, "remove", "remove",&Pex::is_remove).addOptions((CommandParameterOption)1), RegisterCommandHelper::makeMandatory(&Pex::group_permission, "permission"), RegisterCommandHelper::makeOptional(&Pex::world, "world"));
        r->registerOverload<Pex>("pex", RegisterCommandHelper::makeMandatory<CommandParameterDataType::ENUM>(&Pex::op, "group", "group").addOptions((CommandParameterOption)1), RegisterCommandHelper::makeMandatory(&Pex::group, "group"), RegisterCommandHelper::makeMandatory<CommandParameterDataType::ENUM>(&Pex::gr_op, "_add", "_add", &Pex::is_add).addOptions((CommandParameterOption)1), RegisterCommandHelper::makeMandatory(&Pex::group_permission, "permission"), RegisterCommandHelper::makeOptional(&Pex::world, "world"));
        r->registerOverload<Pex>("pex", RegisterCommandHelper::makeMandatory<CommandParameterDataType::ENUM>(&Pex::op, "group", "group").addOptions((CommandParameterOption)1), RegisterCommandHelper::makeMandatory(&Pex::group, "group"), RegisterCommandHelper::makeMandatory<CommandParameterDataType::ENUM>(&Pex::gr_op, "timed", "timed").addOptions((CommandParameterOption)1), RegisterCommandHelper::makeMandatory<CommandParameterDataType::ENUM>(&Pex::gr_tm_op, "_add", "_add",&Pex::is_timeadd).addOptions((CommandParameterOption)1), RegisterCommandHelper::makeMandatory(&Pex::group_permission, "permission"), RegisterCommandHelper::makeOptional(&Pex::lifetime, "lifetime"), RegisterCommandHelper::makeOptional(&Pex::world, "world"));
        r->registerOverload<Pex>("pex", RegisterCommandHelper::makeMandatory<CommandParameterDataType::ENUM>(&Pex::op, "group", "group").addOptions((CommandParameterOption)1), RegisterCommandHelper::makeMandatory(&Pex::group, "group"), RegisterCommandHelper::makeMandatory<CommandParameterDataType::ENUM>(&Pex::gr_op, "timed", "timed").addOptions((CommandParameterOption)1), RegisterCommandHelper::makeMandatory<CommandParameterDataType::ENUM>(&Pex::gr_tm_op, "remove", "remove", &Pex::is_timeremove).addOptions((CommandParameterOption)1), RegisterCommandHelper::makeMandatory(&Pex::group_permission, "permission"), RegisterCommandHelper::makeOptional(&Pex::world, "world"));
        r->registerOverload<Pex>("pex", RegisterCommandHelper::makeMandatory<CommandParameterDataType::ENUM>(&Pex::op, "group", "group").addOptions((CommandParameterOption)1), RegisterCommandHelper::makeMandatory(&Pex::group, "group"), RegisterCommandHelper::makeMandatory<CommandParameterDataType::ENUM>(&Pex::gr_op, "users", "users",&Pex::is_users).addOptions((CommandParameterOption)1));
        r->registerOverload<Pex>("pex", RegisterCommandHelper::makeMandatory<CommandParameterDataType::ENUM>(&Pex::op, "group", "group").addOptions((CommandParameterOption)1), RegisterCommandHelper::makeMandatory(&Pex::group, "group"), RegisterCommandHelper::makeMandatory<CommandParameterDataType::ENUM>(&Pex::gr_op, "user", "user").addOptions((CommandParameterOption)1), RegisterCommandHelper::makeMandatory<CommandParameterDataType::ENUM>(&Pex::gr_op, "_add", "_add",&Pex::is_usradd).addOptions((CommandParameterOption)1), RegisterCommandHelper::makeMandatory(&Pex::player, "user"), RegisterCommandHelper::makeOptional(&Pex::lifetime, "lifetime"), RegisterCommandHelper::makeOptional(&Pex::world, "world"));
        r->registerOverload<Pex>("pex", RegisterCommandHelper::makeMandatory<CommandParameterDataType::ENUM>(&Pex::op, "group", "group").addOptions((CommandParameterOption)1), RegisterCommandHelper::makeMandatory(&Pex::group, "group"), RegisterCommandHelper::makeMandatory<CommandParameterDataType::ENUM>(&Pex::gr_op, "user", "user").addOptions((CommandParameterOption)1), RegisterCommandHelper::makeMandatory<CommandParameterDataType::ENUM>(&Pex::gr_op, "remove", "remove",&Pex::is_usrremove).addOptions((CommandParameterOption)1), RegisterCommandHelper::makeMandatory(&Pex::player, "user"), RegisterCommandHelper::makeOptional(&Pex::world, "world"));
        r->registerOverload<Pex>("pex", RegisterCommandHelper::makeMandatory<CommandParameterDataType::ENUM>(&Pex::op, "worlds", "worlds").addOptions((CommandParameterOption)1));
        r->registerOverload<Pex>("pex", RegisterCommandHelper::makeMandatory<CommandParameterDataType::ENUM>(&Pex::op, "world", "world").addOptions((CommandParameterOption)1), RegisterCommandHelper::makeMandatory(&Pex::world, "world"), RegisterCommandHelper::makeOptional<CommandParameterDataType::ENUM>(&Pex::world_op, "inherit", "inherit",&Pex::is_inherit).addOptions((CommandParameterOption)1), RegisterCommandHelper::makeOptional(&Pex::parent_world, "parentWorld"));
        r->registerOverload<Pex>("pex", RegisterCommandHelper::makeMandatory<CommandParameterDataType::ENUM>(&Pex::op, "set", "set",&Pex::is_setdefgroup).addOptions((CommandParameterOption)1), RegisterCommandHelper::makeMandatory<CommandParameterDataType::ENUM>(&Pex::set_op, "default", "default").addOptions((CommandParameterOption)1), RegisterCommandHelper::makeMandatory<CommandParameterDataType::ENUM>(&Pex::def_set_op, "group", "group").addOptions((CommandParameterOption)1), RegisterCommandHelper::makeMandatory(&Pex::group, "group"), RegisterCommandHelper::makeOptional(&Pex::world, "world"));
        r->registerOverload<Pex>("pex",RegisterCommandHelper::makeMandatory<CommandParameterDataType::ENUM>(&Pex::op, "default", "default").addOptions((CommandParameterOption)1), RegisterCommandHelper::makeMandatory<CommandParameterDataType::ENUM>(&Pex::def_op, "group", "group",&Pex::is_defgr).addOptions((CommandParameterOption)1), RegisterCommandHelper::makeOptional(&Pex::world, "world"));
        r->registerOverload<Pex>("pex", RegisterCommandHelper::makeMandatory<CommandParameterDataType::ENUM>(&Pex::op, "groups", "groups").addOptions((CommandParameterOption)1));
        r->registerOverload<Pex>("pex", RegisterCommandHelper::makeMandatory<CommandParameterDataType::ENUM>(&Pex::op, "users", "users").addOptions((CommandParameterOption)1));
    }
};

void entry();

extern "C" {
    _declspec(dllexport) void onPostInit() {
        std::ios::sync_with_stdio(false);
        entry();
    }
}

bool makeDir(const char* dir)
{
    error_code ec;
    if (exists(dir))
        return is_directory(status(dir));
    else
        return create_directories(dir, ec);
}

void replaceAll(string& s, const string& search, const string& replace) {
    for (size_t pos = 0; ; pos += replace.length()) {
        // Locate the substring to replace
        pos = s.find(search, pos);
        if (pos == string::npos) break;
        // Replace by erasing and inserting
        s.erase(pos, search.length());
        s.insert(pos, replace);
    }
}

void entry()
{
    Event::PlayerPreJoinEvent::subscribe([](const Event::PlayerPreJoinEvent& ev) 
    {
            bool is_succ = false;
            ModifyworldConfig mkb;
            YAML::Node node123 = YAML::LoadFile("plugins/Permissions Ex/modifyworld.yml");
            mkb = node123.as<ModifyworldConfig>();
            if (mkb.whitelist)
            {
                ifstream in("plugins/Permissions Ex/whitelist.txt");
                string nick;
                auto pl = load_user(ev.mPlayer->getName());
                for (auto xh : pl.groups)
                {
                    auto group = load_group(xh);
                    while (getline(in, nick))
                    {
                        if (ev.mPlayer->getName() == nick)
                        {
                            is_succ = true;
                            break;
                        }
                    }

                }
            }
            if (!is_succ && mkb.whitelist)
            {
                auto v = get_msg("whitelist");
                ev.mPlayer->kick(v);
                return 0;
            }
            Users users;
            YAML::Node node = YAML::LoadFile("plugins/Permissions Ex/users.yml");
            for (const auto& p : node["users"])
                users.users.push_back(p.as<_User>());
            bool is_noexits = true;
            for (int i = 0; i < users.users.size(); ++i)
            {
                if (ev.mPlayer->getName() == users.users[i].nickname)
                {
                    is_noexits = false;
                    break;
                }
            }
            if (is_noexits)
            {
                _Groups grs;
                YAML::Node node1 = YAML::LoadFile("plugins/Permissions Ex/groups.yml");
                for (const auto& p : node1["groups"])
                    grs.groups.push_back(p.as<_Group>());
                _Group def_group;
                _User user;
                for (auto v : grs.groups)
                {
                    if (v.is_default)
                    {
                        def_group = v;
                        break;
                    }
                }
                user.nickname = ev.mPlayer->getName();
                user.groups.push_back(def_group.name);
                user.prefix = def_group.prefix;
                user.suffix = def_group.suffix;
                user.permissions = {};
                user.worlds = def_group.worlds;
                users.users.push_back(user);
                node.reset();
                for (auto gr : users.users)
                    node["users"].push_back(gr);
                remove("plugins/Permissions Ex/users.yml");
                ofstream fout("plugins/Permissions Ex/users.yml");
                fout << node;
                fout.close();
                ChatConfig cs;
                YAML::Node node12 = YAML::LoadFile("plugins/Permissions Ex/chat.yml");
                cs = node12.as<ChatConfig>();
                if (user.prefix == "")
                {
                    replaceAll(cs.display_name_format, "%prefix%", utf8_encode(to_wstring("")));
                    replaceAll(cs.display_name_format, "%player%", utf8_encode(to_wstring(ev.mPlayer->getName())));
                    replaceAll(cs.display_name_format, "%suffix%", user.suffix);
                    ev.mPlayer->setNameTag(cs.display_name_format);
                }
                if (user.suffix == "")
                {
                    replaceAll(cs.display_name_format, "%prefix%", user.prefix);
                    replaceAll(cs.display_name_format, "%player%", utf8_encode(to_wstring(ev.mPlayer->getName())));
                    replaceAll(cs.display_name_format, "%suffix%", utf8_encode(to_wstring("")));
                    ev.mPlayer->setNameTag(cs.display_name_format);
                }
                if (user.prefix == "" && user.suffix == "")
                {
                    return 1;
                }
                return 1;
            }
            if (is_succ)
            {
                auto user = load_user(ev.mPlayer->getName());
                ChatConfig cs;
                YAML::Node node12 = YAML::LoadFile("plugins/Permissions Ex/chat.yml");
                cs = node12.as<ChatConfig>();
                if (user.prefix == "")
                {
                    replaceAll(cs.display_name_format, "%prefix%", utf8_encode(to_wstring("")));
                    replaceAll(cs.display_name_format, "%player%", utf8_encode(to_wstring(ev.mPlayer->getName())));
                    replaceAll(cs.display_name_format, "%suffix%", user.suffix);
                    ev.mPlayer->setNameTag(cs.display_name_format);
                }
                if (user.suffix == "")
                {
                    replaceAll(cs.display_name_format, "%prefix%", user.prefix);
                    replaceAll(cs.display_name_format, "%player%", utf8_encode(to_wstring(ev.mPlayer->getName())));
                    replaceAll(cs.display_name_format, "%suffix%", utf8_encode(to_wstring("")));
                    ev.mPlayer->setNameTag(cs.display_name_format);
                }
                if (user.prefix == "" && user.suffix == "")
                {
                    return 1;
                }
                return 1;
            }
            return 1;
    });
    Event::PlayerChatEvent::subscribe([](const Event::PlayerChatEvent& ev) 
    {
            if (!enabled)
             return 1;
            if (!chat_ranged || chat_range == 0)
            {
                ChatConfig cs;
                YAML::Node node = YAML::LoadFile("plugins/Permissions Ex/chat.yml");
                cs = node.as<ChatConfig>();
                Users users;
               // YAML::Node node23 = YAML::LoadFile("plugins/Permissions Ex/users.yml");
                for (const auto& p : config1["users"])
                    users.users.push_back(p.as<_User>());
                string res_nick;
                auto plain = ev.mPlayer->getName();
                auto x = split(plain, " ");
                for (int i = 0; i < x.size(); ++i)
                {
                    for (auto v : users.users)
                    {
                        if (x[i] == v.nickname)
                        {
                            res_nick = x[i];
                            break;
                        }
                    }
                }
                auto pl = load_user(res_nick);
                if (pl.prefix == "")
                {
                    auto sp = split(cs.global_message_format, "%prefix%");
                    if (sp[0][sp[0].size() - 1] == ' ')
                        cs.global_message_format = string(sp[0].begin(), sp[0].end() - 1) + sp[1];
                    if (sp[1][sp[1].size() - 1] == ' ')
                        cs.global_message_format = sp[0] + string(sp[1].begin(), sp[1].end() - 1);
                    replaceAll(cs.global_message_format, "%player%", res_nick);
                    replaceAll(cs.global_message_format, "%suffix%", pl.suffix);
                    replaceAll(cs.global_message_format, "%message%", ev.mMessage);
                }
                if (pl.suffix == "")
                {
                    auto sp = split(cs.global_message_format, "%suffix%");
                    if (sp[0][sp[0].size() - 1] == ' ')
                        cs.global_message_format = string(sp[0].begin(), sp[0].end() - 1) + sp[1];
                    if (sp[1][sp[1].size() - 1] == ' ')
                        cs.global_message_format = sp[0] + string(sp[1].begin(), sp[1].end() - 1);
                    replaceAll(cs.global_message_format, "%prefix%", pl.prefix);
                    replaceAll(cs.global_message_format, "%player%", res_nick);
                    replaceAll(cs.global_message_format, "%message%", ev.mMessage);
                }
                if (pl.prefix == "" && pl.suffix == "")
                {
                    auto sp = split(cs.global_message_format, "%prefix%");
                    cs.global_message_format = sp[0] + sp[1];
                    auto sp1 = split(cs.global_message_format, "%suffix%");
                    cs.global_message_format = sp1[0] + sp1[1];
                    replaceAll(cs.global_message_format, "%player%", res_nick);
                    replaceAll(cs.global_message_format, "%message%", ev.mMessage);
                }
                regex reg("§");
                regex reg1("§k");
                smatch smt, smt1;
                if (regex_search(ev.mMessage, smt, reg) && regex_search(ev.mMessage, smt1, reg1) != true && checkPerm(res_nick, "chatmanager.chat.color") == false)
                {
                    ev.mPlayer->sendText(utf8_encode(L"У вас нет прав использовать цветные сообщения!"));
                    return 0;
                }
                else if (regex_search(ev.mMessage, smt1, reg1) && checkPerm(res_nick, "chatmanager.chat.magic") == false)
                {
                    ev.mPlayer->sendText(utf8_encode(L"У вас нет прав использовать волшебный цвет в сообщениях!"));
                    return 0;
                }
                return 1;
            }
            else if (chat_ranged && chat_range != 0)
            {
                ChatConfig cs;
                YAML::Node node = YAML::LoadFile("plugins/Permissions Ex/chat.yml");
                cs = node.as<ChatConfig>();
                Users users;
               // YAML::Node node23 = YAML::LoadFile("plugins/Permissions Ex/users.yml");
                for (const auto& p : config1["users"])
                    users.users.push_back(p.as<_User>());
                string res_nick;
                auto plain = ev.mPlayer->getName();
                auto x = split(plain, " ");
                for (int i = 0; i < x.size(); ++i)
                {
                    for (auto v : users.users)
                    {
                        if (x[i] == v.nickname)
                        {
                            res_nick = x[i];
                            break;
                        }
                    }
                }
                auto pl123 = load_user(res_nick);
                auto players = Level::getAllPlayers();
                regex reg("§");
                regex reg1("§k");
                smatch smt, smt1;
                if (ev.mMessage[0] != '!' && checkPerm(res_nick,"chatmanager.override.ranged") == false)
                {
                    for (auto p : players)
                    {
                        double x1 = ev.mPlayer->getPos().x, y1 = ev.mPlayer->getPos().y, z1 = ev.mPlayer->getPos().z, x2 = p->getPos().x, y2 = p->getPos().y, z2 = p->getPos().z;
                        double distance = sqrt(pow(x2 - x1, 2) + pow(y2 - y1, 2) + pow(z2 - z1, 2));
                        if (distance <= cs.chat_range)
                        {
                            if (pl123.prefix == "")
                            {
                                auto sp = split(cs.message_format, "%prefix%");
                                if (sp[0][sp[0].size() - 1] == ' ')
                                    cs.message_format = string(sp[0].begin(), sp[0].end() - 1) + sp[1];
                                if (sp[1][sp[1].size() - 1] == ' ')
                                    cs.message_format = sp[0] + string(sp[1].begin(), sp[1].end() - 1);
                                replaceAll(cs.message_format, "%player%", res_nick);
                                replaceAll(cs.message_format, "%suffix%", pl123.suffix);
                                replaceAll(cs.message_format, "%message%", ev.mMessage);
                            }
                            if (pl123.suffix == "")
                            {
                                auto sp = split(cs.message_format, "%suffix%");
                                if (sp[0][sp[0].size() - 1] == ' ')
                                    cs.message_format = string(sp[0].begin(), sp[0].end() - 1) + sp[1];
                                if (sp[1][sp[1].size() - 1] == ' ')
                                    cs.message_format = sp[0] + string(sp[1].begin(), sp[1].end() - 1);
                                replaceAll(cs.message_format, "%prefix%", pl123.prefix);
                                replaceAll(cs.message_format, "%player%", res_nick);
                                replaceAll(cs.message_format, "%message%", ev.mMessage);
                            }
                            if (pl123.prefix == "" && pl123.suffix == "")
                            {
                                auto sp = split(cs.message_format, "%prefix%");
                                cs.message_format = sp[0] + sp[1];
                                auto sp1 = split(cs.message_format, "%suffix%");
                                cs.message_format = sp1[0] + sp1[1];
                                replaceAll(cs.message_format, "%player%", res_nick);
                                replaceAll(cs.message_format, "%message%", ev.mMessage);
                            }
                            if (pl123.prefix != "" && pl123.suffix != "")
                            {
                                replaceAll(cs.message_format, "%prefix%", pl123.prefix);
                                replaceAll(cs.message_format, "%player%", res_nick);
                                replaceAll(cs.message_format, "%suffix%", pl123.suffix);
                                replaceAll(cs.message_format, "%message%", ev.mMessage);
                            }
                            if (regex_search(ev.mMessage, smt, reg) && regex_search(ev.mMessage, smt1, reg1) != true && checkPerm(res_nick, "chatmanager.chat.color") == false)
                            {
                                ev.mPlayer->sendText(utf8_encode(L"У вас нет прав использовать цветные сообщения!"));
                                return 0;
                            }
                            else if (regex_search(ev.mMessage, smt1, reg1) && checkPerm(res_nick, "chatmanager.chat.magic") == false)
                            {
                                ev.mPlayer->sendText(utf8_encode(L"У вас нет прав использовать волшебный цвет в сообщениях!"));
                                return 0;
                            }
                            (ServerPlayer*)p->sendText(utf8_encode(to_wstring("[§eL§r] ")) + cs.message_format);
                            return 0;
                        }
                    }
                    return 0;
                }
                else if (ev.mMessage[0] == '!' && (checkPerm(res_nick, "chatmanager.chat.global") == true || checkPerm(res_nick, "chatmanager.override.ranged") == true))
                {
                    auto msg = string(ev.mMessage.begin()+1, ev.mMessage.end());
                    for (auto p : players)
                    {
                        if (pl123.prefix == "")
                        {
                            auto sp = split(cs.global_message_format, "%prefix%");
                            cs.global_message_format = sp[0] + sp[1];
                            replaceAll(cs.global_message_format, "%player%", res_nick);
                            replaceAll(cs.global_message_format, "%suffix%", pl123.suffix);
                            replaceAll(cs.global_message_format, "%message%", ev.mMessage);
                        }
                        if (pl123.suffix == "")
                        {
                            auto sp = split(cs.global_message_format, "%suffix%");
                            cs.global_message_format = sp[0] + sp[1];
                            replaceAll(cs.global_message_format, "%prefix%", pl123.prefix);
                            replaceAll(cs.global_message_format, "%player%", res_nick);
                            replaceAll(cs.global_message_format, "%message%",msg);
                        }
                        if (pl123.prefix == "" && pl123.suffix == "")
                        {
                            auto sp = split(cs.global_message_format, "%prefix%");
                            cs.global_message_format = sp[0] + sp[1];
                            auto sp1 = split(cs.global_message_format, "%suffix%");
                            cs.global_message_format = sp1[0] + sp1[1];
                            replaceAll(cs.global_message_format, "%player%", res_nick);
                            replaceAll(cs.global_message_format, "%message%", msg);
                        }
                        if (pl123.prefix != "" && pl123.suffix != "")
                        {
                            replaceAll(cs.global_message_format, "%prefix%",pl123.prefix);
                            replaceAll(cs.global_message_format, "%player%", res_nick);
                            replaceAll(cs.global_message_format, "%suffix%", pl123.suffix);
                            replaceAll(cs.global_message_format, "%message%", msg);
                        }
                        if (regex_search(msg, smt, reg) && regex_search(msg, smt1, reg1) != true && checkPerm(res_nick, "chatmanager.chat.color") == false)
                        {
                            ev.mPlayer->sendText(utf8_encode(L"У вас нет прав использовать цветные сообщения!"));
                            return 0;
                        }
                        else if (regex_search(msg, smt1, reg1) && checkPerm(res_nick, "chatmanager.chat.magic") == false)
                        {
                            ev.mPlayer->sendText(utf8_encode(L"У вас нет прав использовать волшебный цвет в сообщениях!"));
                            return 0;
                        }
                        ev.mPlayer->sendText(utf8_encode(to_wstring("[§aG§r] ")) + cs.global_message_format);
                        return 0;
                    }
                }
                else if (ev.mMessage[0] == '!' && checkPerm(res_nick, "chatmanager.chat.global") == false)
                {
                    ev.mPlayer->sendText(utf8_encode(L"У вас недостаточно прав для отправки сообщений в глобальный чат!"));
                    return 0;
                }
            }
    });
    Event::PlayerCmdEvent::subscribe([](const Event::PlayerCmdEvent& ev) 
    {
            if (debug_mode)
            {
                ofstream fout("server.log", ios_base::app);
                Users users;
                YAML::Node node = YAML::LoadFile("plugins/Permissions Ex/users.yml");
                for (const auto& p : node["users"])
                    users.users.push_back(p.as<_User>());
                string res_nick;
                auto plain = ev.mPlayer->getName();
                auto x = split(plain, " ");
                for (int i = 0; i < x.size(); ++i)
                {
                    for (auto v : users.users)
                    {
                        if (x[i] == v.nickname)
                        {
                            res_nick = x[i];
                            break;
                        }
                    }
                }
                fout << "[" + currentDateTime() + "]: Игрок " + res_nick + " использовал команду " + ev.mCommand + "\n";
                fout.close();
                return 1;
            }
            return 1;
    });
    makeDir("plugins/Permissions Ex");
    Event::ServerStartedEvent::subscribe([](const Event::ServerStartedEvent& ev)
        {
            try
            {
                ifstream inpp("plugins/Permissions Ex/worlds.yml");
                if (!inpp.is_open())
                {
                    inpp.close();
                    World overworld;
                    overworld.name = "OverWorld";
                    World nether;
                    nether.name = "Nether";
                    World end;
                    end.name = "End";
                    YAML::Node node;
                    node["worlds"].push_back(overworld);
                    node["worlds"].push_back(nether);
                    node["worlds"].push_back(end);
                    ofstream fout("plugins/Permissions Ex/worlds.yml");
                    fout << node;
                    fout.close();
                }
                inpp.close();
                ifstream inp("plugins/Permissions Ex/chat.yml");
                if (!inp.is_open())
                {
                    inp.close();
                    ChatConfig chat;
                    chat.enabled = false;
                    chat.range_mode = false;
                    chat.chat_range = 0;
                    chat.display_name_format = utf8_encode(L"%prefix% %player% %suffix%"); //здесь я буду парсить уже регуряками,подставляя вместо %prefix% префикс из бекенда юзеров,вместо %suffix% суфикс из бекенда юзеров,а вместо %player% будет ник игрока
                    chat.global_message_format = utf8_encode(L"%prefix% %player% %suffix% §f%message%"); //%message% - само сообщение юзера в чат. Пример(локальный и глобальный чаты выключены): Admin(прфеикс) GolfStreamer341(ник) Paradoxal(суффикс): Hello(сообщение будет белым цветом).  Пример(локальный и глобальный чаты включены)(сообщение отправляется в глобальный чат): [G] Admin(прфеикс) GolfStreamer341(ник) Paradoxal(суффикс): Hello(сообщение будет белым цветом).
                    chat.message_format = utf8_encode(L"%prefix% %player% %suffix% §l%message%"); //это уже формат локального сообщения,цвет тот же,но только жирным шрифтом сообщение будет
                    chatconf = chat;
                    chatconfig = chat;
                    ofstream fout("plugins/Permissions Ex/chat.yml");
                    fout << chatconf;
                    fout.close();
                    enabled = false;
                    chat_ranged = false;
                    chat_range = 0;
                    display_name_format = "%prefix% %player% %suffix%";
                    global_message_format = "%prefix% %player% %suffix% §f%message%";
                    message_format = "%prefix% %player% %suffix% §l%message%";
                }
                inp.close();
                chatconf = YAML::LoadFile("plugins/Permissions Ex/chat.yml");
                inp.open("plugins/Permissions Ex/modifyworld.yml");
                if (!inp.is_open())
                {
                    inp.close();
                    ModifyworldConfig mw;
                    mw.informPlayers = true;
                    mw.messages.push_back("whitelist:You are not allowed to join this server. Goodbye!");
                    mw.messages.push_back("permissionDenied:Sorry, you don't have enough permissions!");
                    mw.messages.push_back("invalidArgument:You have entered the wrong argument(s) or you have not entered one of the required ones!");
                    mw.whitelist = false;
                    modworldconf = mw;
                    ofstream fout("plugins/Permissions Ex/modifyworld.yml");
                    fout << modworldconf;
                    fout.close();
                    informPlayers = true;
                    whitelist = false;
                }
                inp.close();
                modworldconf = YAML::LoadFile("plugins/Permissions Ex/modifyworld.yml");
                using namespace std;
                ifstream in("plugins/Permissions Ex/groups.yml");
                if (!in.is_open())
                {
                    in.close();
                    _Group def_group;
                    def_group.name = "player";
                    wstring tmp = L"§aPlayer§r";
                    char str[64];
                    long uLen = WideCharToMultiByte(CP_UTF8, 0, tmp.c_str(), -1, NULL, NULL, NULL, NULL);
                    WideCharToMultiByte(CP_UTF8, 0, tmp.c_str(), uLen, str, uLen, NULL, NULL);
                    def_group.prefix = str;
                    def_group.is_default = true;
                    def_group.inheritances = {};
                    def_group.perms = {};
                    def_group.build = true;
                    YAML::Node worlds = YAML::LoadFile("plugins/Permissions Ex/worlds.yml");
                    vector<World> worlds_v;
                    for (const auto& p : worlds["worlds"])
                        worlds_v.push_back(p.as<World>());
                    World overworld = worlds_v[0];
                    World nether = worlds_v[1];
                    World end = worlds_v[2];
                    def_group.worlds.push_back(overworld);
                    def_group.worlds.push_back(nether);
                    def_group.worlds.push_back(end);
                    config["groups"].push_back(def_group);
                    std::ofstream fout("plugins/Permissions Ex/groups.yml");
                    fout << config;
                    fout.close();
                }
                inp.close();
                config = YAML::LoadFile("plugins/Permissions Ex/groups.yml");
                in.open("plugins/Permissions Ex/users.yml");;
                if (!in.is_open())
                {
                    in.close();
                    _User u;
                    u.nickname = "This is null-player.Not removed!";
                    u.groups.push_back("player");
                    auto gr = load_group("player");
                    u.prefix = gr.prefix;
                    u.suffix = "";
                    u.permissions = {};
                    YAML::Node worlds = YAML::LoadFile("plugins/Permissions Ex/worlds.yml");
                    vector<World> worlds_v;
                    for (const auto& p : worlds["worlds"])
                        worlds_v.push_back(p.as<World>());
                    World overworld = worlds_v[0];
                    World nether = worlds_v[1];
                    World end = worlds_v[2];
                    u.worlds.push_back(overworld);
                    u.worlds.push_back(nether);
                    u.worlds.push_back(end);
                    users.users.push_back(u);
                    config1["users"].push_back(u);
                    std::ofstream fout("plugins/Permissions Ex/users.yml");
                    locale utfFile("en_US.UTF-8");
                    fout.imbue(utfFile);
                    fout << config1;
                    fout.close();
                }
                in.close();
                config1 = YAML::LoadFile("plugins/Permissions Ex/users.yml");
                for (const auto& p : config["groups"]) {
                    groups.groups.push_back(p.as<_Group>());
                }
                users.users.clear();
                for (const auto& p : config1["users"]) {
                    users.users.push_back(p.as<_User>());
                }
                enabled = chatconf.as<ChatConfig>().enabled;
                chat_ranged = chatconf.as<ChatConfig>().range_mode;
                chat_range = chatconf.as<ChatConfig>().chat_range;
                display_name_format = chatconf.as<ChatConfig>().display_name_format;
                global_message_format = chatconf.as<ChatConfig>().global_message_format;
                message_format = chatconf.as<ChatConfig>().message_format;
                informPlayers = modworldconf.as<ModifyworldConfig>().informPlayers;
                whitelist = modworldconf.as<ModifyworldConfig>().whitelist;
            }
            catch (std::exception& e)
            {
                std::cerr << e.what() << std::endl;
            }
            return 1;
        });

    Event::RegCmdEvent::subscribe([](const Event::RegCmdEvent& ev)
        {
            try
            {
                Pex::setup(ev.mCommandRegistry);
            }
            catch (std::exception s) {
                std::cerr << s.what() << std::endl;
            }
            return true;
        });
}
