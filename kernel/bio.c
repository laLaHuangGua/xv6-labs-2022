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

struct {
  struct spinlock lock;
  struct buf buf[NBUF];
} bcache;

struct {
  struct spinlock lock;
  struct buf *buf;
} st[BUCKET_SIZE];

static uint
hash(uint key)
{
  return key % BUCKET_SIZE;
}

static void
reuse(struct buf **b, uint bukno)
{
  struct buf *temp;

  temp = st[bukno].buf;
  st[bukno].buf = *b;
  st[bukno].buf->next = temp;
}

void
binit(void)
{
  struct buf *b;

  // initlock(&bcache.lock, "bcache");
  for(int i = 0; i < BUCKET_SIZE; i++)
    initlock(&st[i].lock, "bcachebucket");
  for(b = bcache.buf; b < bcache.buf+NBUF; b++)
    initsleeplock(&b->lock, "buffer");
} 

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  uint i = hash(blockno);

  // Is the block already cached?
  acquire(&st[i].lock);
  for(b = st[i].buf; b != 0; b = b->next) {
    if(b->dev == dev && b->blockno == blockno) {
      b->refcnt++;
      release(&st[i].lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached.
  // acquire(&bcache.lock);
  for(int j = 0; j < NBUF; j++) {
    b = &bcache.buf[j];
    if(b->refcnt == 0) {
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      reuse(&b, i);
      // release(&bcache.lock);
      release(&st[i].lock);
      acquiresleep(&b->lock);
      return b;
    }
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

  struct buf *k;
  uint i = hash(b->blockno);

  acquire(&st[i].lock);
  b->refcnt--;
  if(b->refcnt == 0) {
    if(st[i].buf == b) {
      st[i].buf = b->next;
    } else {
      k = st[i].buf;
      while(k->next != b && k->next != 0)
        k = k->next;
      k->next = b->next;
    }
    b->next = 0;
  }

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