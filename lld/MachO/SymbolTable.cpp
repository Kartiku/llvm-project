//===- SymbolTable.cpp ----------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "SymbolTable.h"
#include "Config.h"
#include "InputFiles.h"
#include "Symbols.h"
#include "lld/Common/ErrorHandler.h"
#include "lld/Common/Memory.h"

using namespace llvm;
using namespace lld;
using namespace lld::macho;

Symbol *SymbolTable::find(StringRef name) {
  auto it = symMap.find(llvm::CachedHashStringRef(name));
  if (it == symMap.end())
    return nullptr;
  return symVector[it->second];
}

std::pair<Symbol *, bool> SymbolTable::insert(StringRef name) {
  auto p = symMap.insert({CachedHashStringRef(name), (int)symVector.size()});

  // Name already present in the symbol table.
  if (!p.second)
    return {symVector[p.first->second], false};

  // Name is a new symbol.
  Symbol *sym = reinterpret_cast<Symbol *>(make<SymbolUnion>());
  symVector.push_back(sym);
  return {sym, true};
}

Symbol *SymbolTable::addDefined(StringRef name, InputSection *isec,
                                uint32_t value, bool isWeakDef) {
  Symbol *s;
  bool wasInserted;
  bool overridesWeakDef = false;
  std::tie(s, wasInserted) = insert(name);

  if (!wasInserted) {
    if (auto *defined = dyn_cast<Defined>(s)) {
      if (isWeakDef)
        return s;
      if (!defined->isWeakDef())
        error("duplicate symbol: " + name);
    } else if (auto *dysym = dyn_cast<DylibSymbol>(s)) {
      overridesWeakDef = !isWeakDef && dysym->isWeakDef();
    }
    // Defined symbols take priority over other types of symbols, so in case
    // of a name conflict, we fall through to the replaceSymbol() call below.
  }

  Defined *defined = replaceSymbol<Defined>(s, name, isec, value, isWeakDef,
                                            /*isExternal=*/true);
  defined->overridesWeakDef = overridesWeakDef;
  return s;
}

Symbol *SymbolTable::addUndefined(StringRef name, bool isWeakRef) {
  Symbol *s;
  bool wasInserted;
  std::tie(s, wasInserted) = insert(name);

  auto refState = isWeakRef ? RefState::Weak : RefState::Strong;

  if (wasInserted)
    replaceSymbol<Undefined>(s, name, refState);
  else if (auto *lazy = dyn_cast<LazySymbol>(s))
    lazy->fetchArchiveMember();
  else if (auto *dynsym = dyn_cast<DylibSymbol>(s))
    dynsym->refState = std::max(dynsym->refState, refState);
  else if (auto *undefined = dyn_cast<Undefined>(s))
    undefined->refState = std::max(undefined->refState, refState);
  return s;
}

Symbol *SymbolTable::addCommon(StringRef name, InputFile *file, uint64_t size,
                               uint32_t align) {
  Symbol *s;
  bool wasInserted;
  std::tie(s, wasInserted) = insert(name);

  if (!wasInserted) {
    if (auto *common = dyn_cast<CommonSymbol>(s)) {
      if (size < common->size)
        return s;
    } else if (isa<Defined>(s)) {
      return s;
    }
    // Common symbols take priority over all non-Defined symbols, so in case of
    // a name conflict, we fall through to the replaceSymbol() call below.
  }

  replaceSymbol<CommonSymbol>(s, name, file, size, align);
  return s;
}

Symbol *SymbolTable::addDylib(StringRef name, DylibFile *file, bool isWeakDef,
                              bool isTlv) {
  Symbol *s;
  bool wasInserted;
  std::tie(s, wasInserted) = insert(name);

  auto refState = RefState::Unreferenced;
  if (!wasInserted) {
    if (auto *defined = dyn_cast<Defined>(s)) {
      if (isWeakDef && !defined->isWeakDef())
        defined->overridesWeakDef = true;
    } else if (auto *undefined = dyn_cast<Undefined>(s)) {
      refState = undefined->refState;
    } else if (auto *dysym = dyn_cast<DylibSymbol>(s)) {
      refState = dysym->refState;
    }
  }

  if (wasInserted || isa<Undefined>(s) ||
      (isa<DylibSymbol>(s) && !isWeakDef && s->isWeakDef()))
    replaceSymbol<DylibSymbol>(s, file, name, isWeakDef, refState, isTlv);

  return s;
}

Symbol *SymbolTable::addLazy(StringRef name, ArchiveFile *file,
                             const llvm::object::Archive::Symbol &sym) {
  Symbol *s;
  bool wasInserted;
  std::tie(s, wasInserted) = insert(name);

  if (wasInserted)
    replaceSymbol<LazySymbol>(s, file, sym);
  else if (isa<Undefined>(s) || (isa<DylibSymbol>(s) && s->isWeakDef()))
    file->fetch(sym);
  return s;
}

Symbol *SymbolTable::addDSOHandle(const MachHeaderSection *header) {
  Symbol *s;
  bool wasInserted;
  std::tie(s, wasInserted) = insert(DSOHandle::name);
  if (!wasInserted) {
    // FIXME: Make every symbol (including absolute symbols) contain a
    // reference to their originating file, then add that file name to this
    // error message.
    if (isa<Defined>(s))
      error("found defined symbol with illegal name " + DSOHandle::name);
  }
  replaceSymbol<DSOHandle>(s, header);
  return s;
}

void lld::macho::treatUndefinedSymbol(StringRef symbolName,
                                      StringRef fileName) {
  std::string message = ("undefined symbol: " + symbolName).str();
  if (!fileName.empty())
    message += ("\n>>> referenced by " + fileName).str();
  switch (config->undefinedSymbolTreatment) {
  case UndefinedSymbolTreatment::suppress:
    break;
  case UndefinedSymbolTreatment::error:
    error(message);
    break;
  case UndefinedSymbolTreatment::warning:
    warn(message);
    break;
  case UndefinedSymbolTreatment::dynamic_lookup:
    error("dynamic_lookup unimplemented for " + message);
    break;
  case UndefinedSymbolTreatment::unknown:
    llvm_unreachable("unknown -undefined TREATMENT");
  }
}

SymbolTable *macho::symtab;
