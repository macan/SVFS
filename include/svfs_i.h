/**
 * Copyright (c) 2009 Ma Can <ml.macana@gmail.com>
 *                           <macan@ncic.ac.cn>
 *
 * Time-stamp: <2009-06-18 14:43:49 macan>
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

#define SVFS_ROOT_INODE 0x00

#ifdef SVFS_LOCAL_TEST
struct backing_store_entry
{
    u32 parent_offset;
    u32 depth;
#define SVFS_BS_FREE  0x00000000
#define SVFS_BS_NEW   0x00000001
#define SVFS_BS_DIRTY 0x00000002
#define SVFS_BS_VALID 0x00000004
#define SVFS_BS_DIR   0x80000000
#define SVFS_BS_FILE  0x40000000
#define SVFS_BS_LINK  0x20000000
    u32 state;
    u32 disk_flags;
    loff_t disksize;
    unsigned int nlink;
    umode_t mode;
    uid_t uid;
    gid_t gid;
    struct timespec atime, ctime, mtime;
    u32 llfs_type;
    char relative_path[NAME_MAX];
    char ref_path[NAME_MAX];
};
#endif

struct svfs_super_block
{
#define SVFS_SB_FREE       0x00000000
#define SVFS_SB_RDONLY     0x00000001
#define SVFS_SB_MOUNTED    0x00000002
#define SVFS_SB_LOCAL_TEST 0x80000000
    u32 flags;
    u64 fsid;
    dev_t s_dev;                /* superblock dev numbers */
    struct backing_dev_info backing_dev_info;
    struct timespec mtime;
    spinlock_t next_gen_lock;
    u32 next_generation;
#ifdef SVFS_LOCAL_TEST
    char *backing_store;
    struct file *bs_filp;
    struct backing_store_entry *bse;
    int bs_size;
#endif

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
#define LLFS_TYPE_ANY  0x80
    u32 llfs_type;             /* llfs filesystem type */
    struct path llfs_path;     /* llfs path info */

    char llfs_pathname[NAME_MAX];
};

static inline char *svfs_type_convert(int type)
{
    switch (type) {
    case LLFS_TYPE_EXT4:
        return "ext4";
    case LLFS_TYPE_EXT3:
        return "ext3";
    case LLFS_TYPE_FREE:
        return "nofs";
    default:
        return "ERRFS";
    }
}
    
struct svfs_inode 
{
    u32 version;                     /* inode version */
#define SVFS_IF_SMALL     0x80000000 /* small file */
#define SVFS_IF_LARGE     0x40000000 /* large file */
#define SVFS_IF_NORMAL    0x10000000 /* normal file */
#define SVFS_IF_COMPR     0x00800000 /* compress */
#define SVFS_IF_NOATIME   0x00008000 /* no atime */
#define SVFS_IF_SYNC      0x00000080 /* sync update */
#define SVFS_IF_APPEND    0x00000040 /* append only */
#define SVFS_IF_IMMUTABLE 0x00000020 /* immutable file */
#define SVFS_IF_DIRSYNC   0x00000010 /* dirsync */
    u32 flags;
#define SVFS_STATE_NEW    0x00000001
#define SVFS_STATE_XATTR  0x00000002 /* has xattr in-inode */
#define SVFS_STATE_DISC   0x80000000 /* disconnect inode, no llfs inode */
#define SVFS_STATE_CONN   0x40000000 /* connected inode, has llfs inode */
    u32 state;
    u32 dtime;                /* deletion time */
    loff_t disksize;            /* modified only by get_block and truncate */
    struct timespec crtime;

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

struct svfs_datastore
{
    /* Using TYPE defines in svfs_i.h: LLFS_TYPE_EXT4/... */
#define SVFS_DSTORE_FREE  0x00
#define SVFS_DSTORE_VALID 0x01
    int type, state;
    char pathname[NAME_MAX];
    struct list_head list;
    struct path root_path;
    struct super_block *sb;
};

#endif
