// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

#define BUCKET_SIZE 13

struct buf bcache[NBUF];

struct {
  struct spinlock lock;
  struct buf *buf;
  int avlbuf;
} st[BUCKET_SIZE];

static uint
hash(uint key)
{
  return key % BUCKET_SIZE;
}

static void
setup(struct buf **b, uint dev, uint blockno)
{
  (*b)->dev = dev;
  (*b)->blockno = blockno;
  (*b)->valid = 0;
  (*b)->refcnt = 1;
}

void
binit(void)
{
  struct buf *b;
  int i;

  for(i = 0; i < NBUF; i++) {
    uint hi = hash(i);
    st[hi].avlbuf++;
    b = st[hi].buf;
    st[hi].buf = &bcache[i];
    st[hi].buf->next = b;
    initsleeplock(&bcache[i].lock, "buffer");
  }

  for(i = 0; i < BUCKET_SIZE; i++)
    initlock(&st[i].lock, "bcachebucket");
} 

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  struct buf *freepos = 0;

  uint i = hash(blockno);

  // Is the block already cached?
  acquire(&st[i].lock);
  for(b = st[i].buf; b != 0; b = b->next) {
    if(b->refcnt == 0) {
      freepos = b;
    } else if(b->dev == dev && b->blockno == blockno) {
      b->refcnt++;
      release(&st[i].lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached.
  // Check self 
  if(freepos != 0) {
    st[i].avlbuf--;
    setup(&freepos, dev, blockno);
    release(&st[i].lock);
    acquiresleep(&freepos->lock);
    return freepos;
  }

  // No space, steal it (need recheck?)
  release(&st[i].lock);
  for(int k = 0; k < BUCKET_SIZE; k++) {
    if(k == i)
      continue;
    acquire(&st[k].lock);
    if(st[k].avlbuf == 0) {
      release(&st[k].lock);
      continue;
    }
    if(st[k].buf->refcnt == 0) {
      b = st[k].buf;
      st[k].buf = b->next;
      b->next = 0;
    } else {
      for(b = st[k].buf; b->next->refcnt != 0; b = b->next)
        ;
      freepos = b->next;
      b->next = b->next->next;
      freepos->next = 0;
      b = freepos;
    }
    st[k].avlbuf--;
    release(&st[k].lock);

    setup(&b, dev, blockno);

    acquire(&st[i].lock);
    freepos = st[i].buf;
    st[i].buf = b;
    b->next = freepos;
    release(&st[i].lock);
    acquiresleep(&b->lock);
    return b;
  }
  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  uint i = hash(b->blockno);

  acquire(&st[i].lock);
  b->refcnt--;
  if(b->refcnt == 0)
    st[i].avlbuf++;
  release(&st[i].lock);
}

void
bpin(struct buf *b) 
{
  uint i = hash(b->blockno);

  acquire(&st[i].lock);
  b->refcnt++;
  release(&st[i].lock);
}

void
bunpin(struct buf *b) 
{
  uint i = hash(b->blockno);

  acquire(&st[i].lock);
  b->refcnt--;
  release(&st[i].lock);
}
