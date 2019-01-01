/*
	VRCustomQuickslots - VR Custom Quickslots SKSE extension for SkyrimVR
	Copyright (C) 2018 L Frazer
	https://github.com/lfrazer

	This file is part of VRCustomQuickslots.

	VRCustomQuickslots is free software: you can redistribute it and/or modify
	it under the terms of the GNU Lesser General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	VRCustomQuickslots is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU Lesser General Public License for more details.

	You should have received a copy of the GNU Lesser General Public License
	along with VRCustomQuickslots.  If not, see <https://www.gnu.org/licenses/>.
*/

#include <time.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include "timer.h"
#include <iostream>
#include <iomanip>
#include <sstream>

#ifndef _DEBUG
#undef ASSERT
#define ASSERT(x) 
#endif

#ifndef WIN32
#ifndef LINUX_OS
#ifndef ANDROID_OS
#define IPHONE_OS 1
#endif
#endif
#endif

#ifndef WIN32
#include <sys/time.h>
#endif

#ifdef IPHONE_OS

#include <mach/mach.h>
#include <mach/mach_time.h>

#endif


#define WINDOWS_TICK 10000000
#define SEC_TO_UNIX_EPOCH 11644473600LL

unsigned WindowsTickToUnixSeconds(long long windowsTicks)
{
     return (unsigned)(windowsTicks / WINDOWS_TICK - SEC_TO_UNIX_EPOCH);
}



CTimer::CTimer(void)	// constructor
{

	Init();
}


void CTimer::Init(void)	// init func
{
	// initial values..
	if(mInitComplete != 1)
	{
		mStartTime = 0.0;
		mTimeSlice = 0.0;

		mPauseTime = -1.0;	
		mPauseTotalSubtraction = 0.0;

		#if _POSIX_TIMERS > 0
		#ifdef ANDROID_OS
		LOGI("Using POSIX_TIMERS on Android.");	
		#endif
		#endif


		#ifdef WIN32
		if( QueryPerformanceFrequency((LARGE_INTEGER *)&mTicksPerSecond) ) // on Windows, let the API figure out resolution of timer.
		{
		}

		else 
		{
			mTicksPerSecond = 1000;
		}
		#else
			mTicksPerSecond = 1000000000; // 1 billion ticks since we divide by nanoseconds
		#endif
		
			
		mStartTime = GetTime();
		mLastTime = mStartTime;


		mInitComplete = 1;
	}
}


double CTimer::GetTime(void) // time since start of program
{

	double timeInSeconds = 0.0;
	

#if defined(WIN32)
	  // This is the number of clock ticks since start

	UINT64 ticks;

	if( !QueryPerformanceCounter((LARGE_INTEGER *)&ticks) )
		ticks = (UINT64)timeGetTime();



	// Divide by frequency to get the time in seconds
	timeInSeconds = (double)ticks/(double)mTicksPerSecond;

	mLastRawTime = ticks;


#elif defined(IPHONE_OS)

	static mach_timebase_info_data_t sTimebaseInfo;
	static int tbaseInfoInit = 0;
	static const double convertToSecondsMult = 1.0 / 1000000000.0;
	uint64_t time = mach_absolute_time();
	uint64_t nanos;

	// If this is the first time we've run, get the timebase.
	// We can use denom == 0 to indicate that sTimebaseInfo is
	// uninitialised because it makes no sense to have a zero
	// denominator is a fraction.
	if ( tbaseInfoInit == 0 ) {
		(void) mach_timebase_info(&sTimebaseInfo);
		tbaseInfoInit = 1;
	}

	// Do the maths.  We hope that the multiplication doesn't
	// overflow; the price you pay for working in fixed point.
	nanos = time * sTimebaseInfo.numer / sTimebaseInfo.denom;
	timeInSeconds = ((double)nanos * convertToSecondsMult);

	mLastRawTime = nanos;

#else

//	clock_gettime(CLOCK_REALTIME, &ts);
	struct timespec ts;

	#if _POSIX_TIMERS > 0


	#ifdef ANDROID_OS
	clock_gettime(CLOCK_REALTIME, &ts);
	
	#else
	
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts);	
	
	#endif

	#else

	struct timeval tv;
	gettimeofday(&tv, NULL);
	ts.tv_sec = tv.tv_sec;
	ts.tv_nsec = tv.tv_usec*1000;

	#endif

	timeInSeconds = (double)ts.tv_sec + ( (double)ts.tv_nsec / (double)mTicksPerSecond );
	mLastRawTime = (uint64_t)ts.tv_sec * 1000000000 + (uint64_t)ts.tv_nsec;


#endif

	ASSERT(timeInSeconds > 0.0);

	return timeInSeconds; 

}

double CTimer::GetTimeSlice(void) // time since last timer update
{
	return mTimeSlice;
}

void CTimer::TimerUpdate(void) // update the timer once per loop to use time slice
{
	const double time = GetTime();
	
	mTimeSlice = time - mLastTime;

	/*
	if(mTimeSlice < 0.0)
	{
		printf("Error: timeslice was negative!!\n");
		mTimeSlice = 0.0;
	}
	*/

	ASSERT(mTimeSlice > 0.0);

	mLastTime = time;
}



double CTimer::GetLastTime(void)  // last time calculated in TimerUpdate (this is the game time with pause applied!)
{
	if(mPauseTime > 0.0)  // the timer is now paused
	{
		return mPauseTime; 
	}

	else  // the timer is unpaused, give the real time
	{
		ASSERT(mLastTime - mPauseTotalSubtraction > 0.0);
		return mLastTime - mPauseTotalSubtraction;
	}
}

// get the time since the program started, ignoring pause subtraction time
double CTimer::GetAbsoluteTimeSinceStart(void)
{
	return mLastTime - mStartTime;
}

void CTimer::Pause(void)
{
	mPauseTime = mLastTime;
}

void CTimer::Unpause(void)
{
	// dont do anything unless we have paused already!
	if(mPauseTime > 0.0)
	{
		//trace("Unpausing timer, mPauseTotalSubtraction=%f, mLastTime=%f mPauseTime=%f timeDiff=%f\n", mPauseTotalSubtraction, mLastTime, mPauseTime,  (mLastTime - mPauseTime));

		mPauseTotalSubtraction += (mLastTime - mPauseTime); 
		mPauseTime = -1.0;
	
		//trace("Updated value for mPauseTotalSubtraction=%f\n", mPauseTotalSubtraction);
	}
}


// TODO: verify this actually works at all on all systems
// Should return UNIX timestamp from UTC timezone
time_t CTimer::GetUnixTimestamp()
{
	time_t currtime = time(NULL);
	std::tm* utc_time = nullptr;
	gmtime_s(utc_time, &currtime);

	// ignore daylight savings time (important when we just want to compare UTC time!!)
	utc_time->tm_isdst = 0;

	return mktime(utc_time);

}

// this is not actually needed.. we just wantt oc ompare UTC time (but nice to have just incase)
static int get_utc_offset() {

  time_t zero = 24*60*60L;
  struct tm * timeptr;
  int gmtime_hours;

  /* get the local time for Jan 2, 1900 00:00 UTC */
  localtime_s(timeptr, &zero );
  gmtime_hours = timeptr->tm_hour;

  /* if the local time is the "day before" the UTC, subtract 24 hours
    from the hours to get the UTC offset */
  if( timeptr->tm_mday < 2 )
    gmtime_hours -= 24;

  return gmtime_hours;

}


// NOTE: GCC < 5.0 seems cannot support std::get_time or put_time, so lets try an alternative way
// format looks like this: 2015-02-14T21:55:54+00:00
time_t CTimer::ConvertWebTimeToTimestamp(const char* webTime)
{
	std::tm timeOut;

	//std::istringstream ss(webTime);
	//ss >> std::get_time( &timeOut, "%Y-%m-%dT%H:%M:%S+00:00" );

	//"%Y-%m-%dT%H:%M:%S+00:00"
	
	sscanf_s(webTime, "%d-%d-%dT%d:%d:%d+00:00", &timeOut.tm_year, &timeOut.tm_mon, &timeOut.tm_mday, &timeOut.tm_hour, &timeOut.tm_min, &timeOut.tm_sec);

	// convert time to correct starting point
	timeOut.tm_year -= 1900;
	timeOut.tm_mon -= 1;

	// ignore daylight savings time (important when we just want to compare UTC time!!)
	timeOut.tm_isdst = 0;

	//timeOut.tm_hour -= 1;
	//timeOut.tm_min -= 1;
	//timeOut.tm_sec -= 1;
	


	//trace("Web Time Convert Debug.\nDate Y=%d M=%d D=%d hour:%d min:%d second:%d\n", timeOut.tm_year, timeOut.tm_mon, timeOut.tm_mday, timeOut.tm_hour, timeOut.tm_min, timeOut.tm_sec);
	//trace("Put time: %s\n",  std::put_time(&timeOut, "%c") );
	//std::cout << "Put time " << std::put_time(&timeOut, "%c") << "\n";

	// NOTE: we are supposed to return UTC time, not local time!!
	time_t t = mktime(&timeOut);

	return t;
}


