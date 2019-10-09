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

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <float.h>

#include <stdarg.h>

#include "cpuconfig.h"

#include "cc.h"
#include "ccstr.h"
#include "mm.h"
#include "mmhash.h"

#include "vtlex.h"
#include "vtscript.h"
#include "vtparser.h"
#include "vtexec.h"


////


#ifndef ADDRESS
 #define ADDRESS(p,o) ((void *)(((char *)p)+(o)))
#endif

static inline int intMin( int x, int y )
{
  return ( x < y ? x : y );
}

static inline int intMax( int x, int y )
{
  return ( x > y ? x : y );
}


////


/* Clear the entry so that entryvalid() returns zero */
static void vtHashClearEntry( void *entry )
{
  vtName *name;
  name = (vtName *)entry;
  name->name = 0;
  return;
}

/* Returns non-zero if the entry is valid and existing */
static int vtHashEntryValid( void *entry )
{
  vtName *name;
  name = (vtName *)entry;
  return ( name->name ? 1 : 0 );
}

/* Return key for an arbitrary set of user-defined data */
static uint32_t vtHashEntryKey( void *entry )
{
  vtName *name;
  name = (vtName *)entry;
  return ccHash32Data( name->name, name->namelen );
}

/* Return MM_HASH_ENTRYCMP* to stop or continue the search */
static int vtHashEntryCmp( void *entry, void *entryref )
{
  vtName *name, *nameref;
  name = (vtName *)entry;
  if( !( name->name ) )
    return MM_HASH_ENTRYCMP_INVALID;
  nameref = (vtName *)entryref;
  if( ( name->namelen == nameref->namelen ) && ccMemCmpInline( name->name, nameref->name, name->namelen ) )
    return MM_HASH_ENTRYCMP_FOUND;
  return MM_HASH_ENTRYCMP_SKIP;
}

mmHashAccess vtNameHashAccess =
{
  .clearentry = vtHashClearEntry,
  .entryvalid = vtHashEntryValid,
  .entrykey = vtHashEntryKey,
  .entrycmp = vtHashEntryCmp,
  .entrylist = 0
};


////


#define VT_DEBUG (1)

#define VT_HASH_PAGESHIFT (4)


////


static void vtInitNamespace( vtNamespace *namespace, int hashbits )
{
  size_t hashsize;
  void *hashtable;
  hashsize = mmHashRequiredSize( sizeof(vtName), hashbits, VT_HASH_PAGESHIFT );
  hashtable = malloc( hashsize );
  mmHashInit( hashtable, &vtNameHashAccess, sizeof(vtName), hashbits, VT_HASH_PAGESHIFT, 0x0 );
  namespace->hashtable = hashtable;
  return;
}

static void vtEndNamespace( vtNamespace *namespace )
{
  free( namespace->hashtable );
  namespace->hashtable = 0;
  return;
}

/* Grow or shrink hash table whenever required */
static void vtHashCheckStatus( void **hashtablehandle )
{
  int hashstatus, hashbits, modhashbits;
  void *hashtable, *newtable;
  size_t memsize;

  hashtable = *hashtablehandle;
  hashstatus = mmHashGetStatus( hashtable, &hashbits );
  modhashbits = 0;
  if( hashstatus == MM_HASH_STATUS_MUSTGROW )
    modhashbits = 1;
  else if( hashstatus == MM_HASH_STATUS_MUSTSHRINK )
    modhashbits = -1;
  if( modhashbits )
  {
    hashbits += modhashbits;
    memsize = mmHashRequiredSize( sizeof(vtName), hashbits, VT_HASH_PAGESHIFT );
    newtable = malloc( memsize );
    mmHashResize( newtable, hashtable, &vtNameHashAccess, hashbits, VT_HASH_PAGESHIFT );
    free( hashtable );
    *hashtablehandle = newtable;
  }

  return;
}


////


/* Register an identifier in the namespace */
static int vtRegisterIdentifier( vtSystem *system, vtNamespace *namespace, vtName *name )
{
  /* Register identifier in namespace */
  if( !( mmHashDirectAddEntry( namespace->hashtable, &vtNameHashAccess, name, 1 ) ) )
  {
#if VT_DEBUG
    printf( "WARNING VT: Identifier \"%s\" already exists in namespace.\n", name->name );
#endif
    return 0;
  }
  vtHashCheckStatus( &namespace->hashtable );
  return 1;
}

static int vtUnregisterIdentifier( vtSystem *system, vtNamespace *namespace, vtName *name )
{
  /* Unregister in namespace */
  if( !( mmHashDirectDeleteEntry( namespace->hashtable, &vtNameHashAccess, name, 0 ) ) )
  {
#if VT_DEBUG
    printf( "WARNING VT: Identifier \"%s\" not found in namespace.\n", name->name );
#endif
    return 0;
  }
  vtHashCheckStatus( &namespace->hashtable );
  return 1;
}


////


static void vtBuildName( vtName *name, char *namestring, int namelen, int class, void *object )
{
  name->class = class;
  name->namelen = 0;
  name->name = 0;
  if( namestring )
  {
    name->namelen = namelen;
    if( namelen == 0 )
      name->namelen = strlen( namestring );
  }
  if( name->namelen )
    name->name = namestring;
  name->object = object;
  return;
}

static void vtAllocNameString( char **namestorage, char *namestring, int namelen )
{
  char *namealloc;
  *namestorage = 0;
  if( namestring )
  {
    if( !( namelen ) )
      namelen = strlen( namestring );
    if( namelen )
    {
      namealloc = malloc( namelen+1 );
      memcpy( namealloc, namestring, namelen+1 );
      namealloc[namelen] = 0;
      *namestorage = namealloc;
    }
  }
  return;
}

static int vtValidateName( vtSystem *system, vtNamespace *namespace, vtName *name )
{
  int i;
  char c;
  char *namestring;
  namestring = name->name;

  if( !( name->namelen ) )
  {
    if( name->class == VT_NAME_CLASS_SCOPE )
      return 1;
    printf( "ERROR VT: Illegal empty identifier name.\n" );
    return 0;
  }
  for( i = 0 ; i < name->namelen ; i++ )
  {
    c = namestring[i];
    if( ( c >= 'a' ) && ( c <= 'z' ) )
      continue;
    if( ( c >= 'A' ) && ( c <= 'Z' ) )
      continue;
    if( ( ( c >= '0' ) && ( c <= '9' ) ) || ( c == '_' ) )
    {
      if( i )
        continue;
    }
    printf( "ERROR VT: Illegal identifier name \"%s\".\n", namestring );
    return 0;
  }
  /* Check reserved list of keywords */
  if( mmHashDirectFindEntry( system->sysnamespace.hashtable, &vtNameHashAccess, (void *)name ) )
  {
    printf( "ERROR VT: Identifier name \"%s\" is reserved.\n", namestring );
    return 0;
  }


  /* Check active scope for collision */
  /* TODO: Remove that, it's completely redunded with dup check when namespace insertion! */
  if( ( namespace ) && ( mmHashDirectFindEntry( namespace->hashtable, &vtNameHashAccess, (void *)name ) ) )
  {
    printf( "ERROR VT: Identifier name \"%s\" exists in namespace.\n", namestring );
    return 0;
  }


  return 1;
}


////


int vtInitScope( vtSystem *system, vtScope *scope, vtScope *parentscope, char *namestring )
{
  vtName name;
  vtInitNamespace( &scope->namespace, 8 );
  scope->namestring = 0;
  if( namestring )
  {
    vtAllocNameString( &scope->namestring, namestring, 0 );
    if( parentscope )
    {
      vtBuildName( &name, namestring, 0, VT_NAME_CLASS_SCOPE, (void *)scope );
      if( !( vtValidateName( system, &parentscope->namespace, &name ) ) )
      {
        vtEndScope( system, scope );
        return 0;
      }
      if( !( vtRegisterIdentifier( 0, &parentscope->namespace, &name ) ) )
      {
        vtEndScope( system, scope );
        return 0;
      }
    }
  }
  scope->parentscope = parentscope;
  return 1;
}

void vtEndScope( vtSystem *system, vtScope *scope )
{
  vtScope *parentscope;
  vtName name;
  if( scope->namestring )
  {
    parentscope = scope->parentscope;
    if( parentscope )
    {
      vtBuildName( &name, scope->namestring, 0, VT_NAME_CLASS_SCOPE, (void *)scope );
      if( !( vtUnregisterIdentifier( 0, &parentscope->namespace, &name ) ) )
        printf( "ERROR VT: Failed to unregister scope %s from parent namespace.\n", scope->namestring );
    }
    free( scope->namestring );
  }
  vtEndNamespace( &scope->namespace );
  return;
}


////


int vtSystemInit( vtSystem *system )
{
  vtInitNamespace( &system->sysnamespace, 8 );
  vtInitScope( system, &system->globalscope, 0, 0 );
  system->currentscope = 0;
  return 1;
}

vtScope *vtGetGlobalScope( vtSystem *system )
{
  return &system->globalscope;
}

vtNamespace *vtGetGlobalNamespace( vtSystem *system )
{
  return &system->globalscope.namespace;
}

void vtSystemEnd( vtSystem *system )
{
  vtEndScope( system, &system->globalscope );
  vtEndNamespace( &system->sysnamespace );
  return;
}


////


const int vtBaseTypeMemSize[VT_BASETYPE_COUNT] =
{
 [VT_BASETYPE_CHAR] = sizeof(char),
 [VT_BASETYPE_INT8] = sizeof(int8_t),
 [VT_BASETYPE_UINT8] = sizeof(uint8_t),
 [VT_BASETYPE_INT16] = sizeof(int16_t),
 [VT_BASETYPE_UINT16] = sizeof(uint16_t),
 [VT_BASETYPE_INT32] = sizeof(int32_t),
 [VT_BASETYPE_UINT32] = sizeof(uint32_t),
 [VT_BASETYPE_INT64] = sizeof(int64_t),
 [VT_BASETYPE_UINT64] = sizeof(uint64_t),
 [VT_BASETYPE_FLOAT] = sizeof(float),
 [VT_BASETYPE_DOUBLE] = sizeof(double),
 [VT_BASETYPE_POINTER] = sizeof(void *)
};

const int vtBaseTypeMemAlign[VT_BASETYPE_COUNT] =
{
 [VT_BASETYPE_CHAR] = sizeof(char) - 1,
 [VT_BASETYPE_INT8] = sizeof(int8_t) - 1,
 [VT_BASETYPE_UINT8] = sizeof(uint8_t) - 1,
 [VT_BASETYPE_INT16] = sizeof(int16_t) - 1,
 [VT_BASETYPE_UINT16] = sizeof(uint16_t) - 1,
 [VT_BASETYPE_INT32] = sizeof(int32_t) - 1,
 [VT_BASETYPE_UINT32] = sizeof(uint32_t) - 1,
 [VT_BASETYPE_INT64] = sizeof(int64_t) - 1,
 [VT_BASETYPE_UINT64] = sizeof(uint64_t) - 1,
 [VT_BASETYPE_FLOAT] = sizeof(float) - 1,
 [VT_BASETYPE_DOUBLE] = sizeof(double) - 1,
 [VT_BASETYPE_POINTER] = sizeof(void *) - 1
};

const vtGenericType vtGenericTypeString =
{
  VT_BASETYPE_POINTER,
  (void *)&vtGenericTypeString
};


////


vtSysType *vtCreateSysType( vtSystem *system, char *namestring, int basetype, void *matchtype, void *typevalue, void *(*create)( void *typevalue, int count, void *storage ), void (*destroy)( void *typevalue, int count, void *storagevalue ), int (*read)( void *typevalue, void *storagevalue, int index, void *pointer ), int (*write)( void *typevalue, void *storagevalue, int index, void *pointer ) )
{
  vtName name;
  vtSysType *systype;
  vtNamespace *namespace;

  if( (unsigned)basetype >= VT_BASETYPE_COUNT )
    return 0;
  systype = malloc( sizeof(vtSysType) );
  vtAllocNameString( &systype->namestring, namestring, 0 );
  systype->typevalue = typevalue;
  systype->flags = 0x0;
  systype->create = create;
  systype->destroy = destroy;
  systype->u.typescalar.basetype = basetype;
  systype->u.typescalar.matchtype = matchtype;
  systype->u.typescalar.read = read;
  systype->u.typescalar.write = write;
  if( namestring )
  {
    vtBuildName( &name, namestring, 0, VT_NAME_CLASS_SYSTYPE, (void *)systype );
    namespace = &system->sysnamespace;
    if( !( vtValidateName( system, namespace, &name ) ) )
      goto error;
    if( !( vtRegisterIdentifier( system, namespace, &name ) ) )
      goto error;
  }
  return systype;

  error:
  if( systype->namestring )
  {
    free( systype->namestring );
    systype->namestring = 0;
  }
  free( systype );
  return 0;
}

int vtDestroySysType( vtSystem *system, vtSysType *systype )
{
  vtName name;
  vtNamespace *namespace;

  if( systype->namestring )
  {
    vtBuildName( &name, systype->namestring, 0, VT_NAME_CLASS_SYSTYPE, (void *)systype );
    namespace = &system->sysnamespace;
    if( !( vtUnregisterIdentifier( system, namespace, &name ) ) )
      return 0;
    free( systype->namestring );
    systype->namestring = 0;
  }
  free( systype );
  return 1;
}


////


vtSysType *vtCreateSysStruct( vtSystem *system, char *namestring, void *typevalue, size_t storagesize, void *(*create)( void *typevalue, int count, void *storage ), void (*destroy)( void *typevalue, int count, void *storagevalue ) )
{
  vtName name;
  vtSysType *systype;
  vtNamespace *namespace;

  systype = malloc( sizeof(vtSysType) );
  vtAllocNameString( &systype->namestring, namestring, 0 );
  systype->typevalue = typevalue;
  systype->flags = VT_SYSTYPE_FLAGS_STRUCT;
  systype->create = create;
  systype->destroy = destroy;
  if( namestring )
  {
    vtBuildName( &name, namestring, 0, VT_NAME_CLASS_SYSTYPE, (void *)systype );
    namespace = &system->sysnamespace;
    if( !( vtValidateName( system, namespace, &name ) ) )
      goto error;
    if( !( vtRegisterIdentifier( system, namespace, &name ) ) )
      goto error;
  }
  vtInitNamespace( &systype->u.typestruct.namespace, 8 );
  systype->u.typestruct.storagesize = storagesize;
  systype->u.typestruct.memberlist = 0;
  systype->u.typestruct.methodlist = 0;
  return systype;

  error:
  if( systype->namestring )
  {
    free( systype->namestring );
    systype->namestring = 0;
  }
  free( systype );
  return 0;
}

int vtDestroySysStruct( vtSystem *system, vtSysType *systype )
{
  vtName name;
  vtNamespace *namespace;
  vtSysMember *member, *membernext;
  vtSysMethod *method, *methodnext;

  for( member = systype->u.typestruct.memberlist ; member ; member = membernext )
  {
    membernext = member->list.next;
    if( member->namestring )
      free( member->namestring );
    free( member );
  }
  for( method = systype->u.typestruct.methodlist ; method ; method = methodnext )
  {
    methodnext = method->list.next;
    if( method->namestring )
      free( method->namestring );
    free( method );
  }
  vtEndNamespace( &systype->u.typestruct.namespace );
  if( systype->namestring )
  {
    vtBuildName( &name, systype->namestring, 0, VT_NAME_CLASS_SYSTYPE, (void *)systype );
    namespace = &system->sysnamespace;
    if( !( vtUnregisterIdentifier( system, namespace, &name ) ) )
      return 0;
    free( systype->namestring );
    systype->namestring = 0;
  }
  free( systype );
  return 1;
}


int vtAddSysStructMember( vtSystem *system, vtSysType *systype, char *namestring, int basetype, void *matchtype, void *membervalue, int arrayflag, int (*read)( void *typevalue, void *storagevalue, int index, void *pointer ), int (*write)( void *typevalue, void *storagevalue, int index, void *pointer ) )
{
  vtName name;
  vtSysMember *sysmember;

  sysmember = malloc( sizeof(vtSysMember) );
  vtBuildName( &name, namestring, 0, VT_NAME_CLASS_SYSMEMBER, (void *)sysmember );
  if( !( vtValidateName( system, &systype->u.typestruct.namespace, &name ) ) )
    goto error;
  if( !( vtRegisterIdentifier( system, &systype->u.typestruct.namespace, &name ) ) )
    goto error;
  vtAllocNameString( &sysmember->namestring, namestring, 0 );
  sysmember->basetype = basetype;
  sysmember->matchtype = matchtype;
  sysmember->membervalue = membervalue;
  sysmember->arrayflag = arrayflag;
  sysmember->read = read;
  sysmember->write = write;
  mmListAdd( &systype->u.typestruct.memberlist, sysmember, offsetof(vtSysMember,list) );
  return 1;

  error:
  free( sysmember );
  return 0;
}


int vtAddSysStructMethod( vtSystem *system, vtSysType *systype, char *namestring, int argcount, const vtGenericType *argtype, int retcount, const vtGenericType *rettype, void (*call)( void *storagevalue, void **argv, void **retv ) )
{
  vtName name;
  vtSysMethod *sysmethod;

  if( (unsigned)argcount >= VT_ARG_COUNT_MAX )
    return 0;
  if( (unsigned)retcount >= VT_RET_COUNT_MAX )
    return 0;
  sysmethod = malloc( sizeof(vtSysMethod) );
  vtBuildName( &name, namestring, 0, VT_NAME_CLASS_SYSMETHOD, (void *)sysmethod );
  if( !( vtValidateName( system, &systype->u.typestruct.namespace, &name ) ) )
    goto error;
  if( !( vtRegisterIdentifier( system, &systype->u.typestruct.namespace, &name ) ) )
    goto error;
  vtAllocNameString( &sysmethod->namestring, namestring, 0 );
  sysmethod->format.argcount = argcount;
  memcpy( sysmethod->format.argtype, argtype, argcount*sizeof(vtGenericType) );
  sysmethod->format.retcount = retcount;
  memcpy( sysmethod->format.rettype, rettype, retcount*sizeof(vtGenericType) );
  sysmethod->call = call;
  mmListAdd( &systype->u.typestruct.methodlist, sysmethod, offsetof(vtSysMethod,list) );
  return 1;

  error:
  free( sysmethod );
  return 0;
}


////


vtSysVar *vtCreateSysVar( vtSystem *system, vtScope *scope, vtSysType *systype, char *namestring, int namelen )
{
  vtName name;
  size_t varmemsize;
  void *storagevalue;
  vtSysVar *var;

  if( !( systype->create ) )
    return 0;
  varmemsize = sizeof(vtSysVar);
  if( systype->flags & VT_SYSTYPE_FLAGS_STRUCT )
    varmemsize += systype->u.typestruct.storagesize;
  var = malloc( varmemsize );
  storagevalue = systype->create( systype->typevalue, 1, &var->storage );
  if( !( storagevalue ) )
    goto error;

  vtBuildName( &name, namestring, namelen, VT_NAME_CLASS_SYSVAR, (void *)var );
  if( !( vtValidateName( system, &scope->namespace, &name ) ) )
    goto errordestroy;
  if( !( vtRegisterIdentifier( system, &scope->namespace, &name ) ) )
    goto errordestroy;
  vtAllocNameString( &var->namestring, namestring, namelen );
  var->flags = 0;
  if( systype->flags & VT_SYSTYPE_FLAGS_IMMUTABLE )
    var->flags |= VT_SYSVAR_FLAGS_IMMUTABLE;
  var->type = systype;
  var->storagevalue = storagevalue;

  return var;

  errordestroy:
  systype->destroy( systype->typevalue, 1, storagevalue );
  error:
  free( var );
  return 0;
}

int vtEnableSysVar( vtSystem *system, vtSysVar *var )
{
  vtSysType *systype;
  if( !( var->flags & VT_SYSVAR_FLAGS_DISABLED ) )
    return 1;
  systype = var->type;
  var->storagevalue = systype->create( systype->typevalue, 1, &var->storage );
  var->flags &= ~VT_SYSVAR_FLAGS_DISABLED;
  return 1;
}

int vtDisableSysVar( vtSystem *system, vtSysVar *var )
{
  vtSysType *systype;
  if( var->flags & VT_SYSVAR_FLAGS_DISABLED )
    return 1;
  systype = var->type;
  systype->destroy( systype->typevalue, 1, var->storagevalue );
  var->storagevalue = 0;
  var->flags |= VT_SYSVAR_FLAGS_DISABLED;
  return 1;
}

int vtDestroySysVar( vtSystem *system, vtScope *scope, vtSysVar *var )
{
  vtName name;
  vtSysType *systype;
  vtBuildName( &name, var->namestring, 0, VT_NAME_CLASS_SYSVAR, (void *)var );
  if( !( vtUnregisterIdentifier( system, &scope->namespace, &name ) ) )
    return 0;
  systype = var->type;
  if( !( var->flags & VT_SYSVAR_FLAGS_DISABLED ) )
    systype->destroy( systype->typevalue, 1, var->storagevalue );
  free( var );
  return 1;
}



////////////////////////////////////////////////////////////////////////////////

/*
TODO:
- Regular variables with stack storage
- Generic types for variable declaration
- Arrays, pointer to object keeping track of bounds?
- Lists
- String manipulation
*/

////////////////////////////////////////////////////////////////////////////////



void *MyTypeCreate( void *typevalue, int count, void *storage )
{

printf( "Create Variable\n" );

  return storage;
}

void MyTypeDestroy( void *typevalue, int count, void *storagevalue )
{

printf( "Destroy Variable\n" );

  return;
}

int MyTypeRead( void *typevalue, void *storagevalue, int index, void *pointer )
{
  *((int32_t *)pointer) = *((int32_t *)storagevalue);

printf( "Read Variable : %d\n", *((int32_t *)pointer) );

  return 1;
}

int MyTypeWrite( void *typevalue, void *storagevalue, int index, void *pointer )
{
  *((int32_t *)storagevalue) = *((int32_t *)pointer);

printf( "Write Variable : %d\n", *((int32_t *)pointer) );

  return 1;
}



void *MyStructCreate( void *typevalue, int count, void *storage )
{
  void *storagevalue;
  storagevalue = malloc( sizeof(void *) );

printf( "Create Struct\n" );

  return storagevalue;
}

void MyStructDestroy( void *typevalue, int count, void *storagevalue )
{
  free( storagevalue );

printf( "Destroy Struct\n" );

  return;
}

int MyStructMemberRead( void *typevalue, void *storagevalue, int index, void *pointer )
{
  *((void **)pointer) = *((void **)storagevalue);

printf( "Read Struct : %s\n", (char *)*((void **)pointer) );

  return 1;
}

int MyStructMemberWrite( void *typevalue, void *storagevalue, int index, void *pointer )
{
  *((void **)storagevalue) = *((void **)pointer);

printf( "Write Struct : \"%s\"\n", (char *)*((void **)pointer) );

  return 1;
}

void MyStructMethod( void *storagevalue, void **argv, void **retv )
{
  int32_t arg0;
  char *arg1;
  arg0 = *(int32_t *)(argv[0]);
  arg1 = *(char **)(argv[1]);

printf( "Call Method : %d \"%s\"\n", arg0, arg1 );

  return;
}



int main()
{
  vtSystem vtsystem;
  vtScope MyScope;
  vtSysType *MyCustomType;
  vtSysType *MyCustomStruct;
  vtSysVar *var;
  vtSysVar *var2;
  vtParser parser;




  vtSystemInit( &vtsystem );

  vtInitScope( &vtsystem, &MyScope, vtGetGlobalScope( &vtsystem ), "MyScope" );

  MyCustomType = vtCreateSysType( &vtsystem, "int32_t", VT_BASETYPE_INT32, (void *)0, 0, MyTypeCreate, MyTypeDestroy, MyTypeRead, MyTypeWrite );

  var = vtCreateSysVar( &vtsystem, &MyScope, MyCustomType, "MyVariable", 0 );


  MyCustomStruct = vtCreateSysStruct( &vtsystem, "MyStructType", 0x0, 0, MyStructCreate, MyStructDestroy );
  vtAddSysStructMember( &vtsystem, MyCustomStruct, "member", VT_BASETYPE_POINTER, (void *)&vtGenericTypeString, 0x0, 0, MyStructMemberRead, MyStructMemberWrite );

  static const vtGenericType argtype[2] = { { VT_BASETYPE_INT32, 0 }, { VT_BASETYPE_POINTER, (void *)&vtGenericTypeString } };
  static const vtGenericType rettype[1] = { { VT_BASETYPE_INT32, 0 } };
  vtAddSysStructMethod( &vtsystem, MyCustomStruct, "method", 2, argtype, 1, rettype, MyStructMethod );

  var2 = vtCreateSysVar( &vtsystem, &MyScope, MyCustomStruct, "MyStruct", 0 );


////


  int tokenindex;
  vtTokenBuffer *tokenbuflist, *tokenbuf;
  vtToken *token;
  char *codestring;

/*
  codestring = "aaa.bbb 0.32 56 aaa ccc\n  # as 3.0\n Yar\nif( YAR )\n";
  codestring = "MyScope.MyVariable = 3.0;\naaa.bbb 0.32 56 aaa ccc\n  # as 3.0\n Yar\nif( YAR )\n";
  codestring = "MyScope.MyVariable = 3 + 1.0;\n";
  codestring = "MyScope.MyVariable = 2 * ( 3 + 1.0 );\n";
  codestring = "MyScope.MyVariable = 2 * ( 3 + 1.0 );int32_t a = 4;a = 11 + a;~a;int32_t a = 7.0;a = \"aaaa\";\n";
  codestring = "{MyScope.MyVariable = 2*(3+1.0);int32_t a = 4;a = 11 + a;~a;int32_t a = 7.0; a = '0';MyScope.MyStruct.member = \"abcdGizmo Jambon Froid\";}\n";
*/
  codestring = "{MyScope.MyVariable = 2*(3+1.0);MyScope.MyStruct.member = \"Jambon Froid\";MyScope.MyStruct.method( 2, \"abcdef\" );}\n";


  tokenbuflist = vtLexParse( codestring );
  if( tokenbuflist )
  {
    for( tokenbuf = tokenbuflist ; tokenbuf ; tokenbuf = tokenbuf->next )
    {
      printf( "Token Buf : %d tokens\n", tokenbuf->tokencount );
      for( tokenindex = 0 ; tokenindex < tokenbuf->tokencount ; tokenindex++ )
      {
        token = &tokenbuf->tokenlist[ tokenindex ];
        printf( "  Token %d, type %d : %.*s\n", tokenindex, (int)token->type, (int)token->length, &codestring[ token->offset ] );
      }
    }

    vtInitParser( &vtsystem, &parser );
    if( vtParseScript( &parser, codestring, tokenbuflist ) )
    {
      /* Script parsed successfully, execute */
      vtExec( &parser );
    }
    vtFreeParser( &parser );

    vtLexFree( tokenbuflist );
  }

////


  vtDestroySysVar( &vtsystem, &MyScope, var2 );

  vtDestroySysVar( &vtsystem, &MyScope, var );

  vtDestroySysType( &vtsystem, MyCustomType );

  vtEndScope( &vtsystem, &MyScope );

  vtSystemEnd( &vtsystem );

  return 1;
}



