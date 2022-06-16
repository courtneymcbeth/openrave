// -*- coding: utf-8 -*-
// Copyright (C) 2022 Rosen Diankov (rdiankov@cs.cmu.edu)
//
// This file is part of OpenRAVE.
// OpenRAVE is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
#include <cstdarg>
#include <cstring>
#include <functional>
#include <mutex>

#include <openrave/openraveexception.h>
#include <openrave/logging.h>

#ifdef HAVE_BOOST_FILESYSTEM
#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;
#endif
#include <boost/version.hpp>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <dlfcn.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>

#endif

#ifdef _WIN32
const char s_filesep = '\\';
const char* s_delimiter = ";";
#else
const char s_filesep = '/';
const char* s_delimiter = ":";
#endif

#include "libopenrave.h"
#include "plugindatabase.h"

namespace OpenRAVE {

DynamicRaveDatabase::DynamicLibrary::DynamicLibrary(const std::string& path)
{
#ifdef _WIN32
    _handle = LoadLibraryA(path.c_str());
    if( _handle == NULL ) {
        RAVELOG_WARN_FORMAT("Failed to load %s\n", path);
    }
#else
    dlerror(); // clear error
    _handle = dlopen(path.c_str(), RTLD_NOW);
    char* pstr = dlerror();
    if( pstr != NULL ) {
        RAVELOG_WARN_FORMAT("%s: %s\n", path % pstr);
        if( _handle != NULL ) {
            dlclose(_handle);
        }
        _handle = NULL;
    }
#endif
}

DynamicRaveDatabase::DynamicLibrary::DynamicLibrary(DynamicRaveDatabase::DynamicLibrary&& other)
{
    this->_handle = other._handle;
    other._handle = NULL;
}

DynamicRaveDatabase::DynamicLibrary::~DynamicLibrary() noexcept
{
    if (_handle) {
#ifdef _WIN32
        FreeLibrary((HINSTANCE)_handle);
#else
        // Eagerly closing the library handle will cause segfaults during testing as tests don't fully reset openrave.
        // The problem can be alleviated by adding RTLD_NODELETE flag in dlopen(), but this has to be done everywhere
        // (including other programs that also load dynamic libraries).
        // In order to minimize the amount of changes we have to make, we will simply omit this call.
        // It is safe anyway, as the OS maintains it's own refcount of opened libraries.
        //dlclose(_handle);
#endif
    }
}

void* DynamicRaveDatabase::DynamicLibrary::LoadSymbol(const std::string& name, std::string& errstr) const
{
#ifdef _WIN32
    return GetProcAddress((HINSTANCE)_handle, name.c_str());
#else
    dlerror(); // clear error
    void* psym = dlsym(_handle, name.c_str());
    if (psym == NULL) {
        char* pstr = dlerror();
        errstr.assign(pstr);
    }
    return psym;
#endif
}

DynamicRaveDatabase::DynamicRaveDatabase()
{
}

DynamicRaveDatabase::~DynamicRaveDatabase()
{
    Destroy();
}

void DynamicRaveDatabase::Init()
{
    const char* pOPENRAVE_PLUGINS = getenv("OPENRAVE_PLUGINS"); // getenv not thread-safe?
    if (!pOPENRAVE_PLUGINS) {
        RAVELOG_WARN("Failed to read environment variable OPENRAVE_PLUGINS");
        return;
    }
    std::vector<std::string> vplugindirs;
    utils::TokenizeString(pOPENRAVE_PLUGINS, s_delimiter, vplugindirs);
    for (int iplugindir = vplugindirs.size() - 1; iplugindir > 0; iplugindir--) {
        int jplugindir = 0;
        for(; jplugindir < iplugindir; jplugindir++) {
            if(vplugindirs[iplugindir] == vplugindirs[jplugindir]) {
                break;
            }
        }
        if (jplugindir < iplugindir) {
            vplugindirs.erase(vplugindirs.begin()+iplugindir);
        }
    }
    bool bExists = false;
    std::string installdir = OPENRAVE_PLUGINS_INSTALL_DIR;
#ifdef HAVE_BOOST_FILESYSTEM
    if( !boost::filesystem::is_directory(boost::filesystem::path(installdir)) ) {
#ifdef _WIN32
        HKEY hkey;
        if(RegOpenKeyEx(HKEY_LOCAL_MACHINE, TEXT("Software\\OpenRAVE\\" OPENRAVE_VERSION_STRING), 0, KEY_QUERY_VALUE, &hkey) == ERROR_SUCCESS) {
            DWORD dwType = REG_SZ;
            CHAR szInstallRoot[4096];     // dont' take chances, it is windows
            DWORD dwSize = sizeof(szInstallRoot);
            RegQueryValueEx(hkey, TEXT("InstallRoot"), NULL, &dwType, (PBYTE)szInstallRoot, &dwSize);
            RegCloseKey(hkey);
            installdir.assign(szInstallRoot);
            installdir += str(boost::format("%cshare%copenrave-%d.%d%cplugins")%s_filesep%s_filesep%OPENRAVE_VERSION_MAJOR%OPENRAVE_VERSION_MINOR%s_filesep);
            RAVELOG_VERBOSE(str(boost::format("window registry plugin dir '%s'")%installdir));
        }
        else
#endif
        {
            RAVELOG_WARN_FORMAT("%s doesn't exist", installdir);
        }
    }
    boost::filesystem::path pluginsfilename = boost::filesystem::absolute(boost::filesystem::path(installdir));
    FOREACH(itname, vplugindirs) {
        if( pluginsfilename == boost::filesystem::absolute(boost::filesystem::path(*itname)) ) {
            bExists = true;
            break;
        }
    }
#else
    std::string pluginsfilename=installdir;
    FOREACH(itname, vplugindirs) {
        if( pluginsfilename == *itname ) {
            bExists = true;
            break;
        }
    }
#endif
    if( !bExists ) {
        vplugindirs.push_back(installdir);
    }
    for (std::string& entry : vplugindirs) {
        if (entry.empty()) {
            continue;
        }
        _vPluginDirs.emplace_back(std::move(entry));
    }
    for (const std::string& entry : _vPluginDirs) {
        RAVELOG_DEBUG_FORMAT("Looking for plugins in %s", entry);
        _LoadPluginsFromPath(entry);
    }
}

void DynamicRaveDatabase::Destroy()
{
    while (!_vPlugins.empty()) {
        _vPlugins.back()->Destroy();
        _vPlugins.pop_back();
    }
}

void DynamicRaveDatabase::OnRaveInitialized()
{
    for (const PluginPtr& pluginPtr : _vPlugins) {
        if (pluginPtr) {
            pluginPtr->OnRaveInitialized();
        }
    }
}

void DynamicRaveDatabase::OnRavePreDestroy()
{
    for (const PluginPtr& pluginPtr : _vPlugins) {
        if (pluginPtr) {
            pluginPtr->OnRavePreDestroy();
        }
    }
}

void DynamicRaveDatabase::ReloadPlugins()
{
    std::vector<PluginPtr> vPlugins;
    {
        std::lock_guard<std::mutex> lock(_mutex);
        vPlugins = _vPlugins; // Copy
    }
    for (const PluginPtr& plugin : vPlugins) {
        _LoadPlugin(plugin->GetPluginPath());
    }
}

bool DynamicRaveDatabase::LoadPlugin(const std::string& libraryname)
{
    // If the libraryname matches any of the existing loaded libraries, then reload it
    std::vector<PluginPtr>::iterator it = std::find_if(_vPlugins.begin(), _vPlugins.end(), [&](const PluginPtr& plugin) {
        return (plugin->GetPluginName() == libraryname || plugin->GetPluginPath() == libraryname);
    });
    if (it != _vPlugins.end()) {
        std::lock_guard<std::mutex> lock(_mutex);
        std::rotate(it, it + 1, _vPlugins.end());
        _vPlugins.pop_back();
    }
    return _LoadPlugin(libraryname);
}

UserDataPtr DynamicRaveDatabase::AddVirtualPlugin(InterfaceType type, std::string name, std::function<InterfaceBasePtr(EnvironmentBasePtr, std::istream&)> createfn)
{
    std::lock_guard<std::mutex> lock(_mutex);
    _vPlugins.emplace_back(boost::make_shared<VirtualPlugin>(type, std::move(name), std::move(createfn)));
    return _vPlugins.back();
}

void DynamicRaveDatabase::GetPluginInfo(std::list< std::pair<std::string, PLUGININFO> >& pluginInfo) const
{
    for (const PluginPtr& entry : _vPlugins) {
        PLUGININFO info;
        info.interfacenames = entry->GetInterfaces();
        info.version = entry->GetOpenRAVEVersion();
        pluginInfo.emplace_back(std::make_pair(entry->GetPluginPath(), std::move(info)));
    }
}

InterfaceBasePtr DynamicRaveDatabase::Create(EnvironmentBasePtr penv, InterfaceType type, std::string name)
{
    InterfaceBasePtr pointer;
    if (name.empty()) {
        switch(type) {
        case PT_KinBody: {
            pointer.reset(new KinBody(PT_KinBody,penv));
            pointer->__strxmlid = ""; // don't set to KinBody since there's no officially registered interface
            break;
        }
        case PT_PhysicsEngine: name = "GenericPhysicsEngine"; break;
        case PT_CollisionChecker: name = "GenericCollisionChecker"; break;
        case PT_Robot: name = "GenericRobot"; break;
        case PT_Trajectory: name = "GenericTrajectory"; break;
        default: break;
        }
    }

    if (!pointer) {
        // Some plugins have its creation parameters in the string after its name
        size_t position = std::min<size_t>(name.find_first_of(' '), name.size());
        if (position == 0) {
            RAVELOG_WARN_FORMAT("interface %s name \"%s\" needs to start with a valid character\n", RaveGetInterfaceName(type) % name);
            return InterfaceBasePtr();
        }
        std::string interfacename = name.substr(0, position);
        for (const PluginPtr& plugin : _vPlugins) {
            if (plugin->HasInterface(type, interfacename)) {
                try {
                    pointer = plugin->OpenRAVECreateInterface(type, name, RaveGetInterfaceHash(type), OPENRAVE_ENVIRONMENT_HASH, penv);
                } catch (const std::exception& e) {
                    RAVELOG_WARN_FORMAT("Failed to create interface from %s at %s", plugin->GetPluginName() % plugin->GetPluginPath());
                    plugin->AddBadInterface(type, name); // Bad interface, no cookie
                }
                if (pointer) {
                    if (pointer->GetInterfaceType() != type) {
                        RAVELOG_FATAL_FORMAT("plugin interface name %s, type %s, types do not match\n", name%RaveGetInterfaceName(type));
                        plugin->AddBadInterface(type, interfacename);
                        pointer.reset();
                    } else {
                        pointer->__strpluginname = plugin->GetPluginPath(); // __internal__ if it's a virtual plugin
                        pointer->__strxmlid = name;
                        break;
                    }
                }
            }
        }
    }

    // Some extra checks on validity of Robot objects
    if (pointer && type == PT_Robot) {
        RobotBasePtr probot = RaveInterfaceCast<RobotBase>(pointer);
        //if ( strncmp(probot->GetKinBodyHash(), OPENRAVE_KINBODY_HASH) != 0 ) {}
        if ( !probot->IsRobot() ) {
            RAVELOG_FATAL_FORMAT("interface Robot, name %s should have IsRobot() return true", name);
            pointer.reset();
        }
    }

    if (!pointer) {
        RAVELOG_WARN_FORMAT("env=%d failed to create name %s, interface %s\n", penv->GetId()%name%RaveGetInterfaceNamesMap().find(type)->second);
        return pointer;
    }

    if (pointer->GetInterfaceType() == type) {
        // No-op, this is correct
    } else if ((pointer->GetInterfaceType() == PT_Robot) && (type == PT_KinBody)) {
        // Special case: Robots are also KinBodies.
        // No-op, this is correct
    } else {
        // Return an empty pointer; behaviour inherited from `RaveInterfaceCast`
        pointer.reset();
    }
    return pointer;
}

void DynamicRaveDatabase::GetLoadedInterfaces(std::map<InterfaceType, std::vector<std::string>>& interfacenames) const
{
    interfacenames.clear();
    for (const PluginPtr& plugin : _vPlugins) {
        const RavePlugin::InterfaceMap& interfaces = plugin->GetInterfaces();
        for (const std::pair<InterfaceType, std::vector<std::string>>& entry : interfaces) {
            interfacenames[entry.first].insert(interfacenames[entry.first].end(), entry.second.begin(), entry.second.end());
        }
    }
}

bool DynamicRaveDatabase::HasInterface(InterfaceType type, const std::string& interfacename) const
{
    for (const PluginPtr& plugin : _vPlugins) {
        if (plugin->HasInterface(type, interfacename)) {
            return true;
        }
    }
    return false;
}

void DynamicRaveDatabase::_LoadPluginsFromPath(const std::string& strpath, bool recurse) try
{
#if defined (__APPLE_CC__)
#define PLUGIN_EXT ".dylib"
#elif defined (_WIN32)
#define PLUGIN_EXT ".dll"
#else
#define PLUGIN_EXT ".so"
#endif

#ifdef HAVE_BOOST_FILESYSTEM
    fs::path path(strpath);
    if (fs::is_empty(path)) {
        return;
    } else if (fs::is_directory(path)) {
        for (const auto entry : fs::directory_iterator(path, fs::directory_options::skip_permission_denied)) {
            if (fs::is_directory(entry) && recurse) {
                _LoadPluginsFromPath(entry.path().string(), true);
            } else {
                _LoadPluginsFromPath(entry.path().string(), false);
            }
        }
    } else if (fs::is_regular_file(path)) {
        // Check that the file has a platform-appropriate extension
        if (path.has_extension() && path.extension() == PLUGIN_EXT) {
            _LoadPlugin(path.string());
        }
    } else {
        RAVELOG_WARN_FORMAT("Path is not a valid directory or file: %s", strpath);
    }
#else
    int err = 0;
    struct stat sb;
    err = ::stat(strpath.c_str(), &sb);
    if (err != 0) {
        err = errno;
        throw std::runtime_error("Failed to stat path at " + strpath + ": " + ::strerror(err));
    }
    if (S_ISDIR(sb.st_mode)) {
        DIR* dirptr = ::opendir(strpath.c_str());
        if (dirptr == NULL) {
            err = errno;
            throw std::runtime_error("Failed to open directory at " + strpath + ": " + ::strerror(err));
        }
        struct dirent* entry = NULL;
        std::string pathstr;
        for (entry = ::readdir(dirptr); entry != NULL; entry = ::readdir(dirptr)) {
            switch (entry->d_type) {
            case DT_DIR: {
                if (!recurse) {
                    break;
                }
            }
            case DT_REG: {
                pathstr.assign(entry->d_name, sizeof(entry->d_name));
                _LoadPluginsFromPath(pathstr, recurse);
                break;
            }
            default: continue;
            }
        }
        ::closedir(dirptr);
    } else if (S_ISREG(sb.st_mode)) {
        _LoadPlugin(dirname);
    } else {
        // Not a directory or file, ignore it
    }
#endif

} catch (const std::exception& e) {
    // Some paths have permissions issues, just skip those paths.
    RAVELOG_VERBOSE_FORMAT("%s", e.what());
}

bool DynamicRaveDatabase::_LoadPlugin(const std::string& strpath)
{
    DynamicLibrary dylib(strpath);
    if (!dylib) {
        RAVELOG_DEBUG_FORMAT("Failed to load shared object %s", strpath);
        return false;
    }
    std::string errstr;
    void* psym = dylib.LoadSymbol("CreatePlugin", errstr);
    if (!psym) {
        RAVELOG_DEBUG_FORMAT("%s, might not be an OpenRAVE plugin.", errstr);
        return false;
    }
    RavePlugin* plugin = nullptr;
    try {
        plugin = reinterpret_cast<PluginExportFn_Create>(psym)();
    } catch (const std::exception& e) {
        RAVELOG_WARN_FORMAT("Failed to construct a RavePlugin from %s: %s", strpath % e.what());
    }
    if (!plugin) {
        return false;
    }
    std::lock_guard<std::mutex> lock(_mutex);
    _mapLibraryHandles.emplace(strpath, std::move(dylib)); // Keep the library handle around in case we need it
    _vPlugins.emplace_back(PluginPtr(plugin)); // Ownership passed to the shared_ptr
    _vPlugins.back()->SetPluginPath(strpath);
    RAVELOG_DEBUG_FORMAT("Found %s at %s.", _vPlugins.back()->GetPluginName() % strpath);
    return true;
}

} // namespace OpenRAVE
