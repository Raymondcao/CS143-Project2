#include "BTreeNode.h"
#include <string.h>
#include <cstdio>
using namespace std;



// For each leaf node, we create a integer in beginning of the node buffer to record node entry count. 
// At the end of the buffer, there is a pageid pointing to the next page.
// --------------------------------------------------------------------------------------------------
// |--count(4 bytes)--|--node entries(12 bytes for each)--|--some unused bytes--|--pageid(4 bytes)--|
// --------------------------------------------------------------------------------------------------

// For each entry of a leaf node in buffer,
// ----------------------------------------
// |--key(4 bytes)--|--RecordId(8 bytes)--|
// ----------------------------------------


/*
 * Read the content of the node from the page pid in the PageFile pf.
 * @param pid[IN] the PageId to read
 * @param pf[IN] PageFile to read from
 * @return 0 if successful. Return an error code if there is an error.
 */
RC BTLeafNode::read(PageId pid, const PageFile& pf)
{
	return pf.read(pid, buffer);
}
    
/*
 * Write the content of the node to the page pid in the PageFile pf.
 * @param pid[IN] the PageId to write to
 * @param pf[IN] PageFile to write to
 * @return 0 if successful. Return an error code if there is an error.
 */
RC BTLeafNode::write(PageId pid, PageFile& pf)
{
	return pf.write(pid, buffer);
}

/*
 * Return the number of keys stored in the node.
 * @return the number of keys in the node
 */
int BTLeafNode::getKeyCount()
{
	int count;
	memcpy(&count, buffer, sizeof(count));
	return count;
}

/*
 * Insert a (key, rid) pair to the node.
 * @param key[IN] the key to insert
 * @param rid[IN] the RecordId to insert
 * @return 0 if successful. Return an error code if the node is full.
 */
RC BTLeafNode::insert(int key, const RecordId& rid)
{
	int count = getKeyCount();

	if (count==MAX_NODE_SIZE)
		return RC_NODE_FULL;

	int startPos = sizeof(count);
	int i;
	locate(key,i);
	startPos += i*ENTRY_SIZE;

	// move other entries and insert the new entry
	memmove(buffer+startPos+ENTRY_SIZE, buffer+startPos, (count-i)*ENTRY_SIZE);
	memcpy(buffer+startPos, &key, sizeof(key));
	memcpy(buffer+startPos+sizeof(key), &rid, sizeof(rid));

	// update count;
	count++;
	memcpy(buffer, &count, sizeof(count));

	return 0;
}

/*
 * Insert the (key, rid) pair to the node
 * and split the node half and half with sibling.
 * The first key of the sibling node is returned in siblingKey.
 * @param key[IN] the key to insert.
 * @param rid[IN] the RecordId to insert.
 * @param sibling[IN] the sibling node to split with. This node MUST be EMPTY when this function is called.
 * @param siblingKey[OUT] the first key in the sibling node after split.
 * @return 0 if successful. Return an error code if there is an error.
 */
RC BTLeafNode::insertAndSplit(int key, const RecordId& rid, 
                              BTLeafNode& sibling, int& siblingKey)
{
	int count = getKeyCount();

	int eid;
	locate(key,eid);

	int leftSize = (count+2)/2;
	int rightSize = count+1 - leftSize;

	int curKey;
	RecordId curRid;
	if (eid<leftSize){
		while(count> leftSize-1){
			readEntry(--count, curKey, curRid);
			sibling.insert(curKey, curRid);
		}
		// update count
		memcpy(buffer, &count, sizeof(count));
		//insert uses old count otherwise
		insert(key, rid);
	}else{
		while(count> leftSize){
			readEntry(--count, curKey, curRid);
			sibling.insert(curKey, curRid);
		}
		sibling.insert(key, rid);
		// update count
		memcpy(buffer, &count, sizeof(count));

	}

	if (sibling.readEntry(0, curKey, curRid)<0)return RC_END_OF_TREE;
	siblingKey = curKey;

	return 0;
}

/**
 * If searchKey exists in the node, set eid to the index entry
 * with searchKey and return 0. If not, set eid to the index entry
 * immediately after the largest index key that is smaller than searchKey,
 * and return the error code RC_NO_SUCH_RECORD.
 * Remember that keys inside a B+tree node are always kept sorted.
 * @param searchKey[IN] the key to search for.
 * @param eid[OUT] the index entry number with searchKey or immediately
                   behind the largest key smaller than searchKey.
 * @return 0 if searchKey is found. Otherwise return an error code.
 */
RC BTLeafNode::locate(int searchKey, int& eid)
{
	int count = getKeyCount();
	eid = count;

	for(int i=0; i<count; i++){
		int curKey;
		memcpy(&curKey, buffer+sizeof(count)+ENTRY_SIZE*i, sizeof(curKey));
		
		if (curKey==searchKey){
			eid = i;
			return 0;
		}
		if(curKey > searchKey)
		{
			eid = i;
			return RC_NO_SUCH_RECORD;
		}
	}

	return RC_NO_SUCH_RECORD;
}

/*
 * Read the (key, rid) pair from the eid entry.
 * @param eid[IN] the entry number to read the (key, rid) pair from
 * @param key[OUT] the key from the entry
 * @param rid[OUT] the RecordId from the entry
 * @return 0 if successful. Return an error code if there is an error.
 */
RC BTLeafNode::readEntry(int eid, int& key, RecordId& rid)
{
	int count = getKeyCount();

	if (eid>=count)return RC_END_OF_TREE;
	memcpy(&key, buffer+sizeof(count)+ENTRY_SIZE*eid, sizeof(key));
	memcpy(&rid, buffer+sizeof(count)+sizeof(key)+ENTRY_SIZE*eid, sizeof(RecordId));
	return 0;
}

/*
 * Return the pid of the next slibling node.
 * @return the PageId of the next sibling node 
 */
PageId BTLeafNode::getNextNodePtr()
{
	PageId pid;
	memcpy(&pid, buffer+BUFFER_SIZE-sizeof(pid), sizeof(pid));
	return pid;
}

/*
 * Set the pid of the next slibling node.
 * @param pid[IN] the PageId of the next sibling node 
 * @return 0 if successful. Return an error code if there is an error.
 */
RC BTLeafNode::setNextNodePtr(PageId pid)
{
	memcpy(buffer+BUFFER_SIZE-sizeof(pid), &pid, sizeof(pid));
	return 0;
}

 BTLeafNode::BTLeafNode()
{
	memset(buffer, 0, sizeof(buffer));
	int count = 0;
	memcpy(buffer, &count, sizeof(count));
	setNextNodePtr(-1);
}
// For each non-leaf node, we create a integer in beginning of the node buffer to record node key count. 
// Right behind the count, there is a pageid pointing to the first child page.
// --------------------------------------------------------------------------------------------------
// |--count(4 bytes)--|--PageId(4 bytes)---|--node entries(8 bytes for each)--|--some unused bytes--|
// --------------------------------------------------------------------------------------------------

// For each entry of an non-leaf node,
// --------------------------------------
// |--PageId(4 bytes)--|--Key(4 bytes)--|
// --------------------------------------
BTNonLeafNode::BTNonLeafNode()
{
	memset(buffer, 0, sizeof(buffer));
	int count = 0;
	memcpy(buffer, &count, sizeof(count));
}

/*
 * Read the content of the node from the page pid in the PageFile pf.
 * @param pid[IN] the PageId to read
 * @param pf[IN] PageFile to read from
 * @return 0 if successful. Return an error code if there is an error.
 */
RC BTNonLeafNode::read(PageId pid, const PageFile& pf)
{ 
	return pf.read(pid, buffer);
}
    
/*
 * Write the content of the node to the page pid in the PageFile pf.
 * @param pid[IN] the PageId to write to
 * @param pf[IN] PageFile to write to
 * @return 0 if successful. Return an error code if there is an error.
 */
RC BTNonLeafNode::write(PageId pid, PageFile& pf)
{ 
	return pf.write(pid, buffer);
}

/*
 * Return the number of keys stored in the node.
 * @return the number of keys in the node
 */
int BTNonLeafNode::getKeyCount()
{
	int count;
	memcpy(&count, buffer, sizeof(count));
	return count;
}


/*
 * Insert a (key, pid) pair to the node.
 * @param key[IN] the key to insert
 * @param pid[IN] the PageId to insert
 * @return 0 if successful. Return an error code if the node is full.
 */
RC BTNonLeafNode::insert(int key, PageId pid)
{
	int count = getKeyCount();
	if (count==MAX_NODE_SIZE)return RC_NODE_FULL;

	int curKey;
	int eid;
	for(eid=0; eid<count; eid++){
		memcpy(&curKey, buffer+sizeof(count)+NONLEAF_ENTRY_SIZE*eid+sizeof(pid), sizeof(curKey));

		if (curKey>= key){
			break;
		}
	}

	// move old entries first
	memmove(buffer+sizeof(count)+sizeof(pid)+NONLEAF_ENTRY_SIZE*(eid+1), 
		buffer+sizeof(count)+sizeof(pid)+NONLEAF_ENTRY_SIZE*eid, 
		NONLEAF_ENTRY_SIZE*(count-eid));

	// insert new entry
	memcpy(buffer+sizeof(count)+sizeof(pid)+NONLEAF_ENTRY_SIZE*eid, &key, sizeof(key));
	memcpy(buffer+sizeof(count)+sizeof(pid)+NONLEAF_ENTRY_SIZE*eid+sizeof(key), &pid, sizeof(pid));

	// update count
	count++;
	memcpy(buffer, &count, sizeof(count));
	return 0; 
}

/*
 * Insert the (key, pid) pair to the node
 * and split the node half and half with sibling.
 * The middle key after the split is returned in midKey.
 * @param key[IN] the key to insert
 * @param pid[IN] the PageId to insert
 * @param sibling[IN] the sibling node to split with. This node MUST be empty when this function is called.
 * @param midKey[OUT] the key in the middle after the split. This key should be inserted to the parent node.
 * @return 0 if successful. Return an error code if there is an error.
 */
RC BTNonLeafNode::insertAndSplit(int key, PageId pid, BTNonLeafNode& sibling, int& midKey)
{
	int count = getKeyCount();
	int leftSize = (count+1)/2;
	int rightSize = count - leftSize;

	int curKey;
	int eid;
	for(eid=0; eid<count; eid++){
		memcpy(&curKey, buffer+sizeof(count)+NONLEAF_ENTRY_SIZE*eid+sizeof(pid), sizeof(curKey));

		if (curKey>= key){
			break;
		}
	}

	// since buffer size is actually larger than max_node_size, we can insert new pair first in the current buffer and then split
	// move old entries first
	memmove(buffer+sizeof(count)+sizeof(PageId)+NONLEAF_ENTRY_SIZE*(eid+1), 
		buffer+sizeof(count)+sizeof(PageId)+NONLEAF_ENTRY_SIZE*eid, 
		NONLEAF_ENTRY_SIZE*(count-eid));

	// insert new entry
	memcpy(buffer+sizeof(count)+sizeof(PageId)+NONLEAF_ENTRY_SIZE*eid, &key, sizeof(key));
	memcpy(buffer+sizeof(count)+sizeof(PageId)+NONLEAF_ENTRY_SIZE*eid+sizeof(key), &pid, sizeof(pid));

	// set midkey
	int midOffset = sizeof(count)+sizeof(PageId)+NONLEAF_ENTRY_SIZE*leftSize;
	memcpy(&midKey, buffer+midOffset, sizeof(midKey));

	// initialize sibling node
	PageId pid1, pid2;
	int firstKey;
	memcpy(&pid1, buffer+midOffset+sizeof(midKey), sizeof(pid1));
	memcpy(&firstKey, buffer+midOffset+sizeof(midKey)+sizeof(pid1), sizeof(firstKey));
	memcpy(&pid2, buffer+midOffset+sizeof(midKey)*2+sizeof(pid1), sizeof(pid2));
	sibling.initializeRoot(pid1, firstKey, pid2);

	// insert the rest of entries
	PageId newPid;
	int newKey;
	for (int i=1; i<rightSize; i++){
		memcpy(&newKey, buffer+midOffset+NONLEAF_ENTRY_SIZE*(i+1), sizeof(newKey));
		memcpy(&newPid, buffer+midOffset+NONLEAF_ENTRY_SIZE*(i+1)+sizeof(newKey), sizeof(newPid));
		sibling.insert(newKey, newPid);
	}

	// update count
	count = leftSize;
	memcpy(buffer, &count, sizeof(count));

	return 0;
}

/*
 * Given the searchKey, find the child-node pointer to follow and
 * output it in pid.
 * @param searchKey[IN] the searchKey that is being looked up.
 * @param pid[OUT] the pointer to the child node to follow.
 * @return 0 if successful. Return an error code if there is an error.
 */
RC BTNonLeafNode::locateChildPtr(int searchKey, PageId& pid)
{
	int count = getKeyCount();

	int curKey;
	for(int eid=0; eid<count; eid++){
		memcpy(&curKey, buffer+sizeof(count)+NONLEAF_ENTRY_SIZE*eid+sizeof(pid), sizeof(curKey));

		if (curKey>= searchKey){
			memcpy(&pid, buffer+sizeof(count)+NONLEAF_ENTRY_SIZE*eid, sizeof(pid));
			return 0;
		}
	}
	memcpy(&pid, buffer+sizeof(count)+NONLEAF_ENTRY_SIZE*count, sizeof(pid));
	return 0;
}

/*
 * Initialize the root node with (pid1, key, pid2).
 * @param pid1[IN] the first PageId to insert
 * @param key[IN] the key that should be inserted between the two PageIds
 * @param pid2[IN] the PageId to insert behind the key
 * @return 0 if successful. Return an error code if there is an error.
 */
RC BTNonLeafNode::initializeRoot(PageId pid1, int key, PageId pid2)
{
	// write count
	int count=1;
	memcpy(buffer, &count, sizeof(count));

	memcpy(buffer+sizeof(count), &pid1, sizeof(pid1));
	memcpy(buffer+sizeof(count)+sizeof(pid1), &key, sizeof(key));
	memcpy(buffer+sizeof(count)+sizeof(pid1)+sizeof(key), &pid2, sizeof(pid2));

	return 0;
}
