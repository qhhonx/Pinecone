#include "../h/PineconeProgram.h"
#include "../h/AllOperators.h"
#include "../h/StackFrame.h"
#include "../h/Namespace.h"
#include "../h/CppProgram.h"
#include "../h/utils/stringUtils.h"

#define CONCAT(a,b) a##_##b
#define GET_TYPES_Tuple(t0, t1) t0, t1

#define getPncnType(typeIn) CONCAT(PNCN, typeIn)
#define getCppType(typeIn) CONCAT(CPP, typeIn)

#define CPP_Dub double
#define CPP_Byte unsigned char
#define CPP_Int int
#define CPP_Bool bool
#define CPP_Void char

#define PNCN_String String
#define PNCN_Dub Dub
#define PNCN_Int Int
#define PNCN_Byte Byte
#define PNCN_Bool Bool
#define PNCN_Void Void
//#define PNCN_Tuple(t1, t2) Type(new TypeData(vector<Type>({PNCN##_##t1, PNCN##_##t2}), ""))
#define PNCN_Tuple(t1, t2) makeTuple(vector<NamedType>({{"a", t1}, {"b", t2}}), true)

#define LAMBDA_HEADER [](void* leftIn, void* rightIn)->void*
#define ADD_CPP_HEADER [](Action left, Action right, CppProgram* prog)->void

#define retrn out=

#define GET_PTR_VAL(typeIn, varInName) =*((getCppType(typeIn)*)(varInName))

#define DO_INSTANTIATE(typeIn, varOutName, valIn) getCppType(typeIn) varOutName valIn;
#define DONT_INSTANTIATE(typeIn, varOutName, valIn) ;
#define INSTANTIATE_CPP_TUPLE(t0, t1, varOutName, valIn)\
	DO_INSTANTIATE(t0, CONCAT(varOutName, 0), valIn)\
	DO_INSTANTIATE(t1, CONCAT(varOutName, 1), ((char *)valIn)+sizeof(getCppType(t0)))\

#define DO_RETURN_VAL(typeIn, varName) void* outPtr=malloc(getPncnType(typeIn)->getSize()); memcpy(outPtr, &varName, getPncnType(typeIn)->getSize()); return outPtr;
#define DONT_RETURN_VAL(typeIn, varName) return nullptr;

#define INSTANTIATE_String DO_INSTANTIATE
#define INSTANTIATE_Dub DO_INSTANTIATE
#define INSTANTIATE_Int DO_INSTANTIATE
#define INSTANTIATE_Byte DO_INSTANTIATE
#define INSTANTIATE_Bool DO_INSTANTIATE
#define INSTANTIATE_Void DONT_INSTANTIATE
#define INSTANTIATE_Tuple__(typeIn, varOutName, valIn) INSTANTIATE_CPP_TUPLE(typeIn, varOutName, valIn)
#define INSTANTIATE_Tuple_(typeIn, varOutName, valIn) INSTANTIATE_Tuple__(GET_TYPES##_##typeIn, varOutName, valIn)
#define INSTANTIATE_Tuple(t1, t2) INSTANTIATE_Tuple_

#define RETURN_Dub DO_RETURN_VAL
#define RETURN_Int DO_RETURN_VAL
#define RETURN_Byte DO_RETURN_VAL
#define RETURN_Bool DO_RETURN_VAL
#define RETURN_Void DONT_RETURN_VAL

#define func(nameText, leftType, rightType, returnType, lambdaBody, cpp)					\
addAction(nameText, getPncnType(leftType), getPncnType(rightType), getPncnType(returnType), LAMBDA_HEADER\
{																							\
	INSTANTIATE##_##leftType(leftType, left, GET_PTR_VAL(leftType, leftIn))					\
	INSTANTIATE##_##rightType(rightType, right, GET_PTR_VAL(rightType, rightIn))			\
	INSTANTIATE##_##returnType(returnType, out, ;)											\
	lambdaBody;																				\
	CONCAT(RETURN, returnType)(returnType, out)												\
},																							\
cpp																							\
)																							\

Action voidAction;

//shared_ptr<StackFrame> stdLibStackFrame;
Namespace globalNamespace;
Namespace table;

string getText(Operator op) {return op->getText();}
string getText(string in) {return in;}

template<typename T>
void addAction(T id, Type leftType, Type rightType, Type returnType, function<void*(void*, void*)> lambda, function<void(Action inLeft, Action inRight, CppProgram* prog)> addCppToProg)
{
	globalNamespace->addNode(AstActionWrapper::make(lambdaAction(leftType, rightType, returnType, lambda, addCppToProg, getText(id))), getText(id));
}

void addType(Type type, string id)
{
	auto node=AstTypeType::make(type);
	node->setInput(globalNamespace, false, Void, Void);
	globalNamespace->addNode(move(node), id);
}

function<void(Action inLeft, Action inRight, CppProgram* prog)> stringToLambda(string cppCode)
{
	if (cppCode.empty())
	{
		return nullptr;
	}
	
	return [=](Action inLeft, Action inRight, CppProgram* prog)
	{
		int start=0;
		int i;
		
		do
		{
			i=searchInString(cppCode, "$", start);
			
			if (i<0)
			{
				prog->code(cppCode.substr(start, cppCode.size()-start));
			}
			else
			{
				prog->code(cppCode.substr(start, i-start));
				
				if (substringMatches(cppCode, i, "$."))
				{
					prog->pushExpr();
						inLeft->addToProg(prog);
						start=i+string("$.").size();
					prog->popExpr();
				}
				else if (substringMatches(cppCode, i, "$:"))
				{
					prog->pushExpr();
						inRight->addToProg(prog);
						start=i+string("$:").size();
					prog->popExpr();
				}
				else if (substringMatches(cppCode, i, "$$"))
				{
					prog->code("$");
					start=i+string("$$").size();
				}
				else
				{
					throw PineconeError("invalid '$' escape in C++ code: "+cppCode, INTERNAL_ERROR);
				}
			}
			
		} while (i>=0);
		
		//if (prog->getExprLevel()==0)
		//	prog->endln();
	};
}

void addAction(string text, Type leftType, Type rightType, Type returnType, function<void*(void*, void*)> lambda, string cpp)
{
	addAction(text, leftType, rightType, returnType, lambda, stringToLambda(cpp));
}

void addAction(Operator op, Type leftType, Type rightType, Type returnType, function<void*(void*, void*)> lambda, string cpp)
{
	addAction(op, leftType, rightType, returnType, lambda, stringToLambda(cpp));
}

Type IntArray=nullptr;

template<typename T>
inline T getValFromTuple(void* data, Type type, string name)
{
	OffsetAndType a=type->getSubType(name);
	
	if (!a.type)
		throw PineconeError("tried to get invalid property '"+name+"' from tuple "+type->getString(), INTERNAL_ERROR);
		
	return *((T*)((char*)data+a.offset));
}

template<typename T>
inline void setValInTuple(void* data, Type type, string name, T val)
{
	OffsetAndType a=type->getSubType(name);
	
	if (!a.type)
		throw PineconeError("tried to set invalid property '"+name+"' from tuple "+type->getString(), INTERNAL_ERROR);
	
	*((T*)((char*)data+a.offset))=val;
}

inline string pncnStr2CppStr(void* obj)
{
	int len=getValFromTuple<int>(obj, String, "_size");
	char * data=(char*)malloc((len+1)*sizeof(char));
	memcpy(data, getValFromTuple<char*>(obj, String, "_data"), len*sizeof(char));
	data[len]=0;
	string out(data);
	return out;
}

inline void* cppStr2PncnStr(string cpp)
{
	void * obj=malloc(String->getSize());
	char * strData=(char*)malloc(cpp.size()*sizeof(char));
	memcpy(strData, cpp.c_str(), cpp.size()*sizeof(char));
	
	*((int*)((char*)obj+String->getSubType("_size").offset))=cpp.size();
	*((char**)((char*)obj+String->getSubType("_data").offset))=strData;
	
	return obj;
}


/// add C++ funcs to program

void addToProgPnStr(CppProgram * prog)
{
	if (!prog->hasFunc("$pnStr"))
	{
		string strType=prog->getTypeCode(String);
		
		prog->addFunc("$pnStr", {{"const char *", "in"}}, strType,
			"int size = 0;\n"
			"while (in[size]) size++;\n"
			"unsigned char * data = (unsigned char *)malloc(size);\n"
			"memcpy(data, in, size);\n"
			"return "+strType+"(size, data);\n"
		);
	}
}

void addToProgCStr(CppProgram * prog)
{
	if (!prog->hasFunc("$cStr"))
	{
		if (prog->hasFunc("$cStr"))
			return;
		
		string strType=prog->getTypeCode(String);
		
		prog->addFunc("$cStr", {{strType, "in"}}, "char *",
			"char * out = (char *)malloc(in._size+1);\n"
			"memcpy(out, in._data, in._size);\n"
			"out[in._size] = 0;\n"
			"return out;\n"
		);
	}
}

void addToProgSubStr(CppProgram * prog)
{
	if (!prog->hasFunc("$subStr"))
	{
		string strType=prog->getTypeCode(String);
		
		prog->addFunc("$subStr", {{strType, "in"}, {"int", "a"}, {"int", "b"}}, strType,
			"int size = b - a;\n"
			"unsigned char * data = (unsigned char *)malloc(size);\n"
			"memcpy(data, in._data + a, size);\n"
			"return "+strType+"(size, data);\n"
		);
	}
}

void addToProgIntToStr(CppProgram * prog)
{
	if (!prog->hasFunc("$intToStr"))
	{
		string strType=prog->getTypeCode(String);
		
		prog->addFunc("$intToStr", {{"int", "in"}}, prog->getTypeCode(String),
			"bool neg = in < 0;\n"
			"if (neg) in *= -1;\n"
			"int val = in;\n"
			"int size = 0;\n"
			"while (val)\n"
			"{\n"
			"	size++;\n"
			"	val /= 10;\n"
			"}\n"
			"if (size == 0)\n\tsize = 1;\n"
			"if (neg)\n\tsize++;\n"
			"unsigned char * data = (unsigned char *)malloc(size);\n"
			"val = in;\n"
			"for (int i = 0; i<(neg ? size-1 : size); i++)\n"
			"{\n"
			"	data[size-i-1] = (val % 10) + '0';\n"
			"	val /= 10;\n"
			"}\n"
			"if (neg)\n\tdata[0] = '-';\n"
			"return "+strType+"(size, data);\n"
		);
	}
}

void addToProgStrToInt(CppProgram * prog)
{
	if (!prog->hasFunc("$strToInt"))
	{
		string strType=prog->getTypeCode(String);
		
		prog->addFunc("$strToInt", {{strType, "in"}}, "int",
			"int out = 0;\n"
			"for (int i = 0; i < in._size; i++)\n"
			"{\n"
			"	if (in._data[i] >= '0' && in._data[i] <= '9')\n"
			"	{\n"
			"		out = out * 10 + in._data[i] - '0';\n"
			"	}\n"
			"	else if (in._data[i] == '.')\n"
			"		break;\n"
			"}\n"
			"if (in._size > 0 && in._data[0] == '-')\n"
			"	out *= -1;\n"
			"return out;\n"
		);
	}
}

void addToProgStrToDub(CppProgram * prog)
{
	if (!prog->hasFunc("$strToDub"))
	{
		string strType=prog->getTypeCode(String);
		
		prog->addFunc("$strToDub", {{strType, "in"}}, "double",
			"double out = 0;\n"
			"int divider = 1;\n"
			"for (int i = 0; i < in._size; i++)\n"
			"{\n"
			"	if (divider == 1)\n"
			"	{\n"
			"		if (in._data[i] >= '0' && in._data[i] <= '9')\n"
			"			out = out * 10 + in._data[i] - '0';\n"
			"		else if (in._data[i] == '.')\n"
			"			divider = 10;\n"
			"	}\n"
			"	else\n"
			"	{\n"
			"		if (in._data[i] >= '0' && in._data[i] <= '9')\n"
			"		{\n"
			"			out += ((double)(in._data[i] - '0')) / (double)divider;\n"
			"			divider *= 10;\n"
			"		}\n"
			"	}\n"
			"}\n"
			"if (in._size > 0 && in._data[0] == '-')\n"
			"	out *= -1;\n"
			"return out;\n"
		);
	}
}

void addToProgConcatStr(CppProgram * prog)
{
	if (!prog->hasFunc("$concatStr"))
	{
		string strType=prog->getTypeCode(String);
		
		prog->addFunc("$concatStr", {{strType, "a"}, {strType, "b"}}, strType,
			"int newSize = a._size + b._size;\n"
			"unsigned char * newData = (unsigned char *)malloc(newSize);\n"
			"memcpy(newData, a._data, a._size);\n"
			"memcpy(newData + a._size, b._data, b._size);\n"
			"return "+strType+"(newSize, newData);\n"
		);
	}
}

void addToProgDoubleToStr(CppProgram * prog)
{
	if (!prog->hasFunc("$doubleToStr"))
	{
		addToProgIntToStr(prog);
		addToProgConcatStr(prog);
		addToProgPnStr(prog);
		
		string intToStr=prog->pnToCpp("$intToStr");
		string concat=prog->pnToCpp("$concatStr");
		string pnStr=prog->pnToCpp("$pnStr");
		
		prog->addFunc("$doubleToStr", {{"double", "in"}}, prog->getTypeCode(String),
			
			"long long b = (in - (long long)in) * 10000000000 * (in>=0 ? 1 : -1);\n"
			"if (b % 10 == 9)\n\tb += 1;\n"
			"while (b>0 && !(b % 10))\n\tb /= 10;\n"
			"return "+concat+"("+concat+"("+intToStr+"((int)in), "+pnStr+"(\".\")), "+intToStr+"(b));\n"
		);
	}
}

void addToProgAsciiToStr(CppProgram * prog)
{
	if (!prog->hasFunc("$asciiToStr"))
	{
		string strType=prog->getTypeCode(String);
		
		prog->addFunc("$asciiToStr", {{"int", "in"}}, strType,
			"unsigned char * data = (unsigned char *)malloc(1);\n"
			"*data = in;\n"
			"return "+strType+"(1, data);\n"
		);
	}
}

void addToProgGetInputLine(CppProgram * prog)
{
	if (!prog->hasFunc("$getInputLine"))
	{
		string strType=prog->getTypeCode(String);
		
		addToProgCStr(prog);
		
		prog->addFunc("$getInputLine", {{strType, "prompt"}}, strType,
			"printf("+prog->pnToCpp("$cStr")+"(prompt));\n"
			"fflush(stdout);\n"
			"size_t bufferSize = 4096;\n"
			"char buffer[bufferSize];\n"
			"if (fgets(buffer, bufferSize, stdin))\n"
			"{\n"
			"	int size = strlen(buffer);\n"
			"	while (size > 0 && buffer[size - 1] == '\\n')\n"
			"		size-=1;\n"
			"	unsigned char * data = (unsigned char *)malloc(size);\n"
			"	memcpy(data, buffer, size);\n"
			"	return "+strType+"(size, data);\n"
			"}\nelse\n{\n"
			"	return "+strType+"(0, NULL);\n"
			"}"
		);
	}
}

void addToProgEqStr(CppProgram * prog)
{
	if (!prog->hasFunc("$eqStr"))
	{
		string strType=prog->getTypeCode(String);
		
		prog->addFunc("$eqStr", {{strType, "a"}, {strType, "b"}}, "bool",
			"if (a._size != b._size)\n"
			"	return false;\n"
			"for (int i = 0; i < a._size; i++)\n"
			"{\n"
			"	if (a._data[i] != b._data[i])\n"
			"		return false;\n"
			"}\n"
			"return true;\n"
		);
	}
}

void addToProgRunCmd(CppProgram * prog)
{
	if (!prog->hasFunc("$runCmd"))
	{
		addToProgPnStr(prog);
		addToProgCStr(prog);
		addToProgConcatStr(prog);
		addToProgAsciiToStr(prog);
				
		string strType=prog->getTypeCode(String);
		
		prog->addFunc("$runCmd", {{strType, "cmd"}}, strType,
			strType+" result = "+prog->pnToCpp("$pnStr")+"(\"\");\n"
			"FILE* pipe = popen(cStr(cmd), \"r\");\n"
			"if (!pipe)\n"
			"	return result;\n"
			"while (!feof(pipe)) {\n"
			"	char c;\n"
			"	if ((c=getc(pipe)) != EOF)\n"
			"	{\n"
			"		result="+prog->pnToCpp("$concatStr")+"(result, "+prog->pnToCpp("$asciiToStr")+"(c));\n"
			"	}\n"
			"}\n"
			"pclose(pipe);\n"
			"return result;\n"
		);
	}
}

void addToProgMakeIntArray(CppProgram * prog)
{
	if (!prog->hasFunc("$makeIntArray"))
	{
		string aryType=prog->getTypeCode(IntArray);
		
		prog->addFunc("$makeIntArray", {{"int", "size"}}, aryType,
			"int * data = (int *)malloc(size * sizeof(int));\n"
			"return "+aryType+"(size, data);\n"
		);
	}
}

void addToProgStrWithEscapedNames(CppProgram * prog, string str)
{
	int i=0;
	string buffer = "";
	
	while (i < (int)str.size())
	{
		if (str[i] == '$')
		{
			i++;
			prog->code(buffer);
			buffer = "";
			while (i < (int)str.size())
			{
				char c = str[i];
				
				if (!(
					(c >= '0' && c <= '9') ||
					(c >= 'a' && c <= 'z') ||
					(c >= 'A' && c <= 'Z')
				)) break;
				
				buffer += c;
				i++;
			}
			if (buffer.empty())
				prog->code("$");
			else
				prog->name(buffer);
			buffer = "";
		}
		else
		{
			buffer += str[i];
			i++;
		}
	}
	
	prog->code(buffer);
}


/// setup Pinecone std lib

void basicSetup()
{
	table=globalNamespace=NamespaceData::makeRootNamespace();
	
	//this makes a new void action after type constants have been created, if left to the original the Void type may not be set up yet
	voidAction=createNewVoidAction();
}

void populateBasicTypes()
{
	String=makeTuple(vector<NamedType>{NamedType{"_size", Int}, NamedType{"_data", Byte->getPtr()}}, false);
	
	addType(Void, "Void");
	addType(Bool, "Bool");
	addType(Byte, "Byte");
	addType(Int, "Int");
	addType(Dub, "Dub");
	addType(String, "String");
	addType(Whatev, "Whatev");
}

void populateConstants()
{
	table->addNode(AstActionWrapper::make(voidAction), "void");
	
	bool trueVal=true;
	table->addNode(AstActionWrapper::make(constGetAction(&trueVal, Bool, "tru")), "tru");
	
	bool falseVal=false;
	table->addNode(AstActionWrapper::make(constGetAction(&falseVal, Bool, "fls")), "fls");
	
	// version constant
	{
		Type versionTupleType=
			makeTuple(vector<NamedType>{
				NamedType{"x", Int},
				NamedType{"y", Int},
				NamedType{"z", Int},
			}, false);
		
		void* versionTupleData=malloc(versionTupleType->getSize());
		
		setValInTuple(versionTupleData, versionTupleType, "x", VERSION_X);
		setValInTuple(versionTupleData, versionTupleType, "y", VERSION_Y);
		setValInTuple(versionTupleData, versionTupleType, "z", VERSION_Z);
		
		table->addNode(
			AstActionWrapper::make(
				constGetAction(
					versionTupleData,
					versionTupleType,
					"VERSION"
				)
			),
			"VERSION"
		);
	}
	
	// OS
	bool isLinux=false;
	bool isWindows=false;
	bool isUnix=false;
	bool isMac=false;
	
	#ifdef __linux__
		isLinux=true;
		isUnix=true;
	
	#else
		
		#ifdef _WIN32 //works forboth 32 and 64 bit systems
			isWindows=true;
		
		#else
			#ifdef __APPLE__
				isMac=true;
				isUnix=true;
			#endif // __APPLE__
		#endif // _WIN32
		
	#endif // __linux__
	
	table->addNode(AstActionWrapper::make(constGetAction(&isLinux, Bool, "OS_IS_LINUX")), "OS_IS_LINUX");
	table->addNode(AstActionWrapper::make(constGetAction(&isWindows, Bool, "OS_IS_WINDOWS")), "OS_IS_WINDOWS");
	table->addNode(AstActionWrapper::make(constGetAction(&isMac, Bool, "OS_IS_MAC")), "OS_IS_MAC");
	table->addNode(AstActionWrapper::make(constGetAction(&isUnix, Bool, "OS_IS_UNIX")), "OS_IS_UNIX");
	
	func("IS_TRANSPILED", Void, Void, Bool,
		retrn false;
	,
		"true"
	);
}

void populateOperators()
{
	
	/// +
	
	func(ops->plus, Int, Int, Int,
		retrn left+right;
	,
		"$. + $:"
	);
	
	func(ops->plus, Dub, Dub, Dub,
		retrn left+right;
	,
		"$. + $:"
	);
	
	
	/// -
	
	func(ops->minus, Int, Int, Int,
		retrn left-right;
	,
		"$. - $:"
	);
	
	func(ops->minus, Dub, Dub, Dub,
		retrn left-right;
	,
		"$. - $:"
	);
	
	func(ops->minus, Void, Int, Int,
		retrn -right;
	,
		"- $:"
	);
	
	func(ops->minus, Void, Dub, Dub,
		retrn -right;
	,
		"- $:"
	);
	
	
	/// *
	
	func(ops->multiply, Int, Int, Int,
		retrn left*right;
	,
		"$. * $:"
	);
	
	func(ops->multiply, Dub, Dub, Dub,
		retrn left*right;
	,
		"$. * $:"
	);
	
	
	/// /
	
	func(ops->divide, Int, Int, Int,
		retrn left/right;
	,
		"$. / $:"
	);
	
	func(ops->divide, Dub, Dub, Dub,
		retrn left/right;
	,
		"$. / $:"
	);
	
	
	/// %
	
	func(ops->mod, Int, Int, Int,
		retrn left%right;
	,
		"$. % $:"
	);
	
	func(ops->mod, Dub, Dub, Dub,
		retrn left-int(left/right)*right;
	,
		"$. - int($. / $:) * $:"
	);
	
	/// =
	
	func(ops->equal, Bool, Bool, Bool,
		retrn left==right;
	,
		"$. == $:"
	);
	
	func(ops->equal, Int, Int, Bool,
		retrn left==right;
	,
		"$. == $:"
	);
	
	func(ops->equal, Dub, Dub, Bool,
		retrn left==right;
	,
		"$. == $:"
	);
	
	/// >
	
	func(ops->greater, Int, Int, Bool,
		retrn left>right;
	,
		"$. > $:"
	);
	
	func(ops->greater, Dub, Dub, Bool,
		retrn left>right;
	,
		"$. > $:"
	);
	
	// <
	func(ops->less, Int, Int, Bool,
		retrn left<right;
	,
		"$. < $:"
	);
	
	func(ops->less, Dub, Dub, Bool,
		retrn left<right;
	,
		"$. < $:"
	);
	
	// >=
	func(ops->greaterEq, Int, Int, Bool,
		retrn left>=right;
	,
		"$. >= $:"
	);
	
	func(ops->greaterEq, Dub, Dub, Bool,
		retrn left>=right;
	,
		"$. >= $:"
	);
	
	// <=
	func(ops->lessEq, Int, Int, Bool,
		retrn left<=right;
	,
		"$. <= $:"
	);
	
	func(ops->lessEq, Dub, Dub, Bool,
		retrn left<=right;
	,
		"$. <= $:"
	);
	
	// !
	func(ops->notOp, Void, Bool, Bool,
		retrn !right;
	,
		"!$:"
	);
	
	func(ops->notOp, Void, Int, Bool,
		retrn right==0;
	,
		"$: == 0"
	);
	
	func(ops->notOp, Void, Dub, Bool,
		retrn right==0;
	,
		"$: == 0"
	);
}

void populateConverters()
{
	///initalizers
	
	func("Bool", Void, Void, Bool,
		retrn false;
	,
		"false"
	);
	
	func("Int", Void, Void, Int,
		retrn 0;
	,
		"0"
	);
	
	func("Dub", Void, Void, Dub,
		retrn 0.0;
	,
		"0.0"
	);
	
	func("Byte", Void, Void, Byte,
		retrn 0;
	,
		"0"
	);
	
	
	///casting
	
	//to bool
	func("Bool", Void, Bool, Bool,
		retrn right
	,
		"$:"
	);
	
	func("Bool", Void, Int, Bool,
		retrn right!=0
	,
		"($: != 0)"
	);
	
	func("Bool", Void, Dub, Bool,
		retrn right!=0
	,
		"($: != 0.0)"
	);
	
	func("Bool", Bool, Void, Bool,
		retrn left
	,
		"$."
	);
	
	func("Bool", Int, Void, Bool,
		retrn left!=0
	,
		"($. != 0)"
	);
	
	func("Bool", Dub, Void, Bool,
		retrn left!=0
	,
		"($. != 0.0)"
	);
	
	//to Byte
	func("Byte", Void, Bool, Byte,
		retrn (right?1:0)
	,
		"($: ? 1 : 0)"
	);
	
	func("Byte", Void, Int, Byte,
		retrn (unsigned char)right
	,
		"((unsigned char)$:)"
	);
	
	func("Byte", Bool, Void, Byte,
		retrn (left?1:0)
	,
		"($. ? 1 : 0)"
	);
	
	func("Byte", Int, Void, Byte,
		retrn (unsigned char)left
	,
		"((unsigned char)$.)"
	);
	
	//to Int
	func("Int", Void, Bool, Int,
		retrn (right?1:0)
	,
		"($: ? 1 : 0)"
	);
	
	func("Int", Void, Byte, Int,
		retrn (int)right
	,
		"((int)$:)"
	);
	
	func("Int", Void, Dub, Int,
		retrn (int)right
	,
		"((int)$:)"
	);
	
	func("Int", Bool, Void, Int,
		retrn (left?1:0)
	,
		"($. ? 1 : 0)"
	);
	
	func("Int", Byte, Void, Int,
		retrn (int)left
	,
		"((int)$.)"
	);
	
	func("Int", Dub, Void, Int,
		retrn (int)left
	,
		"((int)$.)"
	);
	
	
	addAction("Int", String, Void, Int,
		LAMBDA_HEADER
		{
			int* out=(int*)malloc(sizeof(int));
			*out=stringToInt(pncnStr2CppStr(leftIn));
			return out;
		},
		ADD_CPP_HEADER
		{
			addToProgStrToInt(prog);
			
			prog->name("$strToInt");
			prog->pushExpr();
				left->addToProg(prog);
			prog->popExpr();
		}
	);
	
	//to Dub
	func("Dub", Void, Bool, Dub,
		retrn (right?1:0)
	,
		"($: ? 1.0 : 0.0)"
	);
		
	func("Dub", Void, Int, Dub,
		retrn (double)right
	,
		"((double)$:)"
	);
	
	func("Dub", Bool, Void, Dub,
		retrn (left?1:0)
	,
		"($. ? 1.0 : 0.0)"
	);
		
	func("Dub", Int, Void, Dub,
		retrn (double)left
	,
		"((double)$.)"
	);
	
	addAction("Dub", String, Void, Dub,
		LAMBDA_HEADER
		{
			double* out=(double*)malloc(sizeof(double));
			*out=stringToDouble(pncnStr2CppStr(leftIn));
			return out;
		},
		ADD_CPP_HEADER
		{
			addToProgStrToDub(prog);
			
			prog->name("$strToDub");
			prog->pushExpr();
				left->addToProg(prog);
			prog->popExpr();
		}
	);
}

void populateStdFuncs()
{
	//print
	
	func("print", Void, Void, Void,
		cout << endl;
	,
		"printf(\"\\n\")"
	);
	
	func("print", Void, Bool, Void,
		cout << (right?"tru":"fls") << endl;
	,
		"printf($:?\"tru\\n\":\"fls\\n\")"
	);
	
	func("print", Void, Byte, Void,
		cout << right << endl;
	,
		"printf(\"%c\", $:)"
	);
	
	func("print", Void, Int, Void,
		cout << right << endl;
	,
		"printf(\"%d\\n\", $:)"
	);
	
	func("print", Void, Dub, Void,
		cout << doubleToString(right) << endl;
	,
		ADD_CPP_HEADER
		{
			addToProgDoubleToStr(prog);
			addToProgCStr(prog);
			
			prog->code("printf");
			prog->pushExpr();
				prog->code("\"%s\\n\", ");
				prog->name("$cStr");
				prog->pushExpr();
					prog->name("$doubleToStr");
					prog->pushExpr();
						right->addToProg(prog);
					prog->popExpr();
				prog->popExpr();
			prog->popExpr();
		}
	);
	
	addAction("print", Void, String, Void,
		LAMBDA_HEADER
		{
			cout << pncnStr2CppStr(rightIn) << endl;
			return nullptr;
		},
		ADD_CPP_HEADER
		{
			addToProgCStr(prog);
			
			prog->code("printf");
			prog->pushExpr();
				prog->code("\"%s\\n\", ");
				prog->name("$cStr");
				prog->pushExpr();
					right->addToProg(prog);
				prog->popExpr();
			prog->popExpr();
		}
	);
}

void populateTypeInfoFuncs()
{
	globalNamespace->addNode(
		AstWhatevToActionFactory::make(
			[](Type leftType, Type rightType) -> Action
			{
				if (leftType->isVoid() || !rightType->isVoid())
					return nullptr;
				
				string val=leftType->getName();
				
				return lambdaAction(
					leftType,
					rightType,
					String,
					
					[=](void* leftIn, void* rightIn) -> void*
					{
						return cppStr2PncnStr(val);
					},
					
					[=](Action inLeft, Action inRight, CppProgram* prog)
					{
						void* pnStr=cppStr2PncnStr(val);
						constGetAction(pnStr, String, val)->addToProg(prog);
						free(pnStr);
					},
					"typeName"
				);
			}
		),
		"typeName"
	);
	
	globalNamespace->addNode(
		AstWhatevToActionFactory::make(
			[](Type leftType, Type rightType) -> Action
			{
				if (leftType->isVoid() || !rightType->isVoid())
					return nullptr;
				
				int val=leftType->getSize();
				
				return lambdaAction(
					leftType,
					rightType,
					Int,
					
					[=](void* leftIn, void* rightIn) -> void*
					{
						return &(*(int*)malloc(sizeof(int))=val);
					},
					
					[=](Action inLeft, Action inRight, CppProgram* prog)
					{
						constGetAction(&val, Int, to_string(val))->addToProg(prog);
					},
					"typeSize"
				);
			}
		),
		"typeSize"
	);
}

void populateMemManagementFuncs()
{
	globalNamespace->addNode(
		AstWhatevToActionFactory::make(
			[](Type leftType, Type rightType) -> Action
			{
				if (!leftType->isVoid() || rightType->isVoid())
					return nullptr;
				
				return lambdaAction(
					leftType,
					rightType,
					rightType->getPtr(),
					
					[=](void* leftIn, void* rightIn) -> void*
					{
						size_t size=rightType->getSize();
						void ** dataPtr=(void**)malloc(sizeof(void*));
						*dataPtr=malloc(size);
						memcpy(*dataPtr, rightIn, size);
						return dataPtr;
					},
					
					[=](Action inLeft, Action inRight, CppProgram* prog)
					{
						prog->code("&");
						prog->pushExpr();
							string typeCode=prog->getTypeCode(rightType);
							prog->code("(*(" + typeCode + "*)malloc(sizeof(" + typeCode + ")))");
							prog->code(" = ");
							prog->pushExpr();
								inRight->addToProg(prog);
							prog->popExpr();
						prog->popExpr();
						
						//"&((*($type)malloc(sizeof($type)))=($data))"
					},
					"new"
				);
			}
		),
		"new"
	);
	
	globalNamespace->addNode(
		AstWhatevToActionFactory::make(
			[](Type leftType, Type rightType) -> Action
			{
				if (leftType->getType()!=TypeBase::PTR || leftType->isVoid() || !rightType->isVoid())
					return nullptr;
				
				return lambdaAction(
					leftType,
					rightType,
					leftType->getSubType(),
					
					[=](void* leftIn, void* rightIn) -> void*
					{
						size_t size=leftType->getSubType()->getSize();
						void * data=malloc(size);
						memcpy(data, *(void**)leftIn, size);
						return data;
					},
					
					[=](Action inLeft, Action inRight, CppProgram* prog)
					{
						prog->code("*");
						prog->pushExpr();
							inLeft->addToProg(prog);
						prog->popExpr();
					},
					"dif"
				);
			}
		),
		"dif"
	);
	
	globalNamespace->addNode(
		AstWhatevToActionFactory::make(
			[](Type leftType, Type rightType) -> Action
			{
				if (leftType->getType()!=TypeBase::PTR || !rightType->matches(leftType->getSubType()))
					return nullptr;
				
				return lambdaAction(
					leftType,
					rightType,
					Void,
					
					[=](void* leftIn, void* rightIn) -> void*
					{
						memcpy(*(void**)leftIn, rightIn, rightType->getSize());
						return nullptr;
					},
					
					[=](Action inLeft, Action inRight, CppProgram* prog)
					{
						prog->code("*");
						prog->pushExpr();
							inLeft->addToProg(prog);
						prog->popExpr();
						prog->code(" = ");
						prog->pushExpr();
							inRight->addToProg(prog);
						prog->popExpr();
					},
					"dif"
				);
			}
		),
		"dif"
	);
}

void populateStringFuncs()
{
	addAction("String", Void, Void, String,
		LAMBDA_HEADER
		{
			return cppStr2PncnStr("");
		},
		ADD_CPP_HEADER
		{
			addToProgPnStr(prog);
			prog->name("$pnStr");
			prog->pushExpr();
				prog->code("\"\"");
			prog->popExpr();
		}
	);
	
	addAction("String", String, Void, String,
		LAMBDA_HEADER
		{
			return cppStr2PncnStr(pncnStr2CppStr(leftIn));
		},
		ADD_CPP_HEADER
		{
			left->addToProg(prog);
		}
	);
	
	addAction("String", Int, Void, String,
		LAMBDA_HEADER
		{
			return cppStr2PncnStr(to_string(*((int*)leftIn)));
		},
		ADD_CPP_HEADER
		{
			addToProgIntToStr(prog);
			
			prog->name("$intToStr");
			prog->pushExpr();
				left->addToProg(prog);
			prog->popExpr();
		}
	);
	
	addAction("String", Dub, Void, String,
		LAMBDA_HEADER
		{
			return cppStr2PncnStr(doubleToString(*((double*)leftIn)));
		},
		ADD_CPP_HEADER
		{
			addToProgDoubleToStr(prog);
			
			prog->name("$doubleToStr");
			prog->pushExpr();
				left->addToProg(prog);
			prog->popExpr();
		}
	);
	
	addAction("String", Bool, Void, String,
		LAMBDA_HEADER
		{
			if (*((bool*)leftIn))
			{
				return cppStr2PncnStr("tru");
			}
			else
			{
				return cppStr2PncnStr("fls");
			}
		},
		ADD_CPP_HEADER
		{
			addToProgPnStr(prog);
			
			prog->name("$pnStr");
			prog->pushExpr();
				prog->pushExpr();
					left->addToProg(prog);
				prog->popExpr();
				prog->code(" ? \"tru\" : \"fls\"");
			prog->popExpr();
		}
	);
	
	addAction("len", String, Void, Int,
		LAMBDA_HEADER
		{
			int* out=(int*)malloc(sizeof(int));
			*out=getValFromTuple<int>(leftIn, String, "_size");
			return out;
		},
		ADD_CPP_HEADER
		{
			getElemFromTupleAction(String, "_size")->addToProg(left, voidAction, prog);
		}
	);
	
	addAction("ascii", Int, Void, String,
		LAMBDA_HEADER
		{
			int val=*((int*)leftIn);
			if (val<0 || val>=256)
			{
				throw PineconeError("tried to make ascii string out of value "+val, RUNTIME_ERROR);
			}
			string out;
			out+=(char)val;
			return cppStr2PncnStr(out);
		},
		ADD_CPP_HEADER
		{
			addToProgAsciiToStr(prog);
			
			prog->name("$asciiToStr");
			prog->pushExpr();
				left->addToProg(prog);
			prog->popExpr();
		}
	);
	
	addAction("at", String, Int, Int,
		LAMBDA_HEADER
		{
			int index=*((int*)rightIn);
			int * out=(int*)malloc(sizeof(int));
			string str=pncnStr2CppStr(leftIn);
			if (index<0 || index>=int(str.size()))
			{
				throw PineconeError("tried to access location "+to_string(index)+" in string "+to_string(str.size())+" long", RUNTIME_ERROR);
			}
			*out=str[index];
			return out;
		},
		ADD_CPP_HEADER
		{
			prog->code("(int)");
			getElemFromTupleAction(String, "_data")->addToProg(left, voidAction, prog);
			prog->code("[");
			prog->pushExpr();
				right->addToProg(prog);
			prog->popExpr();
			prog->code("]");
		}
	);
	
	addAction("sub", String, makeTuple(vector<NamedType>{NamedType{"a", Int}, NamedType{"b", Int}}, true), String,
		LAMBDA_HEADER
		{
			Type RightType=makeTuple(vector<NamedType>{NamedType{"a", Int}, NamedType{"b", Int}}, true);
			int start=getValFromTuple<int>(rightIn, RightType, "a");
			int end=getValFromTuple<int>(rightIn, RightType, "b");
			string str=pncnStr2CppStr(leftIn);
			if (start<0 || end>int(str.size()) || start>end)
			{
				throw PineconeError("invalid arguments sent to String.sub", RUNTIME_ERROR);
			}
			return cppStr2PncnStr(str.substr(start, end-start));
		},
		ADD_CPP_HEADER
		{
			addToProgSubStr(prog);
			
			prog->name("$subStr");
			prog->pushExpr();
				left->addToProg(prog);
				prog->code(", ");
				getElemFromTupleAction(right->getReturnType(), "a")->addToProg(right, voidAction, prog);
				prog->code(", ");
				getElemFromTupleAction(right->getReturnType(), "b")->addToProg(right, voidAction, prog);
			prog->popExpr();
		}
	);
	
	addAction("input", String, Void, String,
		LAMBDA_HEADER
		{
			string in;
			cout << pncnStr2CppStr(leftIn);
			std::getline (std::cin, in);
			return cppStr2PncnStr(in);
		},
		ADD_CPP_HEADER
		{
			addToProgGetInputLine(prog);
			
			prog->name("$getInputLine");
			prog->pushExpr();
				left->addToProg(prog);
			prog->popExpr();
		}
	);
	
	addAction(ops->plus, String, String, String,
		LAMBDA_HEADER
		{
			return cppStr2PncnStr(pncnStr2CppStr(leftIn)+pncnStr2CppStr(rightIn));
		},
		ADD_CPP_HEADER
		{
			addToProgConcatStr(prog);
			
			prog->name("$concatStr");
			prog->pushExpr();
				left->addToProg(prog);
				prog->code(", ");
				right->addToProg(prog);
			prog->popExpr();
		}
	);
	
	addAction(ops->equal, String, String, Bool,
		LAMBDA_HEADER
		{
			bool* out=(bool*)malloc(sizeof(bool));
			*out=pncnStr2CppStr(leftIn)==pncnStr2CppStr(rightIn);
			return out;
		},
		ADD_CPP_HEADER
		{
			addToProgEqStr(prog);
			
			prog->name("$eqStr");
			prog->pushExpr();
				left->addToProg(prog);
				prog->code(", ");
				right->addToProg(prog);
			prog->popExpr();
		}
	);
}

void populateIntArrayAndFuncs()
{
	TupleTypeMaker maker;
	maker.add("_size", Int);
	maker.add("_data", Int->getPtr());
	IntArray=maker.get(false);
	
	addType(IntArray, "IntArray");
	
	//func("IntArray", IntArray, Void, Int,
	//	retrn Tuple(right, (double)((int*)malloc(Int->getSize()*right)));
	//);
	
	addAction("IntArray", Void, Int, IntArray, LAMBDA_HEADER
		{
			char* out=(char*)malloc(IntArray->getSize());
			int sizeIn=*(int*)rightIn;
			int* intPtrData=(int *)malloc(Int->getSize()*sizeIn);
			setValInTuple<int>(out, IntArray, "_size", sizeIn);
			setValInTuple<int *>(out, IntArray, "_data", intPtrData);
			return out;
		},
		ADD_CPP_HEADER
		{
			addToProgMakeIntArray(prog);
			prog->name("$makeIntArray");
			prog->pushExpr();
				right->addToProg(prog);
			prog->popExpr();
		}
	);
	
	addAction("len", IntArray, Void, Int,
		LAMBDA_HEADER
		{
			int* out=(int*)malloc(sizeof(int));
			*out=getValFromTuple<int>(leftIn, IntArray, "_size");
			return out;
		},
		ADD_CPP_HEADER
		{
			getElemFromTupleAction(IntArray, "_size")->addToProg(left, voidAction, prog);
		}
	);
	
	addAction("get", IntArray, Int, Int, LAMBDA_HEADER
		{
			int pos=*(int*)rightIn;
			int size=getValFromTuple<int>(leftIn, IntArray, "_size");
			if (pos<0 || pos>=size)
				throw PineconeError("tried to acces position "+to_string(pos)+" of array "+to_string(size)+" long", RUNTIME_ERROR);
			int* arrayPtr=getValFromTuple<int*>(leftIn, IntArray, "_data");
			int val=*(arrayPtr+pos);
			int* out=(int*)malloc(sizeof(int));
			*out=val;
			return out;
		},
		ADD_CPP_HEADER
		{
			getElemFromTupleAction(IntArray, "_data")->addToProg(left, voidAction, prog);
			prog->code("[");
			prog->pushExpr();
				right->addToProg(prog);
			prog->popExpr();
			prog->code("]");
		}
	);
	
	addAction("set", IntArray, PNCN_Tuple(Int, Int), Void,
		LAMBDA_HEADER
		{
			Type rightType=PNCN_Tuple(Int, Int);
			int pos=getValFromTuple<int>(rightIn, rightType, "a");
			int size=getValFromTuple<int>(leftIn, IntArray, "_size");
			if (pos<0 || pos>=size)
				throw PineconeError("tried to set value at position "+to_string(pos)+" of array "+to_string(size)+" long", RUNTIME_ERROR);
			int val=getValFromTuple<int>(rightIn, rightType, "b");
			int* arrayPtr=getValFromTuple<int*>(leftIn, IntArray, "_data");
			*(arrayPtr+pos)=val;
			return nullptr;
		},
		ADD_CPP_HEADER
		{
			getElemFromTupleAction(IntArray, "_data")->addToProg(left, voidAction, prog);
			prog->code("[");
			prog->pushExpr();
				getElemFromTupleAction(right->getReturnType(), "a")->addToProg(right, voidAction, prog);
			prog->popExpr();
			prog->code("]");
			prog->code(" = ");
			getElemFromTupleAction(right->getReturnType(), "b")->addToProg(right, voidAction, prog);
		}
	);
}

void populateNonStdFuncs()
{
	func("printc", Void, Int, Void,
			putchar((char)right)
		,
			"putchar($:)"
		);
	
	func("inputc", Void, Void, Int,
			retrn getchar()
		,
			"getchar()"
		);
	
	func("inputInt", Void, Void, Int,
			int val;
			std::cin >> val;
			retrn val;
		,
			ADD_CPP_HEADER
			{
				if (!prog->hasFunc("-getIntInput"))
				{
					prog->pushFunc("-getIntInput", Void, Void, Int);
						prog->declareVar("tmp", Int);
						prog->code("scanf");
						prog->pushExpr();
							prog->code("\"%d\", &");
							prog->name("tmp");
						prog->popExpr();
						prog->endln();
						prog->code("return ");
						prog->name("tmp");
						prog->endln();
					prog->popFunc();
				}
				
				prog->name("-getIntInput");
				prog->code("()");
			}
		);
	
	addAction("runCmd", Void, String, String,
		LAMBDA_HEADER
		{
			return cppStr2PncnStr(runCmd(pncnStr2CppStr(rightIn)));
		},
		ADD_CPP_HEADER
		{
			addToProgRunCmd(prog);
			prog->name("$runCmd");
			prog->pushExpr();
				right->addToProg(prog);
			prog->popExpr();
		}
	);
}

void populateCppInterfaceFuncs()
{
	addAction("cppCode", Void, String, Void, LAMBDA_HEADER
		{
			throw PineconeError("you can't run interpreter with code that uses 'cppCode'", SOURCE_ERROR);
		},
		ADD_CPP_HEADER
		{
			prog->pushBlock();
			addToProgStrWithEscapedNames(prog, pncnStr2CppStr(right->execute(nullptr, nullptr)));
			prog->popBlock();
		}
	);
	
	addAction("cppHead", Void, String, Void, LAMBDA_HEADER
		{
			throw PineconeError("you can't run interpreter with code that uses 'cppHead'", SOURCE_ERROR);
		},
		ADD_CPP_HEADER
		{
			prog->addHeadCode(pncnStr2CppStr(right->execute(nullptr, nullptr)));
		}
	);
}

void populatePineconeStdLib()
{
	basicSetup();
	populateBasicTypes();
	populateConstants();
	populateOperators();
	populateConverters();
	populateStdFuncs();
	populateTypeInfoFuncs();
	populateMemManagementFuncs();
	populateStringFuncs();
	populateIntArrayAndFuncs();
	populateNonStdFuncs();
	populateCppInterfaceFuncs();
}


