#include "Lgi.h"
#include "GToken.h"
#include "iHex.h"
#include "GList.h"
#include "GUtf8.h"
#include "GLexCpp.h"
#include "resdefs.h"

int XCmp(char16 *w, const char *utf, int Len = -1)
{
	int Status = -1;
	GUtf8Ptr a((char*)utf);

	if (w && a)
	{
		while (Len == -1 || Len-- > 0)
		{
			#define Upper(c) ( ((c)>='a' && (c)<='z') ? (c) - 'a' + 'A' : (c) )
			uint32 ca = a;
			Status = Upper(*w) - Upper(ca);
			if (Status)
				break;

			if (!a || !*w)
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
	}
	
	return i;
}

enum BaseType
{
	TypeInt,
	TypeFloat,
	TypeChar,
	TypeStrZ,
	TypeNibble,
};

struct Basic
{
	BaseType Type;
	uint8 Bytes;
	bool Signed;
	bool Array;
};

struct ArrayDimension
{
	GArray<char16*> Expression;
	
	~ArrayDimension()
	{
		Expression.DeleteArrays();
	}
	
	ArrayDimension &operator =(const ArrayDimension &a)
	{
		Expression.DeleteArrays();
		for (unsigned i=0; i<Expression.Length(); i++)
		{
			char16 *s = a.Expression.ItemAt(i);
			Expression.Add(NewStrW(s));
		}
		return *this;
	}
};

class StructDef;
class VarDefType
{
public:
	Basic *Base;
	StructDef *Cmplex;
	char *Pad;
	GArray<ArrayDimension> Length; // for arrays

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
	}

	int Sizeof();
};

uint64 DeNibble(char *ptr, int size)
{
	uint64 n = 0;
	int bytes = size >> 1;
	for (int i=0; i<bytes; i++)
	{
		int pos = i << 1;
		n <<= 8;
		n |= (ptr[pos] & 0xf);
		n |= (ptr[pos+1] & 0xf) << 4;
	}
	return n;
}

struct ConditionDef;
struct VarDef;
struct Member
{
	enum MemberType
	{
		MemberNone,
		MemberCondition,
		MemberVar
	}	Type;
	
	Member(MemberType t) : Type(t)
	{
	}
	
	virtual ~Member()
	{
	}
	
	virtual int64 Sizeof() = 0;
	virtual VarDef *IsVar() = 0;
	virtual ConditionDef *IsCondition() = 0;
};

struct ConditionDef : public Member
{
	bool Eval;
	GArray<char16*> Expression;
	GArray<Member*> Members;
	
	ConditionDef() : Member(MemberCondition)
	{
		Eval = true;
	}
	
	~ConditionDef()
	{
		Expression.DeleteArrays();
	}

	VarDef *IsVar() { return NULL; }
	ConditionDef *IsCondition() { return this; }
	int64 Sizeof()
	{
		if (!Eval)
			return 0;
			
		int64 Sz = 0;
		for (unsigned i=0; i<Members.Length(); i++)
		{
			Sz += Members[i]->Sizeof();
		}
		return Sz;
	}
};

struct VarDef : public Member
{
	VarDefType *Type;
	char *Name;
	GVariant Value;
	int Bits;
	bool Hidden;

	VarDef() : Member(MemberVar)
	{
		Name = 0;
		Hidden = false;
		Bits = 0;
	}

	~VarDef()
	{
		DeleteArray(Name);
	}

	VarDef *IsVar() { return this; }
	ConditionDef *IsCondition() { return NULL; }
	int64 Sizeof()
	{
		return Type ? Type->Sizeof() : 0;
	}

	bool HasValue(char *&Str)
	{
		if (Value.Type != GV_NULL &&
			Type &&
			Type->Base &&
			((Type->Base->Type == TypeChar) || (Type->Base->Type == TypeStrZ)))
		{
			Str = Value.Str();
			return true;
		}

		return false;
	}

	bool HasValue(int &Val)
	{
		if (!Value.IsNull() &&
			Type &&
			Type->Base &&
			Type->Base->Type == TypeInt)
		{
			char *v = Value.Str();
			if (strnicmp(v, "0x", 2) == 0)
			{
				Val = htoi(v + 2);
			}
			else
			{
				Val = Value.CastInt32();
			}
			return true;
		}

		return false;
	}

	bool HasValue(float &Val)
	{
		if (!Value.IsNull())
		{
			Val = Value.CastDouble();
			return true;
		}

		return false;
	}


	bool HasValue(double &Val)
	{
		if (!Value.IsNull())
		{
			Val = Value.CastDouble();
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
				default:
					LgiAssert(!"Not impl");
					break;
				case TypeInt:
				{
					switch (Type->Base->Bytes)
					{
						case 1:
							f = *((int8*)Data);
							break;
						case 2:
							f = *((int16*)Data);
							break;
						case 4:
							f = (float) *((int32*)Data);
							break;
						case 8:
							f = (float) *((int64*)Data);
							break;
					}
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
				case TypeNibble:
				{
					uint64 v = DeNibble(Data, Type->Base->Bytes);
					f = (float)(int64)v;
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
				default:
					LgiAssert(!"Not impl");
					break;
				case TypeInt:
				{
					switch (Type->Base->Bytes)
					{
						case 1:
							f = *((int8*)Data);
							break;
						case 2:
							f = *((int16*)Data);
							break;
						case 4:
							f = *((int32*)Data);
							break;
						case 8:
							f = (double)*((int64*)Data);
							break;
					}
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
				case TypeNibble:
				{
					uint64 v = DeNibble(Data, Type->Base->Bytes);
					f = (double)(int64)v;
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
				default:
					LgiAssert(!"Not impl");
					break;
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
							i = (int64)*n;
							break;
						}
						case 8:
						{
							double *n = (double*)Data;
							i = (int64)*n;
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
				case TypeNibble:
				{
					i = DeNibble(Data, Type->Base->Bytes);
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
	GArray<Member*> Members;
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
		for (int i=0; i<Members.Length(); i++)
		{
			Size += Members[i]->Sizeof();
		}
		return Size;
	}
	
	StructDef *GetStruct(const char *n)
	{
		if (Name && !stricmp(Name, n))
			return this;

		for (int i=0; i<Children.Length(); i++)
		{
			StructDef *s = Children[i]->GetStruct(n);
			if (s)
				return s;
		}
		return NULL;
	}

	StructDef *MatchChild(char *Data, int Len, bool Little)
	{
		if (Children.Length() == 0)
			return NULL;

		// Check to see if any child struct specializations match
		for (int c=0; c<Children.Length(); c++)
		{
			char *d = Data;
			int l = Len;
			if (Children[c]->Match(d, l, Little))
			{
				return Children[c];
			}
		}

		return NULL;
	}

	bool Match(char *&Data, int &Len, bool Little)
	{
		bool Status = false;

		if (Data && Len > 0)
		{
			for (int i=0; i<Members.Length(); i++)
			{
				Member *Mem = Members[i];
				VarDef *d = Mem->IsVar();
				if (d)
				{
					if (d->Type->Base)
					{
						if (d->Type &&
							d->Type->Base)
						{
							if ((d->Type->Base->Type == TypeChar) || (d->Type->Base->Type == TypeStrZ))
							{
								char *Str = 0;
								if (d->HasValue(Str))
								{
									if (Str && strnicmp(Data, Str, strlen(Str)) != 0)
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
									uint32 i = d->CastInt(Data, Little);
									if (i != Val)
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
									float f = d->CastFloat(Data, Little);
									if (f != Val)
									{
										return false;
									}
									else Status = true;
								}
							}
						}

						int Length = 1;
						if (d->Type->Length.Length() > 0)
						{
							ArrayDimension &ad = d->Type->Length.First();
							if (ad.Expression.Length() == 1)
								Length = AtoiW(ad.Expression[0]);
							else
								LgiAssert(!"Impl me.");
						}						
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
	GHashTbl<char16*, char16*> Defines;
	GArray<StructDef*> Compiled;

	StructDef *GetStruct(const char *Name)
	{
		if (Name)
		{
			for (int i=0; i<Compiled.Length(); i++)
			{
				StructDef *s = Compiled[i]->GetStruct(Name);
				if (s)
					return s;
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

	bool GetVariant(const char *Name, GVariant &Value, char *Array = 0)
	{
		if (!Name)
			return false;

		VarDef *Ref = 0;
		int n = 0;
		for (; n<ProcessedElements; n++)
		{
			VarDef *d = CurStructDef->Members[n]->IsVar();
			if (d &&
				d->Name &&
				stricmp(d->Name, Name) == 0)
			{
				Ref = d;
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
	
	template<typename T>
	T *DisplayString(T *str, int chars)
	{
		GArray<T> out;
		for (int i=0; i<chars; i++)
		{
			switch (str[i])
			{
				case '\r':
					out.Add('\\'); out.Add('r'); break;
				case '\n':
					out.Add('\\'); out.Add('n'); break;
				case '\t':
					out.Add('\\'); out.Add('t'); break;
				default:
					if (str[i] < ' ' || str[i] >= 0x7f)
					{
						char hex[16];
						int ch = sprintf_s(hex, sizeof(hex), "\\x%x", (uint8)str[i]);
						for (int c=0; c<ch; c++)
							out.Add(hex[c]);
					}
					else
					{
						out.Add(str[i]);
					}
					break;
			}
		}
		out.Add(0);
		return out.Release();
	}

	bool DoStruct(StructDef *s, char *Base, char *&Data, int &Len, GStream &Out, bool little, int Depth = 0)
	{
		char Tabs[256];

		ZeroObj(Tabs);
		memset(Tabs, ' ', Depth * 2);
		CurStructDef = s;
		Addr.Length(0);
		Little = little;

		for (int i=0; i<s->Members.Length() && Len > 0; i++)
		{
			VarDef *d = s->Members[i]->IsVar();
			if (d)
			{
				int Length = 1;
				if (d->Type->Length.Length())
				{
					GArray<char*> DimStr;
					for (unsigned dim = 0; dim < d->Type->Length.Length(); dim++)
					{
						GStringPipe p;
						ArrayDimension &ad = d->Type->Length[dim];
						for (unsigned n=0; n<ad.Expression.Length(); n++)
						{
							p.Print(" %S", ad.Expression[n]);
						}
						DimStr.Add(p.NewStr());
					}
					
					LgiAssert(DimStr.Length() == 1);
					
					// Resolve the array length of this var
					// Evaluate expression
					GScriptEngine e(App, App, NULL);
					GVariant v;
					ProcessedElements = i;
					if (e.EvaluateExpression(&v, this, DimStr[0]))
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
						Out.Print("Error: evaluating the expression '%s'\n", d->Type->Length);
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
						default:
							LgiAssert(!"Not impl");
							break;
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
							
							for (int n=0; n<Length && Len >= b->Bytes; n++)
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
									int i = d->CastInt(Data, Little);
									if (i != Val)
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
							
							for (int n=0; n<Length && Len >= b->Bytes; n++)
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
						case TypeNibble:
						{
							if (!d->Hidden)
							{
								Out.Print("%s%s", Tabs, d->Name);
								if (Length > 1)
									Out.Print(":\n");
								else
									Out.Print(" = ");
							}
							
							for (int n=0; n<Length && Len >= b->Bytes; n++)
							{
								if (!d->Hidden)
								{
									if (Length > 1)
										Out.Print("\t%s[%i] = ", Tabs, n);
									char *LeadIn = Length > 1 ? Tabs : (char*)"";

									switch (b->Bytes)
									{
										case 2:
										{
											uint8 n = DeNibble(Data, b->Bytes);
											Out.Print("%s%u (0x%02.2x)\n", LeadIn, n, n);
											break;
										}
										case 4:
										{
											uint16 v = DeNibble(Data, b->Bytes);
											uint16 n = IfSwap(v, Little);
											Out.Print("%s%u (0x%04.4x)\n", LeadIn, n, n);
											break;
										}
										case 8:
										{
											uint32 v = DeNibble(Data, b->Bytes);
											uint32 n = IfSwap(v, Little);
											Out.Print("%s%i (0x%08.8x)\n", LeadIn, n, n);
											break;
										}
										case 16:
										{
											uint64 v = DeNibble(Data, b->Bytes);
											uint64 n = IfSwap(v, Little);
											Out.Print("%s%I64u (0x%016.16I64x)\n", LeadIn, n, n);
											break;
										}
										default:
											LgiAssert(!"Not impl.");
											break;
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
						case TypeChar:
						{
							if (!d->Hidden)
							{
								bool Long =  Length >= 256;
								if (Long)
								{
									Out.Print("%s%s[%i]\n", Tabs, d->Name, Length);
								}
								else if (d->Type->Base->Bytes == 1)
								{
									// char *u = (char*) LgiNewConvertCp("utf-8", Data, "iso-8859-1", Length);
									char *u = DisplayString(Data, Length);

									Out.Print("%s%s = '%s'\n", Tabs, d->Name, u);
									if (u && d->Value.Str())
									{
										if (strnicmp(u, d->Value.Str(), Length) != 0)
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
									char16 *u = 0;
									if (w)
									{
										char16 *Src = (char16*)Data;
										for (int i=0; i<Length; i++)
										{
											w[i] = IfSwap(Src[i], Little);
										}
										w[Length] = 0;

										// u = LgiNewUtf16To8(w);
										u = DisplayString(w, Length);
									}
									DeleteArray(w);

									Out.Print("%s%s = '%s'\n", Tabs, d->Name, u);
									if (d->Value.WStr())
									{
										if (StrnicmpW(u, d->Value.WStr(), Length) != 0)
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
							if (!d->Hidden && (d->Type->Base->Bytes < 8))
							{
								Out.Print("%s%s", Tabs, d->Name);
								if (Length > 1)
									Out.Print(":\n");
								else
									Out.Print(" = ");
							}
							for (int n=0; n<Length && Len >= d->Type->Base->Bytes; n++)
							{
								if (!d->Hidden && (d->Type->Base->Bytes < 8))
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
										if (u && d->Value.Str())
										{
											if (strnicmp(u, d->Value.Str(), zstringLen) != 0)
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
										if (d->Value.Str())
										{
											if (strnicmp(u, d->Value.Str(), zstringLen) != 0)
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
					StructDef *s = d->Type->Cmplex;
					StructDef *sub = s->MatchChild(Data, Len, Little);

					if (sub)
						Out.Print("%s%s.%s (@ %i/0x%x) =\n", Tabs, sub->Name, d->Name, Data-Base, Data-Base);
					else
						Out.Print("%s%s (@ %i/0x%x) =\n", Tabs, d->Name, Data-Base, Data-Base);
					
					if (Length == 1)
						Out.Print("%s{\n", Tabs);
					
					for (int i=0; (Length < 0 || i < Length) && Len > 0; i++)
					{
						s = d->Type->Cmplex;
						if (i)
							sub = s->MatchChild(Data, Len, Little);
						if (sub)
							s = sub;

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
			else
			{
				LgiAssert(!"Impl condition here");
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
			strcpy_s(s, sizeof(s), d + 1);
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
		if (!Body && File)
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

	class CompileState
	{
		GArray<char*> MemA;
		GArray<char16*> MemW;

	public:
		bool Error;
		GAutoWString Base;
		char16 *s;
		
		CompileState(char *Init)
		{
			Error = false;
			s = NULL;
			if (Init)
			{
				Base.Reset(LgiNewUtf8To16(Init));
				s = Base;
			}
		}
		
		~CompileState()
		{
			MemA.DeleteArrays();
			MemW.DeleteArrays();
		}
		
		char *NextA(bool AutoFree = true)
		{
			char16 *t = LexCpp(s, LexStrdup);
			if (!t)
				return NULL;
			
			char *u = LgiNewUtf16To8(t);
			DeleteArray(t);
			if (!u)
				return NULL;
			
			if (AutoFree)
				MemA.Add(u);
			return u;
		}

		char16 *NextW(bool AutoFree = true)
		{
			char16 *t = LexCpp(s, LexStrdup);
			if (!t)
				return NULL;
			
			if (AutoFree)
				MemW.Add(t);
			return t;
		}
	};

	void Err(CompileState &State, const char *Msg)
	{
		int Line = 1;
		char16 *Cur = State.s;
		while (Cur > State.Base)
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
	
	VarDefType *ParseDefType(char16 *t)
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
		else if (XCmp(t, "nibble", 4) == 0)
		{
			v = new VarDefType;
			v->Base = new Basic;
			v->Base->Array = false;
			v->Base->Signed = false;
			v->Base->Type = TypeNibble;
			int Bits = AtoiW(t + 6);
			v->Base->Bytes = Bits ? Bits / 8 : 2;
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

	VarDef *ParseVar(CompileState &State, char16 *t)
	{
		bool IsHidden = false;
		if (IsHidden = !XCmp(t, "hidden"))
			t = State.NextW();

		VarDefType *Type = ParseDefType(t);
		if (!Type)
		{
			char m[256], *u = LgiNewUtf16To8(t);
			sprintf(m, "expected type, got '%s' instead", u);
			Err(State, m);
			DeleteArray(u);
			return NULL;
		}		

		GAutoPtr<VarDef> Var(new VarDef);
		if (!Var)
		{
			Err(State, "allocation error");
			return NULL;
		}
		
		Var->Type = Type;
		Var->Hidden = IsHidden;
		Var->Name = State.NextA(false);
		if (!Var->Name)
		{
			Err(State, "no name for vardef");
			return NULL;
		}

		t = State.NextW();
		if (!t)
		{
			Err(State, "expecting end of var def ';' or '[' for array");
			return NULL;
		}
		
		while (!XCmp(t, "["))
		{
			// Array
			ArrayDimension &ad = Var->Type->Length.New();
			int Depth = 0;
			while (t = State.NextW(false))
			{
				if (!XCmp(t, "["))
				{
					Depth++;
					ad.Expression.Add(t);
				}
				else if (!XCmp(t, "]"))
				{
					if (Depth)
					{
						Depth--;
						ad.Expression.Add(t);
					}
					else
					{
						// Finished expression
						break;
					}
				}
				else
				{
					ad.Expression.Add(t);
				}
			}
			
			t = State.NextW();
		}
		
		if (t && !XCmp(t, ":"))
		{
			// Bitfield
			t = State.NextW();
			Var->Bits = AtoiW(t);
			LgiAssert(Var->Bits != 0);
			
			t = State.NextW();
		}

		if (t && !XCmp(t, "="))
		{
			// Constraint
			Var->Value.OwnStr(TrimStr(State.NextA(false), "\"\'"));
			t = State.NextW();
		}
		
		if (t && XCmp(t, ";") == 0)
		{
		}
		else
		{
			Err(State, "expected ';'");
			return false;
		}
		
		return Var.Release();
	}

	bool Compile()
	{
		CompileState State(GetBody());
		Compiled.DeleteObjects();
		if (State.Base)
		{
			#ifndef __GNUC__
			#define IsTok(lit) \
				(t != NULL && StricmpW(t, lit) == 0)
			#define CheckTok(lit) \
				if (!(t && StricmpW(t, L##lit) == 0)) \
				{ \
					char m[256], *u = LgiNewUtf16To8(t); \
					sprintf(m, "expecting '" ##lit "', got '%s'", u); \
					Err(State, m); \
					DeleteArray(u) \
					State.Error = true; \
					break; \
				}
			#else
			#define IsTok(lit) \
				(t != NULL && XCmp(t, lit) == 0)
			#define CheckTok(lit) \
				if (!(t && XCmp(t, lit) == 0)) \
				{ \
					char m[256], *u = LgiNewUtf16To8(t); \
					sprintf(m, "expecting '%s', got '%s'", lit, u); \
					Err(State, m); \
					DeleteArray(u) \
					State.Error = true; \
					break; \
				}
			#endif
			#define Literal(lit) \
				t = State.NextW(); \
				CheckTok(lit);

			char16 *t = 0;
			while (!State.Error)
			{
				t = State.NextW();
				if (!t)
					break;

				if (!XCmp(t, "#define"))
				{
					char16 *Name = State.NextW();
					while (*State.s && strchr(WhiteSpace, *State.s))
						State.s++;
					char16 *Eol = StrchrW(State.s, '\n');
					if (!Eol) Eol = State.s + StrlenW(State.s);
					GAutoWString Value(NewStrW(State.s, Eol - State.s));
					if (*State.s)
						State.s++;
					
					Defines.Add(Name, Value.Release());
				}
				else if (IsTok(L"struct"))
				{
					// Parse struct def
					StructDef *Def = new StructDef;
					if (Def)
					{
						Def->Name = State.NextA(false);

						t = State.NextW();
						if (XCmp(t, ":") == 0)
						{
							Literal("inherits");
							Def->Base = State.NextA(false);
							t = State.NextW();
						}
						CheckTok("{");

						while (!State.Error)
						{
							// Parse types...
							t = State.NextW();
							DoType:
							if (!t)
							{
								Err(State, "No token");
								State.Error = true;
								break;
							}
							
							if (IsTok(L"if"))
							{
								t = State.NextW();
								CheckTok("(");
								
								GAutoPtr<ConditionDef> c(new ConditionDef);
								if (!c)
									break;
								
								while (t = State.NextW(false))
								{
									if (!XCmp(t, ")"))
									{
										DeleteArray(t);
										break;
									}
									c->Expression.Add(t);
								}
								
								t = State.NextW();
								CheckTok("{");
								
								while (t = State.NextW())
								{
									if (!XCmp(t, "}"))
									{
										break;
									}
									else
									{
										VarDef *v = ParseVar(State, t);
										if (!v)
											break;
										c->Members.Add(v);
									}
								}
								
								Def->Members.Add(c.Release());
							}
							else if (IsTok(L"}"))
							{
								Literal(";");
								break;
							}
							else
							{
								VarDef *v = ParseVar(State, t);
								if (!v)
									break;
								
								Def->Members.Add(v);
							}
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
								Err(State, "parent class not defined.");
							}
						}
						else
						{
							Compiled.Add(Def);
						}
					}
				}
			}
		}

		return !State.Error;
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
						if (Map && Lst)
						{
							Lst->Insert(Map);
						}
					}
					if (Map && Txt)
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
	
	if (LgiGetSystemPath(LSP_APP_INSTALL, Base, sizeof(Base)))
	{
		GArray<char*> Files;
		GArray<const char*> Ext;
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
			if (f == GITEM_NOTIFY_DBL_CLICK)
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
			if (Map->Lst && Map->Lst->GetSelection(Sel))
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
			if (Map->Lst && Map->Lst->GetSelection(Sel))
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

