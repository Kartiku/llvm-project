//===-- Implementation of fdiml function ----------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "src/__support/common.h"
#include "utils/FPUtil/BasicOperations.h"

namespace __llvm_libc {

long double LLVM_LIBC_ENTRYPOINT(fdiml)(long double x, long double y) {
  return fputil::fdim(x, y);
}

} // namespace __llvm_libc
