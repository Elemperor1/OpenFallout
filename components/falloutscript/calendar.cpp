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

    GameDate Calendar::normalized(GameDate date) const
    {
        date.mMonth = std::clamp(date.mMonth, mMonthBase, mMonthBase + monthsInYear - 1);
        date.mDay = std::clamp(date.mDay, 1, daysInMonth(date.mMonth));
        return date;
    }

    long long Calendar::dayNumber(GameDate date) const
    {
        date = normalized(date);
        constexpr long long yearLength = 365;
        long long days = static_cast<long long>(date.mYear) * yearLength;
        for (int month = mMonthBase; month < date.mMonth; ++month)
            days += daysInMonth(month);
        return days + date.mDay - 1;
    }

    GameDate Calendar::dateOf(long long dayNumber) const
    {
        constexpr long long yearLength = 365;
        dayNumber = std::max(dayNumber, 0ll);
        GameDate date;
        date.mYear = static_cast<int>(dayNumber / yearLength);
        dayNumber %= yearLength;
        date.mMonth = mMonthBase;
        while (dayNumber >= daysInMonth(date.mMonth))
        {
            dayNumber -= daysInMonth(date.mMonth);
            ++date.mMonth;
        }
        date.mDay = static_cast<int>(dayNumber) + 1;
        return date;
    }

    GameDate Calendar::after(GameDate start, long long days) const
    {
        if (days <= 0)
            return normalized(start);
        return dateOf(dayNumber(start) + days);
    }
}
