#include <iostream>
#include <stdlib.h>
#include <memory.h>

#include "hfpage.h"
#include "heapfile.h"
#include "buf.h"
#include "db.h"


// **********************************************************
// page class constructor

void HFPage::init(PageId pageNo)
{
  this->curPage  = pageNo;
  this->prevPage = INVALID_PAGE;
  this->nextPage = INVALID_PAGE;
  this->slotCnt  = 0;
  this->slot[0].length = EMPTY_SLOT;
  this->usedPtr  = (MAX_SPACE - DPFIXED);
  this->freeSpace = (MAX_SPACE - (DPFIXED + sizeof(slot_t)));
}

// **********************************************************
// dump page utlity
void HFPage::dumpPage()
{
    int i;

    cout << "dumpPage, this: " << this << endl;
    cout << "curPage= " << curPage << ", nextPage=" << nextPage << endl;
    cout << "usedPtr=" << usedPtr << ",  freeSpace=" << freeSpace
         << ", slotCnt=" << slotCnt << endl;
   
    for (i=0; i < slotCnt; i++) {
        cout << "slot["<< i <<"].offset=" << slot[i].offset
             << ", slot["<< i << "].length=" << slot[i].length << endl;
    }
}

// **********************************************************
PageId HFPage::getPrevPage()
{
    // fill in the body
    return this->prevPage;
}

// **********************************************************
void HFPage::setPrevPage(PageId pageNo)
{

    // fill in the body
    this->prevPage = pageNo;
}

// **********************************************************
void HFPage::setNextPage(PageId pageNo)
{
  // fill in the body
  this->nextPage = pageNo;
}

// **********************************************************
PageId HFPage::getNextPage()
{
    // fill in the body
    return this->nextPage;

}

// **********************************************************
// Add a new record to the page. Returns OK if everything went OK
// otherwise, returns DONE if sufficient space does not exist
// RID of the new record is returned via rid parameter.
Status HFPage::insertRecord(char* recPtr, int recLen, RID& rid)
{
    // Check if we have space for this record
    if((recLen+sizeof(slot_t)) > (this->freeSpace))
        return DONE;

    // increment the slot count
    ++(this->slotCnt);  

    // first create the slot
    this->slot[(this->slotCnt)-1].length = recLen;
    this->slot[(this->slotCnt)-1].offset = (this->usedPtr)-(recLen); 

    // Make the next slot invalid
    this->slot[(this->slotCnt)].length = INVALID_SLOT;

    // Update usedPtr
    this->usedPtr = (this->usedPtr)-(recLen);

    /**
     * Adjust the metadata for this record and page.
     */
    this->freeSpace = (this->usedPtr) - ((this->slotCnt)*sizeof(slot_t));    

    /**
     * Create and initialize a new record struct.
     */
    rid.pageNo = this->curPage;  
    rid.slotNo = (this->slotCnt)-1;
    
    return OK;
}

// **********************************************************
// Delete a record from a page. Returns OK if everything went okay.
// Compacts remaining records but leaves a hole in the slot array.
// Use memmove() rather than memcpy() as space may overlap.
Status HFPage::deleteRecord(const RID& rid)
{
    PageId pno = rid.pageNo;
    int    sno = rid.slotNo;
    int next_sno = sno+1;

    if((this->slot[sno]).length == EMPTY_SLOT)
        return DONE;
    if((this->slot[sno]).length == INVALID_SLOT)
        return FAIL;

    // check if this is the last slot
    // if yes, then mark this slot empty 
    // and decrement the slotCnt.
    if(next_sno > slotCnt)
    {
        this->slot[sno].length = INVALID_SLOT;
        --(this->slotCnt);
        return OK;
    }

    int off = this->slot[sno].offset;
    int len = this->slot[sno].length;
    this->slot[sno].length = EMPTY_SLOT;
    char* destptr = (char*)this->data+off;

    // for all the recs after sno
    // - memmov by the len of sno
    // - increment the offset
    for(int i=next_sno; i < slotCnt; ++i)
    {
        (this->slot[i].offset)+=len;
        
        //char* srcptr  = (char*)this->data[other_off]; 

        memmove(destptr,destptr-len,len);

        destptr = destptr-len;

    }

    return OK;
}

// **********************************************************
// returns RID of first record on page
Status HFPage::firstRecord(RID& firstRid)
{
    // get the first slot which != empty, check if valid and return RID
    for(int i=0; i < (this->slotCnt); ++i)
    {
        //
        if(this->slot[i].length != EMPTY_SLOT)
        {
            firstRid.pageNo = (this->curPage);
            firstRid.slotNo = i;
            return OK;
        }
    }
    return DONE;
}

// **********************************************************
// returns RID of next record on the page
// returns DONE if no more records exist on the page; otherwise OK
Status HFPage::nextRecord (RID curRid, RID& nextRid)
{
    PageId pno = curRid.pageNo;
    int sno = curRid.slotNo;   

    int next_sno = sno+1;
      
    if((this->slot[next_sno]).length == INVALID_SLOT)
        return DONE;

    nextRid.pageNo = curRid.pageNo;
    if((this->slot[next_sno]).length == EMPTY_SLOT)
    {    
        if((this->slot[next_sno+1]).length == INVALID_SLOT)
            return DONE;
        nextRid.slotNo = next_sno+1;
        return OK;
    }
    nextRid.slotNo = next_sno;
    return OK;
}

// **********************************************************
// returns length and copies out record with RID rid
Status HFPage::getRecord(RID rid, char* recPtr, int& recLen)
{
    PageId pno = rid.pageNo;
    int sno = rid.slotNo;

    if((this->slot[sno].length) == EMPTY_SLOT)
        return FAIL;
    if((this->slot[sno].length) == INVALID_SLOT)
        return DONE;

    int len = this->slot[sno].length;
    int off = this->slot[sno].offset;
    // copy the record from offset upto length
    memcpy(recPtr, (char*)this->data+off, len);
    recLen = this->slot[sno].length;

    return OK;
}

// **********************************************************
// returns length and pointer to record with RID rid.  The difference
// between this and getRecord is that getRecord copies out the record
// into recPtr, while this function returns a pointer to the record
// in recPtr.
Status HFPage::returnRecord(RID rid, char*& recPtr, int& recLen)
{
    int sno = rid.slotNo;

    if(sno > this->slotCnt)
        return DONE;
    if((this->slot[sno]).length == INVALID_SLOT)
        return FAIL;

    int off = this->slot[sno].offset;
    recLen = this->slot[sno].length;
    recPtr = (char*)this->data+off;

    return OK;
}

// **********************************************************
// Returns the amount of available space on the heap file page
int HFPage::available_space(void)
{
    return ((this->freeSpace) - sizeof(slot_t));
}

// **********************************************************
// Returns 1 if the HFPage is empty, and 0 otherwise.
// It scans the slot directory looking for a non-empty slot.
bool HFPage::empty(void)
{
    // fill in the body
    if(this->slotCnt == 0)
        return true;
    else
        return false;
}



