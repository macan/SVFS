/**
 * Copyright (c) 2009 Ma Can <ml.macana@gmail.com>
 *                           <macan@ncic.ac.cn>
 *
 * Time-stamp: <2009-06-29 15:22:24 macan>
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

/*
 * content of the svfs config file:
 * $ cat > /etc/svfs_config
 * <block datastore>
 * fstype = ext4, mountpoint = /mnt/ext4, loading = (static or dynamic)
 * fstype = nfs, mountpoint = /mnt/nfs, loading = (static or dynamic)
 * fstype = ext3, mountpoint = /mnt/ext3, loading = (staic or dynamic)
 * fstype = DCFS3, mountpoint = /mnt/dcfs3, loading = (static or dynamic)
 * </block>
 * <block parameter>
 * svfs_backing_store = /tmp/svfs_backing_store
 * </block>
 * <block tracing>
 * svfs_client_tracing_flags = 0xffffffff
 * svfs_mdc_tracing_flags = 0xffffffff
 * </block>
 */

#include "svfs.h"
#include "lib.h"

static struct file *conf_filp = NULL;

int svfs_lib_config_open(char *config_file)
{
    int ret;
    
    svfs_entry(lib, "tring to open config file '%s'\n", config_file);
    if (conf_filp) {
        svfs_info(lib, "another config file '%s' has already been opened\n",
                  conf_filp->f_dentry->d_name.name);
        return -EINVAL;
    }
    
    conf_filp = filp_open(config_file, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    ret = PTR_ERR(conf_filp);
    if (IS_ERR(conf_filp)) {
        conf_filp = NULL;
    } else
        ret = 0;
    return ret;
}

void svfs_lib_config_close(void)
{
    if (!conf_filp)
        return;
    svfs_entry(lib, "tring to close config file '%s'\n",
               conf_filp->f_dentry->d_name.name);
    fput(conf_filp);
}

ssize_t svfs_lib_file_read(struct file *filp, void *buf, 
                           size_t count)
{
    mm_segment_t oldfs = get_fs();
    ssize_t retval;
    ssize_t br, bl;

    set_fs(KERNEL_DS);
    bl = count;
    while (bl) {
        br = vfs_read(filp, buf, bl, &filp->f_pos);
        if (br < 0) {
            retval = br;
            goto out;
        }
        if (br == 0) {
            retval = count - bl;
            goto out;
        }
        bl -= br;
        buf += br;
    }
    retval = count;
out:
    set_fs(oldfs);
    return retval;
}

ssize_t svfs_lib_file_write(struct file *filp, const void *buf, 
                            size_t count)
{
    mm_segment_t oldfs = get_fs();
    ssize_t retval;
    ssize_t bw, bl;
    const char *p = buf;

    set_fs(KERNEL_DS);
    bl = count;
    while (bl) {
        bw = vfs_write(filp, p, bl, &filp->f_pos);
        if (bw < 0) {
            retval = bw;
            goto out;
        }
        if (bw == 0) {
            retval = count - bl;
            goto out;
        }
        bl -= bw;
        p += bw;
    }
    retval = count;
out:
    set_fs(oldfs);
    return retval;
}

int svfs_lib_config_readline(char *buf, size_t len)
{
    ssize_t br, rc = 0;
    int new_line = 0;
    char p;
    
    if (!conf_filp)
        return -EINVAL;
    do {
        br = svfs_lib_file_read(conf_filp, &p, 1);
        if (br != 1)
            goto out;
        if (p == '\n')
            new_line = 1;
        buf[rc++] = p;
    } while (rc < len && !new_line);

    if (rc > 0)
        buf[rc] = '\0';
    svfs_debug(lib, "readline rc %d: %s", (int)rc, buf);
    return rc;
out:
    if (br < 0)
        rc = br;
    return rc;
}

void svfs_lib_fpos_reset(void)
{
    conf_filp->f_pos = 0;
}

int svfs_lib_get_block(char *block)
{
    char block_start[256];
    char line[256];
    int len, rc, found = 0;

    len = snprintf(block_start, 255, "<block %s>", block);
    if (block_start[len - 1] != '>') {
        svfs_err(lib, "block name is too long!\n");
        return -EINVAL;
    }
    do {
        rc = svfs_lib_config_readline(line, 256);
        if (rc < 0)
            return rc;
        if (rc == 0)
            break;
        if (!strncmp(line, block_start, strlen(block_start))) {
            /* get it */
            found = 1;
            break;
        }
    } while (1);

    if (found) {
        svfs_debug(lib, "found the block '%s', block content start @%ld\n",
                   block_start, (unsigned long)conf_filp->f_pos);
    } else {
        return -EINVAL;
    }
    
    return 0;
}

int svfs_lib_get_value(char *block, char *key, char *value, int reset)
{
    int err, rc, found = 0;
    char line[256];
    
    if (reset) {
        svfs_lib_fpos_reset();
        err = svfs_lib_get_block(block);
        if (err)
            goto out;
    }
    do {
        rc = svfs_lib_config_readline(line, 256);
        if (rc < 0)
            return rc;
        if (rc == 0)
            break;
        if (!strncmp(line, key, strlen(key))) {
            /* get the key now */
            found  = 1;
            break;
        }
        if (!strncmp(line, "</block>", 8)) {
            break;
        }
    } while (1);
    if (found) {
        memcpy(value, line, strlen(line));
        value[strlen(line)] = '\0';
        svfs_info(lib, "Got line: %s", value);
        err = 0;
    } else {
        return -EINVAL;
    }
out:
    return err;
}

static char *__svfs_lib_eat_blanks(char *p)
{
    char *np = p;
    while (1) {
        if (*p == ' ' || *p == '\t') /* skip blanks */
            p++;
        else if (*p == '\0' || *p == '\n') {
            return (p == np) ? ERR_PTR(-EINVAL) : p;
        } else
            break;              /* normal char.s */
    }
    return p;
}

static char *__svfs_lib_eat_nonblanks(char *p)
{
    char *np = p;
    while (1) {
        if (*p == '\n' || *p == '\0')
            return (p == np) ? ERR_PTR(-EINVAL) : p;
        else if (*p == ' ' || *p == '\t')
            break;
        else
            p++;
    }
    return p;
}

int svfs_lib_k2v(char *line, char *key, char *value)
{
    char *p = line;
    int rc = 0;

    if (!line || !key)
        return -EINVAL;

    if (line[0] == '\0' || key[0] == '\0')
        return -EINVAL;

    while (1) {
        if (*p == '\0' || *p == '\n')
            break;
        /* eat blanks */
        p = __svfs_lib_eat_blanks(p);
        if (IS_ERR(p))
            return PTR_ERR(p);
        
        if (!strncmp(p, key, strlen(key))) {
            /* match the key, so get the value now */
            p += strlen(key);
            if (*p == ' ' || *p == '=' || *p == '\t') {
                /* eating the '=' and blanks */
                p = __svfs_lib_eat_blanks(p);
                if (IS_ERR(p))
                    return PTR_ERR(p);
                if (*p == '=') {
                    /* good, get the value, but first eat the blanks */
                    p++;
                    p = __svfs_lib_eat_blanks(p);
                    if (IS_ERR(p))
                        return PTR_ERR(p);
                    /* setting the value now */
                    while (1) {
                        if (*p == ',' || *p == ' ' || *p == '\t' ||
                            *p == '\n' || *p == '\0')
                            break;
                        value[rc++] = *p++;
                    }
                    value[rc] = '\0';
                    return rc;
                } else {
                    svfs_debug(lib, "assume '=' here\n");
                    return -EINVAL;
                }
            } else {
                svfs_debug(lib, "invalid format, "
                           "rewrite the config file with"
                           " format: 'key = value,' pair\n");
                return -EINVAL;
            }
        } else {
            /* skip this key-value pair */
            svfs_debug(lib, "skipping this key-value pair\n");
            p = __svfs_lib_eat_nonblanks(p);
            if (IS_ERR(p))
                return PTR_ERR(p);
            if (*p == ' ' || *p == '=' || *p == '\t') {
                p = __svfs_lib_eat_blanks(p);
                if (IS_ERR(p))
                    return PTR_ERR(p);
                if (*p == '=') {
                    p++;
                    p = __svfs_lib_eat_blanks(p);
                    if (IS_ERR(p))
                        return PTR_ERR(p);
                    p = __svfs_lib_eat_nonblanks(p);
                    if (IS_ERR(p))
                        return PTR_ERR(p);
                    /* OK to continue parsing */
                } else {
                    svfs_debug(lib, "assume '=' here\n");
                    return -EINVAL;
                }
            } else {
                svfs_debug(lib, "invalid format, "
                           "rewrite the config file with"
                           " format: 'key = value,' pair\n");
                return -EINVAL;
            }
        }
    }
    return -EINVAL;
}

void svfs_lib_test(void)
{
}
