/*
Main program for the virtual memory project.
Make all of your modifications to this file.
You may add or rearrange any code or data as you need.
The header files page_table.h and disk.h explain
how to use the page table and disk interfaces.
*/

#include "page_table.h"
#include "disk.h"
#include "program.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define NO_FREE_FRAMES -1
#define VERBOSE 0

// Marks a function as not being implemented yet
void throw_not_implemented_error(const char *error_text) {
  printf("NOT_IMPLEMENTED_ERROR: %s\n", error_text);
  exit(1);
}

typedef struct {
  int dirty; // true if this frame *could* have been written to
  int page; // page index this frame maps to
  int time_added; // the page fault number when this frame was added
  int last_updated; // modification page fault occurrence
  int is_free; // true if this frame does not currently map to a page
} Frame;

//DEBUG Variables
int prev_page = -1;

// Global variables
Frame *frame_list;
struct disk *disk;
int page_faults = 0;
int disk_reads = 0;
int disk_writes = 0;
int (*find_frame_to_remove_function)(struct page_table *pt);


/** Randomly selects a frame to replace
 * @param pt  The page table
 * @returns   Frame index to remove
 */
int remove_frame_using_random(struct page_table *pt) {
  int nframes = page_table_get_nframes(pt);
  return lrand48() % nframes;
}

/** Selects a frame to replace based on which is the oldest.
 * @param pt  The page table
 * @returns   Frame index to remove
 */
int remove_frame_using_fifo(struct page_table *pt) {
  int nframes = page_table_get_nframes(pt);
  int i, frame_to_remove = 0, oldest = 1000000;
  for (i=0; i < nframes; ++i) {
    if (frame_list[i].time_added < oldest) {
      frame_to_remove = i;
      oldest = frame_list[i].time_added;
    }
  }
  return frame_to_remove;
}

/** Selects a frame to replace based on which was the least recently updated.
 * An update is either creation or a permission modification.
 * @param pt  The page table
 * @returns   Frame index to remove
 */
int remove_frame_using_lru(struct page_table *pt) {
  int nframes = page_table_get_nframes(pt);
  int i, frame_to_remove = 0, oldest = 10000000;
  for (i=0; i < nframes; ++i) {
    if (frame_list[i].last_updated < oldest) {
      frame_to_remove = i;
      oldest = frame_list[i].last_updated;
    }
  }
  return frame_to_remove;
}

/** Finds the first free frame of a page table.  
 *  @param pt The page_table that cooresponds to global `frame_list`
 *  returns index of first free frame. returns NO_FREE_FRAMES if no free frames exist.
 */
int find_free_frame(struct page_table *pt) {
  int i, nframes = page_table_get_nframes(pt);
  for(i=0; i < nframes; ++i) {
    if (frame_list[i].is_free) {
      return i;
    }
  }
  return NO_FREE_FRAMES;
}

// Updates the page table to point @page to @frame.
// Loads the desired frame from the disk.
void set_page_to_frame_of_page_table(struct page_table *pt, int page, int frame) {
  frame_list[frame].page = page;
  frame_list[frame].is_free = 0;
  frame_list[frame].dirty = 0;
  frame_list[frame].time_added = page_faults;
  frame_list[frame].last_updated = page_faults;
  page_table_set_entry(pt, page, frame, PROT_READ);

  char *physmem = page_table_get_physmem(pt);
  disk_read(disk, page, &physmem[frame * BLOCK_SIZE]);

  disk_reads++;
}

void page_fault_handler( struct page_table *pt, int page )
{
    page_faults++;
    if (VERBOSE) printf("page fault on page #%d\n", page);
  
    // Check if page is already in page table
    int current_frame;
    int permissions;
    page_table_get_entry(pt, page, &current_frame, &permissions);
    
    //print page table here
    if (VERBOSE) page_table_print(pt);
    if (permissions) {
        //sift the permissions and assign the correct ones
        if(permissions & PROT_WRITE) {
            // page is in page table, but does not have write permissions
            if (VERBOSE) printf("Adding write permissions to page: %d\n", page);
            
            frame_list[current_frame].last_updated = page_faults;
            frame_list[current_frame].dirty = 1;
            page_table_set_entry(pt, page, current_frame, PROT_READ|PROT_WRITE);
        }
        //this should not really ever occur, only if in table without PROT_READ
        else if(permissions & PROT_READ) {
            if (VERBOSE) printf("Adding write permissions to page: %d\n", page);
            frame_list[current_frame].dirty = 1;
            page_table_set_entry(pt, page, current_frame, PROT_READ|PROT_WRITE);
            if (VERBOSE) page_table_print(pt);
        }
    }
    else{
        //no current permissions, need to insert into table
        //either set, or remove and set
        int frame = find_free_frame(pt);
        if (frame == NO_FREE_FRAMES) {
            // No frames are free, find a frame to remove
            frame = find_frame_to_remove_function(pt);
            int curr_page = frame_list[frame].page;
            if (VERBOSE) printf("Matching page #%d to used frame #%d with matching page %d\n", page, frame, curr_page);
            
            char *physmem = page_table_get_physmem(pt);
            disk_write(disk, frame, &physmem[frame * BLOCK_SIZE]);
            disk_writes++;
            
            disk_read(disk, page, &physmem[frame * BLOCK_SIZE]);
            disk_reads++;
            
            frame_list[frame].page = page;
            frame_list[frame].is_free = 0;
            frame_list[frame].dirty = 0;
            frame_list[frame].time_added = page_faults;
            frame_list[frame].last_updated = page_faults;
            page_table_set_entry(pt, page, frame, PROT_READ);
            
            page_table_set_entry(pt, curr_page, 0, 0);
            if (VERBOSE) page_table_print(pt);
            
            if(prev_page == page) exit(0);
            prev_page = page;
            
        }
        else{
            //there was a free frame;
            if (VERBOSE) printf("Matching page #%d to free frame #%d\n", page, frame);
            set_page_to_frame_of_page_table(pt, page, frame);
            if (VERBOSE) page_table_print(pt);
        }
    }
}

int main( int argc, char *argv[] )
{
  if(argc!=5) {
    printf("use: virtmem <npages> <nframes> <rand|fifo|lru|custom> <sort|scan|focus>\n");
    return 1;
  }

  int npages = atoi(argv[1]);
  int nframes = atoi(argv[2]);
  const char *algorithm = argv[3];
  const char *program = argv[4];

  disk = disk_open("myvirtualdisk", npages);
  if(!disk) {
    fprintf(stderr, "couldn't create virtual disk: %s\n", strerror(errno));
    return 1;
  }

  struct page_table *pt = page_table_create( npages, nframes, page_fault_handler );
  if(!pt) {
    fprintf(stderr, "couldn't create page table: %s\n", strerror(errno));
    return 1;
  }

  // Initialize FrameList
  frame_list = malloc(nframes * sizeof(Frame));
  int i;
  for (i=0; i < nframes; ++i) {
    frame_list[i].dirty = 0;
    frame_list[i].is_free = 1;
  }

  char *virtmem = page_table_get_virtmem(pt);

  // Determine replacement algorithm to use
  if(!strcmp(algorithm, "rand")) {
    find_frame_to_remove_function = &remove_frame_using_random;

  } else if(!strcmp(algorithm, "fifo")) {
    find_frame_to_remove_function = &remove_frame_using_fifo;

  } else if(!strcmp(algorithm, "custom")) {
    find_frame_to_remove_function = &remove_frame_using_lru;

  } else if(!strcmp(algorithm, "lru")) {
    find_frame_to_remove_function = &remove_frame_using_lru;

  } else {
    fprintf(stderr, "unknown algorithm: %s\n", algorithm);
    return 1;
  }

  // Determine program to run
  if(!strcmp(program, "sort")) {
    sort_program(virtmem, npages*PAGE_SIZE);

  } else if(!strcmp(program, "scan")) {
    scan_program(virtmem, npages*PAGE_SIZE);

  } else if(!strcmp(program, "focus")) {
    focus_program(virtmem, npages*PAGE_SIZE);

  } else {
    fprintf(stderr, "unknown program: %s\n", program);
    return 1;
  }

  page_table_delete(pt);
  disk_close(disk);

  printf("Page Faults: %d\n", page_faults);
  printf("Disk reads: %d\n", disk_reads);
  printf("Disk writes: %d\n", disk_writes);

  return 0;
}
