#include "console.h"
#include "quickslotutil.h"

// Thanks to DSN (Dragonborn Speaks Naturally) for the method of running console commands


IMenu* CSkyrimConsole::sConsoleMenu = nullptr;

void CSkyrimConsole::RunCommand(const char* cmd)
{
	if (!sConsoleMenu) 
	{
		QSLOG_INFO("Trying to create Console menu");
		sConsoleMenu = SKSEMenuManager::GetSingleton()->GetOrCreateMenu("Console");
	}

	if (sConsoleMenu != NULL) {

		GFxValue methodName;
		methodName.type = GFxValue::kType_String;
		methodName.data.string = "ExecuteCommand";
		GFxValue resphash;
		resphash.type = GFxValue::kType_Number;
		resphash.data.number = -1;
		GFxValue commandVal;
		commandVal.type = GFxValue::kType_String;
		commandVal.data.string = cmd;
		GFxValue args[3];
		args[0] = methodName;
		args[1] = resphash;
		args[2] = commandVal;

		GFxValue resp;
		sConsoleMenu->view->Invoke("flash.external.ExternalInterface.call", &resp, args, 3);
	}
	else
	{
		QSLOG_ERR("Unable to find Console menu");
	}
}


IMenu* SKSEMenuManager::GetOrCreateMenu(const char* menuName)
{
	SKSEMenuManager *menuManager = SKSEMenuManager::GetSingleton();

	if (!menuManager)
		return NULL;

	SKSEMenuManager::MenuTable *t = &menuManager->menuTable;

	for (int i = 0; i < t->m_size; i++) {
		SKSEMenuManager::MenuTable::_Entry * entry = &t->m_entries[i];
		if (entry != t->m_eolPtr && entry->next != NULL) {
			MenuTableItem *item = &entry->item;

			if ((uintptr_t)item > 1 && item->name) {
				BSFixedString *name = &item->name;
				if ((uintptr_t)name > 1 && name->data && std::strncmp(name->data, menuName, 10) == 0) {
					if (!item->menuInstance && item->menuConstructor) 
					{
						// originally in DSN they changed menuConstructor from void* to func ptr, lets cast it to a function here
						IMenu* (*menuConstructorFuncPtr)(void);
						menuConstructorFuncPtr = (IMenu* (*)(void))item->menuConstructor;
						item->menuInstance = menuConstructorFuncPtr();
					}

					return item->menuInstance;
				}
			}
		}
	}

	return NULL;
}