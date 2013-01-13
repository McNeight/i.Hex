#ifndef _DIFF_H_
#define _DIFF_H_

// http://www.mathertel.de/Diff/ViewSrc.aspx
#include "GArray.h"
#include "GToken.h"
#include "GHashTable.h"

struct Diff
{
	typedef GHashTbl<char*,int> Hashtable;

	template<typename T>
	struct DiffData
	{
		GArray<T> data;
		GArray<bool> modified;
		
		DiffData(GArray<T> &initData)
		{
			data = initData;
			modified.Length(data.Length() + 2);
		}
		
		int Length()
		{
			return data.Length();
		}
	};
	
	struct Item
	{
		int StartA;
		int StartB;

		int deletedA;
		int insertedB;
	};

	struct SMSRD
	{
		int x, y;
	};

	GArray<Item> DiffText(char *TextA, char *TextB);
	GArray<Item> DiffText(char *TextA, char *TextB, bool trimSpace, bool ignoreSpace, bool ignoreCase);
	template<typename T> void Optimize(DiffData<T> &Data);
	template<typename T> GArray<Item> DiffInt(GArray<T> &ArrayA, GArray<T> &ArrayB);
	void StripRet(char *a);	
	void Trim(char *str);
	GArray<int> DiffCodes(char *aText, Hashtable &h, bool trimSpace, bool ignoreSpace, bool ignoreCase);
	
	template<typename T>
	SMSRD SMS(	DiffData<T> &DataA, int LowerA, int UpperA,
				DiffData<T> &DataB, int LowerB, int UpperB,
				int *DownVector, int *UpVector);
	template<typename T>
	void LCS(	DiffData<T> &DataA, int LowerA, int UpperA,
				DiffData<T> &DataB, int LowerB, int UpperB,
				GArray<int> &DownVector, GArray<int> &UpVector);
	template<typename T>
	GArray<Item> CreateDiffs(DiffData<T> &DataA, DiffData<T> &DataB);
};

#endif