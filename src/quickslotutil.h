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

//Headers under api/ folder
#include "api/PapyrusVRTypes.h"
#include "api/OpenVRTypes.h"
#include "api/PapyrusVRAPI.h"
#include "api/VRManagerAPI.h"
#include "common/ISingleton.h"
#include "timer.h"

#include <sstream>
#include <random>
#include <locale>
#include <cctype>
#include <algorithm>

#include "skse64/GameObjects.h"
#include "skse64/GameData.h"
#include "skse64/GameRTTI.h"
#include "skse64/GameExtraData.h"

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

inline void ltrim(std::string &s)
{
	s.erase(s.begin(), std::find_if(s.begin(), s.end(),
		std::not1(std::ptr_fun<int, int>(std::isspace))));
}

inline void rtrim(std::string &s)
{
	s.erase(std::find_if(s.rbegin(), s.rend(),
		std::not1(std::ptr_fun<int, int>(std::isspace))).base(), s.end());
}

// trim from both ends (in place)
inline void trim(std::string &s)
{
	ltrim(s);
	rtrim(s);
}

inline bool streq(const char* str1, const char* str2)
{
	return strcmp(str1, str2) == 0;
}

//Gets std::string and returns unsigned int. For converting hex string to formid
inline UInt32 getHex(std::string hexstr)
{
	return std::stoul(hexstr, nullptr, 16);
}


//Splits the supplied string by char delimeter and returns string vector
inline std::vector<std::string> split(const std::string& s, char delimiter)
{
	std::vector<std::string> tokens;
	std::string token;
	std::istringstream tokenStream(s);
	while (std::getline(tokenStream, token, delimiter))
	{
		tokens.emplace_back(token);
	}
	return tokens;
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

//Random number generator function
inline size_t randomGenerator(size_t min, size_t max)
{
	std::mt19937 rng;
	rng.seed(std::random_device()());
	//rng.seed(std::chrono::high_resolution_clock::now().time_since_epoch().count());
	std::uniform_int_distribution<std::mt19937::result_type> dist(min, max);

	return dist(rng);
}

//A modified version of skse VerifyKeywords function to check for keywordsNot array too
inline bool VerifyKeywords(TESForm * form, std::vector<BGSKeyword*> * keywords, std::vector<BGSKeyword*> * keywordsNot)
{
	if (!keywords->empty()) 
	{
		BGSKeywordForm* pKeywords = DYNAMIC_CAST(form, TESForm, BGSKeywordForm);
		if (pKeywords) 
		{
			bool failed = false;
			BGSKeyword * keyword = NULL;
			for (UInt32 k = 0; k < keywords->size(); k++) 
			{
				keyword = keywords->at(k);
				if (keyword && !pKeywords->HasKeyword(keyword))
				{
					failed = true;
					break;
				}
			}

			for (UInt32 k = 0; k < keywordsNot->size(); k++) 
			{
				keyword = keywordsNot->at(k);
				if (keyword && pKeywords->HasKeyword(keyword))
				{
					failed = true;
					break;
				}
			}

			if (failed)
				return false;
		}
	}

	return true;
}

//A modified version of SKSE GetAllPotions function to get vector as input instead of VMArray. Also keywordsNot checks.
inline std::vector<UInt32> GetAllPotions(bool allplugins, UInt8 modIndex, std::vector<BGSKeyword*> keywords, std::vector<BGSKeyword*> keywordsNot, bool potions, bool food, bool poison)
{
	std::vector<UInt32> result;

	DataHandler * dataHandler = DataHandler::GetSingleton();
	if (modIndex != 255)
	{
		AlchemyItem * potion = NULL;
		for (UInt32 i = 0; i < dataHandler->potions.count; i++)
		{
			dataHandler->potions.GetNthItem(i, potion);

			if (!allplugins)
			{
				if ((potion->formID >> 24) != modIndex)
					continue;
			}
			
			if (!VerifyKeywords(potion, &keywords, &keywordsNot))
				continue;

			bool isFood = potion->IsFood();
			bool isPoison = potion->IsPoison();

			bool accept = false;
			if (potions && !isFood && !isPoison)
				accept = true;
			if (food && isFood)
				accept = true;
			if (poison && isPoison)
				accept = true;
			if (!accept)
				continue;

			result.emplace_back(potion->formID);
		}
	}

	return result;
}

//A modified version of SKSE GetAllAmmo function to get vector as input instead of VMArray. Also keywordsNot checks.
inline std::vector<UInt32> GetAllAmmo(bool allplugins, UInt8 modIndex, std::vector<BGSKeyword*> keywords, std::vector<BGSKeyword*> keywordsNot)
{
	std::vector<UInt32> result;

	DataHandler * dataHandler = DataHandler::GetSingleton();
	if (modIndex != 255)
	{
		TESAmmo * ammo = NULL;
		for (UInt32 i = 0; i < dataHandler->ammo.count; i++)
		{
			dataHandler->ammo.GetNthItem(i, ammo);

			if (!allplugins)
			{
				if ((ammo->formID >> 24) != modIndex)
					continue;
			}
			if (!(ammo->IsPlayable()))
				continue;
			
			if (!VerifyKeywords(ammo, &keywords, &keywordsNot))
				continue;

			result.emplace_back(ammo->formID);
		}
	}

	return result;
}

inline bool CanEquipBothHands(Actor* actor, TESForm * item)
{
	BGSEquipType * equipType = DYNAMIC_CAST(item, TESForm, BGSEquipType);
	if (!equipType)
		return false;

	BGSEquipSlot * equipSlot = equipType->GetEquipSlot();
	if (!equipSlot)
		return false;

	// 2H
	if (equipSlot == GetEitherHandSlot())
	{
		return true;
	}
	// 1H
	else if (equipSlot == GetLeftHandSlot() || equipSlot == GetRightHandSlot())
	{
		return (actor->race->data.raceFlags & TESRace::kRace_CanDualWield) && item->IsWeapon();
	}

	return false;
}

inline BGSEquipSlot * GetEquipSlotById(SInt32 slotId)
{
	enum
	{
		kSlotId_Default = 0,
		kSlotId_Right = 1,
		kSlotId_Left = 2
	};

	if (slotId == kSlotId_Right)
		return GetRightHandSlot();
	else if (slotId == kSlotId_Left)
		return GetLeftHandSlot();
	else
		return NULL;
}

//Checks if supplied formId is currently equipped by the player. Checks item, spell, and shout equips.
inline bool FormCurrentlyEquipped(UInt32 formId)
{
	Actor * player = (Actor*)(*g_thePlayer);
	TESForm * rightEquipped = player->GetEquippedObject(false);
	TESForm * leftEquipped = player->GetEquippedObject(true);

	if (rightEquipped && rightEquipped->formID == formId)
	{
		return true;
	}
	else if (leftEquipped && leftEquipped->formID == formId)
	{
		return true;
	}
	else if(player->leftHandSpell && player->leftHandSpell->formID == formId)
	{
		return true;
	}
	else if (player->rightHandSpell && player->rightHandSpell->formID == formId)
	{
		return true;
	}
	else if (player->equippedShout && player->equippedShout->formID == formId)
	{
		return true;
	}
	else
	{
		return false;
	}
}

//A modified version of SKSE EquipItemEx that returns whether or not the equip was successful.
inline bool EquipItemEx(Actor* thisActor, TESForm* item, SInt32 slotId, bool preventUnequip, bool equipSound)
{
	if (!item)
		return false;

	if (!item->Has3D())
		return false;

	EquipManager* equipManager = EquipManager::GetSingleton();
	if (!equipManager)
		return false;

	ExtraContainerChanges* containerChanges = static_cast<ExtraContainerChanges*>(thisActor->extraData.GetByType(kExtraData_ContainerChanges));
	ExtraContainerChanges::Data* containerData = containerChanges ? containerChanges->data : NULL;
	if (!containerData)
		return false;

	// Copy/merge of extraData and container base. Free after use.
	InventoryEntryData* entryData = containerData->CreateEquipEntryData(item);
	if (!entryData)
		return false;

	BGSEquipSlot * targetEquipSlot = GetEquipSlotById(slotId);

	SInt32 itemCount = entryData->countDelta;

	// For ammo, use count, otherwise always equip 1
	SInt32 equipCount = item->IsAmmo() ? itemCount : 1;

	bool isTargetSlotInUse = false;

	// Need at least 1 (maybe 2 for dual wield, checked later)
	bool hasItemMinCount = itemCount > 0;

	BaseExtraList * rightEquipList = NULL;
	BaseExtraList * leftEquipList = NULL;

	BaseExtraList * curEquipList = NULL;
	BaseExtraList * enchantList = NULL;

	if (hasItemMinCount)
	{
		entryData->GetExtraWornBaseLists(&rightEquipList, &leftEquipList);

		// Case 1: Already equipped in both hands.
		if (leftEquipList && rightEquipList)
		{
			isTargetSlotInUse = true;
			curEquipList = (targetEquipSlot == GetLeftHandSlot()) ? leftEquipList : rightEquipList;
			enchantList = NULL;
		}
		// Case 2: Already equipped in right hand.
		else if (rightEquipList)
		{
			isTargetSlotInUse = targetEquipSlot == GetRightHandSlot();
			curEquipList = rightEquipList;
			enchantList = NULL;
		}
		// Case 3: Already equipped in left hand.
		else if (leftEquipList)
		{
			isTargetSlotInUse = targetEquipSlot == GetLeftHandSlot();
			curEquipList = leftEquipList;
			enchantList = NULL;
		}
		// Case 4: Not equipped yet.
		else
		{
			isTargetSlotInUse = false;
			curEquipList = NULL;
			enchantList = entryData->extendDataList->GetNthItem(0);
		}
	}

	// Free temp equip entryData
	entryData->Delete();

	// Normally EquipManager would update CannotWear, if equip is skipped we do it here
	if (isTargetSlotInUse)
	{
		BSExtraData* xCannotWear = curEquipList->GetByType(kExtraData_CannotWear);
		if (xCannotWear && !preventUnequip)
			curEquipList->Remove(kExtraData_CannotWear, xCannotWear);
		else if (!xCannotWear && preventUnequip)
			curEquipList->Add(kExtraData_CannotWear, ExtraCannotWear::Create());

		// Slot in use, nothing left to do
		return false;
	}

	// For dual wield, prevent that 1 item can be equipped in two hands if its already equipped
	bool isEquipped = (rightEquipList || leftEquipList);
	if (targetEquipSlot && isEquipped && CanEquipBothHands(thisActor, item))
		hasItemMinCount = itemCount > 1;

	if (!isTargetSlotInUse && hasItemMinCount)
		CALL_MEMBER_FN(equipManager, EquipItem)(thisActor, item, enchantList, equipCount, targetEquipSlot, equipSound, preventUnequip, false, NULL);
	else
		return false;

	return true;
}


// get mod index from a normal form ID 32 bit unsigned
inline UInt32 GetModIndex(UInt32 formId)
{
	return formId >> 24;
}

// get base formID (without mod index)
inline UInt32 GetBaseFormID(UInt32 formId)
{
	return formId & 0x00FFFFFF;
}

// check if mod index is valid (mod index is the upper 8 bits of form ID)
inline bool IsValidModIndex(UInt32 modIndex)
{
	return modIndex > 0 && modIndex != 0xFF;
}
