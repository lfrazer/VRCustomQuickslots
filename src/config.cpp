#include "quickslots.h"
#include "tinyxml2.h"


bool   CQuickslotManager::ReadConfig(const char* filename)
{

	float defaultRadius = 0.1f;

	tinyxml2::XMLDocument xmldoc;
	tinyxml2::XMLError err = xmldoc.LoadFile(filename);

	if(err != tinyxml2::XMLError::XML_SUCCESS)
	{
		_MESSAGE("Error: %s opening XML doc: %s", xmldoc.ErrorName(), filename);
		return false;
	}

	int quickslotCount = 0;
	tinyxml2::XMLElement* root = xmldoc.RootElement();

	for (tinyxml2::XMLElement* elem = root->FirstChildElement(); elem; elem = elem->NextSiblingElement())
	{
		
		if (strcmp(elem->Name(), "options") == 0)
		{
			elem->QueryFloatAttribute("defaultradius", &defaultRadius);
			elem->QueryIntAttribute("debugloglevel", &mDebugLogVerb);

			mControllerRadius = defaultRadius;
		}

		else if (strcmp(elem->Name(), "quickslot") == 0)
		{
			float position[3];
			float radius = defaultRadius;
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
			_MESSAGE("Read in quickslot #%d, info below:", quickslotCount);
			quickslot.PrintInfo();

		}
	}

	_MESSAGE("Finished reading %s - debugloglevel=%d", filename, mDebugLogVerb);

	return true;
}

