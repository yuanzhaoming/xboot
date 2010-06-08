/*
 * kernel/core/disk.c
 *
 * Copyright (c) 2007-2010  jianjun jiang <jerryjianjun@gmail.com>
 * website: http://xboot.org
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
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <configs.h>
#include <default.h>
#include <xboot.h>
#include <malloc.h>
#include <vsprintf.h>
#include <xboot/printk.h>
#include <xboot/initcall.h>
#include <xboot/list.h>
#include <xboot/proc.h>
#include <xboot/disk.h>

/* the list of disk */
static struct disk_list __disk_list = {
	.entry = {
		.next	= &(__disk_list.entry),
		.prev	= &(__disk_list.entry),
	},
};
static struct disk_list * disk_list = &__disk_list;

/*
 * search disk by name
 */
static struct disk * search_disk(const char * name)
{
	struct disk_list * list;
	struct list_head * pos;

	if(!name)
		return NULL;

	for(pos = (&disk_list->entry)->next; pos != (&disk_list->entry); pos = pos->next)
	{
		list = list_entry(pos, struct disk_list, entry);
		if(strcmp((x_s8*)list->disk->name, (const x_s8 *)name) == 0)
			return list->disk;
	}

	return NULL;
}

/*
 * register a disk into disk_list
 */
x_bool register_disk(struct disk * disk)
{
	struct disk_list * list;
	struct partition * part;

	list = malloc(sizeof(struct disk_list));
	if(!list || !disk)
	{
		free(list);
		return FALSE;
	}

	if(!disk->name || search_disk(disk->name))
	{
		free(list);
		return FALSE;
	}

	if(!partition_parser_probe(disk))
	{
		init_list_head(&(disk->info.entry));

		part = malloc(sizeof(struct partition));
		if(!part)
		{
			free(list);
			return FALSE;
		}

		part->from = 0;
		part->size = disk->sector_count;
		list_add_tail(&part->entry, &(disk->info.entry));
	}

	list->disk = disk;
	list_add(&list->entry, &disk_list->entry);

	return TRUE;
}

/*
 * unregister disk from disk_list
 */
x_bool unregister_disk(struct disk * disk)
{
	struct disk_list * list;
	struct list_head * pos;

	if(!disk || !disk->name)
		return FALSE;

	for(pos = (&disk_list->entry)->next; pos != (&disk_list->entry); pos = pos->next)
	{
		list = list_entry(pos, struct disk_list, entry);
		if(list->disk == disk)
		{
			list_del(pos);
			free(list);
			return TRUE;
		}
	}

	return FALSE;
}

/*
 * disk read function, just used by partition parser.
 */
x_bool disk_read(struct disk * disk, x_u8 * buf, x_u32 offset, x_u32 size)
{
	x_u8 * sector_buf;
	x_u8 * p = buf;
	x_u32 sector_size;
	x_u32 sector, len = 0;
	x_u32 o = 0, l = 0;

	if(!disk)
		return FALSE;

	if( (buf == NULL) || (size <= 0) )
		return FALSE;

	sector_size = disk->sector_size;
	if(sector_size <= 0)
		return FALSE;

	sector_buf = malloc(disk->sector_size);
	if(!sector_buf)
		return FALSE;

	while(len < size)
	{
		sector = offset / sector_size;
		o = offset % sector_size;
		l = sector_size - o;

		if(len + l > size)
			l = size - len;

		if(disk->read_sector(disk, sector, sector_buf) == FALSE)
		{
			free(sector_buf);
			return FALSE;
		}

		memcpy((void *)p, (const void *)(&sector_buf[o]), l);

		offset += l;
		p += l;
		len += l;
	}

	free(sector_buf);
	return TRUE;
}

/*
 * disk proc interface
 */
static x_s32 disk_proc_read(x_u8 * buf, x_s32 offset, x_s32 count)
{
	struct disk_list * list;
	struct list_head * pos;
	x_s8 * p;
	x_s32 len = 0;

	if((p = malloc(SZ_4K)) == NULL)
		return 0;

	for(pos = (&disk_list->entry)->next; pos != (&disk_list->entry); pos = pos->next)
	{
		list = list_entry(pos, struct disk_list, entry);
		len += sprintf((x_s8 *)(p + len), (const x_s8 *)"%s:\r\n", list->disk->name);
	}

	len -= offset;

	if(len < 0)
		len = 0;

	if(len > count)
		len = count;

	memcpy(buf, (x_u8 *)(p + offset), len);
	free(p);

	return len;
}

static struct proc disk_proc = {
	.name	= "disk",
	.read	= disk_proc_read,
};

/*
 * disk pure sync init
 */
static __init void disk_pure_sync_init(void)
{
	/* register disk proc interface */
	proc_register(&disk_proc);
}

static __exit void disk_pure_sync_exit(void)
{
	/* unregister disk proc interface */
	proc_unregister(&disk_proc);
}

module_init(disk_pure_sync_init, LEVEL_PURE_SYNC);
module_exit(disk_pure_sync_exit, LEVEL_PURE_SYNC);
