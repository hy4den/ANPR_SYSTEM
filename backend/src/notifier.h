#pragma once
#include <string>
#include <vector>
#include "database.h"

// Dispatches async e-mail alerts to admins for VIP / Blacklisted events.
void notify_admins_async(const std::vector<AdminUser>& admins,
                         const std::string& plate,
                         const std::string& category,
                         const std::string& timestamp);

// Dispatches async e-mail to the vehicle owner when access is granted.
void notify_owner_access_granted_async(const std::string& owner_email,
                                       const std::string& owner_name,
                                       const std::string& plate,
                                       const std::string& timestamp);

// Dispatches async mobile push notifications via Firebase Cloud Messaging.
void notify_mobile_access_granted_async(const std::vector<DeviceToken>& device_tokens,
                                        const std::string& plate,
                                        const std::string& timestamp);
