#include "Diff.h"
#include "string.h"

/// <summary>
/// Find the difference in 2 texts, comparing by textlines.
/// </summary>
/// <param name="TextA">A-version of the text (usualy the old one)</param>
/// <param name="TextB">B-version of the text (usualy the new one)</param>
/// <returns>Returns a array of Items that describe the differences.</returns>
GArray<Diff::Item> Diff::DiffText(char *TextA, char *TextB)
{
	return DiffText(TextA, TextB, false, false, false);
}


/// <summary>
/// Find the difference in 2 text documents, comparing by textlines.
/// The algorithm itself is comparing 2 arrays of numbers so when comparing 2 text documents
/// each line is converted into a (hash) number. This hash-value is computed by storing all
/// textlines into a common hashtable so i can find dublicates in there, and generating a 
/// new number each time a new textline is inserted.
/// </summary>
/// <param name="TextA">A-version of the text (usualy the old one)</param>
/// <param name="TextB">B-version of the text (usualy the new one)</param>
/// <param name="trimSpace">When set to true, all leading and trailing whitespace characters are stripped out before the comparation is done.</param>
/// <param name="ignoreSpace">When set to true, all whitespace characters are converted to a single space character before the comparation is done.</param>
/// <param name="ignoreCase">When set to true, all characters are converted to their lowercase equivivalence before the comparation is done.</param>
/// <returns>Returns a array of Items that describe the differences.</returns>
GArray<Diff::Item> Diff::DiffText(char *TextA, char *TextB, bool trimSpace, bool ignoreSpace, bool ignoreCase)
{
	// prepare the input-text and convert to comparable numbers.
	Hashtable h((strlen(TextA) + strlen(TextB)) * 3, false, NULL, -1);

	// The A-Version of the data (original data) to be compared.
	GArray<int> dc = DiffCodes(TextA, h, trimSpace, ignoreSpace, ignoreCase);
	DiffData<int> DataA(dc);

	// The B-Version of the data (modified data) to be compared.
	dc = DiffCodes(TextB, h, trimSpace, ignoreSpace, ignoreCase);
	DiffData<int> DataB(dc);

	h.Empty(); // free up hashtable memory (maybe)

	int MAX = DataA.Length() + DataB.Length() + 1;
	/// vector for the (0,0) to (x,y) search
	GArray<int> DownVector(2 * MAX + 2);
	/// vector for the (u,v) to (N,M) search
	GArray<int> UpVector(2 * MAX + 2);

	LCS(DataA, 0, DataA.Length(), DataB, 0, DataB.Length(), DownVector, UpVector);

	Optimize(DataA);
	Optimize(DataB);
	
	return CreateDiffs(DataA, DataB);
}

template<typename T>
void Diff::Optimize(DiffData<T> &Data)
{
	int StartPos, EndPos;

	StartPos = 0;
	while (StartPos < Data.Length())
	{
		while ((StartPos < Data.Length()) && (Data.modified[StartPos] == false))
			StartPos++;
		EndPos = StartPos;
		while ((EndPos < Data.Length()) && (Data.modified[EndPos] == true))
			EndPos++;

		if ((EndPos < Data.Length()) && (Data.data[StartPos] == Data.data[EndPos])) {
			Data.modified[StartPos] = false;
			Data.modified[EndPos] = true;
		} else {
			StartPos = EndPos;
		} // if
	} // while
} // Optimize


template<typename T>
GArray<Diff::Item> Diff::DiffInt(GArray<T> &ArrayA, GArray<T> &ArrayB)
{		
	DiffData<T> DataA(ArrayA);
	DiffData<T> DataB(ArrayB);

	int MAX = DataA.Length() + DataB.Length() + 1;
	GArray<int> DownVector(2 * MAX + 2);
	GArray<int> UpVector(2 * MAX + 2);

	LCS(DataA, 0, DataA.Length(), DataB, 0, DataB.Length(), DownVector, UpVector);
	
	return CreateDiffs(DataA, DataB);
}

void Diff::StripRet(char *a)
{
	char *o = a;
	while (*a)
	{
		if (*a != '\r')
			*o++ = *a;
		a++;
	}
}

void Diff::Trim(char *str)
{
	char *s = str;
	char *o = str;
	while (*s && strchr(WhiteSpace, *s))
		s++;
	while (*s)
		*o++ = *s++;
	while (o > str && strchr(WhiteSpace, o[-1]))
		*(--o) = 0;
}

/// <summary>
/// This function converts all textlines of the text into unique numbers for every unique textline
/// so further work can work only with simple numbers.
/// </summary>
/// <param name="aText">the input text</param>
/// <param name="h">This extern initialized hashtable is used for storing all ever used textlines.</param>
/// <param name="trimSpace">ignore leading and trailing space characters</param>
/// <returns>a array of integers.</returns>
GArray<int> Diff::DiffCodes(char *aText, Hashtable &h, bool trimSpace, bool ignoreSpace, bool ignoreCase)
{
	// get all codes of the text
	GArray<int> Codes;
	int lastUsedCode = h.Length();
	char *s;

	GToken Lines(aText, "\r\n");
	Codes.Length(Lines.Length());

	for (int i = 0; i < Lines.Length(); ++i)
	{
		s = Lines[i];
		if (trimSpace)
			Trim(s);

		/*
		if (ignoreSpace)
			s = Regex.Replace(s, "\\s+", " ");
		*/

		if (ignoreCase)
			strlwr(s);

		int aCode = h.Find(s);
		if (aCode < 0)
		{
			lastUsedCode++;
			h.Add(s, lastUsedCode);
			Codes[i] = lastUsedCode;
		}
		else
		{
			Codes[i] = aCode;
		}
	}
	
	return (Codes);
}

/// <summary>
/// This is the algorithm to find the Shortest Middle Snake (SMS).
/// </summary>
/// <param name="DataA">sequence A</param>
/// <param name="LowerA">lower bound of the actual range in DataA</param>
/// <param name="UpperA">upper bound of the actual range in DataA (exclusive)</param>
/// <param name="DataB">sequence B</param>
/// <param name="LowerB">lower bound of the actual range in DataB</param>
/// <param name="UpperB">upper bound of the actual range in DataB (exclusive)</param>
/// <param name="DownVector">a vector for the (0,0) to (x,y) search. Passed as a parameter for speed reasons.</param>
/// <param name="UpVector">a vector for the (u,v) to (N,M) search. Passed as a parameter for speed reasons.</param>
/// <returns>a MiddleSnakeData record containing x,y and u,v</returns>
template<typename T>
Diff::SMSRD Diff::SMS(	DiffData<T> &DataA, int LowerA, int UpperA,
						DiffData<T> &DataB, int LowerB, int UpperB,
						int *DownVector, int *UpVector)
{
	SMSRD ret = {0, 0};
	int MAX = DataA.Length() + DataB.Length() + 1;
	T *DataAPtr = &DataA.data[0];
	T *DataBPtr = &DataB.data[0];

	int DownK = LowerA - LowerB; // the k-line to start the forward search
	int UpK = UpperA - UpperB; // the k-line to start the reverse search

	int Delta = (UpperA - LowerA) - (UpperB - LowerB);
	bool oddDelta = (Delta & 1) != 0;

	// The vectors in the publication accepts negative indexes. the vectors implemented here are 0-based
	// and are access using a specific offset: UpOffset UpVector and DownOffset for DownVektor
	int DownOffset = MAX - DownK;
	int UpOffset = MAX - UpK;

	int MaxD = ((UpperA - LowerA + UpperB - LowerB) / 2) + 1;

	// Debug.Write(2, "SMS", String.Format("Search the box: A[{0}-{1}] to B[{2}-{3}]", LowerA, UpperA, LowerB, UpperB));

	// init vectors
	DownVector[DownOffset + DownK + 1] = LowerA;
	UpVector[UpOffset + UpK - 1] = UpperA;

	for (int D = 0; D <= MaxD; D++)
	{
		// Extend the forward path.
		for (int k = DownK - D; k <= DownK + D; k += 2)
		{
			// Debug.Write(0, "SMS", "extend forward path " + k.ToString());

			// find the only or better starting point
			int x, y;
			if (k == DownK - D)
			{
				x = DownVector[DownOffset + k + 1]; // down
			}
			else
			{
				x = DownVector[DownOffset + k - 1] + 1; // a step to the right
				if ((k < DownK + D) && (DownVector[DownOffset + k + 1] >= x))
					x = DownVector[DownOffset + k + 1]; // down
			}
			y = x - k;

			// find the end of the furthest reaching forward D-path in diagonal k.
			while ((x < UpperA) && (y < UpperB) && (DataAPtr[x] == DataBPtr[y]))
			{
				x++; y++;
			}
			DownVector[DownOffset + k] = x;

			// overlap ?
			if (oddDelta && (UpK - D < k) && (k < UpK + D))
			{
				if (UpVector[UpOffset + k] <= DownVector[DownOffset + k])
				{
					ret.x = DownVector[DownOffset + k];
					ret.y = DownVector[DownOffset + k] - k;
					return (ret);
				} // if
			} // if

		} // for k

		// Extend the reverse path.
		for (int k = UpK - D; k <= UpK + D; k += 2)
		{
			// Debug.Write(0, "SMS", "extend reverse path " + k.ToString());

			// find the only or better starting point
			int x, y;
			if (k == UpK + D)
			{
				x = UpVector[UpOffset + k - 1]; // up
			}
			else
			{
				x = UpVector[UpOffset + k + 1] - 1; // left
				if ((k > UpK - D) && (UpVector[UpOffset + k - 1] < x))
					x = UpVector[UpOffset + k - 1]; // up
			} // if
			y = x - k;

			while ((x > LowerA) && (y > LowerB) && (DataAPtr[x - 1] == DataBPtr[y - 1]))
			{
				x--; y--; // diagonal
			}
			UpVector[UpOffset + k] = x;

			// overlap ?
			if (!oddDelta && (DownK - D <= k) && (k <= DownK + D))
			{
				if (UpVector[UpOffset + k] <= DownVector[DownOffset + k])
				{
					ret.x = DownVector[DownOffset + k];
					ret.y = DownVector[DownOffset + k] - k;
					return (ret);
				} // if
			} // if

		} // for k

	} // for D

	LgiAssert(!"the algorithm should never come here.");
	return ret;
}


/// <summary>
/// This is the divide-and-conquer implementation of the longes common-subsequence (LCS) 
/// algorithm.
/// The published algorithm passes recursively parts of the A and B sequences.
/// To avoid copying these arrays the lower and upper bounds are passed while the sequences stay constant.
/// </summary>
/// <param name="DataA">sequence A</param>
/// <param name="LowerA">lower bound of the actual range in DataA</param>
/// <param name="UpperA">upper bound of the actual range in DataA (exclusive)</param>
/// <param name="DataB">sequence B</param>
/// <param name="LowerB">lower bound of the actual range in DataB</param>
/// <param name="UpperB">upper bound of the actual range in DataB (exclusive)</param>
/// <param name="DownVector">a vector for the (0,0) to (x,y) search. Passed as a parameter for speed reasons.</param>
/// <param name="UpVector">a vector for the (u,v) to (N,M) search. Passed as a parameter for speed reasons.</param>
template<typename T>
void Diff::LCS(	DiffData<T> &DataA, int LowerA, int UpperA,
				DiffData<T> &DataB, int LowerB, int UpperB,
				GArray<int> &DownVector, GArray<int> &UpVector)
{
	// Debug.Write(2, "LCS", String.Format("Analyse the box: A[{0}-{1}] to B[{2}-{3}]", LowerA, UpperA, LowerB, UpperB));

	// Fast walkthrough equal lines at the start
	while (LowerA < UpperA && LowerB < UpperB && DataA.data[LowerA] == DataB.data[LowerB]) {
		LowerA++; LowerB++;
	}

	// Fast walkthrough equal lines at the end
	while (LowerA < UpperA && LowerB < UpperB && DataA.data[UpperA - 1] == DataB.data[UpperB - 1]) {
		--UpperA; --UpperB;
	}

	if (LowerA == UpperA) {
		// mark as inserted lines.
		while (LowerB < UpperB)
			DataB.modified[LowerB++] = true;

	}
	else if (LowerB == UpperB)
	{
		// mark as deleted lines.
		while (LowerA < UpperA)
			DataA.modified[LowerA++] = true;

	}
	else
	{
		// Find the middle snakea and length of an optimal path for A and B
		SMSRD smsrd = SMS(DataA, LowerA, UpperA, DataB, LowerB, UpperB, &DownVector[0], &UpVector[0]);
		// Debug.Write(2, "MiddleSnakeData", String.Format("{0},{1}", smsrd.x, smsrd.y));

		// The path is from LowerX to (x,y) and (x,y) to UpperX
		LCS(DataA, LowerA, smsrd.x, DataB, LowerB, smsrd.y, DownVector, UpVector);
		LCS(DataA, smsrd.x, UpperA, DataB, smsrd.y, UpperB, DownVector, UpVector);  // 2002.09.20: no need for 2 points 
	}
} // LCS()


/// <summary>Scan the tables of which lines are inserted and deleted,
/// producing an edit script in forward order.  
/// </summary>
/// dynamic array
template<typename T>
GArray<Diff::Item> Diff::CreateDiffs(DiffData<T> &DataA, DiffData<T> &DataB)
{
	// ArrayList a = new ArrayList();
	Item aItem;
	GArray<Item> a;

	int StartA, StartB;
	int LineA, LineB;

	LineA = 0;
	LineB = 0;
	while (LineA < DataA.Length() || LineB < DataB.Length())
	{
		if
		(
			(LineA < DataA.Length())
			&&
			(!DataA.modified[LineA])
			&&
			(LineB < DataB.Length())
			&&
			(!DataB.modified[LineB])
		)
		{
			// equal lines
			LineA++;
			LineB++;
		}
		else
		{
			// maybe deleted and/or inserted lines
			StartA = LineA;
			StartB = LineB;

			while (LineA < DataA.Length() && (LineB >= DataB.Length() || DataA.modified[LineA]))
				// while (LineA < DataA.Length && DataA.modified[LineA])
				LineA++;

			while (LineB < DataB.Length() && (LineA >= DataA.Length() || DataB.modified[LineB]))
				// while (LineB < DataB.Length && DataB.modified[LineB])
				LineB++;

			if ((StartA < LineA) || (StartB < LineB))
			{
				// store a new difference-item
				aItem.StartA = StartA;
				aItem.StartB = StartB;
				aItem.deletedA = LineA - StartA;
				aItem.insertedB = LineB - StartB;
				a.New() = aItem;
			} // if
		} // if
	} // while

	return a;
}


void DiffTest()
{
	Diff d;

	char *a = "a\nb\nc\nd\ne\nf\ng\nh\ni\nj\nk\nl";
	char *b = "a\nb\nc\nd\ne\nf\ng\nh\nweri\nk\nl";
	GArray<Diff::Item> result = d.DiffText(a, b, false, false, false);

	GArray<uint8> c, e;
	uint8 cdata[10] = { 34, 12, 56, 2, 67, 8, 33, 11, 99, 200 };
	c.Length(sizeof(cdata));
	memcpy(&c[0], cdata, c.Length());
	uint8 edata[11] = { 34, 12, 56, 2, 67, 8, 33, 94, 11, 99, 200 };
	e.Length(sizeof(edata));
	memcpy(&e[0], edata, e.Length());
	
	
	result = d.DiffInt(c, e);

	int asd=0;
}
