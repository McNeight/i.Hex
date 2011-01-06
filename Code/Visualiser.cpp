#include "Lgi.h"
#include "iHex.h"
#include "GList.h"
#include "GUtf8.h"
#include "resdefs.h"

int XCmp(char16 *w, char *utf, int Len = -1)
{
	int Status = -1;
	GUtf8Ptr a(utf);

	if (w AND a)
	{
		while (Len == -1 OR Len-- > 0)
		{
			#define Upper(c) ( ((c)>='a' AND (c)<='z') ? (c) - 'a' + 'A' : (c) )
			uint32 ca = a;
			Status = Upper(*w) - Upper(ca);
			if (Status)
				break;

			if (!a OR !*w)
				break;
			
			w++;
			a++;
		}
	}

	return Status;
}

int16 IfSwap(int16 i, bool Little)
{
	if (!Little)
		i = ((i & 0xff00) >> 8) | ((i & 0xff) << 8);
	return i;
}

uint16 IfSwap(uint16 i, bool Little)
{
	if (!Little)
		i = ((i & 0xff00) >> 8) | ((i & 0xff) << 8);
	return i;
}

int32 IfSwap(int32 i, bool Little)
{
	if (!Little)
		i = ((i & 0xff000000) >> 24) |
			((i & 0xff0000) >> 8) |
			((i & 0xff00) << 8) |
			((i & 0xff) << 24);
	return i;
}

uint32 IfSwap(uint32 i, bool Little)
{
	if (!Little)
		i = ((i & 0xff000000) >> 24) |
			((i & 0xff0000) >> 8) |
			((i & 0xff00) << 8) |
			((i & 0xff) << 24);
	return i;
}

int64 IfSwap(int64 i, bool Little)
{
	if (!Little)
	{
		uint8 *p = (uint8*)&i;
		return	((uint64)p[0]) |
				(((uint64)p[1]) << 8) |
				(((uint64)p[2]) << 16) |
				(((uint64)p[3]) << 24) |
				(((uint64)p[4]) << 32) |
				(((uint64)p[5]) << 40) |
				(((uint64)p[6]) << 48) |
				(((uint64)p[7]) << 56);
				

		/* Old
		i = ((i & 0xff00000000000000L) >> 56) |
			((i & 0xff000000000000L) >> 40) |
			((i & 0xff0000000000L) >> 24) |
			((i & 0xff00000000L) >> 8) |
			((i & 0xff000000) << 8) |
			((i & 0xff0000) << 24) |
			((i & 0xff00) << 40) |
			((i & 0xff) << 56);
		*/
	}
	return i;
}

uint64 IfSwap(uint64 i, bool Little)
{
	if (!Little)
	{
		uint8 *p = (uint8*)&i;
		return	((uint64)p[0]) |
				(((uint64)p[1]) << 8) |
				(((uint64)p[2]) << 16) |
				(((uint64)p[3]) << 24) |
				(((uint64)p[4]) << 32) |
				(((uint64)p[5]) << 40) |
				(((uint64)p[6]) << 48) |
				(((uint64)p[7]) << 56);

		/* Old
		i = ((i & 0xff00000000000000) >> 56) |
			((i & 0xff000000000000) >> 40) |
			((i & 0xff0000000000) >> 24) |
			((i & 0xff00000000) >> 8) |
			((i & 0xff000000) << 8) |
			((i & 0xff0000) << 24) |
			((i & 0xff00) << 40) |
			((i & 0xff) << 56);
		*/
	}
	
	return i;
}

enum BaseType
{
	TypeInt,
	TypeFloat,
	TypeChar,
	TypeStrZ,
};

struct Basic
{
	BaseType Type;
	uint8 Bytes;
	bool Signed;
	bool Array;
};

class StructDef;
class VarDefType
{
public:
	Basic *Base;
	StructDef *Cmplex;
	char *Pad;
	char *Length; // for arrays

	VarDefType()
	{
		Pad = 0;
		Base = 0;
		Cmplex = 0;
		Length = 0;
	}

	~VarDefType()
	{
		DeleteObj(Base);
		DeleteArray(Pad);
		DeleteArray(Length);
	}

	int Sizeof();
};

class VarDef
{
public:
	VarDefType *Type;
	char *Name;
	char *Value;
	bool Hidden;

	VarDef()
	{
		Name = 0;
		Value = 0;
		Hidden = false;
	}

	~VarDef()
	{
		DeleteArray(Name);
		DeleteArray(Value);
	}

	int Sizeof()
	{
		return Type ? Type->Sizeof() : 0;
	}

	bool HasValue(char *&Str)
	{
		if (Value AND
			Type AND
			Type->Base AND
			((Type->Base->Type == TypeChar) OR (Type->Base->Type == TypeStrZ)))
		{
			Str = Value;
			return true;
		}

		return false;
	}

	bool HasValue(int &Val)
	{
		if (Value AND
			Type AND
			Type->Base AND
			Type->Base->Type == TypeInt)
		{
			if (strnicmp(Value, "0x", 2) == 0)
			{
				Val = htoi(Value + 2);
			}
			else
			{
				Val = atoi(Value);
			}
			return true;
		}

		return false;
	}

	bool HasValue(float &Val)
	{
		if (Value)
		{
			Val = atof(Value);
			return true;
		}

		return false;
	}


	bool HasValue(double &Val)
	{
		if (Value)
		{
			Val = atof(Value);
			return true;
		}

		return false;
	}

	float CastFloat(char *Data, bool Little)
	{
		float f = 0;

		if (Type->Base)
		{
			switch (Type->Base->Type)
			{
				case TypeInt:
				{
					break;
				}
				case TypeFloat:
				{
					if (Type->Base->Bytes == 4)
					{
						f = *((float*)Data);
						break;
					}
					else if (Type->Base->Bytes == 8)
					{
						f = *((double*)Data);
					}
					break;
				}
				case TypeChar:
				case TypeStrZ:
				{
					f = atof(Data);
					break;
				}
			}
		}

		return f;
	}

	double CastDouble(char *Data, bool Little)
	{
		double f = 0;

		if (Type->Base)
		{
			switch (Type->Base->Type)
			{
				case TypeInt:
				{
					break;
				}
				case TypeFloat:
				{
					if (Type->Base->Bytes == 4)
					{
						f = *((float*)Data);
						break;
					}
					else if (Type->Base->Bytes == 8)
					{
						f = *((double*)Data);
					}
					break;
				}
				case TypeChar:
				case TypeStrZ:
				{
					f = atof(Data);
					break;
				}
			}
		}

		return f;
	}

	int64 CastInt(char *Data, bool Little)
	{
		int64 i = 0;

		if (Type->Base)
		{
			switch (Type->Base->Type)
			{
				case TypeInt:
				{
					switch (Type->Base->Bytes)
					{
						case 1:
						{
							if (Type->Base->Signed)
							{
								int8 *n = (int8*)Data;
								i = *n;
							}
							else
							{
								uint8 *n = (uint8*)Data;
								i = *n;
							}
							break;
						}
						case 2:
						{
							if (Type->Base->Signed)
							{
								int16 *n = (int16*)Data;
								i = IfSwap(*n, Little);
							}
							else
							{
								uint16 *n = (uint16*)Data;
								i = IfSwap(*n, Little);
							}
							break;
						}
						case 4:
						{
							if (Type->Base->Signed)
							{
								int32 *n = (int32*)Data;
								i = IfSwap(*n, Little);
							}
							else
							{
								uint32 *n = (uint32*)Data;
								i = IfSwap(*n, Little);
							}
							break;
						}
						case 8:
						{
							if (Type->Base->Signed)
							{
								int64 *n = (int64*)Data;
								i = IfSwap(*n, Little);
							}
							else
							{
								uint64 *n = (uint64*)Data;
								i = IfSwap(*n, Little);
							}
							break;
						}
					}
					break;
				}
				case TypeFloat:
				{
					switch (Type->Base->Bytes)
					{
						case 4:
						{
							float *n = (float*)Data;
							i = *n;
							break;
						}
						case 8:
						{
							double *n = (double*)Data;
							i = *n;
							break;
						}
					}
					break;
				}
				case TypeChar:
				case TypeStrZ:
				{
					i = atoi(Data);
					break;
				}
			}
		}

		return i;
	}
};

class StructDef
{
public:
	char *Name;
	char *Base;
	GArray<VarDef*> Vars;
	GArray<StructDef*> Children;

	StructDef()
	{
		Name = 0;
		Base = 0;
	}

	~StructDef()
	{
		DeleteArray(Name);
		DeleteArray(Base);
	}

	int Sizeof()
	{
		int Size = 0;
		for (int i=0; i<Vars.Length(); i++)
		{
			Size = Vars[i]->Sizeof();
		}
		return Size;
	}

	bool Match(char *&Data, int &Len, bool Little)
	{
		bool Status = false;

		if (Data AND Len > 0)
		{
			for (int i=0; i<Vars.Length(); i++)
			{
				VarDef *d = Vars[i];
				if (d->Type->Base)
				{
					if (d->Type AND
						d->Type->Base)
					{
						if ((d->Type->Base->Type == TypeChar) OR (d->Type->Base->Type == TypeStrZ))
						{
							char *Str = 0;
							if (d->HasValue(Str))
							{
								if (Str AND strnicmp(Data, Str, strlen(Str)) != 0)
								{
									return false;
								}
								else Status = true;
							}
						}
						else if (d->Type->Base->Type == TypeInt)
						{
							int Val = 0;
							if (d->HasValue(Val))
							{
								if (d->CastInt(Data, Little) != Val)
								{
									return false;
								}
								else Status = true;
							}
						}
						else if (d->Type->Base->Type == TypeFloat)
						{
							float Val = 0;
							if (d->HasValue(Val))
							{
								if (d->CastFloat(Data, Little) != Val)
								{
									return false;
								}
								else Status = true;
							}
						}
					}

					int Length = d->Type->Length ? atoi(d->Type->Length) : 1;
					if (Length == 0)
					{
						break;
					}

					Data += d->Type->Base->Bytes * Length;
					Len -= d->Type->Base->Bytes * Length;
				}
				else if (d->Type->Cmplex)
				{
					Status |= d->Type->Cmplex->Match(Data, Len, Little);
				}
				else
				{
					LgiAssert(0);
				}
			}
		}

		return Status;
	}
};

int VarDefType::Sizeof()
{
	if (Base)
	{
		return Base->Bytes;
	}
	else if (Cmplex)
	{
		return Cmplex->Sizeof();
	}
	
	LgiAssert(!"This should never happen right?");
	return 0;
}

class StructureMap : public GListItem, public GDom
{
	AppWnd *App;
	char *File;
	char *Body;
	GStringPipe Errs;

	bool Little;
	GArray<char*> Addr;
	StructDef *CurStructDef;
	int ProcessedElements;

public:
	GArray<StructDef*> Compiled;

	StructDef *GetStruct(char *Name)
	{
		if (Name)
		{
			for (int i=0; i<Compiled.Length(); i++)
			{
				if (Compiled[i]->Name AND
					stricmp(Compiled[i]->Name, Name) == 0)
				{
					return Compiled[i];
				}
			}
		}
		return 0;
	}

	StructureMap(AppWnd *app, char *file = 0)
	{
		CurStructDef = 0;
		ProcessedElements = 0;
		App = app;
		File = 0;
		Body = 0;
		SetFile(file);
	}

	~StructureMap()
	{
		DeleteArray(File);
		DeleteArray(Body);
		Compiled.DeleteObjects();
	}

	bool GetVariant(char *Name, GVariant &Value, char *Array = 0)
	{
		if (!Name)
			return false;

		VarDef *Ref = 0;
		int n = 0;
		for (; n<ProcessedElements; n++)
		{
			if (CurStructDef->Vars[n]->Name AND
				stricmp(CurStructDef->Vars[n]->Name, Name) == 0)
			{
				Ref = CurStructDef->Vars[n];
				break;
			}
		}
		if (Ref)
		{
			// Resolve to integer
			Value = Ref->CastInt(Addr[n], Little);
			return true;
		}

		return false;
	}

	bool DoStruct(StructDef *s, char *Base, char *&Data, int &Len, GStream &Out, bool little, int Depth = 0)
	{
		char Tabs[256];

		ZeroObj(Tabs);
		memset(Tabs, ' ', Depth * 2);
		CurStructDef = s;
		Addr.Length(0);
		Little = little;

		for (int i=0; i<s->Vars.Length() AND Len > 0; i++)
		{
			VarDef *d = s->Vars[i];

			int Length = 1;
			if (d->Type->Length)
			{
				// Resolve the array length of this var
				if (strlen(d->Type->Length) == 0)
				{
					Length = -1;
				}
				else
				{
					// Evaluate expression
					GScriptEngine1 e(App, App);
					GVariant v;
					ProcessedElements = i;
					if (e.EvaluateExpression(&v, this, d->Type->Length))
					{
						Length = v.CastInt32();

						if (Length < 0)
							Length = 0;

						int Sizeof = d->Sizeof();
						if (Length * Sizeof > Len)
							Length = Len / Sizeof;
					}
					else
					{
						// Fall back code: this should never run.... but it's useful to 
						// have around as a reference.
						if (strnicmp(d->Type->Length, "0x", 2) == 0)
						{
							// Hex
							Length = htoi(d->Type->Length + 2);
						}
						else if (isdigit(*d->Type->Length))
						{
							// Int
							Length = atoi(d->Type->Length);
						}
						else if (isalpha(*d->Type->Length))
						{
							int Pad = 0;
							if (strnicmp(d->Type->Length, "Pad", 3) == 0 AND
								(Pad = atoi(d->Type->Length + 3)) > 0)
							{
								// Padding
								if (Pad % 8 == 0)
								{
									int Bytes = Pad / 8;
									int Off = Data - Base;
									if (Off % Bytes)
									{
										int Inc = Bytes - (Off % Bytes);
										if (Len >= Inc)
										{
											Out.Print("%sPadding: %i byte(s)\n", Tabs, Inc);
											Data += Inc;
											Len -= Inc;
											return true;
										}
										else
										{
											return false;
										}
									}
								}
								else
								{
									Out.Print("%s Error: invalid pad value '%i'\n", Tabs, Pad);
								}
							}
							else
							{
								// Variable
								VarDef *Ref = 0;
								int n=0;
								for (; n<i; n++)
								{
									if (s->Vars[n]->Name AND
										stricmp(s->Vars[n]->Name, d->Type->Length) == 0)
									{
										Ref = s->Vars[n];
										break;
									}
								}
								if (Ref)
								{
									// Resolve to integer
									Length = Ref->CastInt(Addr[n], Little);
								}
							}
						}
					}
				}
			}

			if (Length == 0)
				continue;

			if (d->Type->Base)
			{
				Addr[i] = Data;

				Basic *b = d->Type->Base;
				switch (b->Type)
				{
					case TypeInt:
					{
						if (!d->Hidden)
						{
							Out.Print("%s%s", Tabs, d->Name);
							if (Length > 1)
								Out.Print(":\n");
							else
								Out.Print(" = ");
						}
						
						for (int n=0; n<Length AND Len >= b->Bytes; n++)
						{
							if (!d->Hidden)
							{
								if (Length > 1)
									Out.Print("\t%s[%i] = ", Tabs, n);
								char *LeadIn = Length > 1 ? Tabs : (char*)"";

								switch (b->Bytes)
								{
									case 1:
									{
										if (b->Signed)
											Out.Print("%s%i (0x%02.2x)\n", LeadIn, *Data, *Data);
										else
											Out.Print("%s%u (0x%02.2x)\n", LeadIn, (uint8)*Data, (uint8)*Data);
										break;
									}
									case 2:
									{
										if (b->Signed)
										{
											int16 n = IfSwap(*((int16*)Data), Little);
											Out.Print("%s%i (0x%04.4x)\n", LeadIn, n, n);
										}
										else
										{
											uint16 n = IfSwap(*((uint16*)Data), Little);
											Out.Print("%s%u (0x%04.4x)\n", LeadIn, n, n);
										}
										break;
									}
									case 4:
									{
										if (b->Signed)
										{
											int32 n = IfSwap(*((int32*)Data), Little);
											Out.Print("%s%i (0x%08.8x)\n", LeadIn, n, n);
										}
										else
										{
											uint32 n = IfSwap(*((uint32*)Data), Little);
											Out.Print("%s%u (0x%08.8x)\n", LeadIn, n, n);
										}
										break;
									}
									case 8:
									{
										if (b->Signed)
										{
											int64 n = IfSwap(*((int64*)Data), Little);
											#ifdef WIN32
											Out.Print("%s%I64i (0x%16.16lI64x)\n", LeadIn, n, n);
											#else
											Out.Print("%s%li (0x%16.16lx)\n", LeadIn, n, n);
											#endif
										}
										else
										{
											uint64 n = IfSwap(*((uint64*)Data), Little);
											#ifdef WIN32
											Out.Print("%s%I64u (0x%16.16lI64x)\n", LeadIn, n, n);
											#else
											Out.Print("%s%lu (0x%16.16lx)\n", LeadIn, n, n);
											#endif
										}
										break;
									}
								}
							}

							int Val = 0;
							if (d->HasValue(Val))
							{
								if (d->CastInt(Data, Little) != Val)
								{
									Out.Print("%sValue Mismatch!\n", Tabs);
									return false;
								}
							}

							Data += b->Bytes;
							Len -= b->Bytes;
						}
						break;
					}
					case TypeFloat:
					{
						if (!d->Hidden)
						{
							Out.Print("%s%s", Tabs, d->Name);
							if (Length > 1)
								Out.Print(":\n");
							else
								Out.Print(" = ");
						}
						
						for (int n=0; n<Length AND Len >= b->Bytes; n++)
						{
							if (!d->Hidden)
							{
								if (Length > 1)
									Out.Print("\t%s[%i] = ", Tabs, n);
								char *LeadIn = Length > 1 ? Tabs : (char*)"";

								switch (b->Bytes)
								{
									case 4:
									{
										LgiAssert(sizeof(float) == 4);
										
										float flt = *((float*)Data);
										#define Swap(a, b) { uint8 t = a; a = b; b = t; }
										
										if (!little)
										{
											uint8 *c = (uint8*)&flt;
											Swap(c[0], c[3]);
											Swap(c[1], c[2]);
										}

										double dbl = flt;

										Out.Print("%s%g\n", LeadIn, dbl);

										float Val = 0;
										if (d->HasValue(Val))
										{
											if (d->CastFloat(Data, Little) != Val)
											{
												Out.Print("%sValue Mismatch!\n", Tabs);
												return false;
											}
										}
										break;
									}
									case 8:
									{
										LgiAssert(sizeof(double) == 8);

										double dbl = *((double*)Data);
	
										if (!little)
										{
											uint8 *c = (uint8*)&dbl;
											Swap(c[0], c[7]);
											Swap(c[1], c[6]);
											Swap(c[2], c[5]);
											Swap(c[3], c[4]);
										}

										Out.Print("%s%g\n", LeadIn, dbl);

										double Val = 0;
										if (d->HasValue(Val))
										{
											if (d->CastDouble(Data, Little) != Val)
											{
												Out.Print("%sValue Mismatch!\n", Tabs);
												return false;
											}
										}
										break;
									}
									default:
									{
										Out.Print("#error (%s:%i)\n", LeadIn, __FILE__, __LINE__);
										break;										
									}
								}
							}

							Data += b->Bytes;
							Len -= b->Bytes;
						}
						break;
					}
					case TypeChar:
					{
						if (!d->Hidden AND Length < 1024)
						{
							if (d->Type->Base->Bytes == 1)
							{
								char *u = (char*) LgiNewConvertCp("utf-8", Data, "iso-8859-1", Length);
								Out.Print("%s%s = '%s'\n", Tabs, d->Name, u);
								if (u AND d->Value)
								{
									if (strnicmp(u, d->Value, Length) != 0)
									{
										Out.Print("%sValue Mismatch!\n", Tabs);
										DeleteArray(u);
										return false;
									}
								}
								DeleteArray(u);
							}
							else if (d->Type->Base->Bytes == 2)
							{
								char16 *w = new char16[Length+1];
								char *u = 0;
								if (w)
								{
									char16 *Src = (char16*)Data;
									for (int i=0; i<Length; i++)
									{
										w[i] = IfSwap(Src[i], Little);
									}
									w[Length] = 0;

									u = LgiNewUtf16To8(w);
								}
								DeleteArray(w);

								Out.Print("%s%s = '%s'\n", Tabs, d->Name, u);
								if (d->Value)
								{
									if (strnicmp(u, d->Value, Length) != 0)
									{
										Out.Print("%sValue Mismatch!\n", Tabs);
										DeleteArray(u);
										return false;
									}
								}
								DeleteArray(u);
							}
						}

						Data += Length * d->Type->Base->Bytes;
						Len -= Length * d->Type->Base->Bytes;
						break;
					}
					case TypeStrZ:
					{
						if (!d->Hidden AND (d->Type->Base->Bytes < 8))
						{
							Out.Print("%s%s", Tabs, d->Name);
							if (Length > 1)
								Out.Print(":\n");
							else
								Out.Print(" = ");
						}
						for (int n=0; n<Length AND Len >= d->Type->Base->Bytes; n++)
						{
							if (!d->Hidden AND (d->Type->Base->Bytes < 8))
							{
								if (Length > 1)
									Out.Print("\t%s[%i] = ", Tabs, n);
							}
							int zstringLen = 0;
							if (d->Type->Base->Bytes == 1)
							{
								while (*(Data + zstringLen))
								{
									zstringLen++;
								}
								char *u = (char*) LgiNewConvertCp("utf-8", Data, "iso-8859-1", zstringLen);
								if (!d->Hidden)
								{
									Out.Print("'%s'\n", u);
									if (u AND d->Value)
									{
										if (strnicmp(u, d->Value, zstringLen) != 0)
										{
											Out.Print("%sValue Mismatch!\n", Tabs);
											DeleteArray(u);
											return false;
										}
									}
								}
								DeleteArray(u);
							}
							else if (d->Type->Base->Bytes == 2)
							{
								while ((char16)*(Data + zstringLen * d->Type->Base->Bytes))
								{
									zstringLen++;
								}
								char16 *w = new char16[zstringLen+1];
								char *u = 0;
								if (w)
								{
									char16 *Src = (char16*)Data;
									for (int i=0; i<zstringLen; i++)
									{
										w[i] = IfSwap(Src[i], Little);
									}
									w[zstringLen] = 0;

									u = LgiNewUtf16To8(w);
								}
								DeleteArray(w);

								if (!d->Hidden)
								{
									Out.Print("'%s'\n", u);
									if (d->Value)
									{
										if (strnicmp(u, d->Value, zstringLen) != 0)
										{
											Out.Print("%sValue Mismatch!\n", Tabs);
											DeleteArray(u);
											return false;
										}
									}
								}
								DeleteArray(u);
							}
							else if (d->Type->Base->Bytes == 8)
							{
								// Just skip passed the string
								while ((uint64)*(Data + zstringLen * d->Type->Base->Bytes))
								{
									zstringLen++;
								}
							}
							
							zstringLen += 1; // Take null terminator into account
							Data += zstringLen * d->Type->Base->Bytes;
							Len -= zstringLen * d->Type->Base->Bytes;
						}
						break;
					}
				}
			}
			else if (d->Type->Cmplex)
			{
				Out.Print("%s%s (@ %i/0x%x) =\n", Tabs, d->Name, Data-Base, Data-Base);
				if (Length == 1)
					Out.Print("%s{\n", Tabs);
				
				for (int i=0; (Length < 0 OR i < Length) AND Len > 0; i++)
				{
					StructDef *s = d->Type->Cmplex;
					if (s->Children.Length() > 0)
					{
						// Check to see if any child struct specializations match
						for (int c=0; c<s->Children.Length(); c++)
						{
							char *d = Data;
							int l = Len;
							if (s->Children[c]->Match(d, l, Little))
							{
								s = s->Children[c];
								break;
							}
						}
					}

					if (Length != 1)
						Out.Print("%s  [%i] (%s @ %i/0x%x)\n", Tabs, i, s->Name, Data-Base, Data-Base);

					if (!DoStruct(s, Base, Data, Len, Out, Little, Depth + 1 + (Length != 1 ? 1 : 0)))
					{
						return false;
					}
				}
				
				if (Length == 1)
					Out.Print("%s}\n", Tabs);
			}
		}

		return true;
	}

	void Visualise(char *Data, int Len, GStream &Out, bool Little)
	{
		StructDef *Main = GetStruct("Main");
		if (Main)
		{
			char *d = Data;
			int l = Len;
			DoStruct(Main, d, d, l, Out, Little);
		}
		else
		{
			Out.Print("No main defined.");
		}
	}

	char *GetFile()
	{
		return File;
	}

	void SetFile(char *NewName)
	{
		char *f = NewStr(NewName);
		char *d = f ? strrchr(f, DIR_CHAR) : 0;
		if (f)
		{
			char s[256];
			strsafecpy(s, d + 1, sizeof(s));
			d = strrchr(s, '.');
			if (d) *d = 0;
			SetText(s);

			if (File)
			{
				FileDev->Move(File, f);
				DeleteArray(File);
			}

			File = f;
		}
	}
	
	char *GetBody()
	{
		if (!Body AND File)
		{
			Body = ReadTextFile(File);
		}

		return Body;
	}
	
	void SetBody(char *s)
	{
		char *b = NewStr(s);
		DeleteArray(Body);
		Body = b;

		if (File)
		{
			GFile f;
			if (f.Open(File, O_WRITE))
			{
				f.SetSize(0);
				if (Body)
					f.Write(Body, strlen(Body));
			}
		}
	}

	void Err(char16 *Cur, char16 *Start, char *Msg)
	{
		int Line = 1;
		while (Cur > Start)
		{
			if (*Cur == '\n')
				Line++;
			Cur--;
		}

		char *d = File ? strrchr(File, DIR_CHAR) : 0;
		Errs.Print("%s:%i - %s\n", d ? d + 1 : File, Line, Msg);
	}

	char *To8(char16 *w)
	{
		char *u = LgiNewUtf16To8(w);
		DeleteArray(w);
		return u;
	}

	char *GetErrors()
	{
		return Errs.NewStr();
	}

	VarDefType *ParseType(char16 *t)
	{
		VarDefType *v = 0;

		if (XCmp(t, "int", 3) == 0)
		{
			v = new VarDefType;
			v->Base = new Basic;
			v->Base->Array = false;
			v->Base->Signed = true;
			v->Base->Type = TypeInt;
			if (XCmp(t+3, "8") == 0)
				v->Base->Bytes = 1;
			else if (XCmp(t+3, "16") == 0)
				v->Base->Bytes = 2;
			else if (XCmp(t+3, "64") == 0)
				v->Base->Bytes = 8;
			else
				v->Base->Bytes = 4;
		}
		else if (XCmp(t, "uint", 4) == 0)
		{
			v = new VarDefType;
			v->Base = new Basic;
			v->Base->Array = false;
			v->Base->Signed = false;
			v->Base->Type = TypeInt;
			if (XCmp(t+4, "8") == 0)
				v->Base->Bytes = 1;
			else if (XCmp(t+4, "16") == 0)
				v->Base->Bytes = 2;
			else if (XCmp(t+4, "64") == 0)
				v->Base->Bytes = 8;
			else
				v->Base->Bytes = 4;
		}
		else if (XCmp(t, "float", 5) == 0)
		{
			v = new VarDefType;
			v->Base = new Basic;
			v->Base->Array = false;
			v->Base->Signed = true;
			v->Base->Type = TypeFloat;
			v->Base->Bytes = 4;
		}
		else if (XCmp(t, "double", 6) == 0)
		{
			v = new VarDefType;
			v->Base = new Basic;
			v->Base->Array = false;
			v->Base->Signed = true;
			v->Base->Type = TypeFloat;
			v->Base->Bytes = 8;
		}
		else if (XCmp(t, "char", 4) == 0)
		{
			v = new VarDefType;
			v->Base = new Basic;
			v->Base->Array = false;
			v->Base->Signed = false;
			v->Base->Type = TypeChar;
			if (XCmp(t+4, "16") == 0)
				v->Base->Bytes = 2;
			else if (XCmp(t+4, "64") == 0)
				v->Base->Bytes = 8;
			else
				v->Base->Bytes = 1;
		}
		else if (XCmp(t, "strz", 4) == 0)
		{
			v = new VarDefType;
			v->Base = new Basic;
			v->Base->Array = false;
			v->Base->Signed = false;
			v->Base->Type = TypeStrZ;
			if (XCmp(t+4, "16") == 0)
				v->Base->Bytes = 2;
			else if (XCmp(t+4, "64") == 0)
				v->Base->Bytes = 8;
			else
				v->Base->Bytes = 1;
		}
		else
		{
			char *u = LgiNewUtf16To8(t);
			if (u)
			{
				StructDef *s = GetStruct(u);
				if (s)
				{
					v = new VarDefType;
					v->Cmplex = s;
				}
				DeleteArray(u);
			}			
		}

		return v;
	}

	bool Compile()
	{
		bool Error = false;

		Compiled.DeleteObjects();
		GArray<char16*> Mem;

		char16 *b = LgiNewUtf8To16(GetBody());
		if (b)
		{
			#define NextTok() \
				Mem.Add(t = LexCpp(s))
			
			#ifndef __GNUC__
			#define CheckTok(lit) \
				if (!(t AND StricmpW(t, L##lit) == 0)) \
				{ \
					char m[256], *u = LgiNewUtf16To8(t); \
					sprintf(m, "expecting '" ##lit "', got '%s'", u); \
					Err(s, b, m); \
					DeleteArray(u) \
					Error = true; \
					break; \
				}
			#else
			#define CheckTok(lit) \
				if (!(t AND XCmp(t, lit) == 0)) \
				{ \
					char m[256], *u = LgiNewUtf16To8(t); \
					sprintf(m, "expecting '%s', got '%s'", lit, u); \
					Err(s, b, m); \
					DeleteArray(u) \
					Error = true; \
					break; \
				}
			#endif
			#define Literal(lit) \
				NextTok(); \
				CheckTok(lit);

			char16 *t = 0, *s = b;
			while (!Error)
			{
				Mem.DeleteArrays();
				NextTok();
				if (t)
				{
					CheckTok("struct");
				}
				else break;

				// Parse struct def
				StructDef *Def = new StructDef;
				if (Def)
				{
					Def->Name = To8(LexCpp(s));

					NextTok();
					if (XCmp(t, ":") == 0)
					{
						Literal("inherits");
						Def->Base = To8(LexCpp(s));
						NextTok();
					}
					CheckTok("{");

					while (!Error)
					{
						// Parse types...
						NextTok();
						DoType:

						bool IsHidden = false;
						if (t AND XCmp(t, "hidden") == 0)
						{
							IsHidden = true;
							NextTok();
						}

						VarDefType *Type = ParseType(t);
						if (Type)
						{
							VarDef *Var = new VarDef;
							Var->Type = Type;
							Var->Hidden = IsHidden;
							Var->Name = To8(LexCpp(s));
							if (Var->Name)
							{
								Def->Vars.Add(Var);
								
								NextTok();
								if (t)
								{
									if (XCmp(t, "[") == 0)
									{
										// Array
										int Depth = 0;
										GStringPipe p;
										while (true)
										{
											NextTok();
											if (t)
											{
												if (XCmp(t, "[") == 0)
												{
													p.Push(t);
													Depth++;
													NextTok();
												}
												else if (XCmp(t, "]") == 0)
												{
													if (Depth)
													{
														p.Push(t);
														Depth--;
														NextTok();
													}
													else
													{
														NextTok();
														break;
													}
												}
												else
												{
													p.Push(t);
												}
											}
											else
											{
												Err(s, b, "expected token");
												break;
											}
										}

										char16 *Len = p.NewStrW();
										Var->Type->Length = LgiNewUtf16To8(Len);
										DeleteArray(Len);
										if (!Var->Type->Length)
											Var->Type->Length = NewStr("");
									}

									if (t AND XCmp(t, "=") == 0)
									{
										// Constraint
										Var->Value = TrimStr(To8(LexCpp(s)), "\"\'");
										NextTok();
									}
									
									if (t AND XCmp(t, ";") == 0)
									{
									}
									else
									{
										Err(s, b, "expected ';'");
										break;
									}
								}	
								else
								{
									Err(s, b, "expected ';'");
									break;
								}							
							}
							else
							{
								Err(s, b, "missing variable name");
								break;
							}
						}
						else
						{
							char m[256], *u = LgiNewUtf16To8(t);
							sprintf(m, "expected type, got '%s' instead", u);
							Err(s, b, m);
							DeleteArray(u);
							Error = true;
							break;
						}

						NextTok();
						if (t)
						{
							if (XCmp(t, "}") == 0)
							{
								Literal(";");
								break;
							}
							else
							{
								goto DoType;
							}
						}
						else break;
					}

					if (Def->Base)
					{
						StructDef *Parent = GetStruct(Def->Base);
						if (Parent)
						{
							Parent->Children.Add(Def);
						}
						else
						{
							Err(s, b, "parent class not defined.");
						}
					}
					else
					{
						Compiled.Add(Def);
					}
				}
			}
			
			Mem.DeleteArrays();
			DeleteArray(b);
		}

		return !Error;
	}
};

class MapEditor : public GWindow, public GLgiRes
{
	AppWnd *App;
	StructureMap *Map;
	char *Basepath;
	GList *Lst;
	GTextView3 *Txt;

public:
	MapEditor(AppWnd *app, StructureMap *map, char *basepath, GList *lst)
	{
		App = app;
		Basepath = basepath;
		Lst = lst;
		Map = map;
		Txt = 0;

		GRect r(0, 0, 700, 600);
		SetPos(r);
		Name("Map Editor");
		if (Attach(0))
		{
			MoveToCenter();
			LoadFromResource(IDD_MAP_EDIT, this);
			Txt = dynamic_cast<GTextView3*>(FindControl(IDC_TEXT));
			if (Txt)
			{
				Txt->Sunken(true);
				OnPosChange();
			}
			AttachChildren();

			if (Map)
			{
				SetCtrlName(IDC_NAME, Map->GetText(0));
				SetCtrlName(IDC_TEXT, Map->GetBody());
			}

			Visible(true);
		}
	}

	void OnPosChange()
	{
		if (Txt)
		{
			GRect c = GetClient();
			GRect t = Txt->GetPos();
			t.x2 = c.x2 - 7;
			t.y2 = c.y2 - 7;
			Txt->SetPos(t);
		}
	}

	int OnNotify(GViewI *v, int f)
	{
		switch (v->GetId())
		{
			case IDC_VISUALISER_HELP:
			{
				App->Help("Visual.html");
				break;
			}
			case IDOK:
			{
				if (ValidStr(GetCtrlName(IDC_NAME)))
				{
					if (Basepath)
					{
						char p[300];
						LgiMakePath(p, sizeof(p), Basepath, GetCtrlName(IDC_NAME));
						char *Ext = LgiGetExtension(p);
						if (!Ext)
							strcat(p, ".map");
						if (Map)
						{
							Map->SetFile(p);
						}
						else
						{
							Map = new StructureMap(App, p);
						}
						if (Map AND Lst)
						{
							Lst->Insert(Map);
						}
					}
					if (Map AND Txt)
					{
						Map->SetBody(Txt->Name());
						Map->Compile();
					}
				}
				else
				{
					LgiMsg(this, "Set the structure name.", AppName);
				}

				Quit();
				break;
			}
		}

		return 0;
	}
};

class GMapWnd : public GLayout
{
public:
	GToolBar *Cmds;
	GList *Lst;

	GMapWnd()
	{
		SetPourLargest(true);
		Children.Insert(Cmds = LgiLoadToolbar(this, "MapCmds.gif", 16, 16));
		Children.Insert(Lst = new GList(IDC_LIST, 0, 0, 100, 100));
		Cmds->AppendButton("New", IDM_NEW, TBT_PUSH);
		Cmds->AppendButton("Delete", IDM_DELETE, TBT_PUSH);
		Cmds->AppendButton("Compile", IDM_COMPILE, TBT_PUSH);
		Cmds->AppendButton("Lock Content", IDM_LOCK, TBT_TOGGLE);
		Cmds->Raised(false);
		Lst->SetPourLargest(true);
		Lst->AddColumn("Structure Maps", 300);
	}

	void OnCreate()
	{
		AttachChildren();
	}

	bool Pour(GRegion &r)
	{
		GLayout::Pour(r);
		
		GRegion c(0, 0, X()-1, Y()-1);
		Cmds->Pour(c);
		GRect n(0, Cmds->GetPos().y2 + 1, X()-1, Y()-1);
		Lst->SetPos(n);
		return true;
	}

	void OnPaint(GSurface *pDC)
	{
		pDC->Colour(LC_MED, 24);
		pDC->Rectangle();
	}
};

//////////////////////////////////////////////////////////////////////////////////
GVisualiseView::GVisualiseView(AppWnd *app)
{
	App = 0;
	Value(150);
	IsVertical(false);
	Raised(false);
	Border(false);
	SetViewA(Map = new GMapWnd, false);
	SetViewB(Txt = new GTextView3(80, 0, 0, 100, 100), true);
	
	if (LgiGetExePath(Base, sizeof(Base)))
	{
		#ifdef WIN32
		if (stristr(Base, "\\Release") OR stristr(Base, "\\Debug"))
			LgiTrimDir(Base);
		#endif

		GArray<char*> Files, Ext;
		Ext.Add("*.map");
		if (LgiRecursiveFileSearch(Base, &Ext, &Files))
		{
			for (int i=0; i<Files.Length(); i++)
			{
				Map->Lst->Insert(new StructureMap(App, Files[i]));
			}
		}
	}
}

int GVisualiseView::OnNotify(GViewI *c, int f)
{
	switch (c->GetId())
	{
		case IDC_LIST:
		{
			if (f == GLIST_NOTIFY_DBL_CLICK)
			{
				StructureMap *s = dynamic_cast<StructureMap*>(Map->Lst->GetSelected());
				if (s)
				{
					new MapEditor(App, s, Base, 0);
				}
			}
			break;
		}
		case IDM_NEW:
		{
			new MapEditor(App, 0, Base, Map->Lst);
			break;
		}
		case IDM_DELETE:
		{
			List<GListItem> Sel;
			if (Map->Lst AND Map->Lst->GetSelection(Sel))
			{
				for (GListItem *i=Sel.First(); i; i=Sel.Next())
				{
					StructureMap *m = dynamic_cast<StructureMap*>(i);
					if (m)
					{
						FileDev->Delete(m->GetFile());
						DeleteObj(m);
					}
				}
			}
			break;
		}
		case IDM_COMPILE:
		{
			List<GListItem> Sel;
			if (Map->Lst AND Map->Lst->GetSelection(Sel))
			{
				for (GListItem *i=Sel.First(); i; i=Sel.Next())
				{
					StructureMap *m = dynamic_cast<StructureMap*>(i);
					if (m)
					{
						if (m->Compile())
						{
							Txt->Name("Compile OK.");
						}
						else
						{
							char *Err = m->GetErrors();
							if (Err)
							{
								Txt->Name(Err);
								DeleteArray(Err);
							}
						}
					}
				}
			}
			break;
		}
	}

	return 0;
}

void GVisualiseView::Visualise(char *Data, int Len, bool Little)
{
	if (!GetCtrlValue(IDM_LOCK))
	{
		StructureMap *m = 0;
		if (Map->Lst)
		{
			m = dynamic_cast<StructureMap*>(Map->Lst->GetSelected());
		}

		if (m)
		{
			if (m->Compiled.Length() == 0)
			{
				if (!m->Compile())
				{
					char *e = m->GetErrors();
					if (e)
					{
						Txt->Name(e);
						DeleteArray(e);
					}
					return;
				}
			}

			if (m->Compiled.Length())
			{
				GStringPipe p(4 << 10);
				m->Visualise(Data, Len, p, Little);
				char *s = p.NewStr();
				if (s)
				{
					Txt->Name(s);
					DeleteArray(s);
				}
			}
		}
	}
}

