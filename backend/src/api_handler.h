#pragma once
#include <httplib.h>
#include "database.h"

void register_routes(httplib::Server& svr, Database& db);
