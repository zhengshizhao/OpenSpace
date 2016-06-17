/*****************************************************************************************
 *                                                                                       *
 * OpenSpace                                                                             *
 *                                                                                       *
 * Copyright (c) 2014-2016                                                               *
 *                                                                                       *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this  *
 * software and associated documentation files (the "Software"), to deal in the Software *
 * without restriction, including without limitation the rights to use, copy, modify,    *
 * merge, publish, distribute, sublicense, and/or sell copies of the Software, and to    *
 * permit persons to whom the Software is furnished to do so, subject to the following   *
 * conditions:                                                                           *
 *                                                                                       *
 * The above copyright notice and this permission notice shall be included in all copies *
 * or substantial portions of the Software.                                              *
 *                                                                                       *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,   *
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A         *
 * PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT    *
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF  *
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE  *
 * OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                                         *
 ****************************************************************************************/

#include <modules/base/ephemeris/tleephemeris.h>

#include <ghoul/logging/logmanager.h>

#include <fstream>
#include <vector>

namespace {
    const std::string KeyFile = "File";
}

namespace openspace {

TLEEphemeris::TLEEphemeris(const ghoul::Dictionary& dictionary)
    : KeplerEphemeris()
{
    std::string file;
    bool hasFile = dictionary.hasKeyAndValue<std::string>(KeyFile);
    
    ghoul_assert(hasFile, "TLEEphemeris must have a file");

    file = dictionary.value<std::string>(KeyFile);
    readTLEFile(file);
}

void TLEEphemeris::readTLEFile(const std::string& file) {
    std::ifstream f(file);

    if (!f.is_open()) {
        LERRORC("", "Could not open file '" << file << "'");
        return;
    }

    std::string line;
    while (std::getline(f, line)) {
        if (line[0] == '1') {
            // First line
            // Field	Columns	Content	Example
            //    1	01-01	Line number	1
            //    2	03-07	Satellite number	25544
            //    3	08-08	Classification(U = Unclassified)	U
            //    4	10-11	International Designator(Last two digits of launch year)	98
            //    5	12-14	International Designator(Launch number of the year)	067
            //    6	15-17	International Designator(piece of the launch)	A
            //    7	19-20	Epoch Year(last two digits of year)	08
            //    8	21-32	Epoch(day of the year and fractional portion of the day)	264.51782528
            //    9	34-43	First Time Derivative of the Mean Motion divided by two[10]	-.00002182
            //    10    45-52	Second Time Derivative of Mean Motion divided by six(decimal point assumed)	00000 - 0
            //    11	54-61	BSTAR drag term(decimal point assumed)[10] - 11606 - 4
            //    12	63-63	The number 0 (originally this should have been "Ephemeris type")	0
            //    13	65-68	Element set number.Incremented when a new TLE is generated for this object.[10]	292
            //14    69–69	Checksum(modulo 10)	7
        
            std::string epoch = line.substr(18, 14);

            // According to https://celestrak.com/columns/v04n03/
            // Apparently, US Space Command sees no need to change the two-line element
            // set format yet since no artificial earth satellites existed prior to 1957.
            // By their reasoning, two-digit years from 57-99 correspond to 1957-1999 and
            // those from 00-56 correspond to 2000-2056.

            int y = std::atoi(epoch.substr(0, 2).c_str());
            std::string yearPrefix;
            if (y >= 57)
                yearPrefix = "19";
            else
                yearPrefix = "20";

            int year = std::atoi((yearPrefix + epoch.substr(0, 2)).c_str());
            float day = std::atof(epoch.substr(3).c_str());

            const double J2000 = 2451545.0;
            int DaysPerYear = 365;

            auto countDays = [](int year) -> int {
                // Find the position of the current year in the vector, the difference
                // between its position and the position of 2000 (for J2000) gives the
                // number of leap years
                const std::vector<int> LeapYears = {
                    1956, 1960, 1964, 1968, 1972, 1976, 1980, 1984, 1988, 1992, 1996,
                    2000, 2004, 2008, 2012, 2016, 2020, 2024, 2028, 2032, 2036, 2040,
                    2044, 2048, 2052, 2056
                };

                if (year == 2000)
                    return 0;

                auto lb = std::lower_bound(LeapYears.begin(), LeapYears.end(), year);
                auto y2000 = std::find(LeapYears.begin(), LeapYears.end(), 2000);

                int nLeapYears = std::abs(std::distance(y2000, lb));

                int nYears = std::abs(year - 2000);
                int nNonLeapYears = nYears - nLeapYears;

                int result = nNonLeapYears * 365 + nLeapYears * 366;
                return result;
            };

            auto countLeapSeconds = [](int year, int dayOfYear) -> int {
                // Find the position of the current year in the vector; its position in
                // the vector gives the number of leap seconds
                struct LeapSecond {
                    int year;
                    int dayOfYear;
                    bool operator<(const LeapSecond& rhs) const {
                        return
                            std::tie(year, dayOfYear) < std::tie(rhs.year, rhs.dayOfYear);
                    }
                };

                // List taken from: https://www.ietf.org/timezones/data/leap-seconds.list
                std::vector<LeapSecond> LeapSeconds = {
                    { 1972, 1 },
                    { 1972, 183 },
                    { 1973, 1 },
                    { 1974, 1 },
                    { 1975, 1 },
                    { 1976, 1 },
                    { 1977, 1 },
                    { 1978, 1 },
                    { 1979, 1 },
                    { 1980, 1 },
                    { 1981, 182 },
                    { 1982, 182 },
                    { 1983, 182 },
                    { 1985, 182 },
                    { 1988, 1 },
                    { 1990, 1 },
                    { 1991, 1 },
                    { 1992, 183 },
                    { 1993, 182 },
                    { 1994, 182 },
                    { 1996, 1 },
                    { 1997, 182 },
                    { 1999, 1 },
                    { 2006, 1 },
                    { 2009, 1 },
                    { 2012, 183 },
                    { 2015, 182 }
                };
                
                
                auto it = std::lower_bound(
                    LeapSeconds.begin(),
                    LeapSeconds.end(),
                    LeapSecond{ year, dayOfYear }
                );

                int nLeapSeconds = std::distance(LeapSeconds.begin(), it);
                return nLeapSeconds;

            };

            _epoch =
                J2000 + // J2000 (days and fractions since January 1, 4713 BCE)
                countDays(year) + // counting days between 2000 and 'year' including leap
                day + // day of the year
                countLeapSeconds(year, static_cast<int>(std::floor(day))); // leap secs
        }

        if (line[0] == '2') {
            // Second line
            //Field	Columns	Content	Example
            //    1	01-01	Line number	2
            //    2	03-07	Satellite number	25544
            //    3	09-16	Inclination(degrees)	51.6416
            //    4	18-25	Right ascension of the ascending node(degrees)	247.4627
            //    5	27-33	Eccentricity(decimal point assumed)	0006703
            //    6	35-42	Argument of perigee(degrees)	130.5360
            //    7	44-51	Mean Anomaly(degrees)	325.0288
            //    8	53-63	Mean Motion(revolutions per day)	15.72125391
            //    9	64-68	Revolution number at epoch(revolutions)	56353
            //    10	69–69	Checksum(modulo 10)	7

            std::string inclination = line.substr(8, 8);
            std::string ascendingNode = line.substr(17, 8);
            std::string eccentricity = "0." + line.substr(26, 7);
            std::string argumentOfPeriapsis = line.substr(34, 8);
            std::string meanAnomaly = line.substr(43, 8);
            LINFOC("", "inclination: " << inclination);
            LINFOC("", "ascendingNode: " << ascendingNode);
            LINFOC("", "eccentricity: " << eccentricity);
            LINFOC("", "argumentOfPeriapsis: " << argumentOfPeriapsis);
            LINFOC("", "meanAnomaly: " << meanAnomaly);
            break;
        }
    }

    ghoul_assert(false, "Shouldn't get here");
}

} // namespace openspace
