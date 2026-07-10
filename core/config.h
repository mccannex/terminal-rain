#pragma once
#include "stream_field.h"

// Loads matrix.cfg from next to the running executable (simple key=value
// lines, '#' comments). Falls back to StreamFieldConfig's defaults for any
// key that's missing or unparsable, and if the file itself is missing.
StreamFieldConfig loadConfig(const char* fileName);
