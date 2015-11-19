/*
 * Copyright (C) 2008 by The Regents of the University of California
 * Redistribution of this file is permitted under the terms of the GNU
 * Public License (GPL).
 *
 * @author Junghoo "John" Cho <cho AT cs.ucla.edu>
 * @date 3/24/2008
 */
 
#include "BTreeIndex.h"
#include "BTreeNode.h"
#include <string.h>
using namespace std;

/*
 * BTreeIndex constructor
 */
BTreeIndex::BTreeIndex()
{
    rootPid = -1;
	treeHeight = 0;
	not_read = true;
}

/*
 * Open the index file in read or write mode.
 * Under 'w' mode, the index file should be created if it does not exist.
 * @param indexname[IN] the name of the index file
 * @param mode[IN] 'r' for read, 'w' for write
 * @return error code. 0 if no error
 */
RC BTreeIndex::open(const string& indexname, char mode)
{
	RC code;
	code = pf.open(indexname, mode); 
	char buffer[PageFile::PAGE_SIZE];
	if(pf.endPid() == 0)
	{
		memset(buffer, 0, sizeof(buffer));
		PageId root = -1;
		memcpy(buffer, &root, sizeof(PageId));
		int height = 0;
		memcpy(buffer+sizeof(PageId)+1,&height, sizeof(int));
		pf.write(0, buffer);
	}
	if(mode == 'r' && not_read)
	{
		memset(buffer, 0, sizeof(buffer));
		pf.read(0, buffer);
		memcpy(&rootPid, buffer, sizeof(PageId));
		memcpy(&treeHeight, buffer + sizeof(PageId) + 1, sizeof(int));
		not_read = false;
	}
	return code;
}

/*
 * Close the index file.
 * @return error code. 0 if no error
 */
RC BTreeIndex::close()
{
    return 0;
}

/*
 * Insert (key, RecordId) pair to the index.
 * @param key[IN] the key for the value inserted into the index
 * @param rid[IN] the RecordId for the record being inserted into the index
 * @return error code. 0 if no error
 */
RC BTreeIndex::insert(int key, const RecordId& rid)
{
	BTLeafNode l;
	PageId pid;
	IndexCursor ic;
	if (rootPid != -1)
	{
	    locate(key, ic);//this also sets the path variable
	    pid = ic.pid;
	    l.read(pid, pf);
	}
	else
	{
		pid = pf.endPid();
		rootPid = pid;
		treeHeight = 1;
	}
	if(l.getKeyCount() < MAX_NODE_SIZE)
	{
		l.insert(key, rid);
		l.write(pid, pf);
	}
	else
	{
		BTLeafNode sibling;
		int sibkey;
		l.insertAndSplit(key,rid, sibling, sibkey);
		sibling.setNextNodePtr(l.getNextNodePtr());
		PageId sib_pid = pf.endPid();
		sibling.write(sib_pid, pf);
		l.setNextNodePtr(sib_pid);
		l.write(pid, pf);
		insert_into_parent(treeHeight-2, pid, sibkey, sib_pid);
	}
    return 0;
}

RC BTreeIndex::insert_into_parent(int level, PageId childpid, int key, PageId sib_pid)
{
	if(level == -1)
	{
		BTNonLeafNode root;
		root.initializeRoot(childpid,  key,  sib_pid);
		PageId r = pf.endPid();
		root.write(r, pf);
		rootPid = r;
		return 0;
	}
	BTNonLeafNode parent;
	parent.read(path[level], pf);
	if(parent.getKeyCount() < MAX_NODE_SIZE)
	{
		//inser
		parent.insert(key, sib_pid);
		parent.write(path[level], pf);
	}
	else
	{
		BTNonLeafNode sibling;
		int midkey;
		PageId psibling_pid = pf.endPid();
		parent.insertAndSplit(key, sib_pid, sibling, midkey);
		sibling.write(psibling_pid, pf);
		parent.write(path[level], pf);
		insert_into_parent(level-1, path[level], midkey, psibling_pid);
	}
	return 0;
	
}


/**
 * Run the standard B+Tree key search algorithm and identify the
 * leaf node where searchKey may exist. If an index entry with
 * searchKey exists in the leaf node, set IndexCursor to its location
 * (i.e., IndexCursor.pid = PageId of the leaf node, and
 * IndexCursor.eid = the searchKey index entry number.) and return 0.
 * If not, set IndexCursor.pid = PageId of the leaf node and
 * IndexCursor.eid = the index entry immediately after the largest
 * index key that is smaller than searchKey, and return the error
 * code RC_NO_SUCH_RECORD.
 * Using the returned "IndexCursor", you will have to call readForward()
 * to retrieve the actual (key, rid) pair from the index.
 * @param key[IN] the key to find
 * @param cursor[OUT] the cursor pointing to the index entry with
 *                    searchKey or immediately behind the largest key
 *                    smaller than searchKey.
 * @return 0 if searchKey is found. Othewise an error code
 */
RC BTreeIndex::locate(int searchKey, IndexCursor& cursor)
{
	if(rootPid == -1)
		return RC_NO_SUCH_RECORD;
	int level = 0;
	PageId pid;
	if(treeHeight > 1)
	{
		BTNonLeafNode n;
		n.read(rootPid, pf);
		path[0] = rootPid;
		while(level < treeHeight)
		{
			level++;
			if(level == treeHeight)
				break; 
			n.locateChildPtr(searchKey, pid);
			path[level] = pid;
			n.read(pid, pf);
		}
	}
	else
	{
		pid = rootPid;
	}
	//found the leaf node
	BTLeafNode l;
	l.read(pid,pf);
	int eid;
	if(l.locate(searchKey, eid) == RC_NO_SUCH_RECORD)
		return RC_NO_SUCH_RECORD;
	cursor.eid = eid;
	cursor.pid = pid;
    return 0;
}

/*
 * Read the (key, rid) pair at the location specified by the index cursor,
 * and move foward the cursor to the next entry.
 * @param cursor[IN/OUT] the cursor pointing to an leaf-node index entry in the b+tree
 * @param key[OUT] the key stored at the index cursor location.
 * @param rid[OUT] the RecordId stored at the index cursor location.
 * @return error code. 0 if no error
 */
RC BTreeIndex::readForward(IndexCursor& cursor, int& key, RecordId& rid)
{
    BTLeafNode l;
	l.read(cursor.pid, pf);
	RC code = l.readEntry(cursor.eid, key, rid);
	//
	if(cursor.eid == MAX_NODE_SIZE-1)//last entry in current leaf, move to next node.. what if there is no next node ? what to set indexcursor to ?
	{
		cursor.eid = 0;
		cursor.pid = l.getNextNodePtr();
	}
	else
		cursor.eid+=1;
}
