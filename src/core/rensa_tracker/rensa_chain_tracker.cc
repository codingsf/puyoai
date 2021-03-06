#include "core/rensa_tracker/rensa_chain_tracker.h"

#include <iomanip>
#include <sstream>

using namespace std;

RensaChainTrackResult::RensaChainTrackResult() :
    erasedAt_ {}
{
}

RensaChainTrackResult::RensaChainTrackResult(const string& s) :
    RensaChainTrackResult()
{
    CHECK(s.size() % 6 == 0) << s.size();

    int height = static_cast<int>(s.size() / 6);
    for (size_t i = 0; i < s.size(); ++i) {
        size_t x = (i % 6) + 1;
        size_t y = height - (i / 6);
        if (s[i] == ' ' || s[i] == '.')
            continue;

        if ('0' <= s[i] && s[i] <= '9')
            erasedAt_[x][y] = s[i] - '0';
        else if ('A' <= s[i] && s[i] <= 'F')
            erasedAt_[x][y] = s[i] - 'A' + 10;
        else if ('a' <= s[i] && s[i] <= 'f')
            erasedAt_[x][y] = s[i] - 'a' + 10;
        else
            CHECK(false) << s[i];
    }
}

RensaChainTrackResult& RensaChainTrackResult::operator=(const RensaChainTrackResult& result)
{
    for (int x = 0; x < FieldConstant::MAP_WIDTH; ++x) {
        for (int y = 0; y < FieldConstant::MAP_HEIGHT; ++y)
            erasedAt_[x][y] = result.erasedAt_[x][y];
    }

    return *this;
}

string RensaChainTrackResult::toString() const
{
    ostringstream ss;
    for (int y = FieldConstant::HEIGHT; y >= 1; --y) {
        for (int x = 1; x <= FieldConstant::WIDTH; ++x)
            ss << std::setw(3) << static_cast<int>(erasedAt_[x][y]);
        ss << '\n';
    }

    return ss.str();
}
