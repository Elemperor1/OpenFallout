#ifndef OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_CALENDAR_H
#define OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_CALENDAR_H

namespace FalloutScript
{
    /// A day of the calendar of the games, as the globals GameYear, GameMonth and GameDay hold it
    struct GameDate
    {
        int mYear = 0;
        int mMonth = 0; // counted from monthBase of the calendar
        int mDay = 1; // from 1
    };

    /// The calendar of the games: twelve months of the usual lengths and no leap year (the calendar of the game world
    /// that this engine comes from has the same). The first month is January, and the number the games give it is the
    /// base: 1 when they count from 1.
    class Calendar
    {
    public:
        explicit Calendar(int monthBase = 1)
            : mMonthBase(monthBase)
        {
        }

        static constexpr int monthsInYear = 12;

        /// The number of days of a month (the month in the numbers of the calendar)
        int daysInMonth(int month) const;

        /// The date that is the given number of whole days after the start. A start date that is no date (a month or a
        /// day out of range) is taken to the nearest date there is. Nothing happens for a negative number of days.
        GameDate after(GameDate start, long long days) const;

    private:
        int mMonthBase;
    };
}

#endif
