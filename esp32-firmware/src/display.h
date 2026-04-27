#pragma once
#include <Arduino.h>

bool display_init();
void display_message(const String& line1, const String& line2 = "");
void display_clear();
