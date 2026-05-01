#pragma once

#include <functional>
#include <string>
#include <vector>

namespace thewatcher::service
{

struct WindowsServiceDefinition
{
    std::string name;
    std::string display_name;
    std::string description;
    std::string command_line;
};

std::string current_executable_path();
std::string quote_windows_argument(const std::string& value);
std::string build_service_command_line(const std::vector<std::string>& args);

int install_windows_service(const WindowsServiceDefinition& definition);
int uninstall_windows_service(const std::string& name);
int run_windows_service(const std::string& name, const std::function<void()>& run, const std::function<void()>& stop);

} // namespace thewatcher::service
