/*******************************************************************************
 *
 * Copyright (c) 2014 SenseGrow, Inc.
 *
 * SenseGrow Internet of Things (IoT) Client Frameworks
 * http://www.sensegrow.com
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the
 * Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.

 * Contributors:
 *    Ajay Garg <ajay.garg@sensegrow.com>
 *******************************************************************************/



#include "./include/globals.h"
#include "./include/time.h"
#include "./include/log.h"

/* 2000-03-01 (mod 400 year, immediately after feb29 */
#define LEAPOCH (946684800 + (86400 * (31 + 29)) )

#define DAYS_PER_400Y ((365 * 400) + 97)
#define DAYS_PER_100Y ((365 * 100) + 24)
#define DAYS_PER_4Y   ((365 * 4  ) + 1)

void print_date_info(DateParams *tm, const char *mode)
{
    sg_sprintf(LOG_GLOBAL_BUFFER, PROSTR("%sFrom [%s], Extracted Date in GMT [YY-MM-DD, hh:mm:ss] = [%u-%u-%u, %u:%u:%u]"), CLOCK, mode,
                                  tm->tm_year, tm->tm_mon, tm->tm_mday,
                                  tm->tm_hour, tm->tm_min, tm->tm_sec);
    info_log(LOG_GLOBAL_BUFFER);
}


void extract_date_params(unsigned long t, DateParams *tm, const char *mode)
{
	unsigned long days, secs;
	int remdays, remsecs, remyears;
	int qc_cycles, c_cycles, q_cycles;
	int years, months;
	int wday, yday, leap;
	static const char days_in_month[] = {31,30,31,30,31,31,30,31,30,31,31,29};

#if 0
	/* Reject time_t values whose year would overflow int */
	if (t < INT_MIN * 31622400LL || t > INT_MAX * 31622400LL)
    {
        return FAILURE;
    }
#endif

	secs = t - LEAPOCH;
	days = secs / 86400;
	remsecs = secs % 86400;
	if(remsecs < 0)
    {
		remsecs += 86400;
		days--;
	}

	wday = (3 + days) % 7;
	if(wday < 0)
    {
        wday += 7;
    }

	qc_cycles = days / DAYS_PER_400Y;
	remdays = days % DAYS_PER_400Y;
	if(remdays < 0)
    {
		remdays += DAYS_PER_400Y;
		qc_cycles--;
	}

	c_cycles = remdays / DAYS_PER_100Y;
	if(c_cycles == 4)
    {
        c_cycles--;
    }
	remdays -= c_cycles * DAYS_PER_100Y;

	q_cycles = remdays / DAYS_PER_4Y;
	if(q_cycles == 25)
    {
        q_cycles--;
    }
	remdays -= q_cycles * DAYS_PER_4Y;

	remyears = remdays / 365;
	if(remyears == 4)
    {
        remyears--;
    }
	remdays -= remyears * 365;

	leap = !remyears && (q_cycles || !c_cycles);
	yday = remdays + 31 + 28 + leap;
	if(yday >= 365+leap)
    {
        yday -= 365 + leap;
    }

	years = remyears + (4 * q_cycles) + (100 * c_cycles) + (400 * qc_cycles);

	for(months = 0; days_in_month[months] <= remdays; months++)
    {
		remdays -= days_in_month[months];
    }

#if 0
	if (years + 100 > INT_MAX || years + 100 < INT_MIN)
    {
		return FAILURE;
    }
#endif

	tm->tm_year = years;
	tm->tm_mon = months + 3;
	if(tm->tm_mon > 12)
    {
		tm->tm_mon -=12;
		tm->tm_year++;
	}
	tm->tm_mday = remdays + 1;
	tm->tm_wday = wday;
	tm->tm_yday = yday;

	tm->tm_hour = remsecs / 3600;
	tm->tm_min = remsecs / 60 % 60;
	tm->tm_sec = remsecs % 60;

    print_date_info(tm, mode);
}
