#pragma once

#include "common/ITypes.h"
#include "api/OpenVRTypes.h"

// distance between two vector 3 squared
inline float DistBetweenVecSqr(const Vector3& a, const Vector3& b)
{
	Vector3 diffVec = a - b;
	return (diffVec.x*diffVec.x + diffVec.y*diffVec.y + diffVec.z*diffVec.z);
}


inline float DistBetweenVec(const Vector3& a, const Vector3& b)
{
	return sqrtf(DistBetweenVecSqr(a, b));
}


inline Vector3 GetPositionFromVRPose(PapyrusVR::TrackedDevicePose* pose) 
{
	Vector3 vector;

	vector.x = pose->mDeviceToAbsoluteTracking.m[0][3];
	vector.y = pose->mDeviceToAbsoluteTracking.m[1][3];
	vector.z = pose->mDeviceToAbsoluteTracking.m[2][3];

	return vector;
}