#pragma once

int
gt_get_value_by_nick (GType type, const char *value, int fallback);

const char *
gt_get_value_nick (GType type, int value);
