#include "Lgi.h"
#include "GToken.h"
#include "iHex.h"
#include "GList.h"
#include "GUtf8.h"
#include "GLexCpp.h"
#include "resdefs.h"
#ifndef INT32_MAX
#define INT32_MAX 0x7fffffff
#endif

enum BaseType
{
	TypeNull,
	TypeInteger,
	TypeFloat,
	TypeChar,
	TypeStrZ,
	TypeNibble,
};

struct Basic
{
	BaseType Type;
	uint8 Bytes;
	int Bits;
	bool Signed;
	bool Array;
	
	Basic()
	{
		Type = TypeNull;
		Bytes = 0;
		Bits = 0;
		Signed = false;
		Array = false;
	}
};

struct BitReference
{
	union
	{
		uint8 *Ptr;
		UNativeInt Size;
	};
	int Bit;
	int Len;
	
	BitReference()
	{
		Ptr = NULL;
		Bit = 0;
		Len = 0;
	}
	
	bool IsValid()
	{
		return Ptr != NULL && Len > 0;
	}

	uint8 *Aligned()
	{
		return (Bit) ? Ptr + 1 : Ptr;
	}
	
	UNativeInt AlignedSize()
	{
		return (Bit) ? Size + 1 : Size;
	}

	bool SeekBits(int Bits)
	{
		while (Bits)
		{
			if (Len <= 0)
				return false;
			
			int Avail = 8 - Bit;
			int Sk = min(Bits, Avail);
			Bits -= Sk;
			Bit += Sk;
			if (Bit >= 8)
			{
				LgiAssert(Bit == 8);
				Bit = 0;
				Ptr++;
				Len--;
			}
		}
		
		return true;
	}

	bool SeekBytes(int Bytes)
	{
		LgiAssert(Bit == 0);
		Ptr += Bytes;
		Len -= Bytes;
		return true;
	}
	
	bool Seek(Basic *b)
	{
		if (b->Bits)
		{
			int WordSize = b->Bytes << 3;
			Bit += b->Bits;
			if (Bit >= WordSize)
			{
				Bit -= WordSize;
				Ptr += b->Bytes;
				Len -= b->Bytes;
			}
		}
		else
		{
			Ptr += b->Bytes;
			Len -= b->Bytes;
		}
		
		return true;
	}

	bool AlignForBasic(Basic *b)
	{
		if (b->Bits == 0 && Bit)
		{
			// Byte align..
			if (Len > 0)
			{
				Bit = 0;
				Ptr++;
				Len--;
			}
			else
			{
				// No more data
				return false;
			}
		}
		
		return true;
	}
	
	template<typename T>
	bool ReadBits(T &out, int Bits)
	{
		if (Bits == 0)
		{
			// Read whole type
			LgiAssert(Bit == 0);
			out = *((T*)Ptr);
			Ptr += sizeof(T);
		}
		else
		{
			// Read 'Bits' bits
			out = 0;
			
			int BitSz = sizeof(T) << 3;
			
			while (Bits > 0)
			{
				int Avail = BitSz - Bit;
				int Rd = min(Avail, Bits);
				int Mask = (1 << Rd) - 1;
				int Shift = BitSz - Rd - Bit;
				LgiAssert(Shift >= 0);
				
				out <<= Rd;
				out |= ( *((T*)Ptr) >> Shift) & Mask;
				
				Bits -= Rd;
				Bit += Rd;
				if (Bit >= BitSz)
				{
					Bit -= BitSz;
					Ptr += sizeof(T);
					Len -= sizeof(T);
					if (Len <= 0)
						return Bits > 0;
				}
			}
		}
		
		return true;
	}
};

struct ViewContext : public BitReference
{
	GStream &Out;
	uint8 *Base;
	
	ViewContext(GStream &o) : Out(o)
	{
	}
	
	uint32 Offset()
	{
		LgiAssert(Ptr != NULL && Ptr >= Base);
		return Ptr - Base;
	}

	void Trace(const char *s)
	{
		LgiTrace("%p, %x:%i - %s\n", Ptr, Offset(), Bit, s);
	}
};

struct AddressRef : public GArray<BitReference>
{
	AddressRef &operator = (const ViewContext &c)
	{
		BitReference &r = New();
		r = c;
		return *this;
	}
};

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

	void Sizeof(BitReference &sz);
};

uint64 DeNibble(uint8 *ptr, int size)
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
	
	virtual void Sizeof(BitReference &sz) = 0;
	virtual VarDef *IsVar() = 0;
	virtual ConditionDef *IsCondition() = 0;
};

struct ConditionDef : public Member
{
	bool Little;
	int Eval;
	GArray<char16*> Expression;
	GArray<Member*> Members;
	AddressRef Addr;
	
	ConditionDef() : Member(MemberCondition)
	{
		Little = true;
		Eval = -1;
	}
	
	~ConditionDef()
	{
		Expression.DeleteArrays();
	}

	VarDef *IsVar() { return NULL; }
	ConditionDef *IsCondition() { return this; }

	void Sizeof(BitReference &sz)
	{
		if (!Eval)
			return;
			
		for (unsigned i=0; i<Members.Length(); i++)
		{
			Members[i]->Sizeof(sz);
		}
	}

	bool GetVariant(const char *Name, GVariant &Value, char *Array = 0);
};

struct VarDef : public Member
{
	VarDefType *Type;
	char *Name;
	GVariant Value;
	bool Hidden;
	bool Debug;

	VarDef() : Member(MemberVar)
	{
		Name = 0;
		Hidden = false;
		Debug = false;
	}

	~VarDef()
	{
		DeleteArray(Name);
	}

	VarDef *IsVar() { return this; }
	ConditionDef *IsCondition() { return NULL; }
	void Sizeof(BitReference &sz)
	{
		if (Type)
			Type->Sizeof(sz);
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
			Type->Base->Type == TypeInteger)
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

	float CastFloat(BitReference &Addr, bool Little)
	{
		float f = 0;

		Basic *b = Type->Base;
		if (b)
		{
			switch (b->Type)
			{
				default:
					LgiAssert(!"Not impl");
					break;
				case TypeInteger:
				{
					int64 Val = CastInt(Addr, Little);
					f = (float)Val;
					break;
				}
				case TypeFloat:
				{
					if (b->Bytes == 4)
					{
						f = *((float*)Addr.Aligned());
						break;
					}
					else if (b->Bytes == 8)
					{
						f = *((double*)Addr.Aligned());
					}
					break;
				}
				case TypeChar:
				case TypeStrZ:
				{
					f = atof((char*)Addr.Aligned());
					break;
				}
				case TypeNibble:
				{
					uint64 v = DeNibble(Addr.Aligned(), Type->Base->Bytes);
					f = (float)(int64)v;
					break;
				}
			}
		}

		return f;
	}

	double CastDouble(BitReference &Addr, bool Little)
	{
		double f = 0;

		Basic *b = Type->Base;
		if (b)
		{
			switch (b->Type)
			{
				default:
					LgiAssert(!"Not impl");
					break;
				case TypeInt:
				{
					int64 Val = CastInt(Addr, Little);
					f = (double)Val;
					break;
				}
				case TypeFloat:
				{
					if (b->Bytes == 4)
					{
						f = *((float*)Addr.Aligned());
						break;
					}
					else if (b->Bytes == 8)
					{
						f = *((double*)Addr.Aligned());
					}
					break;
				}
				case TypeChar:
				case TypeStrZ:
				{
					f = atof((char*)Addr.Aligned());
					break;
				}
				case TypeNibble:
				{
					uint64 v = DeNibble(Addr.Aligned(), Type->Base->Bytes);
					f = (double)(int64)v;
					break;
				}
			}
		}

		return f;
	}

	int64 CastInt(BitReference Addr, bool Little)
	{
		Basic *b = Type->Base;
		if (!b)
			return 0;

		int64 i = 0;
		switch (b->Type)
		{
			default:
				LgiAssert(!"Not impl");
				break;
			case TypeInteger:
			{
				switch (b->Bytes)
				{
					case 1:
					{
						uint8 Val;
						Addr.ReadBits(Val, b->Bits);
						if (b->Signed)
							i = (int8)Val;
						else
							i = (uint8)Val;
						break;
					}
					case 2:
					{
						uint16 Val;
						Addr.ReadBits(Val, b->Bits);
						if (b->Signed)
							i = IfSwap((int16)Val, Little);
						else
							i = IfSwap((uint16)Val, Little);
						break;
					}
					case 4:
					{
						uint32 Val;
						Addr.ReadBits(Val, b->Bits);
						if (b->Signed)
							i = IfSwap((int32)Val, Little);
						else
							i = IfSwap((uint32)Val, Little);
						break;
					}
					case 8:
					{
						uint64 Val;
						Addr.ReadBits(Val, b->Bits);
						if (b->Signed)
							i = IfSwap((int64)Val, Little);
						else
							i = IfSwap((uint64)Val, Little);
						break;
					}
				}
				break;
			}
			case TypeFloat:
			{
				LgiAssert(Addr.Bit == 0);
				switch (b->Bytes)
				{
					case 4:
					{
						float *n = (float*)Addr.Aligned();
						i = (int64)*n;
						break;
					}
					case 8:
					{
						double *n = (double*)Addr.Aligned();
						i = (int64)*n;
						break;
					}
					default:
					{
						LgiAssert(!"Unexpected floating pt length");
						break;
					}
				}
				break;
			}
			case TypeChar:
			case TypeStrZ:
			{
				i = atoi((char*)Addr.Aligned());
				break;
			}
			case TypeNibble:
			{
				i = DeNibble(Addr.Aligned(), b->Bytes);
				break;
			}
		}

		return i;
	}
};

bool ConditionDef::GetVariant(const char *Name, GVariant &Value, char *Array)
{
	if (Eval <= 0)
	{
		return false;
	}
	
	int Len = min(Members.Length(), Addr.Length());
	ConditionDef *c = NULL;
	for (int i=0; i<Len; i++)
	{
		Member *m = Members[i];
		VarDef *v = m->IsVar();
		if (v)
		{
			if (v->Name && strcmp(v->Name, Name) == 0)
			{
				Value = v->CastInt(Addr[i], Little);
				return true;
			}
		}
		else if ((c = m->IsCondition()))
		{
			if (c->GetVariant(Name, Value, Array))
				return true;
		}
	}
	
	return false;
}


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

	void Sizeof(BitReference &sz)
	{
		for (int i=0; i<Members.Length(); i++)
		{
			Members[i]->Sizeof(sz);
		}
	}
	
	StructDef *GetStruct(const char *n)
	{
		if (Name && !strcmp(Name, n))
			return this;

		for (int i=0; i<Children.Length(); i++)
		{
			StructDef *s = Children[i]->GetStruct(n);
			if (s)
				return s;
		}
		return NULL;
	}

	StructDef *MatchChild(ViewContext &View, bool Little)
	{
		if (Children.Length() == 0)
			return NULL;

		// Check to see if any child struct specializations match
		for (int c=0; c<Children.Length(); c++)
		{
			BitReference r;
			r = View;
			if (Children[c]->Match(r, Little))
			{
				return Children[c];
			}
		}

		return NULL;
	}

	bool Match(BitReference &Addr, bool Little)
	{
		bool Status = false;

		if (Addr.IsValid())
		{
			for (int i=0; i<Members.Length(); i++)
			{
				Member *Mem = Members[i];
				VarDef *d = Mem->IsVar();
				if (d && d->Type)
				{
					Basic *b = d->Type->Base;
					if (b)
					{
						if ((b->Type == TypeChar) || (b->Type == TypeStrZ))
						{
							char *Str = 0;
							if (d->HasValue(Str))
							{
								if (Str && strnicmp((char*)Addr.Aligned(), Str, strlen(Str)) != 0)
								{
									return false;
								}
								else Status = true;
							}
						}
						else if (d->Type->Base->Type == TypeInteger)
						{
							int Val = 0;
							if (d->HasValue(Val))
							{
								uint64 i = d->CastInt(Addr, Little);
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
								float f = d->CastFloat(Addr, Little);
								if (f != Val)
								{
									return false;
								}
								else Status = true;
							}
						}
						else LgiAssert(0);

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

						LgiAssert(b->Bits == 0); // impl bit seeking support?
						Addr.Ptr += b->Bytes * Length;
						Addr.Len -= b->Bytes * Length;
					}
					else if (d->Type->Cmplex)
					{
						Status |= d->Type->Cmplex->Match(Addr, Little);
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

void VarDefType::Sizeof(BitReference &sz)
{
	if (Base)
	{
		if (Base->Bits)
			sz.SeekBits(Base->Bits);
		else
			sz.SeekBytes(Base->Bytes);
	}
	else if (Cmplex)
	{
		Cmplex->Sizeof(sz);
	}
}

class StructureMap : public GListItem, public GDom
{
	AppWnd *App;
	char *File;
	char *Body;
	GStringPipe Errs;

	bool Little;
	char Tabs[256];

	struct ScopeType
	{
		int Pos;
		GArray<Member*> *Members;
		AddressRef Addr;
	};	
	GArray<ScopeType> Stack;

public:
	GHashTbl<const char*, char16*> Defines;
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

		// Walk up the stack of scopes looking for a variable matching 'Name'
		for (int Scope = Stack.Length() - 1; Scope >= 0; Scope--)
		{
			ScopeType &s = Stack[Scope];
			int n = 0;
			
			VarDef *Var = NULL;
			ConditionDef *Cond = NULL;
			for (; n<s.Pos; n++)
			{
				Member *Mem = (*s.Members)[n];
				Var = Mem->IsVar();
				if (Var)
				{
					if (Var->Name && strcmp(Var->Name, Name) == 0)
					{
						Value = Var->CastInt(s.Addr[n], Little);
						return true;
					}
				}
				else if ((Cond = Mem->IsCondition()))
				{
					Cond->Little = Little;
					if (Cond->GetVariant(Name, Value, Array))
						return true;
				}
			}
		}
		
		// If it's not in any scope, check the #defines as well
		char16 *v = Defines.Find(Name);
		if (v)
		{
			GAutoString u(LgiNewUtf16To8(v));
			GScriptEngine e(App, App, NULL);
			bool Status = e.EvaluateExpression(&Value, this, u);
			return Status;
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

	/// This function reads an arbitrary number of bits from 'View' to 'Out'	
	template<typename T>
	bool Read
	(
		/// The output type (integer only)
		T &Out,
		/// The input stream of bytes. A copy is used to avoid
		/// changing the contents of the source View. Updating
		/// the position is separate to reading the data.
		BitReference Ref,
		/// The number of bits to read
		int Bits
	)
	{
		return Ref.ReadBits(Out, Bits);
	}
	
	bool DoInt(VarDef *d, ViewContext &View, int &ArrayLength)
	{
		Basic *b = d->Type->Base;
		if (!d->Hidden)
		{
			View.Out.Print("%s%s", Tabs, d->Name);
			if (ArrayLength > 1)
				View.Out.Print("[%i]:\n", ArrayLength);
			else
				View.Out.Print(" = ");
		}
		
		for (int n=0; n<ArrayLength && View.Len >= b->Bytes; n++)
		{
			if (!View.AlignForBasic(b))
				return false;
			
			if (!d->Hidden)
			{
				if (ArrayLength > 1)
					View.Out.Print("  %s[%i] = ", Tabs, n);
				char *LeadIn = ArrayLength > 1 ? Tabs : (char*)"";

				/*
				if (b->Bits)
					View.Trace(d->Name);
				*/

				switch (b->Bytes)
				{
					case 1:
					{
						uint8 Byte;
						if (!Read(Byte, View, b->Bits))
							return false;
						if (b->Signed)
							View.Out.Print("%s%i (0x%02.2x)\n", LeadIn, (int8)Byte, (int8)Byte);
						else
							View.Out.Print("%s%u (0x%02.2x)\n", LeadIn, (uint8)Byte, (uint8)Byte);
						break;
					}
					case 2:
					{
						uint16 Short;
						if (!Read(Short, View, b->Bits))
							return false;
						
						if (b->Signed)
						{
							int16 n = IfSwap(((int16)Short), Little);
							View.Out.Print("%s%i (0x%04.4x)\n", LeadIn, n, n);
						}
						else
						{
							uint16 n = IfSwap(Short, Little);
							View.Out.Print("%s%u (0x%04.4x)\n", LeadIn, n, n);
						}
						break;
					}
					case 4:
					{
						uint32 Int;
						if (!Read(Int, View, b->Bits))
							return false;
						
						if (b->Signed)
						{
							int32 n = IfSwap((int32)Int, Little);
							View.Out.Print("%s%i (0x%08.8x)\n", LeadIn, n, n);
						}
						else
						{
							uint32 n = IfSwap(Int, Little);
							View.Out.Print("%s%u (0x%08.8x)\n", LeadIn, n, n);
						}
						break;
					}
					case 8:
					{
						uint64 Long;
						if (!Read(Long, View, b->Bits))
							return false;

						if (b->Signed)
						{
							int64 n = IfSwap((int64)Long, Little);
							#ifdef WIN32
							View.Out.Print("%s%I64i (0x%16.16lI64x)\n", LeadIn, n, n);
							#else
							View.Out.Print("%s%li (0x%16.16lx)\n", LeadIn, n, n);
							#endif
						}
						else
						{
							uint64 n = IfSwap(Long, Little);
							#ifdef WIN32
							View.Out.Print("%s%I64u (0x%16.16lI64x)\n", LeadIn, n, n);
							#else
							View.Out.Print("%s%lu (0x%16.16lx)\n", LeadIn, n, n);
							#endif
						}
						break;
					}
				}
			}

			int Val = 0;
			if (d->HasValue(Val))
			{
				BitReference Ref;
				Ref = View;
				int i = d->CastInt(Ref, Little);
				if (i != Val)
				{
					View.Out.Print("%sValue Mismatch!\n", Tabs);
					return false;
				}
			}

			if (!View.Seek(b))
				return false;
		}
		
		return true;
	}
	
	bool DoFloat(VarDef *d, ViewContext &View, int &ArrayLength)
	{
		Basic *b = d->Type->Base;
		if (!d->Hidden)
		{
			View.Out.Print("%s%s", Tabs, d->Name);
			if (ArrayLength > 1)
				View.Out.Print("[%i]:\n", ArrayLength);
			else
				View.Out.Print(" = ");
		}
		
		for (int n=0; n<ArrayLength && View.Len >= b->Bytes; n++)
		{
			if (!d->Hidden)
			{
				if (ArrayLength > 1)
					View.Out.Print("\t%s[%i] = ", Tabs, n);
				char *LeadIn = ArrayLength > 1 ? Tabs : (char*)"";

				switch (b->Bytes)
				{
					case 4:
					{
						LgiAssert(sizeof(float) == 4);
						
						float flt = *((float*)View.Ptr);
						#define Swap(a, b) { uint8 t = a; a = b; b = t; }
						
						if (!Little)
						{
							uint8 *c = (uint8*)&flt;
							Swap(c[0], c[3]);
							Swap(c[1], c[2]);
						}

						double dbl = flt;

						View.Out.Print("%s%g\n", LeadIn, dbl);

						float Val = 0;
						if (d->HasValue(Val))
						{
							if (d->CastFloat(View, Little) != Val)
							{
								View.Out.Print("%sValue Mismatch!\n", Tabs);
								return false;
							}
						}
						break;
					}
					case 8:
					{
						LgiAssert(sizeof(double) == 8);

						double dbl = *((double*)View.Aligned());

						if (!Little)
						{
							uint8 *c = (uint8*)&dbl;
							Swap(c[0], c[7]);
							Swap(c[1], c[6]);
							Swap(c[2], c[5]);
							Swap(c[3], c[4]);
						}

						View.Out.Print("%s%g\n", LeadIn, dbl);

						double Val = 0;
						if (d->HasValue(Val))
						{
							if (d->CastDouble(View, Little) != Val)
							{
								View.Out.Print("%sValue Mismatch!\n", Tabs);
								return false;
							}
						}
						break;
					}
					default:
					{
						View.Out.Print("#error (%s:%i)\n", LeadIn, __FILE__, __LINE__);
						break;										
					}
				}
			}

			View.Seek(b);
		}
		
		return true;
	}

	bool DoNibble(VarDef *d, ViewContext &View, int &ArrayLength)
	{
		Basic *b = d->Type->Base;
		if (!d->Hidden)
		{
			View.Out.Print("%s%s", Tabs, d->Name);
			if (ArrayLength > 1)
				View.Out.Print("[%i]:\n", ArrayLength);
			else
				View.Out.Print(" = ");
		}
		
		for (int n=0; n<ArrayLength && View.Len >= b->Bytes; n++)
		{
			if (!d->Hidden)
			{
				if (ArrayLength > 1)
					View.Out.Print("\t%s[%i] = ", Tabs, n);
				char *LeadIn = ArrayLength > 1 ? Tabs : (char*)"";

				switch (b->Bytes)
				{
					case 2:
					{
						uint8 n = DeNibble(View.Aligned(), b->Bytes);
						View.Out.Print("%s%u (0x%02.2x)\n", LeadIn, n, n);
						break;
					}
					case 4:
					{
						uint16 v = DeNibble(View.Aligned(), b->Bytes);
						uint16 n = IfSwap(v, Little);
						View.Out.Print("%s%u (0x%04.4x)\n", LeadIn, n, n);
						break;
					}
					case 8:
					{
						uint32 v = DeNibble(View.Aligned(), b->Bytes);
						uint32 n = IfSwap(v, Little);
						View.Out.Print("%s%i (0x%08.8x)\n", LeadIn, n, n);
						break;
					}
					case 16:
					{
						uint64 v = DeNibble(View.Aligned(), b->Bytes);
						uint64 n = IfSwap(v, Little);
						View.Out.Print("%s%I64u (0x%016.16I64x)\n", LeadIn, n, n);
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
				if (d->CastInt(View, Little) != Val)
				{
					View.Out.Print("%sValue Mismatch!\n", Tabs);
					return false;
				}
			}

			View.Seek(b);
		}
		
		return true;
	}
	
	bool DoString(VarDef *d, ViewContext &View, int &ArrayLength)
	{
		if (!d->Hidden)
		{
			bool Long =  ArrayLength >= 256;
			if (Long)
			{
				View.Out.Print("%s%s[%i]\n", Tabs, d->Name, ArrayLength);
			}
			else if (d->Type->Base->Bytes == 1)
			{
				// char *u = (char*) LgiNewConvertCp("utf-8", Data, "iso-8859-1", Length);
				char *u = DisplayString((char*)View.Aligned(), ArrayLength);

				View.Out.Print("%s%s = '%s'\n", Tabs, d->Name, u);
				if (u && d->Value.Str())
				{
					if (strnicmp(u, d->Value.Str(), ArrayLength) != 0)
					{
						View.Out.Print("%sValue Mismatch!\n", Tabs);
						DeleteArray(u);
						return false;
					}
				}
				DeleteArray(u);
			}
			else if (d->Type->Base->Bytes == 2)
			{
				char16 *w = new char16[ArrayLength+1];
				char16 *u = 0;
				if (w)
				{
					char16 *Src = (char16*)View.Aligned();
					for (int i=0; i<ArrayLength; i++)
					{
						w[i] = IfSwap(Src[i], Little);
					}
					w[ArrayLength] = 0;

					// u = LgiNewUtf16To8(w);
					u = DisplayString(w, ArrayLength);
				}
				DeleteArray(w);

				View.Out.Print("%s%s = '%s'\n", Tabs, d->Name, u);
				if (d->Value.WStr())
				{
					if (StrnicmpW(u, d->Value.WStr(), ArrayLength) != 0)
					{
						View.Out.Print("%sValue Mismatch!\n", Tabs);
						DeleteArray(u);
						return false;
					}
				}
				DeleteArray(u);
			}
		}

		View.SeekBytes(ArrayLength * d->Type->Base->Bytes);		
		return true;
	}
	
	bool DoStrZ(VarDef *d, ViewContext &View, int &ArrayLength)
	{
		if (!d->Hidden && (d->Type->Base->Bytes < 8))
		{
			View.Out.Print("%s%s", Tabs, d->Name);
			if (ArrayLength > 1)
				View.Out.Print("[%i]:\n", ArrayLength);
			else
				View.Out.Print(" = ");
		}
		for (int n=0; n<ArrayLength && View.Len >= d->Type->Base->Bytes; n++)
		{
			if (!d->Hidden && (d->Type->Base->Bytes < 8))
			{
				if (ArrayLength > 1)
					View.Out.Print("\t%s[%i] = ", Tabs, n);
			}
			int zstringLen = 0;
			if (d->Type->Base->Bytes == 1)
			{
				while (*(View.Aligned() + zstringLen))
				{
					zstringLen++;
				}
				char *u = (char*) LgiNewConvertCp("utf-8", View.Aligned(), "iso-8859-1", zstringLen);
				if (!d->Hidden)
				{
					View.Out.Print("'%s'\n", u);
					if (u && d->Value.Str())
					{
						if (strnicmp(u, d->Value.Str(), zstringLen) != 0)
						{
							View.Out.Print("%sValue Mismatch!\n", Tabs);
							DeleteArray(u);
							return false;
						}
					}
				}
				DeleteArray(u);
			}
			else if (d->Type->Base->Bytes == 2)
			{
				while ((char16)*(View.Aligned() + zstringLen * d->Type->Base->Bytes))
				{
					zstringLen++;
				}
				char16 *w = new char16[zstringLen+1];
				char *u = 0;
				if (w)
				{
					char16 *Src = (char16*)View.Aligned();
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
					View.Out.Print("'%s'\n", u);
					if (d->Value.Str())
					{
						if (strnicmp(u, d->Value.Str(), zstringLen) != 0)
						{
							View.Out.Print("%sValue Mismatch!\n", Tabs);
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
				while ((uint64)*(View.Aligned() + zstringLen * d->Type->Base->Bytes))
				{
					zstringLen++;
				}
			}
			
			zstringLen += 1; // Take null terminator into account
			View.SeekBytes(zstringLen * d->Type->Base->Bytes);
		}
		
		return true;
	}

	bool DoMember(Member *Mem, ViewContext &View, ScopeType &Scope, int Depth)
	{
		if (!Mem)
			return false;

		ConditionDef *c;
		VarDef *d = Mem->IsVar();
		if (d)
		{
			int ArrayLength = 1;
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
				
				if (ValidStr(DimStr[0]))
				{
					// Resolve the array length of this var
					// Evaluate expression
					GScriptEngine e(App, App, NULL);
					GVariant v;
					if (e.EvaluateExpression(&v, this, DimStr[0]))
					{
						ArrayLength = v.CastInt32();

						if (ArrayLength < 0)
							ArrayLength = 0;

						BitReference Sz;
						Sz.Len = INT32_MAX;
						d->Sizeof(Sz);
						if (ArrayLength * Sz.AlignedSize() > View.Len)
							ArrayLength = View.Len / Sz.AlignedSize();
					}
					else
					{
						View.Out.Print("Error: evaluating the expression '%s'\n", DimStr[0]);
					}
				}
				
				DimStr.DeleteArrays();
			}

			if (ArrayLength == 0)
				return true;

			if (d->Type->Base)
			{
				Scope.Addr[Scope.Pos] = View;

				Basic *b = d->Type->Base;
				switch (b->Type)
				{
					default:
						LgiAssert(!"Not impl");
						break;
					case TypeInteger:
					{
						if (!DoInt(d, View, ArrayLength))
							return false;
						break;
					}
					case TypeFloat:
					{
						if (!DoFloat(d, View, ArrayLength))
							return false;
						break;
					}
					case TypeNibble:
					{
						if (!DoNibble(d, View, ArrayLength))
							return false;
						break;
					}
					case TypeChar:
					{
						if (!DoString(d, View, ArrayLength))
							return false;
						break;
					}
					case TypeStrZ:
					{
						if (!DoStrZ(d, View, ArrayLength))
							return false;
						break;
					}
				}
			}
			else if (d->Type->Cmplex)
			{
				StructDef *s = d->Type->Cmplex;
				StructDef *sub = s->MatchChild(View, Little);

				if (d->Debug)
				{
					int asd=0;
				}

				int Offset = View.Offset();
				if (sub)
					View.Out.Print("%s%s.%s", Tabs, sub->Name, d->Name);
				else
					View.Out.Print("%s%s", Tabs, d->Name);
				if (ArrayLength != 1)
					View.Out.Print("[%i]", ArrayLength);
				View.Out.Print(" (@ %i/0x%x) =\n", Offset, Offset);
				
				if (ArrayLength == 1)
					View.Out.Print("%s{\n", Tabs);
				
				for (int i=0; (ArrayLength < 0 || i < ArrayLength) && View.Len > 0; i++)
				{
					s = d->Type->Cmplex;
					if (i)
						sub = s->MatchChild(View, Little);
					if (sub)
						s = sub;

					if (ArrayLength != 1)
					{
						Offset = View.Offset();
						View.Out.Print("%s  [%i] (%s @ %i/0x%x)\n", Tabs, i, s->Name, Offset, Offset);
					}

					if (!DoStruct(s, View, Little, Depth + 1 + (ArrayLength != 1 ? 1 : 0)))
					{
						return false;
					}
					
					SetTabs(Depth);
				}
				
				if (ArrayLength == 1)
					View.Out.Print("%s}\n", Tabs);
			}
		}
		else if (c = Mem->IsCondition())
		{
			GStringPipe p;
			for (unsigned n=0; n<c->Expression.Length(); n++)
			{
				p.Print(" %S", c->Expression[n]);
			}
			GAutoString Exp(p.NewStr());
			GScriptEngine e(App, App, NULL);
			GVariant v;
			if (!e.EvaluateExpression(&v, this, Exp))
			{
				View.Out.Print("Error: Couldn't evaluate '%s'\n", Exp.Get());
				return false;
			}

			c->Eval = v.CastInt32() != 0;
			View.Out.Print("%s// Expression:%s = %i\n", Tabs, Exp.Get(), c->Eval);
			if (c->Eval)
			{
				int InitLen = Stack.Length();
				ScopeType &Sub = Stack.New();
				Sub.Members = &c->Members;
				for (Sub.Pos=0; Sub.Pos<c->Members.Length(); Sub.Pos++)
				{
					Member *Mem = c->Members[Sub.Pos];

					// This saves the member address in case we need to resolve
					// one of the members to an array index.
					c->Addr[Sub.Pos] = View;

					if (!DoMember(Mem, View, Sub, Depth))
						return false;
				}
				Stack.Length(InitLen);
			}						
		}
		
		return true;
	}

	void SetTabs(int Depth)
	{
		ZeroObj(Tabs);
		memset(Tabs, ' ', Depth * 2);
	}

	bool DoStruct(StructDef *s, ViewContext &View, bool little, int Depth = 0)
	{
		SetTabs(Depth);
		
		uint32 InitLen = Stack.Length();
		ScopeType &Scope = Stack.New();
		Scope.Members = &s->Members;
		Little = little;

		bool Error = false;
		for (Scope.Pos=0; Scope.Pos<s->Members.Length() && View.Len > 0; Scope.Pos++)
		{
			Member *Mem = s->Members[Scope.Pos];
			if (!DoMember(Mem, View, Scope, Depth))
			{
				Error = true;
				break;
			}			
		}

		Stack.Length(InitLen);
		return !Error;
	}

	void Visualise(char *Data, int Len, GStream &Out, bool Little)
	{
		StructDef *Main = GetStruct("Main");
		if (Main)
		{
			ViewContext Ctx(Out);
			Ctx.Base = (uint8*)Data;
			Ctx.Ptr = (uint8*)Data;
			Ctx.Len = Len;
			Ctx.Bit = 0;
			DoStruct(Main, Ctx, Little);
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
			v->Base->Type = TypeInteger;
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
			v->Base->Type = TypeInteger;
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
		bool IsDebug = false;

		if (!XCmp(t, "hidden"))
		{
			IsHidden = true;
			t = State.NextW();
		}

		if (!XCmp(t, "debug"))
		{
			IsDebug = true;
			t = State.NextW();
		}

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
		Var->Debug = IsDebug;
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
			Var->Type->Base->Bits = AtoiW(t);
			LgiAssert(Var->Type->Base->Bits != 0);
			
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

	bool DoDefine(CompileState &State)
	{
		char *Name = State.NextA();
		while (*State.s && strchr(WhiteSpace, *State.s))
			State.s++;
		char16 *Eol = StrchrW(State.s, '\n');
		if (!Eol) Eol = State.s + StrlenW(State.s);
		GAutoWString Value(NewStrW(State.s, Eol - State.s));		
		State.s = *Eol ? Eol + 1 : Eol;
		
		Defines.Add(Name, Value.Release());
		return true;
	}

	bool Compile()
	{
		CompileState State(GetBody());
		Compiled.DeleteObjects();
		if (State.Base)
		{
			#define CheckTok(lit) \
				if (!(t && XCmp(t, #lit) == 0)) \
				{ \
					char m[256], *u = LgiNewUtf16To8(t); \
					sprintf(m, "expecting '%s', got '%s'", lit, u); \
					Err(State, m); \
					DeleteArray(u) \
					State.Error = true; \
					break; \
				}
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
					if (!DoDefine(State))
						return false;
				}
				else if (!XCmp(t, "struct"))
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
							
							if (!XCmp(t, "#define"))
							{
								if (!DoDefine(State))
									return false;
							}
							else if (!XCmp(t, "if"))
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
							else if (!XCmp(t,"}"))
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

		Name("Map Editor");
		if (Attach(0))
		{
			LoadFromResource(IDD_MAP_EDIT, this);
			Txt = dynamic_cast<GTextView3*>(FindControl(IDC_TEXT));
			if (Txt)
			{
				Txt->Sunken(true);
				OnPosChange();
			}

			GRect r(0, 0, 1000, 900);
			SetPos(r);
			MoveSameScreen(App);

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
GVisualiseView::GVisualiseView(AppWnd *app, char *DefVisual)
{
	App = app;
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
				char *f = Files[i];
				StructureMap *sm = new StructureMap(App, f);
				if (sm)
				{
					Map->Lst->Insert(sm);
					sm->Select(DefVisual && stristr(f, DefVisual));
				}
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
			if (f == GNotifyItem_DoubleClick)
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

