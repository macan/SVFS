/**
 * Copyright (c) 2009 Ma Can <ml.macana@gmail.com>
 *                           <macan@ncic.ac.cn>
 *
 * Time-stamp: <2009-06-09 10:53:29 macan>
 *
 * Define SVFS inodes
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

#ifndef __SVFS_I_H__
#define __SVFS_I_H__

#include <linux/types.h>

#define SVFS_ROOT_INODE 0x00

struct svfs_super_block
{
#define SVFS_FREE    0x00000000
#define SVFS_RDONLY  0x00000001
#define SVFS_MOUNTED 0x00000002
    int flags;
    u64 fsid;
    dev_t s_dev;                /* superblock dev numbers */
    struct backing_dev_info backing_dev_info;
    struct timespec mtime;

    struct super_block *sb;
};

struct svfs_sb_mountdata 
{
    struct svfs_super_block *ssb;
    int mntflags;
};

static inline struct svfs_super_block *SVFS_SB(const struct super_block *s)
{
    return (struct svfs_super_block *)(s->s_fs_info);
}

struct svfs_referal
{
#define LLFS_TYPE_FREE 0x00
#define LLFS_TYPE_EXT4 0x01
#define LLFS_TYPE_EXT3 0x02
    u32 llfs_type;             /* llfs filesystem type */
    struct vfs_inode *llfs_inode;            /* llfs vfs inode */

    char llfs_pathname[0];
};

struct svfs_inode 
{
    u32 timestamp;
    u32 version;
    __le32 dtime;

    /* layout */

    /* small dir data & operations */

    /* llfs related */
    struct svfs_referal llfs_md; /* metadata referal for llfs */

    /* VFS inode */
    struct inode vfs_inode;
};

static inline struct svfs_inode *SVFS_I(struct inode *inode)
{
	return container_of(inode, struct svfs_inode, vfs_inode);
}

#endif
