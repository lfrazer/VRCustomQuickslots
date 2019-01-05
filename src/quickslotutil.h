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

#pragma once

#include "api/PapyrusVRTypes.h"
#include "api/OpenVRTypes.h"
#include "common/ISingleton.h"
#include "timer.h"

// Log config & macros
enum eLogLevels
{
	QSLOGLEVEL_ERR = 0,
	QSLOGLEVEL_WARN,
	QSLOGLEVEL_INFO,
};

#define QSLOG(fmt, ...) CUtil::GetSingleton().Log(QSLOGLEVEL_WARN, fmt, ##__VA_ARGS__)
#define QSLOG_ERR(fmt, ...) CUtil::GetSingleton().Log(QSLOGLEVEL_ERR, fmt, ##__VA_ARGS__)
#define QSLOG_INFO(fmt, ...) CUtil::GetSingleton().Log(QSLOGLEVEL_INFO, fmt, ##__VA_ARGS__)

// Util class

class CUtil : public ISingleton<CUtil>
{
public:
	double	GetLastTime() { return mTimer.GetLastTime();  }
	void	Update() { mTimer.TimerUpdate(); }
	void	SetLogLevel(int level) { mLogLevel = level; }

	void Log(const int msgLogLevel, const char * fmt, ...) // write to message log containing time 
	{
		if (msgLogLevel > mLogLevel)
		{
			return;
		}

		va_list args;
		char logBuffer[4096];

		sprintf_s(logBuffer, "[%.2f] ", mTimer.GetLastTime());
		const size_t logWriteOffset = strlen(logBuffer);

		va_start(args, fmt);
		vsprintf_s(logBuffer + logWriteOffset, sizeof(logBuffer) - logWriteOffset, fmt, args);
		va_end(args);

		_MESSAGE(logBuffer);
	}


private:
	CTimer	mTimer;
	int		mLogLevel = 0;
};

// General inline funcs

inline bool streq(const char* str1, const char* str2)
{
	return strcmp(str1, str2) == 0;
}


// Math inline functions

// distance between two vector 3 squared
inline float DistBetweenVecSqr(const PapyrusVR::Vector3& a, const PapyrusVR::Vector3& b)
{
	const PapyrusVR::Vector3 diffVec = a - b;
	return (diffVec.x*diffVec.x + diffVec.y*diffVec.y + diffVec.z*diffVec.z);
}


inline float DistBetweenVec(const PapyrusVR::Vector3& a, const PapyrusVR::Vector3& b)
{
	return sqrtf(DistBetweenVecSqr(a, b));
}


inline PapyrusVR::Vector3 GetPositionFromVRPose(PapyrusVR::TrackedDevicePose* pose)
{
	PapyrusVR::Vector3 vector;

	vector.x = pose->mDeviceToAbsoluteTracking.m[0][3];
	vector.y = pose->mDeviceToAbsoluteTracking.m[1][3];
	vector.z = pose->mDeviceToAbsoluteTracking.m[2][3];

	return vector;
}

// create a rotation matrix only around Y axis (remove vertical rotation)
inline PapyrusVR::Matrix33 CreateRotMatrixAroundY(const PapyrusVR::Matrix34& steamvr_mat)
{
	PapyrusVR::Matrix33 mat;

	mat.m[0][0] = steamvr_mat.m[0][0];
	mat.m[1][0] = 0.0f;
	mat.m[2][0] = steamvr_mat.m[2][0];

	mat.m[0][1] = 0.0f;
	mat.m[1][1] = 1.0f;
	mat.m[2][1] = 0.0f;

	mat.m[0][2] = steamvr_mat.m[0][2];
	mat.m[1][2] = 0.0f;
	mat.m[2][2] = steamvr_mat.m[2][2];;

	return mat;
}


inline PapyrusVR::Vector3 MultMatrix33(const PapyrusVR::Matrix33& lhs, const PapyrusVR::Vector3& rhs)
{
	PapyrusVR::Vector3 transformedVector;

	float ax = lhs.m[0][0] * rhs.x;
	float by = lhs.m[0][1] * rhs.y;
	float cz = lhs.m[0][2] * rhs.z;

	float ex = lhs.m[1][0] * rhs.x;
	float fy = lhs.m[1][1] * rhs.y;
	float gz = lhs.m[1][2] * rhs.z;


	float ix = lhs.m[2][0] * rhs.x;
	float jy = lhs.m[2][1] * rhs.y;
	float kz = lhs.m[2][2] * rhs.z;

	transformedVector.x = ax + by + cz;
	transformedVector.y = ex + fy + gz;
	transformedVector.z = ix + jy + kz;

	return transformedVector;
}