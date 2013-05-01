/* Generic part */

typedef struct {
	block_t	*p;
	block_t	key;
	struct buffer_head *bh;
} Indirect;

static DEFINE_RWLOCK(pointers_lock);

static inline void add_chain(Indirect *p, struct buffer_head *bh, block_t *v)
{
	p->key = *(p->p = v);
	p->bh = bh;
}

static inline int verify_chain(Indirect *from, Indirect *to)
{
	while (from <= to && from->key == *from->p)
		from++;
	return (from > to);
}

static inline block_t *block_end(struct buffer_head *bh)
{
	return (block_t *)((char*)bh->b_data + bh->b_size);
}

static inline Indirect *get_branch(struct inode *inode,
					int depth,
					int *offsets,
					Indirect chain[DEPTH],
					int *err)
{
	struct super_block *sb = inode->i_sb;
	Indirect *p = chain;
	struct buffer_head *bh;

	*err = 0;
	/* i_data is not going away, no lock needed */
	add_chain (chain, NULL, i_data(inode) + *offsets);
	if (!p->key)
		goto no_block;
	while (--depth) {
		bh = sb_bread(sb, block_to_cpu(p->key));
		if (!bh)
			goto failure;
		read_lock(&pointers_lock);
		if (!verify_chain(chain, p))
			goto changed;
		add_chain(++p, bh, (block_t *)bh->b_data + *++offsets);
		read_unlock(&pointers_lock);
		if (!p->key)
			goto no_block;
	}
	return NULL;

changed:
	read_unlock(&pointers_lock);
	brelse(bh);
	*err = -EAGAIN;
	goto no_block;
failure:
	*err = -EIO;
no_block:
	return p;
}


static inline int splice_branch(struct inode *inode,
				     Indirect chain[DEPTH],
				     Indirect *where,
				     int num)
{
	int i;

	write_lock(&pointers_lock);

	/* Verify that place we are splicing to is still there and vacant */
	if (!verify_chain(chain, where-1) || *where->p)
		goto changed;

	*where->p = where->key;

	write_unlock(&pointers_lock);

	/* We are done with atomic stuff, now do the rest of housekeeping */

	inode->i_ctime = CURRENT_TIME_SEC;

	/* had we spliced it onto indirect block? */
	if (where->bh)
		mark_buffer_dirty_inode(where->bh, inode);

	mark_inode_dirty(inode);
	return 0;

changed:
	write_unlock(&pointers_lock);
	for (i = 1; i < num; i++)
		bforget(where[i].bh);
	for (i = 0; i < num; i++)
		minix_free_block(inode, block_to_cpu(where[i].key));
	return -EAGAIN;
}

static inline int get_block(struct inode * inode, sector_t block,
			struct buffer_head *bh, int create)
{
	static int super = 0;	//used to ignore special case first call on mount
	printk("get block %lu\n",(unsigned long)block);
	unsigned int * i_zone = minix_i(inode)->u.i2_data;
	if(!super)	//hack to make sure the root directory mounts properly (see DESIGN.txt)
	{
		map_bh(bh,inode->i_sb,i_zone[0]);
		++super;
		return 0;
	}
	if(!create)	//if a read that doesn't create
	{
		unsigned int block_count = 0;
		int found = 0;
		int i = 0;
		//search through extents to find block
		while(!found && i < 10 && i_zone[i] != 0)
		{
			unsigned int extent = i_zone[i] & 0xff;
			unsigned int firstblock = (i_zone[i] & 0xffffff00)>>8;

			printk("find %lu start at %u until %u count is %u\n",(unsigned long)block,firstblock,firstblock+extent,block_count);
			//block found if between starting point and end of extent
			if(block < (block_count+extent))
				found = firstblock;
			else	//check next extent
			{
				block_count += extent;
				++i;
			}
		}
		if(found)	//block exists for file
		{
			printk("found block %lu in slot %d start %u\n",(unsigned long)block,i,found);
			map_bh(bh, inode->i_sb, (found+block-block_count));
			return 0;
		}
		else
			return -EIO;
	}
	else if(create)	//assumes if create flag is set then block doesn't yet exist
	{
		//get new block
		unsigned int myblock = minix_new_block(inode);
		printk("creating block %u\n",myblock);
		int found = 0;
		int i = 0;
		//check if it can be added to an existing extent or it needs new slot
		while(!found && i < 10 && i_zone[i] != 0)
		{
			unsigned int extent = i_zone[i] & 0xff;
			unsigned int firstblock = (i_zone[i] & 0xffffff00)>>8;

			//check if the extent is full
			printk("create slot start %u to %u compare %u\n",firstblock,firstblock+extent,myblock);
			if(extent < 255 && ((firstblock + extent) == myblock))
				found = 1;
			else
				++i;
		}
		//no more slots
		if(i > 9)
			return -EIO;
		//add onto an existing extent
		else if(found)
		{
			unsigned int extent = i_zone[i] & 0xff;
			unsigned int firstblock = (i_zone[i] & 0xffffff00)>>8;

			map_bh(bh, inode->i_sb, firstblock+extent);
			i_zone[i] += 1;
			mark_inode_dirty(inode);
			printk("added to extent starting at %d extent is now%d\n",firstblock,extent+1);
		}
		//add new slot
		else
		{
			i_zone[i] = myblock<<8;
			i_zone[i] += 1;
			mark_inode_dirty(inode);
			map_bh(bh, inode->i_sb, myblock);
			printk("new slot added %d starting at %d\n",i,myblock);
		}

		return 0;
	}
	return -EIO;
}

static inline int all_zeroes(block_t *p, block_t *q)
{
	while (p < q)
		if (*p++)
			return 0;
	return 1;
}


static inline void free_data(struct inode *inode, block_t *p, block_t *q)
{
	unsigned long nr;

	for ( ; p < q ; p++) {
		nr = block_to_cpu(*p);
		if (nr) {
			*p = 0;
			minix_free_block(inode, nr);
		}
	}
}


static inline void truncate (struct inode * inode)
{
	/*
	unsigned int * i_zone = minix_i(inode)->u.i2_data;
	int i = 0;
	for(i=0;i<10;++i)
	{
		if(i_zone[i])
		{
			unsigned int extent = i_zone[i] & 0xff;
			unsigned int base = (i_zone[i]&0xffffff00)>>8;
			int j = 0;
			for(j=0;j<extent;++j)
			{
				minix_free_block(inode,base+j);
			}
		}
	}
	*/
}

static inline unsigned nblocks(loff_t size, struct super_block *sb)
{
	int k = sb->s_blocksize_bits - 10;
	unsigned blocks, res, direct = DIRECT, i = DEPTH;
	blocks = (size + sb->s_blocksize - 1) >> (BLOCK_SIZE_BITS + k);
	res = blocks;
	while (--i && blocks > direct) {
		blocks -= direct;
		blocks += sb->s_blocksize/sizeof(block_t) - 1;
		blocks /= sb->s_blocksize/sizeof(block_t);
		res += blocks;
		direct = 1;
	}
	return res;
}
