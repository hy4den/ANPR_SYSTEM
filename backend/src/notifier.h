#pragma once
#include <string>
#include <vector>
#include "database.h"

// Dispatches async e-mail alerts to admins for VIP / Blacklisted events.
void notify_admins_async(const std::vector<AdminUser>& admins,
                         const std::string& plate,
                         const std::string& category,
                         const std::string& timestamp);
