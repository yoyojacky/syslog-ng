/*
 * Copyright (c) 2002-2013 BalaBit IT Ltd, Budapest, Hungary
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 */
#include "compat/mmap.h"

#ifdef _WIN32
void *
mmap(void *addr, size_t len, int prot, int flags,
       int fildes, off_t off)
{
  /*
   *    Win 32         		   POSIX
   * PAGE_READONLY		PROT_READ
   * PAGE_READWRITE		(PROT_READ | PROT_WRITE)
   * PAGE_NOACCESS		PROT_NONE
   * PAGE_EXECUTE		PROT_EXEC
   * PAGE_EXECUTE_READ		(PROT_EXEC |PROT_READ)
   * PAGE_EXECUTE_READWRITE	(PROT_EXEC | PROT_READ | PROT_WRITE)
   *
   *
   * CreateFileMapping http://msdn.microsoft.com/en-us/library/aa366537(VS.85).aspx
   *
   * mmap and CreateFileMapping http://www.ibm.com/developerworks/systems/library/es-MigratingWin32toLinux.html#N102D8
   *
   * HANDLE WINAPI CreateFileMapping(
   *   __in      HANDLE hFile,
   *   __in_opt  LPSECURITY_ATTRIBUTES lpAttributes,
   *   __in      DWORD flProtect,
   *   __in      DWORD dwMaximumSizeHigh,
   *   __in      DWORD dwMaximumSizeLow,
   *   __in_opt  LPCTSTR lpName
   * );
   *
   * LPVOID WINAPI MapViewOfFile(
   *   __in  HANDLE hFileMappingObject,
   *   __in  DWORD dwDesiredAccess,
   *   __in  DWORD dwFileOffsetHigh,
   *   __in  DWORD dwFileOffsetLow,
   *   __in  SIZE_T dwNumberOfBytesToMap
   *   );
   */

  LARGE_INTEGER offset;
  DWORD flProtect = 0;
  DWORD dwDesiredAccess = 0;
  HANDLE file_mapping;
  void *ret_addr;
  offset.QuadPart = off ;

  switch (prot)
   {
     case PROT_READ:
      flProtect = PAGE_READONLY;
      dwDesiredAccess = FILE_MAP_READ;
     break;
     case PROT_READ | PROT_WRITE:
      flProtect = PAGE_READWRITE;
      dwDesiredAccess = FILE_MAP_ALL_ACCESS;
     break;
     case PROT_NONE:
      flProtect = PAGE_NOACCESS;
     break;
     case PROT_EXEC:
      flProtect = PAGE_EXECUTE;
     break;
     case PROT_EXEC | PROT_READ:
      flProtect = PAGE_EXECUTE_READ;
     break;
     case PROT_EXEC | PROT_READ | PROT_WRITE:
      flProtect = PAGE_EXECUTE_READWRITE;
      dwDesiredAccess = FILE_MAP_ALL_ACCESS;
     break;
   };

  file_mapping = CreateFileMapping((HANDLE)_get_osfhandle(fildes), NULL, flProtect, 0, 0, NULL);

  if (file_mapping == NULL)
    return MAP_FAILED;

  ret_addr = MapViewOfFile(file_mapping, dwDesiredAccess, offset.HighPart, offset.LowPart, len);

  /*
   * Mapped views of a file mapping object maintain internal references to the object,
   * and a file mapping object does not close until all references to it are released.
   * Therefore, to fully close a file mapping object, an application must unmap all mapped
   * views of the file mapping object by calling UnmapViewOfFile and close the file mapping
   * object handle by calling CloseHandle. These functions can be called in any order.
   */

   CloseHandle(file_mapping);
   if (ret_addr)
     return ret_addr;
   else
     return MAP_FAILED;
}

int
munmap(void *addr, size_t len)
{
  return UnmapViewOfFile(addr);
}

int
madvise(void *addr, size_t len, int advice)
{
  return TRUE;
}
#endif
