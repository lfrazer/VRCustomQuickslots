#include <stddef.h>
#include "common/IPrefix.h"
#include "skse64_common/Utilities.h"
#include "skse64/GameTypes.h"
#include "skse64/GameMenus.h"

// Copy of tHashSet class from SKSE - reason for this is we need access to some private members for console code - make SKSEMenuManager a friend class to accomplish this
// 30
template <typename Item, typename Key = Item>
class SKSE_tHashSet
{
	friend class SKSEMenuManager;

	class _Entry
	{
	public:
		Item	item;
		_Entry	* next;

		_Entry() : next(NULL) {}

		bool	IsFree() const { return next == NULL; }
		void	SetFree() { next = NULL; }

		void	Dump(void)
		{
			item.Dump();
			_MESSAGE("\t\tnext: %08X", next);
		}
	};

	// When creating a new tHashSet, init sentinel pointer with address of this entry
	static _Entry sentinel;

	void		* unk00;		// 000
	UInt32		unk_000;		// 008
	UInt32		m_size;			// 00C
	UInt32		m_freeCount;	// 010
	UInt32		m_freeOffset;	// 014
	_Entry 		* m_eolPtr;		// 018
	UInt64		unk_018;		// 020
	_Entry		* m_entries;	// 028


	_Entry * GetEntry(UInt32 hash) const
	{
		return (_Entry*)(((uintptr_t)m_entries) + sizeof(_Entry) * (hash & (m_size - 1)));
	}

	_Entry * GetEntryAt(UInt32 index) const
	{
		return (_Entry*)(((uintptr_t)m_entries) + sizeof(_Entry) * index);
	}

	_Entry * NextFreeEntry(void)
	{
		_Entry * result = NULL;

		if (m_freeCount == 0)
			return NULL;

		do
		{
			m_freeOffset = (m_size - 1) & (m_freeOffset - 1);
			_Entry * entry = GetEntryAt(m_freeOffset);

			if (entry->IsFree())
				result = entry;
		} while (!result);

		m_freeCount--;

		return result;
	}

	enum InsertResult
	{
		kInsert_Duplicate = -1,
		kInsert_OutOfSpace = 0,
		kInsert_Success = 1
	};

	InsertResult Insert(Item * item)
	{
		if (!m_entries)
			return kInsert_OutOfSpace;

		Key k = (Key)*item;
		_Entry * targetEntry = GetEntry(Item::GetHash(&k));

		// Case 1: Target entry is free
		if (targetEntry->IsFree())
		{
			targetEntry->item = *item;
			targetEntry->next = m_eolPtr;
			--m_freeCount;

			return kInsert_Success;
		}

		// -- Target entry is already in use

		// Case 2: Item already included
		_Entry * p = targetEntry;
		do
		{
			if (p->item == *item)
				return kInsert_Duplicate;
			p = p->next;
		} while (p != m_eolPtr);

		// -- Either hash collision or bucket overlap

		_Entry * freeEntry = NextFreeEntry();
		// No more space?
		if (!freeEntry)
			return kInsert_OutOfSpace;

		// Original position of the entry that currently uses the target position
		k = (Key)targetEntry->item;
		p = GetEntry(Item::GetHash(&k));

		// Case 3a: Hash collision - insert new entry between target entry and successor
		if (targetEntry == p)
		{
			freeEntry->item = *item;
			freeEntry->next = targetEntry->next;
			targetEntry->next = freeEntry;

			return kInsert_Success;
		}
		// Case 3b: Bucket overlap
		else
		{
			while (p->next != targetEntry)
				p = p->next;

			freeEntry->item = targetEntry->item;
			freeEntry->next = targetEntry->next;

			p->next = freeEntry;
			targetEntry->item = *item;
			targetEntry->next = m_eolPtr;

			return kInsert_Success;
		}
	}

	bool CopyEntry(_Entry * sourceEntry)
	{
		if (!m_entries)
			return false;

		Key k = (Key)sourceEntry->item;
		_Entry * targetEntry = GetEntry(Item::GetHash(&k));

		// Case 1: Target location is unused
		if (!targetEntry->next)
		{
			targetEntry->item = sourceEntry->item;
			targetEntry->next = m_eolPtr;
			--m_freeCount;

			return true;
		}

		// Target location is in use. Either hash collision or bucket overlap.

		_Entry * freeEntry = NextFreeEntry();
		k = (Key)targetEntry->item;
		_Entry * p = GetEntry(Item::GetHash(&k));

		// Case 2a: Hash collision - insert new entry between target entry and successor
		if (targetEntry == p)
		{
			freeEntry->item = sourceEntry->item;
			freeEntry->next = targetEntry->next;
			targetEntry->next = freeEntry;

			return true;
		}

		// Case 2b: Bucket overlap - forward until hash collision is found, then insert
		while (p->next != targetEntry)
			p = p->next;

		// Source entry takes position of target entry - not completely understood yet
		freeEntry->item = targetEntry->item;
		freeEntry->next = targetEntry->next;
		p->next = freeEntry;
		targetEntry->item = sourceEntry->item;
		targetEntry->next = m_eolPtr;

		return true;
	}

	void Grow(void)
	{
		UInt32 oldSize = m_size;
		UInt32 newSize = oldSize ? 2 * oldSize : 8;

		_Entry * oldEntries = m_entries;
		_Entry * newEntries = (_Entry*)Heap_Allocate(newSize * sizeof(_Entry));
		ASSERT(newEntries);

		m_entries = newEntries;
		m_size = m_freeCount = m_freeOffset = newSize;

		// Initialize new table data (clear next pointers)
		_Entry * p = newEntries;
		for (UInt32 i = 0; i < newSize; i++, p++)
			p->SetFree();

		// Copy old entries, free old table data
		if (oldEntries)
		{
			_Entry * p = oldEntries;
			for (UInt32 i = 0; i < oldSize; i++, p++)
				if (p->next)
					CopyEntry(p);
			Heap_Free(oldEntries);
		}
	}

public:

	SKSE_tHashSet() : m_size(0), m_freeCount(0), m_freeOffset(0), m_entries(NULL), m_eolPtr(&sentinel) { }

	UInt32	Size() const { return m_size; }
	UInt32	FreeCount() const { return m_freeCount; }
	UInt32	FillCount() const { return m_size - m_freeCount; }

	Item * Find(Key * key) const
	{
		if (!m_entries)
			return NULL;

		_Entry * entry = GetEntry(Item::GetHash(key));
		if (!entry->next)
			return NULL;

		while (!(entry->item == *key))
		{
			entry = entry->next;
			if (entry == m_eolPtr)
				return NULL;
		}

		return &entry->item;
	}

	bool Add(Item * item)
	{
		InsertResult result;

		for (result = Insert(item); result == kInsert_OutOfSpace; result = Insert(item))
			Grow();

		return result == kInsert_Success;
	}

	bool Remove(Key * key)
	{
		if (!m_entries)
			return false;

		_Entry * entry = GetEntry(Item::GetHash(key));
		if (!entry->next)
			return NULL;

		_Entry * prevEntry = NULL;
		while (!(entry->item == *key))
		{
			prevEntry = entry;
			entry = entry->next;
			if (entry == m_eolPtr)
				return false;
		}

		// Remove tail?
		_Entry * nextEntry = entry->next;
		if (nextEntry == m_eolPtr)
		{
			if (prevEntry)
				prevEntry->next = m_eolPtr;
			entry->next = NULL;
		}
		else
		{
			entry->item = nextEntry->item;
			entry->next = nextEntry->next;
			nextEntry->next = NULL;
		}

		++m_freeCount;
		return true;
	}

	void Clear(void)
	{
		if (m_entries)
		{
			_Entry * p = m_entries;
			for (UInt32 i = 0; i < m_size; i++, p++)
				p->next = NULL;
		}
		else
		{
			m_size = 0;
		}
		m_freeCount = m_freeOffset = m_size;
	}

	template <typename T>
	void ForEach(T& functor)
	{
		if (!m_entries)
			return;

		if (m_size == m_freeCount) // The whole thing is free
			return;

		_Entry * cur = m_entries;
		_Entry * end = GetEntryAt(m_size); // one index beyond the entries data to check if we reached that point

		if (cur == end)
			return;

		if (cur->IsFree())
		{
			// Forward to first non-free entry
			do cur++;
			while (cur != end && cur->IsFree());
		}

		do
		{
			if (!functor(&cur->item))
				return;

			// Forward to next non-free entry
			do cur++;
			while (cur != end && cur->IsFree());

		} while (cur != end);
	}

	void Dump(void)
	{
		_MESSAGE("tHashSet:");
		_MESSAGE("> size: %d", Size());
		_MESSAGE("> free: %d", FreeCount());
		_MESSAGE("> filled: %d", FillCount());
		if (m_entries)
		{
			_Entry * p = m_entries;
			for (UInt32 i = 0; i < m_size; i++, p++) {
				_MESSAGE("* %d %s:", i, p->IsFree() ? "(free)" : "");
				if (!p->IsFree())
					p->Dump();
			}
		}
	}

	DEFINE_STATIC_HEAP(Heap_Allocate, Heap_Free)
};



// Copy of MenuManager class from SKSE - reason for this is we need access to some private members for console code
// 1C8
class SKSEMenuManager
{
public:
	typedef SKSE_tHashSet<MenuTableItem, BSFixedString> MenuTable;

	// 030-040
	struct Unknown3
	{
		UInt64		unk00;		// 000 - New in SE. Not init'd?

		UInt64		frequency;	// 008 

		UInt64		unk_010;	// 010 (= 0)
		UInt32		unk_018;	// 018 (= 0)
		UInt32		unk_01C;	// 01C (= 0)
		UInt32		unk_020;	// 020 (= 0)
		UInt32		unk_024;	// 024 (= 0)
		float		unk_028;	// 028 (= frequency related)

		UInt32		unk_02C;	// 02C
		UInt32		unk_030;	// 030

		UInt32		unk_034;	// 034 (= 0)
		UInt16		unk_038;	// 038
		UInt8		unk_03A;	// 03A (= 0)
		UInt8		pad[5];		// 03B
	};
	STATIC_ASSERT(sizeof(Unknown3) == 0x40);

	UInt64					unk_000;	// 000

	EventDispatcher<MenuOpenCloseEvent>		menuOpenCloseEventDispatcher;	// 008
	EventDispatcher<MenuModeChangeEvent>	menuModeChangeEventDispatcher;	// 060
	EventDispatcher<void*>					unk_064;						// 0B8 - New in 1.6.87.0 - Kinect related?

	UnkArray			menuStack;					// 110
	MenuTable			menuTable;					// 128   (Entries ptr at 150)
	SimpleLock			menuTableLock;				// 158
	UInt32				numPauseGame;				// 160 (= 0) += 1 if (imenu->flags & 0x0001)
	UInt32				numItemMenu;				// 164 (= 0) += 1 if (imenu->flags & 0x2000)
	UInt32				numPreventGameLoad;			// 168 (= 0) += 1 if (imenu->flags & 0x0080)
	UInt32				numDoNotPreventSaveGame;	// 16C (= 0) += 1 if (imenu->flags & 0x0800)
	UInt32				numStopCrosshairUpdate;		// 170 (= 0) += 1 if (imenu->flags & 0x4000)
	UInt32				numFlag8000;				// 174 (= 0) += 1 if (imenu->flags & 0x8000)
	UInt32				numFlag20000;				// 178 (= 0)  = 1 if (imenu->flags & 0x20000)
	UInt8				numModal;					// 17C (= 0)  = 1 if (imenu->flags & 0x10)
	UInt8				pad_17D[3];					// 17D
	Unknown3			unk_180;					// 180
	bool				showMenus;					// 1C0 (= 0)
	bool				unk_1C1;					// 1C1 (= 0)
	char				pad[6];						// 1C2
private:

public:
	typedef IMenu*	(*CreatorFunc)(void);

private:
	MEMBER_FN_PREFIX(SKSEMenuManager);
	DEFINE_MEMBER_FN(IsMenuOpen, bool, 0x00F1A3B0, BSFixedString * menuName);
	DEFINE_MEMBER_FN(Register_internal, void, 0x00F1BE20, const char * name, CreatorFunc creator);

public:

	static SKSEMenuManager * GetSingleton(void)
	{
		// 502FDB8FEA80C3705F9E228F79D4EA7A399CC7FD+32
		static RelocPtr<SKSEMenuManager *> g_menuManager(0x01F83200);
		return *g_menuManager;
	}

	EventDispatcher<MenuOpenCloseEvent> * MenuOpenCloseEventDispatcher()
	{
		return &menuOpenCloseEventDispatcher;
	}

	bool				IsMenuOpen(BSFixedString * menuName);
	IMenu *				GetMenu(BSFixedString * menuName);
	GFxMovieView *		GetMovieView(BSFixedString * menuName);
	void				ShowMenus(bool show) { showMenus = show; }
	bool				IsShowingMenus() const { return showMenus; }

	typedef IMenu* (*CreatorFunc)(void);

	void Register(const char* name, CreatorFunc creator)
	{
		CALL_MEMBER_FN(this, Register_internal)(name, creator);
	}

	IMenu* GetOrCreateMenu(const char* menuName);
};
STATIC_ASSERT(sizeof(SKSEMenuManager) == 0x1C8);




class CSkyrimConsole
{
	static IMenu* sConsoleMenu;
public:
	static void RunCommand(const char* cmd);

};