#ifndef __GAME_DESK__
#define __GAME_DESK__

#include <assert.h>

template<class DESKTYPE, int SIZE>
class game_desk_list
{
private:
	DESKTYPE desks_[SIZE];

public:
	game_desk_list()
	{
		//for (int i = 0; i < SIZE; i++)
		//{
		//	desks_[i].set_id(i);
		//}
	}

	DESKTYPE* at(unsigned index)
	{
		assert(index >= 0 && index < SIZE);

		if (index < 0 || index >= SIZE)
			return NULL;

		return &desks_[index];
	}

	DESKTYPE* operator[] (unsigned index)
	{
		return this->at(index);
	}

	int size()
	{
		return SIZE;
	}
};

#endif