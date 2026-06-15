#pragma once

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>

namespace world::time
{
    class UniverseClock
    {
    public:
        static constexpr int UniverseEpochYear  = 3098;
        static constexpr int UniverseEpochMonth = 12;
        static constexpr int UniverseEpochDay   = 21;

        static constexpr int RealEpochYear  = 1998;
        static constexpr int RealEpochMonth = 12;
        static constexpr int RealEpochDay   = 21;

    public:
        void reset()
        {
            m_timeScale = 1.0;
            m_currentTimeSeconds = realSecondsSinceEpoch();
        }

        void update(double realDtSeconds)
        {
            const double safeDt =
                std::max(0.0, realDtSeconds);

            m_currentTimeSeconds += safeDt * m_timeScale;
        }

        double timeSeconds() const
        {
            return m_currentTimeSeconds;
        }

        double timeScale() const
        {
            return m_timeScale;
        }

        void setTimeScale(double scale)
        {
            m_timeScale = std::max(0.0, scale);
        }

        std::string dateTimeString() const
        {
            return formatUniverseDate(timeSeconds());
        }

    private:
        static int64_t daysFromCivil(int y, unsigned m, unsigned d)
        {
            y -= m <= 2;
            const int era = (y >= 0 ? y : y - 399) / 400;
            const unsigned yoe = static_cast<unsigned>(y - era * 400);
            const unsigned doy =
                (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
            const unsigned doe =
                yoe * 365 + yoe / 4 - yoe / 100 + doy;

            return era * 146097 + static_cast<int>(doe) - 719468;
        }

        static int64_t unixSecondsUtc(int year, int month, int day)
        {
            return daysFromCivil(
                year,
                static_cast<unsigned>(month),
                static_cast<unsigned>(day)
            ) * 86400;
        }

        static double realSecondsSinceEpoch()
        {
            using namespace std::chrono;

            const auto now =
                system_clock::now().time_since_epoch();

            const int64_t nowSeconds =
                duration_cast<seconds>(now).count();

            const int64_t realEpochSeconds =
                unixSecondsUtc(
                    RealEpochYear,
                    RealEpochMonth,
                    RealEpochDay
                );

            return static_cast<double>(nowSeconds - realEpochSeconds);
        }

        static bool isLeapYear(int year)
        {
            if (year % 400 == 0)
                return true;

            if (year % 100 == 0)
                return false;

            return year % 4 == 0;
        }

        static int daysInMonth(int year, int month)
        {
            switch (month)
            {
                case 1:  return 31;
                case 2:  return isLeapYear(year) ? 29 : 28;
                case 3:  return 31;
                case 4:  return 30;
                case 5:  return 31;
                case 6:  return 30;
                case 7:  return 31;
                case 8:  return 31;
                case 9:  return 30;
                case 10: return 31;
                case 11: return 30;
                case 12: return 31;
                default: return 30;
            }
        }

        static std::string formatUniverseDate(double secondsSinceUniverseEpoch)
        {
            constexpr int64_t SecondsPerDay = 86400;

            int64_t totalSeconds =
                static_cast<int64_t>(secondsSinceUniverseEpoch);

            int64_t days =
                totalSeconds / SecondsPerDay;

            int64_t secondsInDay =
                totalSeconds % SecondsPerDay;

            int year = UniverseEpochYear;
            int month = UniverseEpochMonth;
            int day = UniverseEpochDay;

            while (days > 0)
            {
                const int dim = daysInMonth(year, month);
                const int remainingInMonth = dim - day;

                if (days <= remainingInMonth)
                {
                    day += static_cast<int>(days);
                    days = 0;
                }
                else
                {
                    days -= remainingInMonth + 1;
                    day = 1;
                    month++;

                    if (month > 12)
                    {
                        month = 1;
                        year++;
                    }
                }
            }

            const int hour =
                static_cast<int>(secondsInDay / 3600);

            secondsInDay %= 3600;

            const int minute =
                static_cast<int>(secondsInDay / 60);

            const int second =
                static_cast<int>(secondsInDay % 60);

            std::ostringstream ss;

            ss
                << std::setfill('0')
                << std::setw(4) << year << "-"
                << std::setw(2) << month << "-"
                << std::setw(2) << day << " "
                << std::setw(2) << hour << ":"
                << std::setw(2) << minute << ":"
                << std::setw(2) << second
                << " UTC";

            return ss.str();
        }

    private:
        double m_timeScale = 1.0;
        double m_currentTimeSeconds = 0.0;
    };
}