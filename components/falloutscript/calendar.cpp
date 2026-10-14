#include "calendar.hpp"

#include <algorithm>
#include <array>

namespace FalloutScript
{
    int Calendar::daysInMonth(int month) const
    {
        static constexpr std::array<int, monthsInYear> lengths{ 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
        return lengths[static_cast<std::size_t>(std::clamp(month - mMonthBase, 0, monthsInYear - 1))];
    }

    GameDate Calendar::after(GameDate start, long long days) const
    {
        GameDate date = start;
        date.mMonth = std::clamp(date.mMonth, mMonthBase, mMonthBase + monthsInYear - 1);
        date.mDay = std::clamp(date.mDay, 1, daysInMonth(date.mMonth));
        if (days <= 0)
            return date;

        // Whole years first (a year is 365 days), so that a long time costs little
        constexpr long long yearLength = 365;
        date.mYear += static_cast<int>(days / yearLength);
        days %= yearLength;

        while (days > 0)
        {
            const long long left = daysInMonth(date.mMonth) - date.mDay;
            if (days <= left)
            {
                date.mDay += static_cast<int>(days);
                break;
            }
            days -= left + 1;
            date.mDay = 1;
            if (date.mMonth == mMonthBase + monthsInYear - 1)
            {
                date.mMonth = mMonthBase;
                ++date.mYear;
            }
            else
                ++date.mMonth;
        }
        return date;
    }
}
