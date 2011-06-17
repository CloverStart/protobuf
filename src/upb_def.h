/*
 * upb - a minimalist implementation of protocol buffers.
 *
 * Copyright (c) 2009-2011 Google Inc.  See LICENSE for details.
 * Author: Josh Haberman <jhaberman@gmail.com>
 *
 * Provides a mechanism for creating and linking proto definitions.
 * These form the protobuf schema, and are used extensively throughout upb:
 * - upb_msgdef: describes a "message" construct.
 * - upb_fielddef: describes a message field.
 * - upb_enumdef: describes an enum.
 * (TODO: definitions of services).
 *
 * These defs are mutable (and not thread-safe) when first created.
 * Once they are added to a defbuilder (and later its symtab) they become
 * immutable.
 */

#ifndef UPB_DEF_H_
#define UPB_DEF_H_

#include "upb_atomic.h"
#include "upb_table.h"

#ifdef __cplusplus
extern "C" {
#endif

struct _upb_symtab;
typedef struct _upb_symtab upb_symtab;

// All the different kind of defs we support.  These correspond 1:1 with
// declarations in a .proto file.
typedef enum {
  UPB_DEF_MSG = 0,
  UPB_DEF_ENUM,
  UPB_DEF_SERVICE,          // Not yet implemented.

  UPB_DEF_ANY = -1,         // Wildcard for upb_symtab_get*()
  UPB_DEF_UNRESOLVED = 99,  // Internal-only.
} upb_deftype_t;


/* upb_def: base class for defs  **********************************************/

typedef struct {
  upb_string *fqname;     // Fully qualified.
  upb_symtab *symtab;     // Def is mutable iff symtab == NULL.
  upb_atomic_t refcount;  // Owns a ref on symtab iff (symtab && refcount > 0).
  upb_deftype_t type;
} upb_def;

// Call to ref/unref a def.  Can be used at any time, but is not thread-safe
// until the def is in a symtab.  While a def is in a symtab, everything
// reachable from that def (the symtab and all defs in the symtab) are
// guaranteed to be alive.
void upb_def_ref(upb_def *def);
void upb_def_unref(upb_def *def);
upb_def *upb_def_dup(upb_def *def);

#define UPB_UPCAST(ptr) (&(ptr)->base)


/* upb_fielddef ***************************************************************/

// A upb_fielddef describes a single field in a message.  It isn't a full def
// in the sense that it derives from upb_def.  It cannot stand on its own; it
// must be part of a upb_msgdef.  It is also reference-counted.
struct _upb_fielddef {
  struct _upb_msgdef *msgdef;
  upb_def *def;  // if upb_hasdef(f)
  upb_atomic_t refcount;
  bool finalized;

  // The following fields may be modified until the def is finalized.
  uint8_t type;          // Use UPB_TYPE() constants.
  uint8_t label;         // Use UPB_LABEL() constants.
  int16_t hasbit;
  uint16_t offset;
  int32_t number;
  upb_string *name;
  upb_value defaultval;  // Only meaningful for non-repeated scalars and strings.
  upb_value fval;
  struct _upb_accessor_vtbl *accessor;
};

upb_fielddef *upb_fielddef_new();
void upb_fielddef_ref(upb_fielddef *f);
void upb_fielddef_unref(upb_fielddef *f);
upb_fielddef *upb_fielddef_dup(upb_fielddef *f);

// Read accessors.  May be called any time.
INLINE uint8_t upb_fielddef_type(upb_fielddef *f) { return f->type; }
INLINE uint8_t upb_fielddef_label(upb_fielddef *f) { return f->label; }
INLINE int32_t upb_fielddef_number(upb_fielddef *f) { return f->number; }
INLINE upb_string *upb_fielddef_name(upb_fielddef *f) { return f->name; }
INLINE upb_value upb_fielddef_default(upb_fielddef *f) { return f->defaultval; }
INLINE upb_value upb_fielddef_fval(upb_fielddef *f) { return f->fval; }
INLINE bool upb_fielddef_finalized(upb_fielddef *f) { return f->finalized; }
INLINE struct _upb_msgdef *upb_fielddef_msgdef(upb_fielddef *f) {
  return f->msgdef;
}
INLINE struct _upb_accessor_vtbl *upb_fielddef_accessor(upb_fielddef *f) {
  return f->accessor;
}

// Only meaningful once the def is in a symtab (returns NULL otherwise, or for
// a fielddef where !upb_hassubdef(f)).
upb_def *upb_fielddef_subdef(upb_fielddef *f);

// NULL until the fielddef has been added to a msgdef.

// Write accessors.  "Number" and "name" must be set before the fielddef is
// added to a msgdef.  For the moment we do not allow these to be set once
// the fielddef is added to a msgdef -- this could be relaxed in the future.
void upb_fielddef_setnumber(upb_fielddef *f, int32_t number);
void upb_fielddef_setname(upb_fielddef *f, upb_string *name);

// These writers may be called at any time prior to being put in a symtab.
void upb_fielddef_settype(upb_fielddef *f, uint8_t type);
void upb_fielddef_setlabel(upb_fielddef *f, uint8_t label);
void upb_fielddef_setdefault(upb_fielddef *f, upb_value value);
void upb_fielddef_setfval(upb_fielddef *f, upb_value fval);
void upb_fielddef_setaccessor(upb_fielddef *f, struct _upb_accessor_vtbl *vtbl);
// The name of the message or enum this field is referring to.  Must be found
// at name resolution time (when the symtabtxn is committed to the symtab).
void upb_fielddef_settypename(upb_fielddef *f, upb_string *name);

// A variety of tests about the type of a field.
INLINE bool upb_issubmsgtype(upb_fieldtype_t type) {
  return type == UPB_TYPE(GROUP) || type == UPB_TYPE(MESSAGE);
}
INLINE bool upb_isstringtype(upb_fieldtype_t type) {
  return type == UPB_TYPE(STRING) || type == UPB_TYPE(BYTES);
}
INLINE bool upb_isprimitivetype(upb_fieldtype_t type) {
  return !upb_issubmsgtype(type) && !upb_isstringtype(type);
}
INLINE bool upb_issubmsg(upb_fielddef *f) { return upb_issubmsgtype(f->type); }
INLINE bool upb_isstring(upb_fielddef *f) { return upb_isstringtype(f->type); }
INLINE bool upb_isseq(upb_fielddef *f) { return f->label == UPB_LABEL(REPEATED); }

// Does the type of this field imply that it should contain an associated def?
INLINE bool upb_hasdef(upb_fielddef *f) {
  return upb_issubmsg(f) || f->type == UPB_TYPE(ENUM);
}


/* upb_msgdef *****************************************************************/

// Structure that describes a single .proto message type.
typedef struct _upb_msgdef {
  upb_def base;

  // Tables for looking up fields by number and name.
  upb_inttable itof;  // int to field
  upb_strtable ntof;  // name to field

  // The following fields may be modified until finalized.
  uint16_t size;
  uint8_t hasbit_bytes;
  // The range of tag numbers used to store extensions.
  uint32_t extension_start;
  uint32_t extension_end;
} upb_msgdef;

// Hash table entries for looking up fields by name or number.
typedef struct {
  bool junk;
  upb_fielddef *f;
} upb_itof_ent;
typedef struct {
  upb_strtable_entry e;
  upb_fielddef *f;
} upb_ntof_ent;

upb_msgdef *upb_msgdef_new();
INLINE void upb_msgdef_unref(upb_msgdef *md) { upb_def_unref(UPB_UPCAST(md)); }
INLINE void upb_msgdef_ref(upb_msgdef *md) { upb_def_ref(UPB_UPCAST(md)); }

// Returns a new msgdef that is a copy of the given msgdef (and a copy of all
// the fields) but with any references to submessages broken and replaced with
// just the name of the submessage.  This can be put back into another symtab
// and the names will be re-resolved in the new context.
upb_msgdef *upb_msgdef_dup(upb_msgdef *m);

// Read accessors.  May be called at any time.
INLINE uint16_t upb_msgdef_size(upb_msgdef *m) { return m->size; }
INLINE uint8_t upb_msgdef_hasbit_bytes(upb_msgdef *m) {
  return m->hasbit_bytes;
}
INLINE uint32_t upb_msgdef_extension_start(upb_msgdef *m) {
  return m->extension_start;
}
INLINE uint32_t upb_msgdef_extension_end(upb_msgdef *m) {
  return m->extension_end;
}

// Write accessors.  May only be called before the msgdef is in a symtab.
void upb_msgdef_setsize(upb_msgdef *m, uint16_t size);
void upb_msgdef_sethasbit_bytes(upb_msgdef *m, uint16_t bytes);
void upb_msgdef_setextension_start(upb_msgdef *m, uint32_t start);
void upb_msgdef_setextension_end(upb_msgdef *m, uint32_t end);

// Adds a fielddef to a msgdef, and passes a ref on the field to the msgdef.
// May only be done before the msgdef is in a symtab.  The fielddef's name and
// number must be set, and the message may not already contain any field with
// this name or number -- if it does, the fielddef is unref'd and false is
// returned.  The fielddef may not already belong to another message.
bool upb_msgdef_addfield(upb_msgdef *m, upb_fielddef *f);

// Sets the layout of all fields according to default rules:
// 1. Hasbits for required fields come first, then optional fields.
// 2. Values are laid out in a way that respects alignment rules.
// 3. The order is chosen to minimize memory usage.
// This should only be called once all fielddefs have been added.
// TODO: will likely want the ability to exclude strings/submessages/arrays.
// TODO: will likely want the ability to define a header size.
void upb_msgdef_layout(upb_msgdef *m);

// Looks up a field by name or number.  While these are written to be as fast
// as possible, it will still be faster to cache the results of this lookup if
// possible.  These return NULL if no such field is found.
INLINE upb_fielddef *upb_msgdef_itof(upb_msgdef *m, uint32_t i) {
  upb_itof_ent *e = (upb_itof_ent*)
      upb_inttable_fastlookup(&m->itof, i, sizeof(upb_itof_ent));
  return e ? e->f : NULL;
}

INLINE upb_fielddef *upb_msgdef_ntof(upb_msgdef *m, upb_string *name) {
  upb_ntof_ent *e = (upb_ntof_ent*)upb_strtable_lookup(&m->ntof, name);
  return e ? e->f : NULL;
}

INLINE int upb_msgdef_numfields(upb_msgdef *m) {
  return upb_strtable_count(&m->ntof);
}

// Iteration over fields.  The order is undefined.
// Iterators are invalidated when a field is added or removed.
//   upb_msg_iter i;
//   for(i = upb_msg_begin(m); !upb_msg_done(i); i = upb_msg_next(m, i)) {
//     upb_fielddef *f = upb_msg_iter_field(i);
//     // ...
//   }
typedef upb_inttable_iter upb_msg_iter;

upb_msg_iter upb_msg_begin(upb_msgdef *m);
upb_msg_iter upb_msg_next(upb_msgdef *m, upb_msg_iter iter);
INLINE bool upb_msg_done(upb_msg_iter iter) { return upb_inttable_done(iter); }

// Iterator accessor.
INLINE upb_fielddef *upb_msg_iter_field(upb_msg_iter iter) {
  upb_itof_ent *ent = (upb_itof_ent*)upb_inttable_iter_value(iter);
  return ent->f;
}


/* upb_enumdef ****************************************************************/

typedef struct _upb_enumdef {
  upb_def base;
  upb_strtable ntoi;
  upb_inttable iton;
  int32_t defaultval;
} upb_enumdef;

typedef struct {
  upb_strtable_entry e;
  uint32_t value;
} upb_ntoi_ent;

typedef struct {
  bool junk;
  upb_string *string;
} upb_iton_ent;

upb_enumdef *upb_enumdef_new();
INLINE void upb_enumdef_ref(upb_enumdef *e) { upb_def_ref(UPB_UPCAST(e)); }
INLINE void upb_enumdef_unref(upb_enumdef *e) { upb_def_unref(UPB_UPCAST(e)); }
upb_enumdef *upb_enumdef_dup(upb_enumdef *e);

INLINE int32_t upb_enumdef_default(upb_enumdef *e) { return e->defaultval; }

// May only be set before the enumdef is in a symtab.
void upb_enumdef_setdefault(upb_enumdef *e, int32_t val);

// Adds a value to the enumdef.  Requires that no existing val has this
// name or number (returns false and does not add if there is).  May only
// be called before the enumdef is in a symtab.
bool upb_enumdef_addval(upb_enumdef *e, upb_string *name, int32_t num);

// Lookups from name to integer and vice-versa.
bool upb_enumdef_ntoi(upb_enumdef *e, upb_string *name, int32_t *num);
// Caller does not own a ref on the returned string.
upb_string *upb_enumdef_iton(upb_enumdef *e, int32_t num);

// Iteration over name/value pairs.  The order is undefined.
// Adding an enum val invalidates any iterators.
//   upb_enum_iter i;
//   for(i = upb_enum_begin(e); !upb_enum_done(i); i = upb_enum_next(e, i)) {
//     // ...
//   }
typedef upb_inttable_iter upb_enum_iter;

upb_enum_iter upb_enum_begin(upb_enumdef *e);
upb_enum_iter upb_enum_next(upb_enumdef *e, upb_enum_iter iter);
INLINE bool upb_enum_done(upb_enum_iter iter) { return upb_inttable_done(iter); }

// Iterator accessors.
INLINE upb_string *upb_enum_iter_name(upb_enum_iter iter) {
  upb_iton_ent *e = (upb_iton_ent*)upb_inttable_iter_value(iter);
  return e->string;
}
INLINE int32_t upb_enum_iter_number(upb_enum_iter iter) {
  return upb_inttable_iter_key(iter);
}


/* upb_symtabtxn **************************************************************/

// A symbol table transaction is a map of defs that can be added to a symtab
// in one single atomic operation that either succeeds or fails.  Mutable defs
// can be added to this map (and perhaps removed, in the future).
//
// A symtabtxn is not thread-safe.

typedef struct {
  upb_strtable deftab;
} upb_symtabtxn;

void upb_symtabtxn_init(upb_symtabtxn *t);
void upb_symtabtxn_uninit(upb_symtabtxn *t);

// Adds a def to the symtab.  Caller passes a ref on the def to the symtabtxn.
// The def's name must be set and there must not be any existing defs in the
// symtabtxn with this name, otherwise false will be returned and no operation
// will be performed (and the ref on the def will be released).
bool upb_symtabtxn_add(upb_symtabtxn *t, upb_def *def);

// Gets the def (if any) that is associated with this name in the symtab.
// Caller does *not* inherit a ref on the def.
upb_def *upb_symtabtxn_get(upb_symtabtxn *t, upb_string *name);

// Iterate over the defs that are part of the transaction.
// The order is undefined.
// The iterator is invalidated by upb_symtabtxn_add().
//   upb_symtabtxn_iter i;
//   for(i = upb_symtabtxn_begin(t); !upb_symtabtxn_done(t);
//       i = upb_symtabtxn_next(t, i)) {
//     upb_def *def = upb_symtabtxn_iter_def(i);
//   }
typedef void* upb_symtabtxn_iter;

upb_symtabtxn_iter upb_symtabtxn_begin(upb_symtabtxn *t);
upb_symtabtxn_iter upb_symtabtxn_next(upb_symtabtxn *t, upb_symtabtxn_iter i);
bool upb_symtabtxn_done(upb_symtabtxn_iter i);
upb_def *upb_symtabtxn_iter_def(upb_symtabtxn_iter iter);


/* upb_symtab *****************************************************************/

// A SymbolTable is where upb_defs live.  It is empty when first constructed.
// Clients add definitions to the symtab (or replace existing definitions) by
// using a upb_symtab_commit() or calling upb_symtab_add().

// upb_deflist: A little dynamic array for storing a growing list of upb_defs.
typedef struct {
  upb_def **defs;
  uint32_t len;
  uint32_t size;
} upb_deflist;

void upb_deflist_init(upb_deflist *l);
void upb_deflist_uninit(upb_deflist *l);
void upb_deflist_push(upb_deflist *l, upb_def *d);

struct _upb_symtab {
  upb_atomic_t refcount;
  upb_rwlock_t lock;       // Protects all members except the refcount.
  upb_strtable symtab;     // The symbol table.
  upb_deflist olddefs;
};

upb_symtab *upb_symtab_new(void);
INLINE void upb_symtab_ref(upb_symtab *s) { upb_atomic_ref(&s->refcount); }
void upb_symtab_unref(upb_symtab *s);

// Resolves the given symbol using the rules described in descriptor.proto,
// namely:
//
//    If the name starts with a '.', it is fully-qualified.  Otherwise, C++-like
//    scoping rules are used to find the type (i.e. first the nested types
//    within this message are searched, then within the parent, on up to the
//    root namespace).
//
// If a def is found, the caller owns one ref on the returned def.  Otherwise
// returns NULL.
// TODO: make return const
upb_def *upb_symtab_resolve(upb_symtab *s, upb_string *base, upb_string *sym);

// Find an entry in the symbol table with this exact name.  If a def is found,
// the caller owns one ref on the returned def.  Otherwise returns NULL.
// TODO: make return const
upb_def *upb_symtab_lookup(upb_symtab *s, upb_string *sym);

// Gets an array of pointers to all currently active defs in this symtab.  The
// caller owns the returned array (which is of length *count) as well as a ref
// to each symbol inside.  If type is UPB_DEF_ANY then defs of all types are
// returned, otherwise only defs of the required type are returned.
// TODO: make return const
upb_def **upb_symtab_getdefs(upb_symtab *s, int *n, upb_deftype_t type);

// Adds a single upb_def into the symtab.  A ref on the def is passed to the
// symtab.  If any references cannot be resolved, false is returned and the
// symtab is unchanged.  The error (if any) is saved to status if non-NULL.
bool upb_symtab_add(upb_symtab *s, upb_def *d, upb_status *status);

// Adds the set of defs contained in the transaction to the symtab, clearing
// the txn.  The entire operation either succeeds or fails.  If the operation
// fails, the symtab is unchanged, false is returned, and status indicates
// the error.
bool upb_symtab_commit(upb_symtab *s, upb_symtabtxn *t, upb_status *status);

// Frees defs that are no longer active in the symtab and are no longer
// reachable.  Such defs are not freed when they are replaced in the symtab
// if they are still reachable from defs that are still referenced.
void upb_symtab_gc(upb_symtab *s);


/* upb_def casts **************************************************************/

// Dynamic casts, for determining if a def is of a particular type at runtime.
#define UPB_DYNAMIC_CAST_DEF(lower, upper) \
  struct _upb_ ## lower;  /* Forward-declare. */ \
  INLINE struct _upb_ ## lower *upb_dyncast_ ## lower(upb_def *def) { \
    if(def->type != UPB_DEF_ ## upper) return NULL; \
    return (struct _upb_ ## lower*)def; \
  }
UPB_DYNAMIC_CAST_DEF(msgdef, MSG);
UPB_DYNAMIC_CAST_DEF(enumdef, ENUM);
UPB_DYNAMIC_CAST_DEF(svcdef, SERVICE);
UPB_DYNAMIC_CAST_DEF(unresolveddef, UNRESOLVED);
#undef UPB_DYNAMIC_CAST_DEF

// Downcasts, for when some wants to assert that a def is of a particular type.
// These are only checked if we are building debug.
#define UPB_DOWNCAST_DEF(lower, upper) \
  struct _upb_ ## lower;  /* Forward-declare. */ \
  INLINE struct _upb_ ## lower *upb_downcast_ ## lower(upb_def *def) { \
    assert(def->type == UPB_DEF_ ## upper); \
    return (struct _upb_ ## lower*)def; \
  }
UPB_DOWNCAST_DEF(msgdef, MSG);
UPB_DOWNCAST_DEF(enumdef, ENUM);
UPB_DOWNCAST_DEF(svcdef, SERVICE);
UPB_DOWNCAST_DEF(unresolveddef, UNRESOLVED);
#undef UPB_DOWNCAST_DEF

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif  /* UPB_DEF_H_ */
