#pragma once
#include <ntdll.h>
#include <string>
#include <mutex>
#include "..\ic_char_traits.h"
#include "segment.h"
#include "..\ntapi\string_span.h"
#include "..\ntapi\critical_section.h"

EXTERN_C IMAGE_DOS_HEADER __ImageBase;

namespace pe
{
  uintptr_t module::handle() const
  {
    return reinterpret_cast<uintptr_t>(this);
  }

  module::operator HINSTANCE() const
  {
    return reinterpret_cast<HINSTANCE>(const_cast<pe::module *>(this));
  }

  ic_wstring module::base_name() const
  {
    ntapi::critical_section crit(NtCurrentPeb()->LoaderLock);
    std::lock_guard<ntapi::critical_section> guard(crit);

    const auto ModuleListHead = &NtCurrentPeb()->Ldr->InLoadOrderModuleList;
    for ( auto Entry = ModuleListHead->Flink; Entry != ModuleListHead; Entry = Entry->Flink ) {
      auto Module = CONTAINING_RECORD(Entry, LDR_DATA_TABLE_ENTRY, InLoadOrderLinks);

      if ( Module->DllBase == this ) {
        const auto name = static_cast<ntapi::ustring_span *>(&Module->BaseDllName);
        return ic_wstring(name->begin(), name->end());
      }
    }
    return {};
  }

  ic_wstring module::full_name() const
  {
    ntapi::critical_section crit(NtCurrentPeb()->LoaderLock);
    std::lock_guard<ntapi::critical_section> guard(crit);

    const auto ModuleListHead = &NtCurrentPeb()->Ldr->InLoadOrderModuleList;
    for ( auto Entry = ModuleListHead->Flink; Entry != ModuleListHead; Entry = Entry->Flink ) {
      auto Module = CONTAINING_RECORD(Entry, LDR_DATA_TABLE_ENTRY, InLoadOrderLinks);

      if ( Module->DllBase == this ) {
        const auto name = static_cast<ntapi::ustring_span *>(&Module->FullDllName);
        return ic_wstring(name->begin(), name->end());
      }
    }
    return {};
  }

  IMAGE_DOS_HEADER *module::dos_header()
  {
    return reinterpret_cast<IMAGE_DOS_HEADER *>(this);
  }

  const IMAGE_DOS_HEADER *module::dos_header() const
  {
    return reinterpret_cast<const IMAGE_DOS_HEADER *>(this);
  }

  IMAGE_NT_HEADERS *module::nt_header()
  {
    if ( this->e_magic != IMAGE_DOS_SIGNATURE )
      return nullptr;

    const auto ntheader = this->rva_to<IMAGE_NT_HEADERS>(this->e_lfanew);
    if ( ntheader->Signature != IMAGE_NT_SIGNATURE )
      return nullptr;

    return ntheader;
  }

  const IMAGE_NT_HEADERS *module::nt_header() const
  {
    return const_cast<pe::module *>(this)->nt_header();
  }

  size_t module::size() const
  {
    if ( this->e_magic != IMAGE_DOS_SIGNATURE )
      return 0;

    const auto ntheader = this->nt_header();
    if ( !ntheader
      || !ntheader->FileHeader.SizeOfOptionalHeader
      || ntheader->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR_MAGIC )
      return 0;

    return ntheader->OptionalHeader.SizeOfImage;
  }

  gsl::span<class pe::segment> module::segments()
  {
    const auto ntheader = this->nt_header();
    if ( !ntheader )
      return {};

    return gsl::make_span(
      reinterpret_cast<class pe::segment *>(IMAGE_FIRST_SECTION(ntheader)),
      ntheader->FileHeader.NumberOfSections);
  }

  gsl::span<const class pe::segment> module::segments() const
  {
    return const_cast<pe::module *>(this)->segments();
  }

  class pe::segment *module::segment(const std::string_view &name)
  {
    const auto segments = this->segments();
    const auto &it = std::find_if(segments.begin(), segments.end(), [&](const pe::segment &x) {
      return x.name() == name;
    });
    if ( it != segments.end() )
      return &*it;

    throw nullptr;
  }

  const class pe::segment *module::segment(const std::string_view &name) const
  {
    return const_cast<pe::module *>(this)->segment(name);
  }

  class export_directory *module::export_directory()
  {
    const auto ntheader = this->nt_header();
    if ( !ntheader
      || !ntheader->FileHeader.SizeOfOptionalHeader
      || ntheader->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR_MAGIC
      || !ntheader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress
      || !ntheader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].Size )
      return nullptr;

    return this->rva_to<pe::export_directory>(
      ntheader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress);
  }

  const class export_directory *module::export_directory() const
  {
    return const_cast<pe::module *>(this)->export_directory();
  }

  void *module::find_function(const char *name) const
  {
    if ( !name ) return nullptr;

    if ( PVOID ProcedureAddress;
      NT_SUCCESS(LdrGetProcedureAddress(const_cast<pe::module *>(this), ntapi::string_span(name), 0, &ProcedureAddress)) ) {
      return ProcedureAddress;
    }
    return nullptr;
  }

  void *module::find_function(uint32_t num) const
  {
    if ( !num ) return nullptr;

    if ( PVOID ProcedureAddress;
      NT_SUCCESS(LdrGetProcedureAddress(const_cast<pe::module *>(this), nullptr, num, &ProcedureAddress)) ) {
      return ProcedureAddress;
    }
    return nullptr;
  };

  module *get_module(const wchar_t *name)
  {
    if ( !name )
      return reinterpret_cast<module *>(NtCurrentPeb()->ImageBaseAddress);

    if ( PVOID DllHandle;
      NT_SUCCESS(LdrGetDllHandle(nullptr, nullptr, ntapi::ustring_span(name), &DllHandle)) ) {
      return reinterpret_cast<module *>(DllHandle);
    }
    return nullptr;
  }

  module *get_module_from_address(void *pc)
  {
    void *Unused;
    return reinterpret_cast<module *>(RtlPcToFileHeader(pc, &Unused));
  }

  const module *get_module_from_address(const void *pc)
  {
    return get_module_from_address(const_cast<void *>(pc));
  }

  module *get_instance_module()
  {
    return reinterpret_cast<module *>(&__ImageBase);
  }
}
