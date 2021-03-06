/**
 * Copyright (c) 2009 Ma Can <ml.macana@gmail.com>
 *                           <macan@ncic.ac.cn>
 *
 * Time-stamp: <2009-06-29 15:56:59 macan>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef __SVFS_TRACING_H__
#define __SVFS_TRACING_H__

#include "../mdc/mdc.h"
#include "../lib/lib.h"

#ifdef SVFS_ASSERT
#define ASSERT(expr)                            \
    if(!(expr)) {                               \
        printk( "\n" __FILE__ ":%d: Assertion " \
                #expr " failed!\n",__LINE__);   \
        panic(#expr);                           \
    }
#else
#define ASSERT(expr)
#endif

struct tracing_flag
{
    char name[64];
    unsigned int *loc;
    struct list_head list;
};

#endif
