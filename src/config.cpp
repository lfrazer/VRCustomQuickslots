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

#include "quickslots.h"
#include "quickslotutil.h"
#include "tinyxml2.h"


bool   CQuickslotManager::ReadConfig(const char* filename)
{

	tinyxml2::XMLDocument xmldoc;
	tinyxml2::XMLError err = xmldoc.LoadFile(filename);

	if(err != tinyxml2::XMLError::XML_SUCCESS)
	{
		_MESSAGE("Error: %s opening XML doc: %s", xmldoc.ErrorName(), filename);
		_MESSAGE("Full XML Error Info: %s", xmldoc.ErrorStr());
		return false;
	}

	int quickslotCount = 0;
	tinyxml2::XMLElement* root = xmldoc.RootElement();

	for (tinyxml2::XMLElement* elem = root->FirstChildElement(); elem; elem = elem->NextSiblingElement())
	{
		
		if (strcmp(elem->Name(), "options") == 0)
		{
			elem->QueryFloatAttribute("defaultradius", &mDefaultRadius);
			elem->QueryIntAttribute("debugloglevel", &mDebugLogVerb);
			elem->QueryIntAttribute("hapticfeedback", &mHapticOnOverlap);
			elem->QueryIntAttribute("alloweditslots", &mAllowEditSlots);
			elem->QueryDoubleAttribute("longpresstime", &mLongPressTime);

			mControllerRadius = mDefaultRadius;

			elem->QueryFloatAttribute("controllerradius", &mControllerRadius);

			CUtil::GetSingleton().SetLogLevel(mDebugLogVerb);
		}

		else if (strcmp(elem->Name(), "quickslot") == 0)
		{
			float position[3];
			float radius = mDefaultRadius;
			CQuickslot::CQuickslotCmd cmd[2]; // two commands, one for each hand
			const char* altCmd = "none";
			const char* slotname = nullptr;

			elem->QueryFloatAttribute("posx", &position[0]);
			elem->QueryFloatAttribute("posy", &position[1]);
			elem->QueryFloatAttribute("posz", &position[2]);
			elem->QueryFloatAttribute("radius", &radius);
			elem->QueryStringAttribute("name", &slotname);

			int cmdCount = 0;
			for(tinyxml2::XMLElement* subElem = elem->FirstChildElement(); subElem; subElem = subElem->NextSiblingElement())
			{
				if (cmdCount < 2)
				{
					if (strcmp(subElem->Name(), "equipitem") == 0)
					{
						cmd[cmdCount].mAction = CQuickslot::EQUIP_ITEM;
					}
					else if (strcmp(subElem->Name(), "equipspell") == 0)
					{
						cmd[cmdCount].mAction = CQuickslot::EQUIP_SPELL;
					}
					else if (strcmp(subElem->Name(), "equipshout") == 0)
					{
						cmd[cmdCount].mAction = CQuickslot::EQUIP_SHOUT;
					}
					else if (strcmp(subElem->Name(), "consolecmd") == 0)
					{
						cmd[cmdCount].mAction = CQuickslot::CONSOLE_CMD;
					}

					const char* formIdStr;
					subElem->QueryStringAttribute("formid", &formIdStr);
					if (formIdStr != nullptr)
					{
						cmd[cmdCount].mFormID = std::stoul(formIdStr, nullptr, 16);  // convert hex string to int
					}

					// get which slot to use
					subElem->QueryIntAttribute("slot", &cmd[cmdCount].mSlot);

					if (subElem->GetText())
					{
						cmd[cmdCount].mCommand = subElem->GetText();
					}
				}
				cmdCount++;

			}

			CQuickslot quickslot(PapyrusVR::Vector3(position[0], position[1], position[2]), radius, cmd[0], cmd[1], slotname);
			mQuickslotArray.push_back(quickslot);

			quickslotCount++;
			QSLOG_INFO("Read in quickslot #%d, info below:", quickslotCount);
			quickslot.PrintInfo();

		}
	}

	QSLOG("Finished reading %s - debugloglevel=%d", filename, mDebugLogVerb);

	return true;
}



bool	CQuickslotManager::WriteConfig(const char* filename)
{

	// Start document
	tinyxml2::XMLDocument xmldoc;
	tinyxml2::XMLElement* root = xmldoc.NewElement("vrcustomquickslots");
	xmldoc.InsertFirstChild(root);

	// write options
	tinyxml2::XMLElement* options = xmldoc.NewElement("options");
	options->SetAttribute("defaultradius", mDefaultRadius);
	options->SetAttribute("debugloglevel", mDebugLogVerb);
	options->SetAttribute("hapticfeedback", mHapticOnOverlap);
	options->SetAttribute("alloweditslots", mAllowEditSlots);
	options->SetAttribute("longpresstime", mLongPressTime);
	options->SetAttribute("controllerradius", mControllerRadius);

	root->InsertFirstChild(options);

	// lambda func for processing command actions and writing to XML (will be used in loop below)
	auto WriteAction = [&](const CQuickslot::CQuickslotCmd& cmd, tinyxml2::XMLElement* quickslotElem)
	{
		tinyxml2::XMLElement* actionElem = nullptr;

		if (cmd.mAction == CQuickslot::EQUIP_ITEM)
		{
			actionElem = xmldoc.NewElement("equipitem");
		}
		else if (cmd.mAction == CQuickslot::EQUIP_SPELL)
		{
			actionElem = xmldoc.NewElement("equipspell");
		}
		else if (cmd.mAction == CQuickslot::EQUIP_SHOUT)
		{
			actionElem = xmldoc.NewElement("equipshout");
		}
		else if (cmd.mAction == CQuickslot::CONSOLE_CMD)
		{
			actionElem = xmldoc.NewElement("consolecmd");
			actionElem->SetText(cmd.mCommand.c_str());
		}
		else
		{
			return;
		}
		
		// Special case for formID, write as hex by string
		char hexFormId[100] = { 0 };
		sprintf_s(hexFormId, "%x", cmd.mFormID);
		actionElem->SetAttribute("formid", hexFormId);
		
		actionElem->SetAttribute("slot", cmd.mSlot);

		quickslotElem->InsertEndChild(actionElem);

	};

	// write in each quickslot
	for (auto it = mQuickslotArray.begin(); it != mQuickslotArray.end(); ++it)
	{
		tinyxml2::XMLElement* quickslotElem = xmldoc.NewElement("quickslot");
		
		// do quickslot settings
		quickslotElem->SetAttribute("name", it->mName.c_str());
		quickslotElem->SetAttribute("radius", it->mRadius);
		quickslotElem->SetAttribute("posx", it->mPosition.x);
		quickslotElem->SetAttribute("posy", it->mPosition.y);
		quickslotElem->SetAttribute("posz", it->mPosition.z);

		// do quickslot actions
		WriteAction(it->mCommand, quickslotElem);
		WriteAction(it->mCommandAlt, quickslotElem);

		// store quickslot
		root->InsertEndChild(quickslotElem);
	}


	tinyxml2::XMLError err = xmldoc.SaveFile(filename);

	if (err != tinyxml2::XMLError::XML_SUCCESS)
	{
		_MESSAGE("Error: %s saving XML doc: %s", xmldoc.ErrorName(), filename);
		_MESSAGE("Full XML Save Error Info: %s", xmldoc.ErrorStr());
		return false;
	}

	QSLOG("Finished writing XML config to %s", filename);

	return true;
}

