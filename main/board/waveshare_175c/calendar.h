#pragma once
#include "board.h"

namespace ws {
// The system epoch stores local civil time as if it were UTC. No global TZ or
// daylight-saving rule is imposed on the app's clock editor.
inline bool valid_date(const board::DateTime &t) {
    constexpr int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (t.year < 2000 || t.year > 2099 || t.month < 1 || t.month > 12)
        return false;
    int limit = days[t.month - 1] + (t.month == 2 && t.year % 4 == 0);
    return t.day >= 1 && t.day <= limit && t.hour >= 0 && t.hour < 24 && t.minute >= 0 && t.minute < 60 &&
           t.second >= 0 && t.second < 60;
}
inline int64_t civil_epoch(const board::DateTime &t) {
    // Called only after validation; 2000..2099 includes no non-leap century.
    constexpr int before[] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
    int years = t.year - 2000;
    int days = years * 365 + (years + 3) / 4 + before[t.month - 1] + t.day - 1;
    if (t.month > 2 && t.year % 4 == 0)
        ++days;
    return 946684800LL + int64_t(days) * 86400 + t.hour * 3600 + t.minute * 60 + t.second;
}
inline bool valid_epoch(int64_t t) { return t >= 946684800LL && t < 4102444800LL; }
} // namespace ws
