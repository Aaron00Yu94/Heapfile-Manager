#include "heapfile.h"
#include "error.h"

// routine to create a heapfile
const Status createHeapFile(const string fileName)
{
    File* 		file;
    Status 		status;
    FileHdrPage*	hdrPage;
    int			hdrPageNo;
    int			newPageNo;
    Page*		newPage;

    // try to open the file. This should return an error
    status = db.openFile(fileName, file);
    // if the file doesn't exist, we will get an error
    if (status != OK)
    {
		// file doesn't exist. First create it and allocate
        // an empty header page and data page.
        //
        // This function creates an empty (well, almost empty) heap file.
        // To do this create a db level file by calling db->createfile().
        status = db.createFile(fileName);
        // if create file fails, return error
        if (status != OK)
            return status;
        // open the newly created file
        status = db.openFile(fileName, file);
        // if openfile fails, return error
        if (status != OK)
            return status;
        // Then, allocate an empty page by invoking bm->allocPage() appropriately. 
        status = bufMgr->allocPage(file, hdrPageNo, newPage);
        // if allocPage fails, return error
        if (status != OK)
            return status;
        
        // As you know allocPage() will return a pointer to an empty page in the buffer 
        // pool along with the page number of the page. 
        // Take the Page* pointer returned from allocPage() and cast it to a FileHdrPage*. 
        // Using this pointer initialize the values in the header page.
        hdrPage = (FileHdrPage*) newPage;
        
        strncpy(hdrPage->fileName, fileName.c_str(), MAXNAMESIZE);
        hdrPage->fileName[MAXNAMESIZE - 1] = '\0';  
        hdrPage->pageCnt = 2;  
        hdrPage->recCnt = 0;

        // Then make a second call to bm->allocPage(). 
        // This page will be the first data page of the file
        status = bufMgr->allocPage(file, newPageNo, newPage);
        // if allocPage fails, return error
        if (status != OK)
            return status;
        // Using the Page* pointer returned, 
        // invoke its init() method to initialize the page contents.
        newPage->init(newPageNo);
        // Finally, store the page number of the data page 
        // in firstPage and lastPage attributes of the FileHdrPage.
        hdrPage->firstPage = newPageNo;
        hdrPage->lastPage  = newPageNo;
        // When you have done all this unpin both pages and mark them as dirty.
        status = bufMgr->unPinPage(file, hdrPageNo, true);
        if (status != OK)
            return status;
        status = bufMgr->unPinPage(file, newPageNo, true);
        if (status != OK)
            return status;

        return OK;	
    }
    // file already exists
    return (FILEEXISTS);
}

// routine to destroy a heapfile
const Status destroyHeapFile(const string fileName)
{
	return (db.destroyFile (fileName));
}

// constructor opens the underlying file
HeapFile::HeapFile(const string & fileName, Status& returnStatus)
{
    Status 	status;
    Page*	pagePtr;

    cout << "opening file " << fileName << endl;

    // open the file and read in the header page and the first data page
    if ((status = db.openFile(fileName, filePtr)) == OK)
    {
		
		// Next, it reads and pins the header page for the file in the buffer pool, 
        //initializing the private data members headerPage, headerPageNo, and hdrDirtyFlag.
        
        // This is what file->getFirstPage() is used for (see description of the I/O layer)
        status = filePtr->getFirstPage(headerPageNo);
        if (status != OK)
        {
            returnStatus = status;
            return;
        }
        // Read the header page into the buffer pool
        status = bufMgr->readPage(filePtr, headerPageNo, pagePtr);
        if (status != OK)
        {
            returnStatus = status;
            return;
        }
        
        // Finally, read and pin the first page of the file into the buffer pool, 
        headerPage = (FileHdrPage*) pagePtr;
        hdrDirtyFlag = false;
        int firstDataPageNo = headerPage->firstPage;
        status = bufMgr->readPage(filePtr, firstDataPageNo, pagePtr);
        if (status != OK)
        {
            returnStatus = status;
            return;
        }
        // initializing the values of curPage, curPageNo, and curDirtyFlag appropriately. 
        curPage = pagePtr;
        curPageNo = firstDataPageNo;
        curDirtyFlag = false;
        // Set curRec to NULLRID
        curRec = NULLRID;

        returnStatus = OK;
						
    }
    else
    {
    	cerr << "open of heap file failed\n";
		returnStatus = status;
		return;
    }
}

// the destructor closes the file
HeapFile::~HeapFile()
{
    Status status;
    cout << "invoking heapfile destructor on file " << headerPage->fileName << endl;

    // see if there is a pinned data page. If so, unpin it 
    if (curPage != NULL)
    {
    	status = bufMgr->unPinPage(filePtr, curPageNo, curDirtyFlag);
		curPage = NULL;
		curPageNo = 0;
		curDirtyFlag = false;
		if (status != OK) cerr << "error in unpin of date page\n";
    }
	
	 // unpin the header page
    status = bufMgr->unPinPage(filePtr, headerPageNo, hdrDirtyFlag);
    if (status != OK) cerr << "error in unpin of header page\n";
	
	// status = bufMgr->flushFile(filePtr);  // make sure all pages of the file are flushed to disk
	// if (status != OK) cerr << "error in flushFile call\n";
	// before close the file
	status = db.closeFile(filePtr);
    if (status != OK)
    {
		cerr << "error in closefile call\n";
		Error e;
		e.print (status);
    }
}

// Return number of records in heap file

const int HeapFile::getRecCnt() const
{
  return headerPage->recCnt;
}

// retrieve an arbitrary record from a file.
// if record is not on the currently pinned page, the current page
// is unpinned and the required page is read into the buffer pool
// and pinned.  returns a pointer to the record via the rec parameter

const Status HeapFile::getRecord(const RID & rid, Record & rec)
{
    Status status;
    Page *page;

    
    if(curPage != nullptr && curPageNo == rid.pageNo){
        page = curPage;
    }
    else{
        
        if(curPage != nullptr){
            status = bufMgr->unPinPage(filePtr, curPageNo, curDirtyFlag);
            if(status != OK)
                return status;
        }
        
        status = bufMgr->readPage(filePtr, rid.pageNo, page);
        if(status != OK)
            return status;
        
        curPage = page;
        curPageNo = rid.pageNo;
        curDirtyFlag = false;
    }

    
    status = page->getRecord(rid, rec);
    if(status != OK){
        if(curPageNo != rid.pageNo){
            bufMgr->unPinPage(filePtr, rid.pageNo, false);
        }
        return status;
    }

    curRec = rid;
    return OK;
}



HeapFileScan::HeapFileScan(const string & name,
			   Status & status) : HeapFile(name, status)
{
    filter = NULL;
}

const Status HeapFileScan::startScan(const int offset_,
				     const int length_,
				     const Datatype type_, 
				     const char* filter_,
				     const Operator op_)
{
    if (!filter_) {                        // no filtering requested
        filter = NULL;
        return OK;
    }
    
    if ((offset_ < 0 || length_ < 1) ||
        (type_ != STRING && type_ != INTEGER && type_ != FLOAT) ||
        (type_ == INTEGER && length_ != sizeof(int)
         || type_ == FLOAT && length_ != sizeof(float)) ||
        (op_ != LT && op_ != LTE && op_ != EQ && op_ != GTE && op_ != GT && op_ != NE))
    {
        return BADSCANPARM;
    }

    offset = offset_;
    length = length_;
    type = type_;
    filter = filter_;
    op = op_;

    return OK;
}


const Status HeapFileScan::endScan()
{
    Status status;
    // generally must unpin last page of the scan
    if (curPage != NULL)
    {
        status = bufMgr->unPinPage(filePtr, curPageNo, curDirtyFlag);
        curPage = NULL;
        curPageNo = 0;
		curDirtyFlag = false;
        return status;
    }
    return OK;
}

HeapFileScan::~HeapFileScan()
{
    endScan();
}

const Status HeapFileScan::markScan()
{
    // make a snapshot of the state of the scan
    markedPageNo = curPageNo;
    markedRec = curRec;
    return OK;
}

const Status HeapFileScan::resetScan()
{
    Status status;
    if (markedPageNo != curPageNo) 
    {
		if (curPage != NULL)
		{
			status = bufMgr->unPinPage(filePtr, curPageNo, curDirtyFlag);
			if (status != OK) return status;
		}
		// restore curPageNo and curRec values
		curPageNo = markedPageNo;
		curRec = markedRec;
		// then read the page
		status = bufMgr->readPage(filePtr, curPageNo, curPage);
		if (status != OK) return status;
		curDirtyFlag = false; // it will be clean
    }
    else curRec = markedRec;
    return OK;

}


const Status HeapFileScan::scanNext(RID& outRid)
{
    Status     status = OK;
    RID        nextRid;
    RID        tmpRid;
    int     nextPageNo;
    Record      rec;

    while (true)
    {
        if (curRec.pageNo == -1 && curRec.slotNo == -1)
            status = curPage->firstRecord(nextRid);
        else
            status = curPage->nextRecord(curRec, nextRid);
        if (status == NORECORDS || status == ENDOFPAGE)
        {
            status = bufMgr->unPinPage(filePtr, curPageNo, curDirtyFlag);
            if (status != OK) return status;
            int nextPageNo;
            status = curPage->getNextPage(nextPageNo);
            if (status != OK) return status;
            if (nextPageNo == -1)
            {
                curPage = nullptr;
                return NOMORERECS;
            }
            status = bufMgr->readPage(filePtr, nextPageNo, curPage);
            if (status != OK) return status;
            curPageNo = nextPageNo;
            curRec = NULLRID;
            curDirtyFlag = false;
        }
        else if (status != OK)
        {
            return status;
        }
        else
        {
            status = curPage->getRecord(nextRid, rec);
            if (status != OK) return status;
            if (matchRec(rec))
            {
                curRec = nextRid;
                outRid = nextRid;
                return OK;
            }
            else
            {
                curRec = nextRid;
            }
        }
    }
    return OK;
}


// returns pointer to the current record.  page is left pinned
// and the scan logic is required to unpin the page 

const Status HeapFileScan::getRecord(Record & rec)
{
    return curPage->getRecord(curRec, rec);
}

// delete record from file. 
const Status HeapFileScan::deleteRecord()
{
    Status status;

    // delete the "current" record from the page
    status = curPage->deleteRecord(curRec);
    curDirtyFlag = true;

    // reduce count of number of records in the file
    headerPage->recCnt--;
    hdrDirtyFlag = true; 
    return status;
}


// mark current page of scan dirty
const Status HeapFileScan::markDirty()
{
    curDirtyFlag = true;
    return OK;
}

const bool HeapFileScan::matchRec(const Record & rec) const
{
    // no filtering requested
    if (!filter) return true;

    // see if offset + length is beyond end of record
    // maybe this should be an error???
    if ((offset + length -1 ) >= rec.length)
	return false;

    float diff = 0;                       // < 0 if attr < fltr
    switch(type) {

    case INTEGER:
        int iattr, ifltr;                 // word-alignment problem possible
        memcpy(&iattr,
               (char *)rec.data + offset,
               length);
        memcpy(&ifltr,
               filter,
               length);
        diff = iattr - ifltr;
        break;

    case FLOAT:
        float fattr, ffltr;               // word-alignment problem possible
        memcpy(&fattr,
               (char *)rec.data + offset,
               length);
        memcpy(&ffltr,
               filter,
               length);
        diff = fattr - ffltr;
        break;

    case STRING:
        diff = strncmp((char *)rec.data + offset,
                       filter,
                       length);
        break;
    }

    switch(op) {
    case LT:  if (diff < 0.0) return true; break;
    case LTE: if (diff <= 0.0) return true; break;
    case EQ:  if (diff == 0.0) return true; break;
    case GTE: if (diff >= 0.0) return true; break;
    case GT:  if (diff > 0.0) return true; break;
    case NE:  if (diff != 0.0) return true; break;
    }

    return false;
}

InsertFileScan::InsertFileScan(const string & name,
                               Status & status) : HeapFile(name, status)
{
  //Do nothing. Heapfile constructor will bread the header page and the first
  // data page of the file into the buffer pool
}

InsertFileScan::~InsertFileScan()
{
    Status status;
    // unpin last page of the scan
    if (curPage != NULL)
    {
        status = bufMgr->unPinPage(filePtr, curPageNo, true);
        curPage = NULL;
        curPageNo = 0;
        if (status != OK) cerr << "error in unpin of data page\n";
    }
}

// Insert a record into the file
const Status InsertFileScan::insertRecord(const Record & rec, RID& outRid)
{
    Page*	newPage;
    int		newPageNo;
    Status	status, unpinstatus;
    RID		rid;

    // check for very large records
    if ((unsigned int) rec.length > PAGESIZE-DPFIXED)
    {
        // will never fit on a page, so don't even bother looking
        return INVALIDRECLEN;
    }

    // First, attempt to insert the record into the current page.
    status = curPage->insertRecord(rec, rid);
    if (status == OK)
    {
        outRid = rid;
        headerPage->recCnt++;
        hdrDirtyFlag = true;
        curDirtyFlag = true;
        return OK;
    }
    else if (status == NOSPACE)
    {
        // The current page is full, so we need to allocate a new page.
        status = bufMgr->allocPage(filePtr, newPageNo, newPage);
        if (status != OK)
            return status;
        // Initialize the new page by invoking its init() method.
        newPage->init(newPageNo);
        // Update the current (last) page's nextPage pointer to link the new page.
        status = curPage->setNextPage(newPageNo);
        if (status != OK)
            return status;
        // Unpin the old current page since it is now complete.
        status = bufMgr->unPinPage(filePtr, curPageNo, true);
        if (status != OK)
            return status;
        // Update the header page: set the new page as the last data page and increment the page count.
        headerPage->lastPage = newPageNo;
        headerPage->pageCnt++;
        hdrDirtyFlag = true;
        // Set the new page as the current page.
        curPage = newPage;
        curPageNo = newPageNo;
        curDirtyFlag = false; // New page is freshly allocated and clean.
        // Now, try to insert the record into the new current page.
        status = curPage->insertRecord(rec, rid);
        if (status != OK)
            return status;
        outRid = rid;
        headerPage->recCnt++;
        hdrDirtyFlag = true;
        curDirtyFlag = true;
        return OK;
    }
    else
    {
        // For any other error, propagate the error code.
        return status;
    }
}



