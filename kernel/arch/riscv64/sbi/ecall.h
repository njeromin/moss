#pragma once

#include "sbi.h"

struct sbi_ret sbi_ecall(long eid, long fid, long arg0, long arg1,
                         long arg2, long arg3, long arg4, long arg5);
