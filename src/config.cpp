#include "quickslots.h"
#include "tinyxml2.h"


bool   CQuickslotManager::ReadConfig(const char* filename)
{

	float defaultRadius = 1.0f;

	tinyxml2::XMLDocument xmldoc;
	tinyxml2::XMLError err = xmldoc.LoadFile(filename);

	if(err != tinyxml2::XMLError::XML_SUCCESS)
	{
		return false;
	}

	tinyxml2::XMLElement* root = xmldoc.RootElement();

	for (tinyxml2::XMLElement* elem = root->FirstChildElement(); elem; elem = elem->NextSiblingElement())
	{
		
		if (strcmp(elem->Name(), "options") == 0)
		{
			elem->QueryFloatAttribute("defaultradius", &defaultRadius);
		}

		else if (strcmp(elem->Name(), "quickslot") == 0)
		{
			float position[3];
			float radius = defaultRadius;
			const char* cmd[2] = { 0 }; // two commands, one for each hand
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
					cmd[cmdCount] = subElem->GetText();
				}
				cmdCount++;

			}

			CQuickslot quickslot(Vector3(position[0], position[1], position[2]), radius, cmd[0], cmd[1], slotname);
			mQuickslotArray.push_back(quickslot);

		}
	}

	return true;
}

