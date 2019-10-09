/* -----------------------------------------------------------------------------
 *
 * Copyright (c) 2014-2019 Alexis Naveros.
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 * claim that you wrote the original software. If you use this software
 * in a product, an acknowledgment in the product documentation would be
 * appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 * misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 * -----------------------------------------------------------------------------
 */

typedef struct
{
  int class;
  int namelen;
  char *name;
  void *object;
} vtName;

enum
{
  VT_NAME_CLASS_SCOPE,
  VT_NAME_CLASS_SYSTYPE,
  VT_NAME_CLASS_SYSVAR,
  VT_NAME_CLASS_SYSMEMBER,
  VT_NAME_CLASS_SYSFUNCTION,
  VT_NAME_CLASS_SYSMETHOD,

  VT_NAME_CLASS_TYPE,
  VT_NAME_CLASS_VARIABLE,
  VT_NAME_CLASS_MEMBER,
  VT_NAME_CLASS_FUNCTION,
  VT_NAME_CLASS_METHOD,
  VT_NAME_CLASS_LABEL,

  VT_NAME_CLASS_COUNT
};

enum
{
  VT_BASETYPE_CHAR,
  VT_BASETYPE_INT8,
  VT_BASETYPE_UINT8,
  VT_BASETYPE_INT16,
  VT_BASETYPE_UINT16,
  VT_BASETYPE_INT32,
  VT_BASETYPE_UINT32,
  VT_BASETYPE_INT64,
  VT_BASETYPE_UINT64,
  VT_BASETYPE_FLOAT,
  VT_BASETYPE_DOUBLE,
  VT_BASETYPE_POINTER,

  VT_BASETYPE_COUNT
};

typedef struct
{
  int basetype;
  /* If basetype is pointer, unique value to identify pointer type */
  void *matchtype;
} vtGenericType;

extern const int vtBaseTypeMemSize[VT_BASETYPE_COUNT];
extern const int vtBaseTypeMemAlign[VT_BASETYPE_COUNT];

extern const vtGenericType vtGenericTypeString;



#if CPUCONF_INT_SIZE == 8
 #define VT_CMP_BASETYPE (VT_BASETYPE_INT8)
 typedef int8_t vtCmpType;
#elif CPUCONF_INT_SIZE == 16
 #define VT_CMP_BASETYPE (VT_BASETYPE_INT16)
 typedef int16_t vtCmpType;
#elif CPUCONF_INT_SIZE == 32
 #define VT_CMP_BASETYPE (VT_BASETYPE_INT32)
 typedef int32_t vtCmpType;
#elif CPUCONF_INT_SIZE == 64
 #define VT_CMP_BASETYPE (VT_BASETYPE_INT64)
 typedef int64_t vtCmpType;
#else
 #define VT_CMP_BASETYPE (VT_BASETYPE_INT32)
 typedef int32_t vtCmpType;
#endif


////


/* Space for identifier names sharing scope */
typedef struct
{
  /* Hash */
  void *hashtable;
} vtNamespace;

#define VT_ARG_COUNT_MAX (16)
#define VT_RET_COUNT_MAX (16)

////


typedef struct
{
  /* Argument(s) */
  int argcount;
  vtGenericType argtype[VT_ARG_COUNT_MAX];
  /* Return value(s) */
  int retcount;
  vtGenericType rettype[VT_RET_COUNT_MAX];
} vtCallFormat;

/* System Function */
typedef struct
{
  char *namestring;
  /* Call parameters */
  vtCallFormat format;
  /* Function pointer */
  void (*call)( void **argv, void **retv );
} vtSysFunction;

/* Type for struct member */
typedef struct
{
  char *namestring;
  /* Base type, used for casting and assignments between types */
  int basetype;
  /* Unique abstract type for pointers */
  void *matchtype;
  /* Custom type information */
  void *membervalue;
  /* Array flag, zero if scalar */
  int arrayflag;
  /* Acquire pointer to read from member */
  int (*read)( void *membervalue, void *storagevalue, int index, void *pointer );
  /* Write new value for member, return 0 on access denied */
  int (*write)( void *membervalue, void *storagevalue, int index, void *pointer );
  /* Linked list of struct members */
  mmListNode list;
} vtSysMember;

/* Method for struct */
typedef struct
{
  char *namestring;
  /* Call parameters */
  vtCallFormat format;
  /* Function pointer */
  void (*call)( void *storagevalue, void **argv, void **retv );
  /* Linked list of struct methods */
  mmListNode list;
} vtSysMethod;

typedef struct
{
  /* Base type, used for casting and assignments between types */
  int basetype;
  /* Unique abstract type for pointers */
  void *matchtype;
  /* Acquire pointer to read from type */
  int (*read)( void *typevalue, void *storagevalue, int index, void *pointer );
  /* Write new value for type, return 0 on access denied */
  int (*write)( void *typevalue, void *storagevalue, int index, void *pointer );
} vtSysTypeScalar;

typedef struct
{
  /* Namespace for all members */
  vtNamespace namespace;
  /* Optional internal variable storage */
  size_t storagesize;
  /* Linked list of struct members and methods */
  void *memberlist;
  void *methodlist;
} vtSysTypeStruct;

/* Type for base data type */
typedef struct
{
  char *namestring;
  /* Custom type information */
  void *typevalue;
  /* Flags */
  int flags;
  /* Return storagevalue for instance */
  void *(*create)( void *typevalue, int count, void *storage );
  /* Destroy any custom storage as storagevalue */
  void (*destroy)( void *typevalue, int count, void *storagevalue );
  /* Other type data depends on class defined by flags */
  union
  {
    vtSysTypeScalar typescalar;
    vtSysTypeStruct typestruct;
  } u;
} vtSysType;

/* Type is a struct, otherwise scalar */
#define VT_SYSTYPE_FLAGS_STRUCT (0x1)
/* Variables can not be declared/discarded by user */
#define VT_SYSTYPE_FLAGS_IMMUTABLE (0x2)


////


/* Declaration scope with namespace and permission checks */
typedef struct
{
  /* Name of scope?... */
  char *namestring;
  vtNamespace namespace;
  void *parentscope;
} vtScope;


////


typedef struct
{
  vtScope globalscope;
  vtNamespace sysnamespace;
  vtScope *currentscope;
} vtSystem;


////


typedef union
{
  char c;
  int8_t i8;
  uint8_t u8;
  int16_t i16;
  uint16_t u16;
  int32_t i32;
  uint32_t u32;
  int64_t i64;
  uint64_t u64;
  float f;
  double d;
  void *p;
  char *s;
} vtVarStorage;


/* Instance of a type */
typedef struct
{
  int flags;
  char *namestring;
  vtSysType *type;
  void *storagevalue;
  vtVarStorage storage;
} vtSysVar;

/* Variables can not be declared/discarded by user */
#define VT_SYSVAR_FLAGS_IMMUTABLE (0x1)
/* Variable was destroyed by user, persistent in namespace */
#define VT_SYSVAR_FLAGS_DISABLED (0x2)


#if 0
/* Array of instances of type */
typedef struct
{
  char *namestring;
  vtSysType *type;
  int count;
  int maxcount;
  void *storagevalue;
  int flags;
  void *storage;
} vtArray;

#define VT_ARRAY_FLAGS_FIXEDCOUNT (0x1)
#define VT_ARRAY_FLAGS_GROWCOUNT (0x2)

#endif

#if 0
/* Linked list of instances of type */
typedef struct
{
  char *namestring;
  vtSysType *type;
  /* FOO */
} vtList;
#endif


////


int vtSystemInit( vtSystem *system );
vtScope *vtGetGlobalScope( vtSystem *system );
vtNamespace *vtGetGlobalNamespace( vtSystem *system );
void vtSystemEnd( vtSystem *system );



int vtInitScope( vtSystem *system, vtScope *scope, vtScope *parentscope, char *namestring );
void vtEndScope( vtSystem *system, vtScope *scope );

vtSysType *vtCreateSysType( vtSystem *system, char *namestring, int basetype, void *matchtype, void *typevalue, void *(*create)( void *typevalue, int count, void *storage ), void (*destroy)( void *typevalue, int count, void *storagevalue ), int (*read)( void *typevalue, void *storagevalue, int index, void *pointer ), int (*write)( void *typevalue, void *storagevalue, int index, void *pointer ) );
int vtDestroySysType( vtSystem *system, vtSysType *type );

vtSysType *vtCreateSysStruct( vtSystem *system, char *namestring, void *typevalue, size_t storagesize, void *(*create)( void *typevalue, int count, void *storage ), void (*destroy)( void *typevalue, int count, void *storagevalue ) );
int vtDestroySysStruct( vtSystem *system, vtSysType *typestruct );
int vtAddSysStructMember( vtSystem *system, vtSysType *typestruct, char *namestring, int basetype, void *matchtype, void *membervalue, int arrayflag, int (*read)( void *typevalue, void *storagevalue, int index, void *pointer ), int (*write)( void *typevalue, void *storagevalue, int index, void *pointer ) );
int vtAddSysStructMethod( vtSystem *system, vtSysType *systype, char *namestring, int argcount, const vtGenericType *argtype, int retcount, const vtGenericType *rettype, void (*call)( void *storagevalue, void **argv, void **retv ) );

vtSysVar *vtCreateSysVar( vtSystem *system, vtScope *scope, vtSysType *type, char *namestring, int namelen );
int vtEnableSysVar( vtSystem *system, vtSysVar *var );
int vtDisableSysVar( vtSystem *system, vtSysVar *var );
int vtDestroySysVar( vtSystem *system, vtScope *scope, vtSysVar *variable );


/*
Each new type must belong to a "basetype"
Same handling/cast/operations, but storage/read/write can be custom
*/

